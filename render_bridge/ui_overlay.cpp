/**
 * ui_overlay.cpp — Full-screen UI overlay for dialogs/buttons/messages.
 *
 * Snapshots the ENTIRE HidPage after draw list replay (world-only state).
 * Diffs with HidPage after Buttons/Messages/ActionMenu draw.
 * Changed pixels = UI overlay. Rendered as FULL SCREEN GL quad on top
 * of everything (tactical, sidebar, tab) with palette shader + transparency.
 *
 * This ensures dialogs, options, and any UI element renders correctly
 * regardless of where it appears on screen.
 */

#include "render_bridge.h"
#include "function.h"
#include <cstring>
#include <cstdlib>

// Full HidPage snapshot (world-only state)
static uint8_t* g_world_snap = nullptr;
static int      g_snap_w = 0;
static int      g_snap_h = 0;
static int      g_snap_alloc = 0;

// UI overlay (diff: changed pixels)
static uint8_t* g_ui_overlay = nullptr;
static bool     g_ui_has_content = false;

void Render_Bridge_Snapshot_World(GraphicViewPortClass& page)
{
    extern bool InMainLoop;
    if (!InMainLoop) return;

    uint8_t* buf = static_cast<uint8_t*>(page.Get_Buffer());
    if (!buf) return;
    int w = page.Get_Width();
    int h = page.Get_Height();
    int pitch = page.Get_Full_Pitch();
    if (w <= 0 || h <= 0) return;

    int needed = w * h;
    if (!g_world_snap || g_snap_alloc < needed) {
        free(g_world_snap);
        free(g_ui_overlay);
        g_world_snap = static_cast<uint8_t*>(malloc(needed));
        g_ui_overlay = static_cast<uint8_t*>(malloc(needed));
        g_snap_alloc = needed;
    }
    g_snap_w = w;
    g_snap_h = h;

    // Copy entire HidPage
    for (int row = 0; row < h; row++) {
        memcpy(g_world_snap + row * w, buf + row * pitch, w);
    }
}

void Render_Bridge_Capture_UI_Overlay(GraphicViewPortClass& page)
{
    extern bool InMainLoop;
    if (!InMainLoop || !g_world_snap || !g_ui_overlay) {
        g_ui_has_content = false;
        return;
    }

    uint8_t* buf = static_cast<uint8_t*>(page.Get_Buffer());
    if (!buf) { g_ui_has_content = false; return; }
    int w = g_snap_w;
    int h = g_snap_h;
    int pitch = page.Get_Full_Pitch();
    if (w <= 0 || h <= 0) { g_ui_has_content = false; return; }

    bool has_ui = false;
    for (int row = 0; row < h; row++) {
        const uint8_t* cur = buf + row * pitch;
        const uint8_t* snap = g_world_snap + row * w;
        uint8_t* overlay = g_ui_overlay + row * w;

        for (int col = 0; col < w; col++) {
            if (cur[col] != snap[col]) {
                overlay[col] = cur[col];
                has_ui = true;
            } else {
                overlay[col] = 0; // transparent
            }
        }
    }

    g_ui_has_content = has_ui;
}

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
