/**
 * zoom_tactical.cpp — Draw list recording, replay with world/overlay separation.
 *
 * At zoom 1.0:
 *   Replay all commands at original positions (identical to legacy).
 *
 * At zoom > 1.0:
 *   1. Replay WORLD (terrain + sprites) at 1:1
 *   2. Pixel-zoom the world in-place (nearest-neighbor)
 *   3. Replay OVERLAYS (health bars, selection) at zoomed POSITIONS
 *      but native pixel SIZE — bars stay readable, positioned correctly
 *
 * Buttons/Messages/ActionMenu draw after this at 1:1, unaffected.
 */

#include "render_bridge.h"
#include "draw_list.h"
#include "function.h"
#include "dbg.h"

extern void CC_Draw_Shape(void const* shapefile, int shapenum, int x, int y,
                          WindowNumberType window, ShapeFlags_Type flags,
                          void const* fadingdata, void const* ghostdata);

void Render_Bridge_Begin_Draw_List()
{
    g_draw_list.Clear();
    g_draw_list.SetRecording(true);
}

/// Replay world commands (terrain + sprites) at 1:1.
static void replay_world()
{
    for (int i = 0; i < g_draw_list.Command_Count(); i++) {
        const DrawCommand& cmd = g_draw_list.Get(i);

        if (cmd.type == CMD_STAMP) {
            const StampCmd& s = cmd.stamp;
            LogicPage->Draw_Stamp(s.icondata, s.icon, s.x, s.y, s.remap, s.window);
        } else if (cmd.type == CMD_SHAPE) {
            const ShapeCmd& s = cmd.shape;
            CC_Draw_Shape(s.shapefile, s.shapenum, s.x, s.y,
                          static_cast<WindowNumberType>(s.window),
                          static_cast<ShapeFlags_Type>(s.flags),
                          s.fadingdata, s.ghostdata);
        }
    }
}

/// Replay overlay commands with zoom-transformed positions.
/// Coordinates are transformed: (x - viewport) * zoom
/// Sizes stay at 1:1 — health bars and selection boxes remain readable.
static void replay_overlays_zoomed(float zoom, float vp_x, float vp_y)
{
    auto tx = [zoom, vp_x](int x) -> int {
        return static_cast<int>((static_cast<float>(x) - vp_x) * zoom);
    };
    auto ty = [zoom, vp_y](int y) -> int {
        return static_cast<int>((static_cast<float>(y) - vp_y) * zoom);
    };

    for (int i = 0; i < g_draw_list.Command_Count(); i++) {
        const DrawCommand& cmd = g_draw_list.Get(i);

        switch (cmd.type) {
        case CMD_FILL_RECT:
            LogicPage->Fill_Rect(tx(cmd.prim.x1), ty(cmd.prim.y1),
                                  tx(cmd.prim.x2), ty(cmd.prim.y2),
                                  cmd.prim.color);
            break;
        case CMD_DRAW_RECT:
            LogicPage->Draw_Rect(tx(cmd.prim.x1), ty(cmd.prim.y1),
                                  tx(cmd.prim.x2), ty(cmd.prim.y2),
                                  cmd.prim.color);
            break;
        case CMD_DRAW_LINE:
            LogicPage->Draw_Line(tx(cmd.prim.x1), ty(cmd.prim.y1),
                                  tx(cmd.prim.x2), ty(cmd.prim.y2),
                                  cmd.prim.color);
            break;
        case CMD_PUT_PIXEL:
            LogicPage->Put_Pixel(tx(cmd.prim.x1), ty(cmd.prim.y1),
                                  cmd.prim.color);
            break;
        default:
            break;
        }
    }
}

/// Replay overlay commands at original 1:1 positions.
static void replay_overlays()
{
    for (int i = 0; i < g_draw_list.Command_Count(); i++) {
        const DrawCommand& cmd = g_draw_list.Get(i);

        switch (cmd.type) {
        case CMD_FILL_RECT:
            LogicPage->Fill_Rect(cmd.prim.x1, cmd.prim.y1,
                                  cmd.prim.x2, cmd.prim.y2, cmd.prim.color);
            break;
        case CMD_DRAW_RECT:
            LogicPage->Draw_Rect(cmd.prim.x1, cmd.prim.y1,
                                  cmd.prim.x2, cmd.prim.y2, cmd.prim.color);
            break;
        case CMD_DRAW_LINE:
            LogicPage->Draw_Line(cmd.prim.x1, cmd.prim.y1,
                                  cmd.prim.x2, cmd.prim.y2, cmd.prim.color);
            break;
        case CMD_PUT_PIXEL:
            LogicPage->Put_Pixel(cmd.prim.x1, cmd.prim.y1, cmd.prim.color);
            break;
        default:
            break;
        }
    }
}

/// Zoom the tactical area in HidPage in-place (8-bit nearest-neighbor).
static void zoom_tactical_inplace(GraphicViewPortClass& page,
                                   float zoom, float vp_x, float vp_y)
{
    int tac_x = Map.TacPixelX;
    int tac_y = Map.TacPixelY;
    int tac_w = Lepton_To_Pixel(Map.TacLeptonWidth);
    int tac_h = Lepton_To_Pixel(Map.TacLeptonHeight);
    if (tac_w <= 0 || tac_h <= 0) return;

    uint8_t* buf = static_cast<uint8_t*>(page.Get_Buffer());
    if (!buf) return;
    int pitch = page.Get_Full_Pitch();

    static uint8_t* tmp = nullptr;
    static int tmp_size = 0;
    int needed = tac_w * tac_h;
    if (!tmp || tmp_size < needed) {
        free(tmp);
        tmp = static_cast<uint8_t*>(malloc(needed));
        tmp_size = needed;
    }
    if (!tmp) return;

    for (int row = 0; row < tac_h; row++) {
        int sy = static_cast<int>(vp_y + static_cast<float>(row) / zoom);
        uint8_t* dp = tmp + row * tac_w;
        if (sy < 0 || sy >= tac_h) {
            memset(dp, 0, tac_w);
            continue;
        }
        const uint8_t* sp = buf + (tac_y + sy) * pitch + tac_x;
        for (int col = 0; col < tac_w; col++) {
            int sx = static_cast<int>(vp_x + static_cast<float>(col) / zoom);
            dp[col] = (sx >= 0 && sx < tac_w) ? sp[sx] : 0;
        }
    }

    for (int row = 0; row < tac_h; row++) {
        memcpy(buf + (tac_y + row) * pitch + tac_x, tmp + row * tac_w, tac_w);
    }
}

void Render_Bridge_End_Draw_List(GraphicViewPortClass& page)
{
    g_draw_list.SetRecording(false);

    float zoom = Render_Bridge_Get_Zoom_Level();

    static int frame_count = 0;
    if (++frame_count % 300 == 1) {
        int stamps = 0, shapes = 0, prims = 0;
        for (int i = 0; i < g_draw_list.Command_Count(); i++) {
            switch (g_draw_list.Get(i).type) {
                case CMD_STAMP: stamps++; break;
                case CMD_SHAPE: shapes++; break;
                default: prims++; break;
            }
        }
        DBG("draw_list: %d cmds (%d stamps, %d shapes, %d prims), zoom=%.2f",
            g_draw_list.Command_Count(), stamps, shapes, prims, zoom);
    }

    if (zoom != 1.0f) {
        float vp_x = Render_Bridge_Get_Viewport_X();
        float vp_y = Render_Bridge_Get_Viewport_Y();

        // Step 1: Replay world (terrain + sprites) at 1:1
        replay_world();

        // Step 2: Pixel-zoom the world in-place
        zoom_tactical_inplace(page, zoom, vp_x, vp_y);

        // Step 3: Replay overlays at zoomed positions, native pixel size
        replay_overlays_zoomed(zoom, vp_x, vp_y);
    } else {
        // No zoom: replay everything at 1:1 (identical to legacy)
        replay_world();
        replay_overlays();
    }
}
