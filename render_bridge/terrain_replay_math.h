#pragma once

#include "draw_list.h"
#include "texture_atlas.h"

struct TerrainReplayQuad {
    float screen_x;
    float screen_y;
    float screen_w;
    float screen_h;
    bool visible;
};

static inline TerrainReplayQuad Render_Bridge_Compute_HD_Terrain_Quad(
    const StampCmd& cmd, const AtlasRegion& region,
    int tac_screen_x, int tac_screen_y, int tac_screen_w, int tac_screen_h,
    float scale, float vp_x, float vp_y)
{
    TerrainReplayQuad out = {};

    float native_scale = region.native_scale > 0.0f ? region.native_scale : 1.0f;
    float game_x = static_cast<float>(cmd.x) + static_cast<float>(region.origin_x) / native_scale;
    float game_y = static_cast<float>(cmd.y) + static_cast<float>(region.origin_y) / native_scale;

    out.screen_x = tac_screen_x + (game_x - vp_x) * scale;
    out.screen_y = tac_screen_y + (game_y - vp_y) * scale;
    out.screen_w = static_cast<float>(region.w) * scale / native_scale;
    out.screen_h = static_cast<float>(region.h) * scale / native_scale;
    out.visible = !(
        out.screen_x + out.screen_w < tac_screen_x ||
        out.screen_x > tac_screen_x + tac_screen_w ||
        out.screen_y + out.screen_h < tac_screen_y ||
        out.screen_y > tac_screen_y + tac_screen_h);

    return out;
}
