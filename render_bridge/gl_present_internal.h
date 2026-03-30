#ifndef CNC_RENDER_BRIDGE_GL_PRESENT_INTERNAL_H
#define CNC_RENDER_BRIDGE_GL_PRESENT_INTERNAL_H

#include <SDL3/SDL.h>
#include <GLES2/gl2.h>
#include <cstdint>

struct GLPresentFrameContext {
    // Final drawable size in native window pixels.
    int win_w = 0;
    int win_h = 0;
    // Legacy game buffer size (SeenBuff / full UI overlay texture).
    int buffer_w = 0;
    int buffer_h = 0;
    // Uniform game-buffer-to-window scale used for non-world UI regions.
    float ui_scale = 1.0f;
    // Letterbox / pillarbox offset for the scaled game buffer.
    int offset_x = 0;
    int offset_y = 0;
    // Tactical rect in game-buffer pixels.
    int tactical_game_x = 0;
    int tactical_game_y = 0;
    int tactical_game_w = 0;
    int tactical_game_h = 0;
    // Tactical rect mapped into final window pixels.
    int tactical_screen_x = 0;
    int tactical_screen_y = 0;
    int tactical_screen_w = 0;
    int tactical_screen_h = 0;
    // Sidebar region in game-buffer pixels.
    int side_game_x = 0;
    int side_game_w = 0;
    bool in_main_loop = false;
};

extern SDL_Window* g_window;

extern SDL_GLContext g_gl_ctx;
extern GLuint g_gl_program;
extern GLuint g_indexed_tex;
extern GLuint g_palette_tex;
extern GLuint g_tac_tex;
extern int g_tac_tex_w;
extern int g_tac_tex_h;
extern bool g_tac_tex_active;
extern GLuint g_ui_tex;
extern int g_ui_tex_w;
extern int g_ui_tex_h;
extern GLuint g_cursor_tex;
extern int g_cursor_tex_w;
extern int g_cursor_tex_h;
extern uint8_t* g_cursor_overlay;
extern int g_cursor_overlay_alloc;
extern int g_tex_w;
extern int g_tex_h;
extern bool g_gl_ready;
extern bool g_gl_failed;
extern GLint g_u_indexed;
extern GLint g_u_palette;
extern GLint g_u_src_rect;
extern GLint g_u_dst_rect;
extern GLuint g_ui_program;
extern GLint g_ui_u_indexed;
extern GLint g_ui_u_palette;
extern GLint g_ui_u_src_rect;
extern GLint g_ui_u_dst_rect;

/// Bind a client-side unit quad for a full-screen or region draw.
void GL_Present_Bind_Client_Quad(const float* quad);

/// Restore the indexed framebuffer + palette shader after a pass changes programs.
void GL_Present_Bind_Palette_Program();

/// Draw one SeenBuff-space quad into a destination rectangle in final window pixels.
void GL_Present_Draw_Quad(int src_x, int src_y, int src_w, int src_h,
                          int dst_x, int dst_y, int dst_w, int dst_h,
                          int win_w, int win_h);

/// Draw the main gameplay regions: header, sidebar, tactical world, and tactical overlays.
bool GL_Present_Draw_Regions(const GLPresentFrameContext& ctx, const uint8_t* vga_palette);

/// Draw the bridge UI overlay texture on top of the composed frame.
bool GL_Present_Draw_UI_Overlay(const GLPresentFrameContext& ctx);

/// Draw the source-ownership overlay that highlights SeenBuff, tactical, and UI regions.
void GL_Present_Draw_Source_Overlay(const GLPresentFrameContext& ctx, bool has_ui_overlay);

/// Draw diagnostic overlays and the debug HUD.
void GL_Present_Draw_Debug_Overlays(const GLPresentFrameContext& ctx);

/// Draw the final mouse cursor overlay after every other pass.
void GL_Present_Draw_Cursor(const GLPresentFrameContext& ctx);

#endif
