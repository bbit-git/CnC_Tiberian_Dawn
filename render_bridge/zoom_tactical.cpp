/**
 * zoom_tactical.cpp — In-place zoom of the tactical area in HidPage.
 *
 * Called from GScreenClass::Render() after Draw_It() (world + objects)
 * but before Buttons/Messages/ActionMenu (UI overlays).
 *
 * Reads the tactical rectangle from HidPage, applies nearest-neighbor
 * zoom with viewport offset, writes the zoomed result back. UI overlays
 * drawn afterwards land on top at 1:1 — unaffected by zoom.
 */

#include "render_bridge.h"
#include "function.h"

extern float g_scroll_zoom_delta;
extern int   g_mouse_x, g_mouse_y;

/// In-place zoom of the tactical area within a GraphicViewPortClass buffer.
/// The buffer is 8-bit indexed — zoom operates on palette indices directly.
void Render_Bridge_Zoom_Tactical(GraphicViewPortClass& page)
{
    float zoom = Render_Bridge_Get_Zoom_Level();
    if (zoom == 1.0f) return;

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

    // Allocate a temp buffer for the zoomed tactical area (8-bit indexed)
    static uint8_t* tmp = nullptr;
    static int tmp_size = 0;
    int needed = tac_w * tac_h;
    if (!tmp || tmp_size < needed) {
        free(tmp);
        tmp = static_cast<uint8_t*>(malloc(needed));
        tmp_size = needed;
    }
    if (!tmp) return;

    // Nearest-neighbor zoom: sample source pixels into temp buffer
    for (int row = 0; row < tac_h; row++) {
        int sy = static_cast<int>(vp_y + static_cast<float>(row) / zoom);
        uint8_t* dp = tmp + row * tac_w;

        if (sy < 0 || sy >= tac_h) {
            memset(dp, 0, tac_w); // black (palette index 0)
            continue;
        }

        const uint8_t* sp = buf + (tac_y + sy) * pitch + tac_x;

        for (int col = 0; col < tac_w; col++) {
            int sx = static_cast<int>(vp_x + static_cast<float>(col) / zoom);
            if (sx < 0 || sx >= tac_w) {
                dp[col] = 0;
            } else {
                dp[col] = sp[sx];
            }
        }
    }

    // Write zoomed result back to HidPage tactical area
    for (int row = 0; row < tac_h; row++) {
        uint8_t* dp = buf + (tac_y + row) * pitch + tac_x;
        memcpy(dp, tmp + row * tac_w, tac_w);
    }
}
