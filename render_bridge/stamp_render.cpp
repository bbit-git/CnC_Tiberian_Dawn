/**
 * stamp_render.cpp — Scaled terrain tile rendering.
 *
 * Renders Draw_Stamp (terrain icon tiles) at zoomed size directly
 * to HidPage, replacing the two-step "render 1:1 then pixel-zoom"
 * approach. Each tile is decoded from the icon set and scaled
 * with nearest-neighbor interpolation.
 */

#include "render_bridge.h"
#include "draw_list.h"
#include "function.h"

/// Blit an 8-bit indexed terrain tile with scaling and optional remap.
static void blit_tile_scaled(
    const uint8_t* src, int src_w, int src_h,
    const uint8_t* remap_table,
    bool has_transparency,
    uint8_t* dst, int dst_pitch, int dst_w, int dst_h,
    int dst_x, int dst_y, float scale)
{
    int out_w = static_cast<int>(src_w * scale);
    int out_h = static_cast<int>(src_h * scale);
    if (out_w <= 0 || out_h <= 0) return;

    for (int row = 0; row < out_h; row++) {
        int py = dst_y + row;
        if (py < 0 || py >= dst_h) continue;

        int sy = row * src_h / out_h;
        if (sy >= src_h) sy = src_h - 1;
        const uint8_t* sp = src + sy * src_w;
        uint8_t* dp = dst + py * dst_pitch;

        for (int col = 0; col < out_w; col++) {
            int px = dst_x + col;
            if (px < 0 || px >= dst_w) continue;

            int sx = col * src_w / out_w;
            if (sx >= src_w) sx = src_w - 1;

            uint8_t pixel = sp[sx];
            if (has_transparency && pixel == 0) continue;
            if (remap_table) pixel = remap_table[pixel];
            dp[px] = pixel;
        }
    }
}

/// Render a terrain tile at zoomed position and size.
bool Render_Bridge_Draw_Stamp_Scaled(const StampCmd& cmd, float zoom,
                                      float vp_x, float vp_y)
{
    if (!cmd.icondata) return false;

    const IControl_Type* ictl = static_cast<const IControl_Type*>(cmd.icondata);
    int icon_w = ictl->Width;
    int icon_h = ictl->Height;
    if (icon_w <= 0 || icon_h <= 0) return false;

    unsigned char* icons_p = IControl_Icons(cmd.icondata);
    unsigned char* map_p = IControl_Map(cmd.icondata);
    if (!icons_p) return false;

    // Map logical icon → physical
    int physical = cmd.icon;
    if (map_p) {
        unsigned char mapped = map_p[cmd.icon];
        if (mapped == 0xFF) return true; // transparent, skip
        physical = mapped;
    }

    const uint8_t* src = &icons_p[physical * icon_w * icon_h];

    // Transparency check
    bool has_transparency = false;
    if (ictl->TransFlag) {
        const uint8_t* trans_flags = static_cast<const uint8_t*>(cmd.icondata) + ictl->TransFlag;
        has_transparency = (trans_flags[physical] != 0);
    }

    const uint8_t* remap_table = static_cast<const uint8_t*>(cmd.remap);

    // Target buffer
    uint8_t* dst = static_cast<uint8_t*>(LogicPage->Get_Buffer());
    if (!dst) return false;
    int dst_pitch = LogicPage->Get_Full_Pitch();
    int dst_w = LogicPage->Get_Width();
    int dst_h = LogicPage->Get_Height();

    // Window Y offset (tactical window)
    extern int WindowList[][8];
    int win_y = WindowList[cmd.window][1];

    // Zoomed position
    float x = static_cast<float>(cmd.x);
    float y = static_cast<float>(cmd.y);

    if (zoom != 1.0f) {
        x = (x - vp_x) * zoom;
        y = (y - vp_y) * zoom;
    }

    int screen_x = static_cast<int>(x);
    int screen_y = win_y + static_cast<int>(y);

    blit_tile_scaled(src, icon_w, icon_h, remap_table, has_transparency,
                     dst, dst_pitch, dst_w, dst_h,
                     screen_x, screen_y, zoom);

    return true;
}
