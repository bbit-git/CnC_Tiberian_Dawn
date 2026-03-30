/**
 * gl_present.cpp — GL frame orchestration and shared GL presentation state.
 */

#include "gl_present_internal.h"
#include "render_bridge.h"
#include "function.h"
#include "dbg.h"
#include <SDL3/SDL.h>
#include <cmath>
#include <cstdlib>

extern int g_shake_remaining;
extern bool InMainLoop;

#ifdef USE_RENDER_BRIDGE_GL_SPRITES
extern int GL_Sprites_Build_Atlas(const uint8_t* vga_palette);
#endif

// Shared GL objects used by every presentation pass.
SDL_GLContext g_gl_ctx = nullptr;
GLuint g_gl_program = 0;
GLuint g_indexed_tex = 0;
GLuint g_palette_tex = 0;
// Native tactical replay texture: world-only source sampled by the tactical pass.
GLuint g_tac_tex = 0;
int g_tac_tex_w = 0;
int g_tac_tex_h = 0;
bool g_tac_tex_active = false;
// Full-frame UI and cursor overlays stay in game-buffer space, then scale once at present time.
GLuint g_ui_tex = 0;
int g_ui_tex_w = 0;
int g_ui_tex_h = 0;
GLuint g_cursor_tex = 0;
int g_cursor_tex_w = 0;
int g_cursor_tex_h = 0;
uint8_t* g_cursor_overlay = nullptr;
int g_cursor_overlay_alloc = 0;
int g_tex_w = 0;
int g_tex_h = 0;
bool g_gl_ready = false;
bool g_gl_failed = false;
GLint g_u_indexed = -1;
GLint g_u_palette = -1;
GLint g_u_src_rect = -1;
GLint g_u_dst_rect = -1;
GLuint g_ui_program = 0;
GLint g_ui_u_indexed = -1;
GLint g_ui_u_palette = -1;
GLint g_ui_u_src_rect = -1;
GLint g_ui_u_dst_rect = -1;

static const char* vert_src = R"(
    attribute vec2 a_pos;
    varying vec2 v_uv;
    uniform vec4 u_src_rect;
    uniform vec4 u_dst_rect;
    void main() {
        vec2 pos = mix(u_dst_rect.xy, u_dst_rect.zw, a_pos);
        vec2 uv  = mix(u_src_rect.xy, u_src_rect.zw, a_pos);
        gl_Position = vec4(pos, 0.0, 1.0);
        v_uv = uv;
    }
)";

static const char* frag_src = R"(
    precision mediump float;
    varying vec2 v_uv;
    uniform sampler2D u_indexed;
    uniform sampler2D u_palette;
    void main() {
        float idx = texture2D(u_indexed, v_uv).r;
        float pal_u = (idx * 255.0 + 0.5) / 256.0;
        gl_FragColor = texture2D(u_palette, vec2(pal_u, 0.5));
    }
)";

static const char* frag_ui_src = R"(
    precision mediump float;
    varying vec2 v_uv;
    uniform sampler2D u_indexed;
    uniform sampler2D u_palette;
    void main() {
        float idx = texture2D(u_indexed, v_uv).r;
        if (idx < 0.002) discard;
        float pal_u = (idx * 255.0 + 0.5) / 256.0;
        gl_FragColor = texture2D(u_palette, vec2(pal_u, 0.5));
    }
)";

static GLuint compile_shader(GLenum type, const char* src)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[256];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        DBG("GL shader error: %s", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

void GL_Present_Bind_Client_Quad(const float* quad)
{
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, quad);
}

void GL_Present_Bind_Palette_Program()
{
    glUseProgram(g_gl_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_indexed_tex);
    glUniform1i(g_u_indexed, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, g_palette_tex);
    glUniform1i(g_u_palette, 1);
}

bool GL_Present_Is_Active()
{
    return g_gl_ready;
}

void GL_Present_Upload_Tactical(const uint8_t* pixels, int w, int h)
{
    if (!g_gl_ready) return;

    SDL_GL_MakeCurrent(g_window, g_gl_ctx);
    // The tactical world is uploaded separately from SeenBuff so zoom and source
    // viewport selection operate on a clean world-only texture.
    if (!g_tac_tex || g_tac_tex_w != w || g_tac_tex_h != h) {
        if (g_tac_tex) glDeleteTextures(1, &g_tac_tex);
        glGenTextures(1, &g_tac_tex);
        glBindTexture(GL_TEXTURE_2D, g_tac_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, w, h, 0,
                     GL_LUMINANCE, GL_UNSIGNED_BYTE, pixels);
        g_tac_tex_w = w;
        g_tac_tex_h = h;
    } else {
        glBindTexture(GL_TEXTURE_2D, g_tac_tex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h,
                        GL_LUMINANCE, GL_UNSIGNED_BYTE, pixels);
    }
    g_tac_tex_active = true;
}

bool GL_Present_Init(int w, int h)
{
    if (g_gl_failed) return false;
    if (g_gl_ready) return true;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    g_gl_ctx = SDL_GL_CreateContext(g_window);
    if (!g_gl_ctx) {
        DBG("GL context failed: %s", SDL_GetError());
        g_gl_failed = true;
        return false;
    }
    SDL_GL_MakeCurrent(g_window, g_gl_ctx);

    GLuint vs = compile_shader(GL_VERTEX_SHADER, vert_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, frag_src);
    if (!vs || !fs) {
        g_gl_failed = true;
        return false;
    }

    g_gl_program = glCreateProgram();
    glAttachShader(g_gl_program, vs);
    glAttachShader(g_gl_program, fs);
    glBindAttribLocation(g_gl_program, 0, "a_pos");
    glLinkProgram(g_gl_program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(g_gl_program, GL_LINK_STATUS, &ok);
    if (!ok) {
        g_gl_failed = true;
        return false;
    }

    GLuint ui_vs = compile_shader(GL_VERTEX_SHADER, vert_src);
    GLuint ui_fs = compile_shader(GL_FRAGMENT_SHADER, frag_ui_src);
    if (ui_vs && ui_fs) {
        g_ui_program = glCreateProgram();
        glAttachShader(g_ui_program, ui_vs);
        glAttachShader(g_ui_program, ui_fs);
        glBindAttribLocation(g_ui_program, 0, "a_pos");
        glLinkProgram(g_ui_program);
        glDeleteShader(ui_vs);
        glDeleteShader(ui_fs);
        g_ui_u_indexed = glGetUniformLocation(g_ui_program, "u_indexed");
        g_ui_u_palette = glGetUniformLocation(g_ui_program, "u_palette");
        g_ui_u_src_rect = glGetUniformLocation(g_ui_program, "u_src_rect");
        g_ui_u_dst_rect = glGetUniformLocation(g_ui_program, "u_dst_rect");
    }

    g_u_indexed = glGetUniformLocation(g_gl_program, "u_indexed");
    g_u_palette = glGetUniformLocation(g_gl_program, "u_palette");
    g_u_src_rect = glGetUniformLocation(g_gl_program, "u_src_rect");
    g_u_dst_rect = glGetUniformLocation(g_gl_program, "u_dst_rect");

    glGenTextures(1, &g_indexed_tex);
    glBindTexture(GL_TEXTURE_2D, g_indexed_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, w, h, 0,
                 GL_LUMINANCE, GL_UNSIGNED_BYTE, nullptr);

    glGenTextures(1, &g_palette_tex);
    glBindTexture(GL_TEXTURE_2D, g_palette_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

    g_tex_w = w;
    g_tex_h = h;
    g_gl_ready = true;

    int sw = 0;
    int sh = 0;
    SDL_GetWindowSizeInPixels(g_window, &sw, &sh);
    DBG("GL present: buffer %dx%d, window %dx%d", w, h, sw, sh);
    return true;
}

void GL_Present_Shutdown()
{
    if (g_indexed_tex) { glDeleteTextures(1, &g_indexed_tex); g_indexed_tex = 0; }
    if (g_palette_tex) { glDeleteTextures(1, &g_palette_tex); g_palette_tex = 0; }
    if (g_tac_tex) { glDeleteTextures(1, &g_tac_tex); g_tac_tex = 0; }
    if (g_ui_tex) { glDeleteTextures(1, &g_ui_tex); g_ui_tex = 0; }
    if (g_cursor_tex) { glDeleteTextures(1, &g_cursor_tex); g_cursor_tex = 0; }
    if (g_cursor_overlay) { free(g_cursor_overlay); g_cursor_overlay = nullptr; g_cursor_overlay_alloc = 0; }
    if (g_ui_program) { glDeleteProgram(g_ui_program); g_ui_program = 0; }
    if (g_gl_program) { glDeleteProgram(g_gl_program); g_gl_program = 0; }
    if (g_gl_ctx) { SDL_GL_DestroyContext(g_gl_ctx); g_gl_ctx = nullptr; }
    g_gl_ready = false;
}

static void upload_indexed_frame(const uint8_t* indexed_pixels, int pitch, int w, int h)
{
    // SeenBuff remains the source for chrome and full-frame UI-space overlays.
    glBindTexture(GL_TEXTURE_2D, g_indexed_tex);
    if (pitch == w) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h,
                        GL_LUMINANCE, GL_UNSIGNED_BYTE, indexed_pixels);
    } else {
        for (int row = 0; row < h; row++) {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, row, w, 1,
                            GL_LUMINANCE, GL_UNSIGNED_BYTE,
                            indexed_pixels + row * pitch);
        }
    }
}

static void upload_palette(const uint8_t* vga_palette)
{
    // The legacy palette mutates in place during fades, so refresh it every frame.
    static uint8_t pal_rgb[256 * 3];
    for (int i = 0; i < 256; i++) {
        pal_rgb[i * 3 + 0] = vga_palette[i * 3 + 0] << 2;
        pal_rgb[i * 3 + 1] = vga_palette[i * 3 + 1] << 2;
        pal_rgb[i * 3 + 2] = vga_palette[i * 3 + 2] << 2;
    }
    glBindTexture(GL_TEXTURE_2D, g_palette_tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1,
                    GL_RGB, GL_UNSIGNED_BYTE, pal_rgb);
}

static bool resolve_window_size(int& win_w, int& win_h, int buffer_w, int buffer_h)
{
    // Fullscreen SDL windows can report the logical game-buffer size here.
    // Fall back to the desktop mode so final pixel mapping stays correct.
    SDL_GetWindowSizeInPixels(g_window, &win_w, &win_h);
    if (win_w <= 0 || win_h <= 0 || (win_w == buffer_w && win_h == buffer_h)) {
        SDL_DisplayID disp = SDL_GetDisplayForWindow(g_window);
        if (!disp) disp = SDL_GetPrimaryDisplay();
        const SDL_DisplayMode* dm = SDL_GetDesktopDisplayMode(disp);
        if (dm && dm->w > 0 && dm->h > 0) {
            win_w = dm->w;
            win_h = dm->h;
        }
    }
    return win_w > 0 && win_h > 0;
}

bool GL_Present_Frame(const uint8_t* indexed_pixels, int pitch,
                      int w, int h, const uint8_t* vga_palette)
{
    if (!g_gl_ready) return false;

    SDL_GL_MakeCurrent(g_window, g_gl_ctx);
    upload_indexed_frame(indexed_pixels, pitch, w, h);
    upload_palette(vga_palette);

#ifdef USE_RENDER_BRIDGE_GL_SPRITES
    GL_Sprites_Build_Atlas(vga_palette);
#endif

    int win_w = 0;
    int win_h = 0;
    if (!resolve_window_size(win_w, win_h, w, h)) {
        return false;
    }

    float sx = static_cast<float>(win_w) / static_cast<float>(w);
    float sy = static_cast<float>(win_h) / static_cast<float>(h);
    // UI/chrome uses one uniform scale from game-buffer pixels to final window pixels.
    float ui_scale = (sx < sy) ? sx : sy;

    int offset_x = static_cast<int>((win_w - w * ui_scale) * 0.5f);
    int offset_y = static_cast<int>((win_h - h * ui_scale) * 0.5f);
    if (g_shake_remaining > 0) {
        offset_x += (rand() % 5) - 2;
        offset_y += (rand() % 5) - 2;
        g_shake_remaining--;
    }

    GLPresentFrameContext ctx;
    ctx.win_w = win_w;
    ctx.win_h = win_h;
    ctx.buffer_w = w;
    ctx.buffer_h = h;
    ctx.ui_scale = ui_scale;
    ctx.offset_x = offset_x;
    ctx.offset_y = offset_y;
    ctx.in_main_loop = InMainLoop;
    if (ctx.in_main_loop) {
        Render_Bridge_Get_Tactical_Rect(ctx.tactical_game_x, ctx.tactical_game_y,
                                        ctx.tactical_game_w, ctx.tactical_game_h);
        ctx.side_game_x = Map.SideX;
        ctx.side_game_w = Map.SideBarWidth;
    } else {
        ctx.tactical_game_x = 0;
        ctx.tactical_game_y = 0;
        ctx.tactical_game_w = w;
        ctx.tactical_game_h = h;
        ctx.side_game_x = w;
        ctx.side_game_w = 0;
    }
    ctx.tactical_screen_x = offset_x + static_cast<int>(std::round(ctx.tactical_game_x * ui_scale));
    ctx.tactical_screen_y = offset_y + static_cast<int>(std::round(ctx.tactical_game_y * ui_scale));
    ctx.tactical_screen_w = static_cast<int>(std::round(ctx.tactical_game_w * ui_scale));
    ctx.tactical_screen_h = static_cast<int>(std::round(ctx.tactical_game_h * ui_scale));

    // Frame order:
    // 1. legacy SeenBuff regions and tactical world
    // 2. UI overlay
    // 3. debug overlays
    // 4. cursor
    glViewport(0, 0, win_w, win_h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glEnableVertexAttribArray(0);

    GL_Present_Bind_Palette_Program();
    GL_Present_Draw_Regions(ctx, vga_palette);

    if (ctx.in_main_loop) {
        bool has_ui_overlay = GL_Present_Draw_UI_Overlay(ctx);
        GL_Present_Draw_Source_Overlay(ctx, has_ui_overlay);
    }

    // Bridge-native UI overlay (header, sidebar chrome, controls)
    if (ctx.in_main_loop) {
        extern void Render_Bridge_UI_Begin_Frame();
        extern void Render_Bridge_UI_End_Frame();
        extern void UI_Header_Emit();
        extern void UI_Messages_Emit();
        extern void UI_Sidebar_Emit();
        extern void UI_Tooltip_Emit(int, int, int, int);
        extern void GL_UI_Render(int, int, int, int, float);
        extern int g_mouse_x, g_mouse_y;

        Render_Bridge_UI_Begin_Frame();
        UI_Header_Emit();
        UI_Messages_Emit();
        UI_Sidebar_Emit();
        UI_Tooltip_Emit(g_mouse_x, g_mouse_y,
                        ctx.buffer_w, ctx.buffer_h);
        Render_Bridge_UI_End_Frame();
        GL_UI_Render(ctx.win_w, ctx.win_h,
                     ctx.offset_x, ctx.offset_y, ctx.ui_scale);
    }

    GL_Present_Draw_Debug_Overlays(ctx);
    GL_Present_Draw_Cursor(ctx);

    glDisableVertexAttribArray(0);
    SDL_GL_SwapWindow(g_window);
    return true;
}
