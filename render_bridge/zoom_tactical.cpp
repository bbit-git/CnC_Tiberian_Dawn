/**
 * zoom_tactical.cpp — Draw list recording, replay, and tactical zoom.
 *
 * Flow:
 *   Begin_Draw_List()  — start recording, CC_Draw_Shape + Draw_Stamp deferred
 *   Draw_It()          — terrain + sprites captured in draw list, not rendered
 *   End_Draw_List()    — replay at 1:1, then zoom in-place
 *   Buttons/Messages   — drawn after at 1:1 (unaffected by zoom)
 *
 * Replay renders terrain + sprites at their original positions (1:1).
 * Then the tactical area is zoomed in-place (nearest-neighbor pixel scale).
 * This produces correct zoomed output while keeping the draw list as the
 * foundation for future GL-based rendering with proper sprite scaling.
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

/// Replay all recorded commands at original 1:1 positions to HidPage.
static void replay_draw_list()
{
    for (int i = 0; i < g_draw_list.Command_Count(); i++) {
        const DrawCommand& cmd = g_draw_list.Get(i);

        switch (cmd.type) {
        case CMD_SHAPE: {
            const ShapeCmd& s = cmd.shape;
            CC_Draw_Shape(s.shapefile, s.shapenum, s.x, s.y,
                          static_cast<WindowNumberType>(s.window),
                          static_cast<ShapeFlags_Type>(s.flags),
                          s.fadingdata, s.ghostdata);
            break;
        }
        case CMD_STAMP: {
            const StampCmd& s = cmd.stamp;
            LogicPage->Draw_Stamp(s.icondata, s.icon, s.x, s.y, s.remap, s.window);
            break;
        }
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

    static int frame_count = 0;
    if (++frame_count % 300 == 1) {
        int shapes = 0, stamps = 0;
        for (int i = 0; i < g_draw_list.Command_Count(); i++) {
            if (g_draw_list.Get(i).type == CMD_SHAPE) shapes++;
            if (g_draw_list.Get(i).type == CMD_STAMP) stamps++;
        }
        DBG("draw_list: %d cmds (%d stamps, %d shapes), zoom=%.2f",
            g_draw_list.Command_Count(), stamps, shapes,
            Render_Bridge_Get_Zoom_Level());
    }

    // Step 1: Replay all terrain + sprites at 1:1 to HidPage
    replay_draw_list();

    // Step 2: Zoom tactical area in-place (only when zoomed)
    float zoom = Render_Bridge_Get_Zoom_Level();
    if (zoom != 1.0f) {
        float vp_x = Render_Bridge_Get_Viewport_X();
        float vp_y = Render_Bridge_Get_Viewport_Y();
        zoom_tactical_inplace(page, zoom, vp_x, vp_y);
    }
}
