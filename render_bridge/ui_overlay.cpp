/**
 * ui_overlay.cpp — Separate UI overlay buffer for dialogs/buttons/messages.
 *
 * After draw list replay (world only), HidPage tactical area is snapshotted.
 * After Buttons/Messages draw, HidPage is diffed with the snapshot.
 * Changed pixels = UI overlay (dialogs, buttons, health bars, messages).
 * Unchanged pixels = 0 (transparent in the overlay).
 *
 * The overlay is uploaded as a GL indexed texture and rendered on top
 * of the native tactical texture with palette shader + transparency.
 */

#include "render_bridge.h"
#include "function.h"
#include "dbg.h"
#include <cstring>
#include <cstdlib>

// World-only snapshot of HidPage tactical area
static uint8_t* g_world_snapshot = nullptr;
static int      g_snap_w = 0;
static int      g_snap_h = 0;
static int      g_snap_alloc = 0;

// UI overlay: pixels that changed after Buttons/Messages drew
static uint8_t* g_ui_overlay = nullptr;
static bool     g_ui_has_content = false;

/// Snapshot the tactical area of HidPage (after draw list replay, before UI).
void Render_Bridge_Snapshot_World(GraphicViewPortClass& page)
{
    extern bool InMainLoop;
    if (!InMainLoop) return;

    uint8_t* buf = static_cast<uint8_t*>(page.Get_Buffer());
    if (!buf) return;
    int pitch = page.Get_Full_Pitch();

    int tac_x = Map.TacPixelX;
    int tac_y = Map.TacPixelY;
    int tac_w = Lepton_To_Pixel(Map.TacLeptonWidth);
    int tac_h = Lepton_To_Pixel(Map.TacLeptonHeight);
    if (tac_w <= 0 || tac_h <= 0) return;

    int needed = tac_w * tac_h;
    if (!g_world_snapshot || g_snap_alloc < needed) {
        free(g_world_snapshot);
        free(g_ui_overlay);
        g_world_snapshot = static_cast<uint8_t*>(malloc(needed));
        g_ui_overlay     = static_cast<uint8_t*>(malloc(needed));
        g_snap_alloc = needed;
    }
    g_snap_w = tac_w;
    g_snap_h = tac_h;

    // Copy tactical area from HidPage
    for (int row = 0; row < tac_h; row++) {
        memcpy(g_world_snapshot + row * tac_w,
               buf + (tac_y + row) * pitch + tac_x,
               tac_w);
    }
}

/// Diff HidPage with world snapshot. Changed pixels = UI overlay.
void Render_Bridge_Capture_UI_Overlay(GraphicViewPortClass& page)
{
    extern bool InMainLoop;
    if (!InMainLoop || !g_world_snapshot || !g_ui_overlay) {
        g_ui_has_content = false;
        return;
    }

    uint8_t* buf = static_cast<uint8_t*>(page.Get_Buffer());
    if (!buf) { g_ui_has_content = false; return; }
    int pitch = page.Get_Full_Pitch();

    int tac_x = Map.TacPixelX;
    int tac_y = Map.TacPixelY;
    int tac_w = g_snap_w;
    int tac_h = g_snap_h;
    if (tac_w <= 0 || tac_h <= 0) { g_ui_has_content = false; return; }

    bool has_ui = false;
    for (int row = 0; row < tac_h; row++) {
        const uint8_t* cur = buf + (tac_y + row) * pitch + tac_x;
        const uint8_t* snap = g_world_snapshot + row * tac_w;
        uint8_t* overlay = g_ui_overlay + row * tac_w;

        for (int col = 0; col < tac_w; col++) {
            if (cur[col] != snap[col]) {
                overlay[col] = cur[col]; // UI pixel
                has_ui = true;
            } else {
                overlay[col] = 0; // transparent (palette index 0)
            }
        }
    }

    g_ui_has_content = has_ui;
}

/// Get the UI overlay buffer for GL upload.
/// Returns null if no UI overlay this frame.
const uint8_t* Render_Bridge_Get_UI_Overlay(int& w, int& h)
{
    if (!g_ui_has_content || !g_ui_overlay) {
        w = 0; h = 0;
        return nullptr;
    }
    w = g_snap_w;
    h = g_snap_h;
    return g_ui_overlay;
}
