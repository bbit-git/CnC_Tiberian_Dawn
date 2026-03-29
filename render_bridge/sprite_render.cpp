/**
 * sprite_render.cpp — Scaled sprite rendering via ISpriteProvider.
 *
 * Replaces the CC_Draw_Shape → Buffer_Frame_To_Page path during
 * draw list replay. Uses LegacySpriteProvider to get decompressed
 * pixel data, then blits with nearest-neighbor scaling for zoom.
 *
 * At zoom 1.0, produces identical output to CC_Draw_Shape.
 * At zoom > 1.0, sprites are rendered at zoomed size directly
 * instead of the two-step "render 1:1 then pixel-scale buffer".
 */

#include "render_bridge.h"
#include "draw_list.h"
#include "legacy_sprite_provider.h"
#include "function.h"

static LegacySpriteProvider g_legacy_provider;

/// Blit an 8-bit indexed sprite to a buffer with scaling and transparency.
/// src: indexed pixel data (pitch = src_w)
/// dst: target buffer
/// Handles SHAPE_CENTER flag and clips to dst bounds.
static void blit_indexed_scaled(
    const uint8_t* src, int src_w, int src_h,
    uint8_t* dst, int dst_pitch, int dst_w, int dst_h,
    int dst_x, int dst_y, float scale, bool center, bool transparent)
{
    int out_w = static_cast<int>(src_w * scale);
    int out_h = static_cast<int>(src_h * scale);
    if (out_w <= 0 || out_h <= 0) return;

    if (center) {
        dst_x -= out_w / 2;
        dst_y -= out_h / 2;
    }

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
            if (transparent && pixel == 0) continue;
            dp[px] = pixel;
        }
    }
}

/// Render a shape command via ISpriteProvider with optional zoom scaling.
/// Returns true if rendered, false if fallback to CC_Draw_Shape is needed.
bool Render_Bridge_Draw_Shape_Scaled(const ShapeCmd& cmd, float zoom,
                                      float vp_x, float vp_y)
{
    if (!cmd.shapefile) return false;

    SpriteFrame frame;
    if (!g_legacy_provider.Get_Frame(cmd.shapefile, cmd.shapenum, frame))
        return false;

    if (!frame.pixels || frame.width <= 0 || frame.height <= 0)
        return false;

    // Get target buffer (LogicPage = HidPage during Render)
    uint8_t* dst = static_cast<uint8_t*>(LogicPage->Get_Buffer());
    if (!dst) return false;
    int dst_pitch = LogicPage->Get_Full_Pitch();
    int dst_w = LogicPage->Get_Width();
    int dst_h = LogicPage->Get_Height();

    // Window offset (tactical window origin)
    extern int WindowList[][8];
    int win_x = WindowList[cmd.window][0] << 3; // WINDOWX in byte units → pixels
    int win_y = WindowList[cmd.window][1];       // WINDOWY in pixels

    // Sprite position in screen space
    float x = static_cast<float>(cmd.x);
    float y = static_cast<float>(cmd.y);

    if (zoom != 1.0f) {
        x = (x - vp_x) * zoom;
        y = (y - vp_y) * zoom;
    }

    int screen_x = win_x + static_cast<int>(x);
    int screen_y = win_y + static_cast<int>(y);

    bool center = (cmd.flags & SHAPE_CENTER) != 0;
    bool transparent = true; // Always use transparency for sprites

    blit_indexed_scaled(
        static_cast<const uint8_t*>(frame.pixels), frame.width, frame.height,
        dst, dst_pitch, dst_w, dst_h,
        screen_x, screen_y, zoom, center, transparent);

    return true;
}
