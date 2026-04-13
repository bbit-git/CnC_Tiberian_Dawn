#include "gl_present_internal.h"
#include "render_bridge.h"
#include "draw_list.h"
#include "function.h"

extern int GL_Primitives_Render(int win_w, int win_h,
                                 int tac_screen_x, int tac_screen_y,
                                 int tac_screen_w, int tac_screen_h,
                                 float scale, float vp_x, float vp_y,
                                 const uint8_t* palette);
extern void GL_Primitives_Render_Source_Overlay(int win_w, int win_h,
                                                int game_screen_x, int game_screen_y,
                                                int game_screen_w, int game_screen_h,
                                                int header_screen_h,
                                                int tac_screen_x, int tac_screen_y,
                                                int tac_screen_w, int tac_screen_h,
                                                int side_screen_x, int side_screen_w,
                                                bool has_ui_overlay);
extern bool Render_Bridge_Debug_Sources_Enabled();
#ifdef USE_RENDER_BRIDGE_GL_SPRITES
extern int GL_Sprites_Render_Terrain(int win_w, int win_h,
                                      int tac_screen_x, int tac_screen_y,
                                      int tac_screen_w, int tac_screen_h,
                                      float scale, float vp_x, float vp_y);
extern int GL_Sprites_Render(int win_w, int win_h,
                              int tac_screen_x, int tac_screen_y,
                              int tac_screen_w, int tac_screen_h,
                              int tac_game_x, int tac_game_y,
                              int tac_game_w, int tac_game_h,
                              float scale, float vp_x, float vp_y);
extern int GL_Sprites_Render_Shadow(int win_w, int win_h,
                                    int tac_screen_x, int tac_screen_y,
                                    int tac_screen_w, int tac_screen_h,
                                    int tac_game_x, int tac_game_y,
                                    int tac_game_w, int tac_game_h,
                                    float scale, float vp_x, float vp_y);
#endif

static constexpr bool k_enable_gl_sprite_overlay = true;
static constexpr bool k_enable_seenbuff_chrome = false;

static bool frame_has_shroud_overlay()
{
    for (int i = 0; i < g_draw_list.Command_Count(); i++) {
        const DrawCommand& cmd = g_draw_list.Get(i);
        if (cmd.layer != LAYER_SHADOW) {
            continue;
        }
        if (cmd.type == CMD_SHAPE || cmd.type == CMD_FILL_RECT) {
            return true;
        }
    }
    return false;
}

void GL_Present_Draw_Quad(int src_x, int src_y, int src_w, int src_h,
                          int dst_x, int dst_y, int dst_w, int dst_h,
                          int win_w, int win_h)
{
    // Source coordinates are in game-buffer pixels. Destination coordinates are
    // already in final window pixels after layout scaling.
    float u0 = static_cast<float>(src_x) / g_tex_w;
    float v0 = static_cast<float>(src_y) / g_tex_h;
    float u1 = static_cast<float>(src_x + src_w) / g_tex_w;
    float v1 = static_cast<float>(src_y + src_h) / g_tex_h;

    float x0 = static_cast<float>(dst_x) / win_w * 2.0f - 1.0f;
    float y0 = 1.0f - static_cast<float>(dst_y) / win_h * 2.0f;
    float x1 = static_cast<float>(dst_x + dst_w) / win_w * 2.0f - 1.0f;
    float y1 = 1.0f - static_cast<float>(dst_y + dst_h) / win_h * 2.0f;

    glUniform4f(g_u_src_rect, u0, v0, u1, v1);
    glUniform4f(g_u_dst_rect, x0, y0, x1, y1);

    static const float quad[] = { 0,0, 1,0, 0,1, 1,1 };
    GL_Present_Bind_Client_Quad(quad);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

bool GL_Present_Draw_Regions(const GLPresentFrameContext& ctx,
                             const uint8_t* indexed_pixels,
                             const uint8_t* vga_palette)
{
    if (!ctx.in_main_loop) {
        // Menus are still a single SeenBuff-backed full-frame present.
        GL_Present_Draw_Indexed_RGBA(indexed_pixels, ctx.buffer_w, ctx.buffer_h,
                                     vga_palette, false,
                                     ctx.legacy_offset_x, ctx.legacy_offset_y,
                                     static_cast<int>(ctx.buffer_w * ctx.legacy_ui_scale),
                                     static_cast<int>(ctx.buffer_h * ctx.legacy_ui_scale),
                                     ctx.win_w, ctx.win_h);
        return false;
    }

    if (k_enable_seenbuff_chrome) {
        if (ctx.tactical_game_y > 0) {
            // Legacy header/tab chrome can still be presented from SeenBuff
            // while the bridge-native UI path is incomplete.
            GL_Present_Draw_Quad(0, 0, ctx.buffer_w, ctx.tactical_game_y,
                                 ctx.offset_x, ctx.offset_y,
                                 static_cast<int>(ctx.buffer_w * ctx.ui_scale),
                                 static_cast<int>(ctx.tactical_game_y * ctx.ui_scale),
                                 ctx.win_w, ctx.win_h);
        }

        if (ctx.side_game_w > 0) {
            // Legacy sidebar chrome uses the same game-buffer mapping as the header.
            GL_Present_Draw_Quad(ctx.side_game_x, 0, ctx.side_game_w, ctx.buffer_h,
                                 ctx.offset_x + static_cast<int>(ctx.side_game_x * ctx.ui_scale),
                                 ctx.offset_y,
                                 static_cast<int>(ctx.side_game_w * ctx.ui_scale),
                                 static_cast<int>(ctx.buffer_h * ctx.ui_scale),
                                 ctx.win_w, ctx.win_h);
        }
    }

    // Keep presenting the last uploaded tactical texture even if a given
    // frame did not produce a fresh upload. Some engine/UI-only redraw paths
    // can reach the presenter without rebuilding the native tactical buffer,
    // and treating the texture as single-use causes visible blinking.
    if (!g_tac_tex) {
        return false;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_tac_tex);

    // The visible portion of the native tactical texture (vis_w × vis_h) is
    // fitted uniformly into the render tactical screen area. The UV selects the
    // source region; zoom controls how much of the texture is visible. The
    // destination always fills the available screen so content scales up as the
    // player zooms in. Sidebar-independent: the render_tac rect is stable.
    float tex_w = static_cast<float>(g_tac_tex_w);
    float tex_h = static_cast<float>(g_tac_tex_h);
    float vp_x = Render_Bridge_Get_Viewport_X();
    float vp_y = Render_Bridge_Get_Viewport_Y();
    float vis_w = 0.0f;
    float vis_h = 0.0f;
    Render_Bridge_Get_Visible_Size(vis_w, vis_h);
    if (vis_w <= 0.0f || vis_h <= 0.0f) {
        vis_w = tex_w;
        vis_h = tex_h;
    }

    float u0 = vp_x / tex_w;
    float v0 = vp_y / tex_h;
    float u1 = (vp_x + vis_w) / tex_w;
    float v1 = (vp_y + vis_h) / tex_h;
    if (u0 < 0.0f) u0 = 0.0f;
    if (v0 < 0.0f) v0 = 0.0f;
    if (u1 > 1.0f) u1 = 1.0f;
    if (v1 > 1.0f) v1 = 1.0f;

    // Uniform-fit the visible source into the render tactical area.
    float fit_sx = static_cast<float>(ctx.render_tac_screen_w) / vis_w;
    float fit_sy = static_cast<float>(ctx.render_tac_screen_h) / vis_h;
    float fit_scale = (fit_sx < fit_sy) ? fit_sx : fit_sy;
    int dst_w = static_cast<int>(std::round(vis_w * fit_scale));
    int dst_h = static_cast<int>(std::round(vis_h * fit_scale));
    int dst_x = ctx.render_tac_screen_x + (ctx.render_tac_screen_w - dst_w) / 2;
    int dst_y = ctx.render_tac_screen_y + (ctx.render_tac_screen_h - dst_h) / 2;

    // Scissor to the actual render tactical rect so world pixels are clipped
    // against the same screen area used for presentation.
    glEnable(GL_SCISSOR_TEST);
    glScissor(ctx.render_tac_screen_x,
              ctx.win_h - (ctx.render_tac_screen_y + ctx.render_tac_screen_h),
              ctx.render_tac_screen_w, ctx.render_tac_screen_h);

    float nx0 = static_cast<float>(dst_x) / ctx.win_w * 2.0f - 1.0f;
    float ny0 = 1.0f - static_cast<float>(dst_y) / ctx.win_h * 2.0f;
    float nx1 = static_cast<float>(dst_x + dst_w) / ctx.win_w * 2.0f - 1.0f;
    float ny1 = 1.0f - static_cast<float>(dst_y + dst_h) / ctx.win_h * 2.0f;

    // Render scale must match the native buffer UV→screen mapping exactly.
    // Using fit_scale introduces sub-pixel drift because dst_w = round(vis_w * fit_scale)
    // differs from vis_w * fit_scale. Use the actual dst/vis ratio instead.
    float render_scale = (vis_w > 0.0f) ? static_cast<float>(dst_w) / vis_w : fit_scale;

#ifdef USE_RENDER_BRIDGE_GL_SPRITES
    // Layer 1: HD terrain tiles. Drawn first so they sit below everything.
    if (k_enable_gl_sprite_overlay) {
        GL_Sprites_Render_Terrain(ctx.win_w, ctx.win_h,
                                   dst_x, dst_y, dst_w, dst_h,
                                   render_scale, vp_x, vp_y);
    }
#endif

    // Layer 2: Native tactical buffer. Uses the tactical shader that discards
    // TERRAIN_KEY_INDEX pixels, making HD terrain holes transparent. Alpha
    // blending lets the terrain layer underneath show through.
    bool has_hd_terrain = (g_tac_program != 0);
    if (has_hd_terrain) {
        glUseProgram(g_tac_program);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_tac_tex);
        glUniform1i(g_tac_u_indexed, 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, g_palette_tex);
        glUniform1i(g_tac_u_palette, 1);
        glUniform4f(g_tac_u_src_rect, u0, v0, u1, v1);
        glUniform4f(g_tac_u_dst_rect, nx0, ny0, nx1, ny1);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glUniform4f(g_u_src_rect, u0, v0, u1, v1);
        glUniform4f(g_u_dst_rect, nx0, ny0, nx1, ny1);
    }

    // Re-enable attrib 0 — the sprite batch Flush() in Render_Terrain
    // disables vertex attrib arrays. Without this the tactical quad draws
    // nothing and the native buffer (with trees, tiberium, shroud) is invisible.
    glEnableVertexAttribArray(0);
    static const float quad[] = { 0,0, 1,0, 0,1, 1,1 };
    GL_Present_Bind_Client_Quad(quad);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    if (has_hd_terrain) {
        glDisable(GL_BLEND);
        // Restore the default palette program for subsequent draws.
        GL_Present_Bind_Palette_Program();
    }

#ifdef USE_RENDER_BRIDGE_GL_SPRITES
    // Layer 3: HD sprite overlays (units, buildings).
    if (k_enable_gl_sprite_overlay) {
        GL_Sprites_Render(ctx.win_w, ctx.win_h,
                          dst_x, dst_y, dst_w, dst_h,
                          0, 0, g_tac_tex_w, g_tac_tex_h,
                          render_scale, vp_x, vp_y);
    }
#endif
    GL_Shroud_Render(ctx.win_w, ctx.win_h,
                     dst_x, dst_y, dst_w, dst_h,
                     render_scale, vp_x, vp_y, vga_palette);
#ifdef USE_RENDER_BRIDGE_GL_SPRITES
    if (k_enable_gl_sprite_overlay && Render_Bridge_Get_HD_Graphics()) {
        // SHADOW.SHP edge tiles must draw after the solid-black shroud fill so
        // diagonal corners remain visible instead of being covered by the late
        // black rect pass.
        GL_Sprites_Render_Shadow(ctx.win_w, ctx.win_h,
                                 dst_x, dst_y, dst_w, dst_h,
                                 0, 0, g_tac_tex_w, g_tac_tex_h,
                                 render_scale, vp_x, vp_y);
    }
#endif

    GL_Primitives_Render(ctx.win_w, ctx.win_h,
                         dst_x, dst_y, dst_w, dst_h,
                         render_scale, vp_x, vp_y, vga_palette);

    glDisable(GL_SCISSOR_TEST);

    GL_Present_Bind_Palette_Program();
    return true;
}

bool GL_Present_Draw_UI_Overlay(const GLPresentFrameContext& ctx, const uint8_t* vga_palette)
{
    extern bool Render_Bridge_UI_Has_Active_Dialog();
    if (Render_Bridge_UI_Has_Active_Dialog()) {
        return false;
    }

    extern const uint8_t* Render_Bridge_Get_UI_Overlay(int& w, int& h);
    int ui_w = 0;
    int ui_h = 0;
    const uint8_t* ui_pixels = Render_Bridge_Get_UI_Overlay(ui_w, ui_h);
    if (!ui_pixels || ui_w <= 0 || ui_h <= 0) {
        return false;
    }

    return GL_Present_Draw_Indexed_RGBA(ui_pixels, ui_w, ui_h,
                                        vga_palette, true,
                                        ctx.legacy_offset_x, ctx.legacy_offset_y,
                                        static_cast<int>(ctx.buffer_w * ctx.legacy_ui_scale),
                                        static_cast<int>(ctx.buffer_h * ctx.legacy_ui_scale),
                                        ctx.win_w, ctx.win_h);
}

void GL_Present_Draw_Source_Overlay(const GLPresentFrameContext& ctx, bool has_ui_overlay)
{
    if (!Render_Bridge_Debug_Sources_Enabled()) {
        return;
    }

    // This overlay is for ownership debugging only: it shows which presenter
    // pass supplies each screen region.
    int header_screen_h = k_enable_seenbuff_chrome
        ? static_cast<int>(ctx.tactical_game_y * ctx.ui_scale)
        : 0;
    int side_screen_x = k_enable_seenbuff_chrome
        ? ctx.offset_x + static_cast<int>(ctx.side_game_x * ctx.ui_scale)
        : 0;
    int side_screen_w = k_enable_seenbuff_chrome
        ? static_cast<int>(ctx.side_game_w * ctx.ui_scale)
        : 0;

    GL_Primitives_Render_Source_Overlay(ctx.win_w, ctx.win_h,
                                        ctx.offset_x, ctx.offset_y,
                                        static_cast<int>(ctx.buffer_w * ctx.ui_scale),
                                        static_cast<int>(ctx.buffer_h * ctx.ui_scale),
                                        header_screen_h,
                                        ctx.tactical_screen_x, ctx.tactical_screen_y,
                                        ctx.tactical_screen_w, ctx.tactical_screen_h,
                                        side_screen_x, side_screen_w,
                                        has_ui_overlay);
}
