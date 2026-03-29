/**
 * sprite_render.cpp — Scaled sprite rendering via ISpriteProvider.
 *
 * Replaces CC_Draw_Shape → Buffer_Frame_To_Page during draw list replay.
 * Uses LegacySpriteProvider to get decoded pixel data, then blits with
 * nearest-neighbor scaling. Supports transparency, fading, and ghost modes.
 */

#include "render_bridge.h"
#include "draw_list.h"
#include "legacy_sprite_provider.h"
#include "function.h"

static LegacySpriteProvider g_legacy_provider;

/// Blit an 8-bit indexed sprite to a buffer with scaling and rendering modes.
static void blit_indexed_scaled(
    const uint8_t* src, int src_w, int src_h,
    uint8_t* dst, int dst_pitch, int dst_w, int dst_h,
    int dst_x, int dst_y, float scale,
    bool center, bool transparent,
    const uint8_t* ghost_table,     // 256-byte translucency lookup + 256*N blend table
    const uint8_t* fading_table,    // 256-byte color remap
    int fading_count)               // number of fading passes
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

            // Ghost mode: translucency blend with destination
            if (ghost_table) {
                uint8_t trans_idx = ghost_table[pixel];
                if (trans_idx != 0xFF) {
                    pixel = ghost_table[256 + (static_cast<unsigned>(trans_idx) << 8) + dp[px]];
                }
            }

            // Fading mode: apply color remap table N times
            if (fading_table && fading_count > 0) {
                for (int f = 0; f < fading_count; f++) {
                    pixel = fading_table[pixel];
                }
            }

            dp[px] = pixel;
        }
    }
}

/// Render a shape command via ISpriteProvider with zoom scaling.
bool Render_Bridge_Draw_Shape_Scaled(const ShapeCmd& cmd, float zoom,
                                      float vp_x, float vp_y)
{
    if (!cmd.shapefile) return false;

    // Skip predator effect (shimmer reads from destination — can't scale easily)
    if (cmd.flags & SHAPE_PREDATOR) return false;

    SpriteFrame frame;
    if (!g_legacy_provider.Get_Frame(cmd.shapefile, cmd.shapenum, frame))
        return false;

    if (!frame.pixels || frame.width <= 0 || frame.height <= 0)
        return false;

    uint8_t* dst = static_cast<uint8_t*>(LogicPage->Get_Buffer());
    if (!dst) return false;
    int dst_pitch = LogicPage->Get_Full_Pitch();
    int dst_w = LogicPage->Get_Width();
    int dst_h = LogicPage->Get_Height();

    extern int WindowList[][8];
    int win_x = WindowList[cmd.window][0] << 3;
    int win_y = WindowList[cmd.window][1];

    float x = static_cast<float>(cmd.x);
    float y = static_cast<float>(cmd.y);

    if (zoom != 1.0f) {
        x = (x - vp_x) * zoom;
        y = (y - vp_y) * zoom;
    }

    int screen_x = win_x + static_cast<int>(x);
    int screen_y = win_y + static_cast<int>(y);

    bool center = (cmd.flags & SHAPE_CENTER) != 0;

    // Extract rendering mode parameters from flags
    const uint8_t* ghost_table = nullptr;
    const uint8_t* fading_table = nullptr;
    int fading_count = 0;

    // Special shadow case: FADING|PREDATOR → convert to GHOST
    int effective_flags = cmd.flags;
    if ((effective_flags & (SHAPE_FADING | SHAPE_PREDATOR)) == (SHAPE_FADING | SHAPE_PREDATOR)) {
        effective_flags &= ~(SHAPE_FADING | SHAPE_PREDATOR);
        effective_flags |= SHAPE_GHOST;
        ghost_table = static_cast<const uint8_t*>((void*)Map.SpecialGhost);
    }

    if ((effective_flags & SHAPE_GHOST) && cmd.ghostdata) {
        ghost_table = static_cast<const uint8_t*>(cmd.ghostdata);
    }

    if ((effective_flags & SHAPE_FADING) && cmd.fadingdata) {
        fading_table = static_cast<const uint8_t*>(cmd.fadingdata);
        fading_count = 1; // Default single pass
    }

    blit_indexed_scaled(
        static_cast<const uint8_t*>(frame.pixels), frame.width, frame.height,
        dst, dst_pitch, dst_w, dst_h,
        screen_x, screen_y, zoom, center, true,
        ghost_table, fading_table, fading_count);

    return true;
}
