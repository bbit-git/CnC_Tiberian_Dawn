/**
 * gl_present.cpp — GL ES 2.0 presentation at native window resolution.
 *
 * Renders the game's 8-bit indexed SeenBuff through a palette lookup
 * shader. Instead of one stretched quad, renders multiple quads — one
 * per screen region — mapped from game-buffer coordinates to native
 * screen-pixel coordinates. Each layer scales independently.
 *
 * At 1920x1080 with a 712x400 game buffer:
 *   Tab bar:  game [0,0,712,16]      → screen [0,0,1920,43]
 *   Tactical: game [0,16,552,400]    → screen [0,43,1490,1080]
 *   Sidebar:  game [552,0,160,400]   → screen [1490,0,430,1080]
 *
 * Palette conversion runs entirely on the GPU.
 */

#include <SDL3/SDL.h>
#include <GLES2/gl2.h>
#include "render_bridge.h"
#include "function.h"
#include "dbg.h"

extern SDL_Window* g_window;
extern int g_shake_remaining;

static SDL_GLContext g_gl_ctx = nullptr;
static GLuint g_gl_program    = 0;
static GLuint g_indexed_tex   = 0;
static GLuint g_palette_tex   = 0;
static GLuint g_tac_tex       = 0;     // expanded tactical texture (if different from main)
static int    g_tac_tex_w = 0, g_tac_tex_h = 0;
static bool   g_tac_tex_active = false;
static int    g_tex_w = 0, g_tex_h = 0;
static bool   g_gl_ready = false;
static bool   g_gl_failed = false;

// Uniform locations
static GLint g_u_indexed = -1;
static GLint g_u_palette = -1;
static GLint g_u_src_rect = -1;
static GLint g_u_dst_rect = -1;

static const char* vert_src = R"(
    attribute vec2 a_pos;
    varying vec2 v_uv;
    uniform vec4 u_src_rect;  // source rect in texture UV space [u0, v0, u1, v1]
    uniform vec4 u_dst_rect;  // dest rect in NDC [-1,1] space [x0, y0, x1, y1]
    void main() {
        // a_pos is [0,0]-[1,1] unit quad
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
        // GL_LUMINANCE normalizes 0-255 → 0.0-1.0 (divides by 255).
        // Palette texture is 256 texels. Map to texel center:
        // texel N center = (N + 0.5) / 256.
        // idx = N/255, so N = idx * 255. Texel center = (idx*255 + 0.5) / 256.
        float pal_u = (idx * 255.0 + 0.5) / 256.0;
        gl_FragColor = texture2D(u_palette, vec2(pal_u, 0.5));
    }
)";

static GLuint compile_shader(GLenum type, const char* src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[256];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        DBG("GL shader error: %s", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

bool GL_Present_Is_Active()
{
    return g_gl_ready;
}

void GL_Present_Upload_Tactical(const uint8_t* pixels, int w, int h)
{
    if (!g_gl_ready) return;

    SDL_GL_MakeCurrent(g_window, g_gl_ctx);

    // Create or resize tactical texture
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
    if (!vs || !fs) { g_gl_failed = true; return false; }

    g_gl_program = glCreateProgram();
    glAttachShader(g_gl_program, vs);
    glAttachShader(g_gl_program, fs);
    glBindAttribLocation(g_gl_program, 0, "a_pos");
    glLinkProgram(g_gl_program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(g_gl_program, GL_LINK_STATUS, &ok);
    if (!ok) { g_gl_failed = true; return false; }

    g_u_indexed  = glGetUniformLocation(g_gl_program, "u_indexed");
    g_u_palette  = glGetUniformLocation(g_gl_program, "u_palette");
    g_u_src_rect = glGetUniformLocation(g_gl_program, "u_src_rect");
    g_u_dst_rect = glGetUniformLocation(g_gl_program, "u_dst_rect");

    // Indexed texture
    glGenTextures(1, &g_indexed_tex);
    glBindTexture(GL_TEXTURE_2D, g_indexed_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, w, h, 0,
                 GL_LUMINANCE, GL_UNSIGNED_BYTE, nullptr);

    // Palette texture
    glGenTextures(1, &g_palette_tex);
    glBindTexture(GL_TEXTURE_2D, g_palette_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 1, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, nullptr);

    g_tex_w = w;
    g_tex_h = h;
    g_gl_ready = true;

    int sw = 0, sh = 0;
    SDL_GetWindowSizeInPixels(g_window, &sw, &sh);
    DBG("GL present: buffer %dx%d, window %dx%d", w, h, sw, sh);
    return true;
}

void GL_Present_Shutdown()
{
    if (g_indexed_tex) { glDeleteTextures(1, &g_indexed_tex); g_indexed_tex = 0; }
    if (g_palette_tex) { glDeleteTextures(1, &g_palette_tex); g_palette_tex = 0; }
    if (g_gl_program) { glDeleteProgram(g_gl_program); g_gl_program = 0; }
    if (g_gl_ctx) { SDL_GL_DestroyContext(g_gl_ctx); g_gl_ctx = nullptr; }
    g_gl_ready = false;
}

/// Draw a quad mapping a game-buffer rect to a screen-pixel rect.
/// src: pixel coordinates in game buffer (712x400 space)
/// dst: pixel coordinates in native screen space (1920x1080 space)
static void draw_quad(int src_x, int src_y, int src_w, int src_h,
                       int dst_x, int dst_y, int dst_w, int dst_h,
                       int win_w, int win_h)
{
    // Source rect → UV space [0,1]
    float u0 = static_cast<float>(src_x) / g_tex_w;
    float v0 = static_cast<float>(src_y) / g_tex_h;
    float u1 = static_cast<float>(src_x + src_w) / g_tex_w;
    float v1 = static_cast<float>(src_y + src_h) / g_tex_h;

    // Dest rect → NDC [-1,1] (GL: Y up, screen: Y down)
    float x0 = static_cast<float>(dst_x) / win_w * 2.0f - 1.0f;
    float y0 = 1.0f - static_cast<float>(dst_y) / win_h * 2.0f;
    float x1 = static_cast<float>(dst_x + dst_w) / win_w * 2.0f - 1.0f;
    float y1 = 1.0f - static_cast<float>(dst_y + dst_h) / win_h * 2.0f;

    glUniform4f(g_u_src_rect, u0, v0, u1, v1);
    glUniform4f(g_u_dst_rect, x0, y0, x1, y1);

    // Unit quad: (0,0) → (1,0) → (0,1) → (1,1)
    static const float quad[] = { 0,0, 1,0, 0,1, 1,1 };
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, quad);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

bool GL_Present_Frame(const uint8_t* indexed_pixels, int pitch,
                       int w, int h, const uint8_t* vga_palette)
{
    if (!g_gl_ready) return false;

    SDL_GL_MakeCurrent(g_window, g_gl_ctx);

    // Upload indexed framebuffer
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

    // Upload palette every frame — palette fades modify data in-place
    // (same pointer, different values). 768 bytes is negligible.
    {
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

    // GL drawable size = actual pixel dimensions of the GL surface.
    // For fullscreen: this is the display resolution (1920x1080).
    // SDL_GL_GetDrawableSize returns the correct value after context creation.
    int win_w = 0, win_h = 0;
    SDL_GetWindowSizeInPixels(g_window, &win_w, &win_h);
    // Fullscreen windows may report game buffer size — use display mode instead
    if (win_w <= 0 || win_h <= 0 || (win_w == w && win_h == h)) {
        SDL_DisplayID disp = SDL_GetDisplayForWindow(g_window);
        if (!disp) disp = SDL_GetPrimaryDisplay();
        const SDL_DisplayMode* dm = SDL_GetDesktopDisplayMode(disp);
        if (dm && dm->w > 0 && dm->h > 0) {
            win_w = dm->w;
            win_h = dm->h;
        }
    }
    if (win_w <= 0 || win_h <= 0) return false;

    // UI scale: game fills screen, maintaining 640x400 aspect ratio.
    // Computed directly from window/buffer — independent of zoom state.
    float sx = static_cast<float>(win_w) / static_cast<float>(w);
    float sy = static_cast<float>(win_h) / static_cast<float>(h);
    float ui_scale = (sx < sy) ? sx : sy;  // fit with letterbox/pillarbox

    float zoom = Render_Bridge_Get_Zoom_Level();

    // Centering offset (UI fills screen with letterbox)
    int offset_x = static_cast<int>((win_w - w * ui_scale) * 0.5f);
    int offset_y = static_cast<int>((win_h - h * ui_scale) * 0.5f);

    // Screen shake
    if (g_shake_remaining > 0) {
        offset_x += (rand() % 5) - 2;
        offset_y += (rand() % 5) - 2;
        g_shake_remaining--;
    }

    // Setup GL state
    glViewport(0, 0, win_w, win_h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    glUseProgram(g_gl_program);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_indexed_tex);
    glUniform1i(g_u_indexed, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, g_palette_tex);
    glUniform1i(g_u_palette, 1);

    glEnableVertexAttribArray(0);

    // Get tactical area dimensions from game state
    extern bool InMainLoop;
    int tac_x = 0, tac_y = 0, tac_w = w, tac_h = h;
    int side_x = w, side_w = 0;

    if (InMainLoop) {
        tac_x = Map.TacPixelX;
        tac_y = Map.TacPixelY;
        tac_w = Lepton_To_Pixel(Map.TacLeptonWidth);
        tac_h = Lepton_To_Pixel(Map.TacLeptonHeight);
        side_x = Map.SideX;
        side_w = Map.SideBarWidth;
    }

    if (!InMainLoop || side_w == 0) {
        // Menu or no sidebar: single fullscreen quad
        draw_quad(0, 0, w, h,
                  offset_x, offset_y,
                  static_cast<int>(w * ui_scale),
                  static_cast<int>(h * ui_scale),
                  win_w, win_h);
    } else {
        // Tab bar (top strip, full width)
        if (tac_y > 0) {
            draw_quad(0, 0, w, tac_y,
                      offset_x, offset_y,
                      static_cast<int>(w * ui_scale),
                      static_cast<int>(tac_y * ui_scale),
                      win_w, win_h);
        }

        // Tactical area — use expanded texture if available.
        {
            float vp_x = Render_Bridge_Get_Viewport_X();
            float vp_y = Render_Bridge_Get_Viewport_Y();

            float rel_zoom = zoom / ui_scale; // relative to default (fills screen)

            // Dest: tactical area fills its screen region
            int dst_x = offset_x + static_cast<int>(tac_x * ui_scale);
            int dst_y = offset_y + static_cast<int>(tac_y * ui_scale);
            int dst_w = static_cast<int>(tac_w * ui_scale);
            int dst_h = static_cast<int>(tac_h * ui_scale);

            if (g_tac_tex_active && g_tac_tex) {
                // Expanded tactical texture: bind it and sample the full area.
                // The expanded texture contains MORE cells than the normal buffer.
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, g_tac_tex);

                // Source = full expanded texture, viewport applied
                float exp_vp_x = vp_x;
                float exp_vp_y = vp_y;
                float exp_src_w = static_cast<float>(g_tac_tex_w);
                float exp_src_h = static_cast<float>(g_tac_tex_h);

                // UV coordinates within expanded texture
                float u0 = exp_vp_x / exp_src_w;
                float v0 = exp_vp_y / exp_src_h;
                float u1 = (exp_vp_x + tac_w / rel_zoom) / exp_src_w;
                float v1 = (exp_vp_y + tac_h / rel_zoom) / exp_src_h;

                // Clamp UVs
                if (u0 < 0.0f) u0 = 0.0f;
                if (v0 < 0.0f) v0 = 0.0f;
                if (u1 > 1.0f) u1 = 1.0f;
                if (v1 > 1.0f) v1 = 1.0f;

                // NDC coordinates
                float nx0 = static_cast<float>(dst_x) / win_w * 2.0f - 1.0f;
                float ny0 = 1.0f - static_cast<float>(dst_y) / win_h * 2.0f;
                float nx1 = static_cast<float>(dst_x + dst_w) / win_w * 2.0f - 1.0f;
                float ny1 = 1.0f - static_cast<float>(dst_y + dst_h) / win_h * 2.0f;

                glUniform4f(g_u_src_rect, u0, v0, u1, v1);
                glUniform4f(g_u_dst_rect, nx0, ny0, nx1, ny1);

                static const float quad[] = { 0,0, 1,0, 0,1, 1,1 };
                glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, quad);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

                // Reset to main indexed texture for sidebar/tab
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, g_indexed_tex);

                g_tac_tex_active = false; // consumed this frame
            } else {
                // Normal path: sample from main indexed texture
                float src_x = static_cast<float>(tac_x) + vp_x;
                float src_y = static_cast<float>(tac_y) + vp_y;
                float src_w = static_cast<float>(tac_w) / rel_zoom;
                float src_h = static_cast<float>(tac_h) / rel_zoom;

                if (src_x < tac_x) src_x = static_cast<float>(tac_x);
                if (src_y < tac_y) src_y = static_cast<float>(tac_y);
                if (src_x + src_w > tac_x + tac_w) src_w = tac_x + tac_w - src_x;
                if (src_y + src_h > tac_y + tac_h) src_h = tac_y + tac_h - src_y;

                draw_quad(static_cast<int>(src_x), static_cast<int>(src_y),
                          static_cast<int>(src_w), static_cast<int>(src_h),
                          dst_x, dst_y, dst_w, dst_h,
                          win_w, win_h);
            }
        }

        // GL sprite overlay disabled — CPU replay through palette shader
        // already handles house colors, transparency, and all 16 rendering
        // modes correctly. GL sprite atlas builds in background for future use.
        // TODO: enable when GL sprites support house color remap + fading.

        // Sidebar
        if (side_w > 0) {
            draw_quad(side_x, 0, side_w, h,
                      offset_x + static_cast<int>(side_x * ui_scale),
                      offset_y,
                      static_cast<int>(side_w * ui_scale),
                      static_cast<int>(h * ui_scale),
                      win_w, win_h);
        }
    }

    glDisableVertexAttribArray(0);

    // Debug HUD overlay (GL quad at screen resolution)
    { extern void Render_Bridge_Debug_HUD_GL(int, int); Render_Bridge_Debug_HUD_GL(win_w, win_h); }

    SDL_GL_SwapWindow(g_window);
    return true;
}
