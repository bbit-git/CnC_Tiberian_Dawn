/**
 * gl_present.cpp — GL frame orchestration and shared GL presentation state.
 */

#include "gl_present_internal.h"
#include "ui/ui_dialog.h"
#include "ui/ui_input.h"
#include "ui/ui_main_menu.h"
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
extern void GL_Sprites_Init();
extern void Options_Set_HD_Graphics_Default(bool enabled);
#endif

// Runtime chrome toggle — default false (bridge-native sidebar).
// Set to true by NATIVE_SIDEBAR=0 env var to fall back to legacy SeenBuff sidebar.
bool k_enable_seenbuff_chrome = false;

void Render_Bridge_Chrome_Toggle()
{
    k_enable_seenbuff_chrome = !k_enable_seenbuff_chrome;
    DBG("SeenBuff chrome %s", k_enable_seenbuff_chrome ? "enabled" : "disabled");
}

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
GLuint g_tac_program = 0;
GLint g_tac_u_indexed = -1;
GLint g_tac_u_palette = -1;
GLint g_tac_u_src_rect = -1;
GLint g_tac_u_dst_rect = -1;
GLuint g_ui_program = 0;
GLint g_ui_u_indexed = -1;
GLint g_ui_u_palette = -1;
GLint g_ui_u_src_rect = -1;
GLint g_ui_u_dst_rect = -1;
GLuint g_rgba_program = 0;
GLuint g_rgba_tex = 0;
int g_rgba_tex_w = 0;
int g_rgba_tex_h = 0;
GLint g_rgba_u_tex = -1;
GLint g_rgba_u_src_rect = -1;
GLint g_rgba_u_dst_rect = -1;
static uint8_t* g_rgba_upload = nullptr;
static int g_rgba_upload_alloc = 0;

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

// Tactical shader: identical to frag_src but discards the HD terrain key index
// (254) so that HD terrain drawn underneath shows through the holes.
static const char* frag_tac_src = R"(
    precision mediump float;
    varying vec2 v_uv;
    uniform sampler2D u_indexed;
    uniform sampler2D u_palette;
    void main() {
        float idx = texture2D(u_indexed, v_uv).r;
        float idx_i = floor(idx * 255.0 + 0.5);
        if (idx_i > 253.5 && idx_i < 254.5) discard;
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

static const char* frag_rgba_src = R"(
    precision mediump float;
    varying vec2 v_uv;
    uniform sampler2D u_tex;
    void main() {
        gl_FragColor = texture2D(u_tex, v_uv);
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

    // Bridge-native sidebar is the default. NATIVE_SIDEBAR=0 re-enables
    // the legacy SeenBuff sidebar/header chrome as a fallback.
    const char* ns_env = std::getenv("NATIVE_SIDEBAR");
    if (ns_env && ns_env[0] == '0') {
        k_enable_seenbuff_chrome = true;
        DBG("NATIVE_SIDEBAR=0: SeenBuff chrome enabled (legacy fallback)");
    }

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

    GLuint tac_vs = compile_shader(GL_VERTEX_SHADER, vert_src);
    GLuint tac_fs = compile_shader(GL_FRAGMENT_SHADER, frag_tac_src);
    if (tac_vs && tac_fs) {
        g_tac_program = glCreateProgram();
        glAttachShader(g_tac_program, tac_vs);
        glAttachShader(g_tac_program, tac_fs);
        glBindAttribLocation(g_tac_program, 0, "a_pos");
        glLinkProgram(g_tac_program);
        glDeleteShader(tac_vs);
        glDeleteShader(tac_fs);
        g_tac_u_indexed = glGetUniformLocation(g_tac_program, "u_indexed");
        g_tac_u_palette = glGetUniformLocation(g_tac_program, "u_palette");
        g_tac_u_src_rect = glGetUniformLocation(g_tac_program, "u_src_rect");
        g_tac_u_dst_rect = glGetUniformLocation(g_tac_program, "u_dst_rect");
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

    GLuint rgba_vs = compile_shader(GL_VERTEX_SHADER, vert_src);
    GLuint rgba_fs = compile_shader(GL_FRAGMENT_SHADER, frag_rgba_src);
    if (rgba_vs && rgba_fs) {
        g_rgba_program = glCreateProgram();
        glAttachShader(g_rgba_program, rgba_vs);
        glAttachShader(g_rgba_program, rgba_fs);
        glBindAttribLocation(g_rgba_program, 0, "a_pos");
        glLinkProgram(g_rgba_program);
        glDeleteShader(rgba_vs);
        glDeleteShader(rgba_fs);
        g_rgba_u_tex = glGetUniformLocation(g_rgba_program, "u_tex");
        g_rgba_u_src_rect = glGetUniformLocation(g_rgba_program, "u_src_rect");
        g_rgba_u_dst_rect = glGetUniformLocation(g_rgba_program, "u_dst_rect");
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
#ifdef USE_RENDER_BRIDGE_GL_SPRITES
    GL_Sprites_Init();
    Options_Set_HD_Graphics_Default(Render_Bridge_Get_HD_Graphics());
#endif

    int sw = 0;
    int sh = 0;
    SDL_GetWindowSizeInPixels(g_window, &sw, &sh);
    DBG("GL present: buffer %dx%d, window %dx%d", w, h, sw, sh);
    return true;
}

void GL_Present_Shutdown()
{
    UI_Main_Menu_Shutdown();
    if (g_indexed_tex) { glDeleteTextures(1, &g_indexed_tex); g_indexed_tex = 0; }
    if (g_palette_tex) { glDeleteTextures(1, &g_palette_tex); g_palette_tex = 0; }
    if (g_tac_tex) { glDeleteTextures(1, &g_tac_tex); g_tac_tex = 0; }
    if (g_ui_tex) { glDeleteTextures(1, &g_ui_tex); g_ui_tex = 0; }
    if (g_cursor_tex) { glDeleteTextures(1, &g_cursor_tex); g_cursor_tex = 0; }
    if (g_rgba_tex) { glDeleteTextures(1, &g_rgba_tex); g_rgba_tex = 0; }
    if (g_cursor_overlay) { free(g_cursor_overlay); g_cursor_overlay = nullptr; g_cursor_overlay_alloc = 0; }
    if (g_rgba_upload) { free(g_rgba_upload); g_rgba_upload = nullptr; g_rgba_upload_alloc = 0; }
    if (g_tac_program) { glDeleteProgram(g_tac_program); g_tac_program = 0; }
    if (g_rgba_program) { glDeleteProgram(g_rgba_program); g_rgba_program = 0; }
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

bool GL_Present_Draw_Indexed_RGBA(const uint8_t* indexed_pixels, int src_w, int src_h,
                                  const uint8_t* vga_palette, bool transparent_zero,
                                  int dst_x, int dst_y, int dst_w, int dst_h,
                                  int win_w, int win_h)
{
    if (!indexed_pixels || src_w <= 0 || src_h <= 0 || !vga_palette || !g_rgba_program) {
        return false;
    }

    int needed = src_w * src_h * 4;
    if (!g_rgba_upload || g_rgba_upload_alloc < needed) {
        free(g_rgba_upload);
        g_rgba_upload = static_cast<uint8_t*>(malloc(needed));
        g_rgba_upload_alloc = needed;
    }
    if (!g_rgba_upload) {
        return false;
    }

    for (int i = 0; i < src_w * src_h; i++) {
        uint8_t idx = indexed_pixels[i];
        g_rgba_upload[i * 4 + 0] = vga_palette[idx * 3 + 0] << 2;
        g_rgba_upload[i * 4 + 1] = vga_palette[idx * 3 + 1] << 2;
        g_rgba_upload[i * 4 + 2] = vga_palette[idx * 3 + 2] << 2;
        g_rgba_upload[i * 4 + 3] = (transparent_zero && idx == 0) ? 0 : 255;
    }

    if (!g_rgba_tex || g_rgba_tex_w != src_w || g_rgba_tex_h != src_h) {
        if (g_rgba_tex) glDeleteTextures(1, &g_rgba_tex);
        glGenTextures(1, &g_rgba_tex);
        glBindTexture(GL_TEXTURE_2D, g_rgba_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, src_w, src_h, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, g_rgba_upload);
        g_rgba_tex_w = src_w;
        g_rgba_tex_h = src_h;
    } else {
        glBindTexture(GL_TEXTURE_2D, g_rgba_tex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, src_w, src_h,
                        GL_RGBA, GL_UNSIGNED_BYTE, g_rgba_upload);
    }

    glUseProgram(g_rgba_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_rgba_tex);
    glUniform1i(g_rgba_u_tex, 0);

    float x0 = static_cast<float>(dst_x) / win_w * 2.0f - 1.0f;
    float y0 = 1.0f - static_cast<float>(dst_y) / win_h * 2.0f;
    float x1 = static_cast<float>(dst_x + dst_w) / win_w * 2.0f - 1.0f;
    float y1 = 1.0f - static_cast<float>(dst_y + dst_h) / win_h * 2.0f;
    glUniform4f(g_rgba_u_src_rect, 0.0f, 0.0f, 1.0f, 1.0f);
    glUniform4f(g_rgba_u_dst_rect, x0, y0, x1, y1);

    static const float quad[] = { 0,0, 1,0, 0,1, 1,1 };
    GL_Present_Bind_Client_Quad(quad);
    if (transparent_zero) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    if (transparent_zero) {
        glDisable(GL_BLEND);
    }
    GL_Present_Bind_Palette_Program();
    return true;
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

    if (InMainLoop) {
        // Sidebar/tab toggles and other layout changes can happen without a
        // zoom or screen-size event. Refresh the cached bridge layout every
        // gameplay frame so presentation, input, and UI use the same rects.
        Render_Bridge_Refresh_Layout();
    }

    SDL_GL_MakeCurrent(g_window, g_gl_ctx);
    // SeenBuff indexed texture is needed for menu passthrough and, when
    // k_enable_seenbuff_chrome is true, for legacy sidebar/header chrome.
    // Skip the upload during gameplay only when the chrome is fully native.
    if (!InMainLoop || k_enable_seenbuff_chrome) {
        upload_indexed_frame(indexed_pixels, pitch, w, h);
    }
    upload_palette(vga_palette);

#ifdef USE_RENDER_BRIDGE_GL_SPRITES
    GL_Sprites_Build_Atlas(vga_palette);
#endif

    int win_w = 0;
    int win_h = 0;
    if (!resolve_window_size(win_w, win_h, w, h)) {
        return false;
    }

    int layout_w = w;
    int layout_h = h;
    if (InMainLoop) {
        Render_Bridge_Get_Logical_Screen_Size(layout_w, layout_h);
        if (layout_w <= 0 || layout_h <= 0) {
            layout_w = w;
            layout_h = h;
        }
    }

    float legacy_sx = static_cast<float>(win_w) / static_cast<float>(w);
    float legacy_sy = static_cast<float>(win_h) / static_cast<float>(h);
    // Legacy menus/UI/cursor keep the original game-buffer transform.
    float legacy_ui_scale = (legacy_sx < legacy_sy) ? legacy_sx : legacy_sy;
    int legacy_offset_x = static_cast<int>((win_w - w * legacy_ui_scale) * 0.5f);
    int legacy_offset_y = static_cast<int>((win_h - h * legacy_ui_scale) * 0.5f);

    float sx = static_cast<float>(win_w) / static_cast<float>(layout_w);
    float sy = static_cast<float>(win_h) / static_cast<float>(layout_h);
    // Bridge-owned gameplay regions use the promoted logical screen layout.
    float ui_scale = (sx < sy) ? sx : sy;

    int offset_x = static_cast<int>((win_w - layout_w * ui_scale) * 0.5f);
    int offset_y = static_cast<int>((win_h - layout_h * ui_scale) * 0.5f);
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
    ctx.legacy_ui_scale = legacy_ui_scale;
    ctx.legacy_offset_x = legacy_offset_x;
    ctx.legacy_offset_y = legacy_offset_y;
    ctx.ui_scale = ui_scale;
    ctx.offset_x = offset_x;
    ctx.offset_y = offset_y;
    ctx.in_main_loop = InMainLoop;
    if (ctx.in_main_loop) {
        Render_Bridge_Get_Tactical_Rect(ctx.tactical_game_x, ctx.tactical_game_y,
                                        ctx.tactical_game_w, ctx.tactical_game_h);
        int side_y = 0, side_h = 0;
        Render_Bridge_Get_Sidebar_Rect(ctx.side_game_x, side_y,
                                       ctx.side_game_w, side_h);
        int rtac_x = 0, rtac_y = 0, rtac_w = 0, rtac_h = 0;
        Render_Bridge_Get_Render_Tactical_Rect(rtac_x, rtac_y, rtac_w, rtac_h);
        ctx.render_tac_screen_x = offset_x + static_cast<int>(std::round(rtac_x * ui_scale));
        ctx.render_tac_screen_y = offset_y + static_cast<int>(std::round(rtac_y * ui_scale));
        ctx.render_tac_screen_w = static_cast<int>(std::round(rtac_w * ui_scale));
        ctx.render_tac_screen_h = static_cast<int>(std::round(rtac_h * ui_scale));
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

    // Skip SeenBuff (PCX) draw when the main menu has an HD background —
    // the HD texture covers the full screen and the PCX would show through edges.
    bool skip_legacy_bg = !ctx.in_main_loop &&
                          UI_Main_Menu_Has_HD_Background();
    if (!skip_legacy_bg) {
        GL_Present_Bind_Palette_Program();
        GL_Present_Draw_Regions(ctx, indexed_pixels, vga_palette);
    }

    if (ctx.in_main_loop) {
        bool has_ui_overlay = GL_Present_Draw_UI_Overlay(ctx, vga_palette);
        GL_Present_Draw_Source_Overlay(ctx, has_ui_overlay);
    }

    // Bridge-native UI overlay (header, sidebar chrome, controls)
    if (ctx.in_main_loop) {
        extern void Render_Bridge_UI_Begin_Frame();
        extern void Render_Bridge_UI_End_Frame();
        extern void UI_Header_Emit();
        extern void UI_Help_Emit();
        extern void UI_Messages_Emit();
        extern void UI_Dialog_Emit();
        extern void UI_Sidebar_Emit();
        extern void UI_Radar_Emit();
        extern void UI_Game_Options_Emit();
        extern void UI_Load_Dialog_Emit();
        extern void UI_Game_Controls_Emit();
        extern void UI_Visual_Controls_Emit();
        extern void UI_Sound_Controls_Emit();
        extern void UI_Special_Dialog_Emit();
        extern void UI_Expansion_Dialog_Emit();
        extern void UI_Com_Scenario_Emit();
        extern void UI_Main_Menu_Options_Emit();

        extern void UI_Tooltip_Emit(int, int, int, int);
        extern void GL_UI_Render(int, int, int, int, float);
        extern int g_mouse_x, g_mouse_y;

        Render_Bridge_UI_Begin_Frame();
        UI_Header_Emit();
        UI_Help_Emit();
        UI_Messages_Emit();
        UI_Sidebar_Emit();
        UI_Radar_Emit();
        // Complex dialogs (only one active at a time)
        UI_Game_Options_Emit();
        UI_Load_Dialog_Emit();
        UI_Game_Controls_Emit();
        UI_Visual_Controls_Emit();
        UI_Sound_Controls_Emit();
        UI_Special_Dialog_Emit();
        UI_Expansion_Dialog_Emit();
        UI_Com_Scenario_Emit();
        UI_Main_Menu_Options_Emit();
        UI_Main_Menu_Emit();
        // Simple confirm (covers CCMessageBox + Surrender)
        UI_Dialog_Emit();
        UI_Tooltip_Emit(g_mouse_x, g_mouse_y,
                        layout_w, layout_h);
        Render_Bridge_UI_End_Frame();
        GL_UI_Render(ctx.win_w, ctx.win_h,
                     ctx.offset_x, ctx.offset_y, ctx.ui_scale);
    }

    // Outside the main loop, emit any active bridge-native dialogs so that this
    // single swap shows both the background and the dialog — no second swap needed.
    // Dialog emitters use Render_Bridge_Get_Logical_Screen_Size() for positioning
    // (physical display coordinates), so GL_UI_Render must use the matching
    // logical-screen-derived scale, NOT the legacy SeenBuff-based scale.
    if (!ctx.in_main_loop && Render_Bridge_UI_Has_Any_Active_Dialog()) {
        if (UI_Main_Menu_Has_HD_Background()) {
            UI_Main_Menu_Draw_HD_Background(ctx.win_w, ctx.win_h);
        }

        extern void Render_Bridge_UI_Begin_Frame();
        extern void Render_Bridge_UI_End_Frame();
        extern void UI_Game_Options_Emit();
        extern void UI_Load_Dialog_Emit();
        extern void UI_Game_Controls_Emit();
        extern void UI_Visual_Controls_Emit();
        extern void UI_Sound_Controls_Emit();
        extern void UI_Special_Dialog_Emit();
        extern void UI_Expansion_Dialog_Emit();
        extern void UI_Com_Scenario_Emit();
        extern void UI_Main_Menu_Options_Emit();

        extern void UI_Dialog_Emit();
        extern void GL_UI_Render(int, int, int, int, float);

        Render_Bridge_UI_Begin_Frame();
        UI_Game_Options_Emit();
        UI_Load_Dialog_Emit();
        UI_Game_Controls_Emit();
        UI_Visual_Controls_Emit();
        UI_Sound_Controls_Emit();
        UI_Special_Dialog_Emit();
        UI_Expansion_Dialog_Emit();
        UI_Com_Scenario_Emit();
        UI_Main_Menu_Options_Emit();
        UI_Main_Menu_Emit();
        UI_Dialog_Emit();
        Render_Bridge_UI_End_Frame();

        GL_UI_Render(ctx.win_w, ctx.win_h,
                     ctx.legacy_offset_x, ctx.legacy_offset_y, ctx.legacy_ui_scale);
    }

    GL_Present_Draw_Debug_Overlays(ctx);
    GL_Present_Draw_Cursor(ctx);

    glDisableVertexAttribArray(0);
    SDL_GL_SwapWindow(g_window);
    return true;
}

void Render_Bridge_Idle_Frame()
{
    if (!g_gl_ready) return;

    SDL_GL_MakeCurrent(g_window, g_gl_ctx);

    int buf_w = SeenBuff.Get_Width();
    int buf_h = SeenBuff.Get_Height();
    if (buf_w <= 0) buf_w = 640;
    if (buf_h <= 0) buf_h = 400;

    int win_w = 0, win_h = 0;
    if (!resolve_window_size(win_w, win_h, buf_w, buf_h)) return;

    float sx = static_cast<float>(win_w) / static_cast<float>(buf_w);
    float sy = static_cast<float>(win_h) / static_cast<float>(buf_h);
    float ui_scale = (sx < sy) ? sx : sy;
    int offset_x = static_cast<int>((win_w - buf_w * ui_scale) * 0.5f);
    int offset_y = static_cast<int>((win_h - buf_h * ui_scale) * 0.5f);

    glViewport(0, 0, win_w, win_h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glEnableVertexAttribArray(0);

    // Upload current palette so the indexed SeenBuff renders if needed
    const uint8_t* pal = static_cast<const uint8_t*>((void*)Get_Palette());
    if (!pal) pal = GamePalette;
    if (pal) {
        upload_palette(pal);
    }

    // Draw the SeenBuff as background (shows the menu screen behind the dialog)
    const uint8_t* pixels = static_cast<const uint8_t*>(SeenBuff.Get_Buffer());
    if (pixels) {
        int pitch = SeenBuff.Get_Full_Pitch();
        upload_indexed_frame(pixels, pitch, buf_w, buf_h);
        GL_Present_Bind_Palette_Program();
        // Build a minimal context for legacy region draw
        GLPresentFrameContext ctx;
        ctx.win_w = win_w;
        ctx.win_h = win_h;
        ctx.buffer_w = buf_w;
        ctx.buffer_h = buf_h;
        ctx.legacy_ui_scale = ui_scale;
        ctx.legacy_offset_x = offset_x;
        ctx.legacy_offset_y = offset_y;
        ctx.ui_scale = ui_scale;
        ctx.offset_x = offset_x;
        ctx.offset_y = offset_y;
        ctx.in_main_loop = false;
        ctx.tactical_game_x = 0;
        ctx.tactical_game_y = 0;
        ctx.tactical_game_w = buf_w;
        ctx.tactical_game_h = buf_h;
        ctx.side_game_x = buf_w;
        ctx.side_game_w = 0;
        GL_Present_Draw_Regions(ctx, pixels, pal);
    }

    // If the main menu is active with an HD background, draw it over the SeenBuff.
    if (!InMainLoop && UI_Main_Menu_Has_HD_Background()) {
        UI_Main_Menu_Draw_HD_Background(win_w, win_h);
    }

    // Run only dialog emitters — no gameplay UI (header, sidebar, etc.)
    {
        extern void Render_Bridge_UI_Begin_Frame();
        extern void Render_Bridge_UI_End_Frame();
        extern void UI_Game_Options_Emit();
        extern void UI_Load_Dialog_Emit();
        extern void UI_Game_Controls_Emit();
        extern void UI_Visual_Controls_Emit();
        extern void UI_Sound_Controls_Emit();
        extern void UI_Special_Dialog_Emit();
        extern void UI_Expansion_Dialog_Emit();
        extern void UI_Com_Scenario_Emit();
        extern void UI_Main_Menu_Options_Emit();

        extern void UI_Dialog_Emit();
        extern void GL_UI_Render(int, int, int, int, float);

        Render_Bridge_UI_Begin_Frame();
        UI_Game_Options_Emit();
        UI_Load_Dialog_Emit();
        UI_Game_Controls_Emit();
        UI_Visual_Controls_Emit();
        UI_Sound_Controls_Emit();
        UI_Special_Dialog_Emit();
        UI_Expansion_Dialog_Emit();
        UI_Com_Scenario_Emit();
        UI_Main_Menu_Options_Emit();
        UI_Main_Menu_Emit();
        UI_Dialog_Emit();
        Render_Bridge_UI_End_Frame();
        GL_UI_Render(win_w, win_h, offset_x, offset_y, ui_scale);
    }

    glDisableVertexAttribArray(0);
    SDL_GL_SwapWindow(g_window);
}
