/**
 * zoom_tactical.cpp — Draw list recording and zoomed replay.
 *
 * Called from GScreenClass::Render() around Draw_It():
 *   Render_Bridge_Begin_Draw_List()  — start recording
 *   Draw_It()                        — engine renders + draw list captures CC_Draw_Shape calls
 *   Render_Bridge_End_Draw_List()    — apply zoom in-place, log stats
 *
 * The draw list records CC_Draw_Shape calls alongside normal rendering.
 * When zoomed, the tactical area in HidPage is zoomed in-place after
 * Draw_It completes. Buttons/Messages/ActionMenu draw after this at 1:1.
 */

#include "render_bridge.h"
#include "draw_list.h"
#include "function.h"
#include "dbg.h"

void Render_Bridge_Begin_Draw_List()
{
    g_draw_list.Clear();
    g_draw_list.SetRecording(true);
}

void Render_Bridge_End_Draw_List(GraphicViewPortClass& page)
{
    g_draw_list.SetRecording(false);

    float zoom = Render_Bridge_Get_Zoom_Level();

    static int frame_count = 0;
    if (++frame_count % 300 == 1) {
        DBG("draw_list: %d commands, zoom=%.2f", g_draw_list.Command_Count(), zoom);
    }

    // At zoom 1.0, draw list is just a record — no transform needed
    if (zoom == 1.0f) return;

    // Apply in-place zoom to the tactical area in HidPage (8-bit indexed)
    uint8_t* buf = static_cast<uint8_t*>(page.Get_Buffer());
    if (!buf) return;
    int pitch = page.Get_Full_Pitch();

    int tac_x = Map.TacPixelX;
    int tac_y = Map.TacPixelY;
    int tac_w = Lepton_To_Pixel(Map.TacLeptonWidth);
    int tac_h = Lepton_To_Pixel(Map.TacLeptonHeight);
    if (tac_w <= 0 || tac_h <= 0) return;

    float vp_x = Render_Bridge_Get_Viewport_X();
    float vp_y = Render_Bridge_Get_Viewport_Y();

    // Temp buffer for zoomed result
    static uint8_t* tmp = nullptr;
    static int tmp_size = 0;
    int needed = tac_w * tac_h;
    if (!tmp || tmp_size < needed) {
        free(tmp);
        tmp = static_cast<uint8_t*>(malloc(needed));
        tmp_size = needed;
    }
    if (!tmp) return;

    // Nearest-neighbor zoom: source → temp
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
            if (sx < 0 || sx >= tac_w)
                dp[col] = 0;
            else
                dp[col] = sp[sx];
        }
    }

    // Write zoomed result back to HidPage
    for (int row = 0; row < tac_h; row++) {
        memcpy(buf + (tac_y + row) * pitch + tac_x, tmp + row * tac_w, tac_w);
    }
}
