#include "gl_present_internal.h"
#include "render_bridge.h"
#include "function.h"
#include <cmath>

extern void TD_SDL_Get_Raw_Mouse_Position(int& x, int& y);
extern void GL_Primitives_Render_Debug_Overlay(int win_w, int win_h,
                                               int game_screen_x, int game_screen_y,
                                               int game_screen_w, int game_screen_h,
                                               int header_screen_h,
                                               int tac_screen_x, int tac_screen_y,
                                               int tac_screen_w, int tac_screen_h,
                                               int side_screen_x, int side_screen_w,
                                               int mouse_clamp_x, int mouse_clamp_y,
                                               int mouse_clamp_w, int mouse_clamp_h,
                                               int shroud_screen_x, int shroud_screen_y,
                                               int shroud_screen_w, int shroud_screen_h,
                                               float scroll_zone_px,
                                               float vp_x, float vp_y,
                                               float vis_w, float vis_h,
                                               int native_w, int native_h,
                                               bool clamp_left, bool clamp_right,
                                               bool clamp_top, bool clamp_bottom,
                                               int raw_mouse_screen_x, int raw_mouse_screen_y,
                                               int mouse_screen_x, int mouse_screen_y);
extern bool Render_Bridge_Debug_Bars_Enabled();
extern void Render_Bridge_Debug_HUD_GL(int win_w, int win_h);

void GL_Present_Draw_Debug_Overlays(const GLPresentFrameContext& ctx)
{
    if (!ctx.in_main_loop || !Render_Bridge_Debug_Bars_Enabled()) {
        Render_Bridge_Debug_HUD_GL(ctx.win_w, ctx.win_h);
        return;
    }

    float vis_w = 0.0f;
    float vis_h = 0.0f;
    Render_Bridge_Get_Visible_Size(vis_w, vis_h);
    float vp_x = Render_Bridge_Get_Viewport_X();
    float vp_y = Render_Bridge_Get_Viewport_Y();

    int req_x = 0;
    int req_y = 0;
    Render_Bridge_Get_Requested_Tactical_Position(req_x, req_y);
    int clamp_max_x = 0;
    int clamp_max_y = 0;
    Render_Bridge_Get_Clamp_Ranges(clamp_max_x, clamp_max_y);
    int tac_range_x = Lepton_To_Pixel(clamp_max_x);
    int tac_range_y = Lepton_To_Pixel(clamp_max_y);
    bool clamp_left = req_x <= 0;
    bool clamp_right = tac_range_x > 0 && req_x >= clamp_max_x;
    bool clamp_top = req_y <= 0;
    bool clamp_bottom = tac_range_y > 0 && req_y >= clamp_max_y;

    int raw_mouse_x = 0;
    int raw_mouse_y = 0;
    TD_SDL_Get_Raw_Mouse_Position(raw_mouse_x, raw_mouse_y);
    int mapped_mouse_x = raw_mouse_x;
    int mapped_mouse_y = raw_mouse_y;
    Render_Bridge_Map_Tactical_Point(raw_mouse_x, raw_mouse_y, mapped_mouse_x, mapped_mouse_y);
    int raw_mouse_screen_x = ctx.offset_x + static_cast<int>(std::round(raw_mouse_x * ctx.ui_scale));
    int raw_mouse_screen_y = ctx.offset_y + static_cast<int>(std::round(raw_mouse_y * ctx.ui_scale));
    int mouse_screen_x = ctx.offset_x + static_cast<int>(std::round(mapped_mouse_x * ctx.ui_scale));
    int mouse_screen_y = ctx.offset_y + static_cast<int>(std::round(mapped_mouse_y * ctx.ui_scale));
    int visible_world_x = 0;
    int visible_world_y = 0;
    int visible_world_w = 0;
    int visible_world_h = 0;
    Render_Bridge_Get_Visible_World_Rect(visible_world_x, visible_world_y,
                                         visible_world_w, visible_world_h);
    // The shroud debug rectangle visualizes the current bridge-visible world
    // window projected back onto the tactical screen area.
    int shroud_screen_x = ctx.tactical_screen_x;
    int shroud_screen_y = ctx.tactical_screen_y;
    int shroud_screen_w = static_cast<int>(std::round(Lepton_To_Pixel(visible_world_w) * ctx.ui_scale));
    int shroud_screen_h = static_cast<int>(std::round(Lepton_To_Pixel(visible_world_h) * ctx.ui_scale));

    GL_Primitives_Render_Debug_Overlay(
        ctx.win_w, ctx.win_h,
        ctx.offset_x, ctx.offset_y,
        static_cast<int>(std::round(ctx.buffer_w * ctx.ui_scale)),
        static_cast<int>(std::round(ctx.buffer_h * ctx.ui_scale)),
        ctx.offset_y + static_cast<int>(std::round(ctx.tactical_game_y * ctx.ui_scale)),
        ctx.tactical_screen_x, ctx.tactical_screen_y, ctx.tactical_screen_w, ctx.tactical_screen_h,
        ctx.offset_x + static_cast<int>(std::round(ctx.side_game_x * ctx.ui_scale)),
        static_cast<int>(std::round(ctx.side_game_w * ctx.ui_scale)),
        ctx.tactical_screen_x, ctx.tactical_screen_y, ctx.tactical_screen_w, ctx.tactical_screen_h,
        shroud_screen_x, shroud_screen_y, shroud_screen_w, shroud_screen_h,
        CELL_PIXEL_W * 2.0f * ctx.ui_scale,
        vp_x, vp_y, vis_w, vis_h,
        g_tac_tex_w, g_tac_tex_h,
        clamp_left, clamp_right, clamp_top, clamp_bottom,
        raw_mouse_screen_x, raw_mouse_screen_y,
        mouse_screen_x, mouse_screen_y);

    GL_Present_Bind_Palette_Program();
    Render_Bridge_Debug_HUD_GL(ctx.win_w, ctx.win_h);
}
