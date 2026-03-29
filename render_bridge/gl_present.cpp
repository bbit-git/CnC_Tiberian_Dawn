/**
 * gl_present.cpp — OpenGL ES presentation for the render bridge.
 *
 * Replaces the CPU palette LUT conversion in present.cpp with a GL
 * palette lookup shader. The 8-bit indexed SeenBuff is uploaded as
 * a GL_LUMINANCE texture, the VGA palette as a 256x1 GL_RGB texture.
 * A fragment shader does the palette lookup on the GPU.
 *
 * Falls back to CPU path if GL init fails (headless, missing drivers).
 */

#include <SDL3/SDL.h>
#include <GLES2/gl2.h>
#include "render_bridge.h"
#include "function.h"
#include "dbg.h"

extern SDL_Window* g_window;

extern int g_shake_remaining; // from present.cpp

static SDL_GLContext g_gl_ctx = nullptr;
static GLuint g_gl_program    = 0;
static GLuint g_indexed_tex   = 0;
static GLuint g_palette_tex   = 0;
static GLuint g_quad_vbo      = 0;
static int    g_tex_w = 0, g_tex_h = 0;
static bool   g_gl_ready = false;
static bool   g_gl_failed = false;

static const char* vert_src = R"(
    attribute vec2 a_pos;
    attribute vec2 a_uv;
    varying vec2 v_uv;
    void main() {
        gl_Position = vec4(a_pos, 0.0, 1.0);
        v_uv = a_uv;
    }
)";

static const char* frag_src = R"(
    precision mediump float;
    varying vec2 v_uv;
    uniform sampler2D u_indexed;
    uniform sampler2D u_palette;
    void main() {
        float idx = texture2D(u_indexed, v_uv).r;
        gl_FragColor = texture2D(u_palette, vec2(idx, 0.5));
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

bool GL_Present_Init(int w, int h)
{
    if (g_gl_failed) return false;
    if (g_gl_ready) return true;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    g_gl_ctx = SDL_GL_CreateContext(g_window);
    if (!g_gl_ctx) {
        DBG("GL context creation failed: %s", SDL_GetError());
        g_gl_failed = true;
        return false;
    }
    SDL_GL_MakeCurrent(g_window, g_gl_ctx);

    // Compile shader
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vert_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, frag_src);
    if (!vs || !fs) { g_gl_failed = true; return false; }

    g_gl_program = glCreateProgram();
    glAttachShader(g_gl_program, vs);
    glAttachShader(g_gl_program, fs);
    glBindAttribLocation(g_gl_program, 0, "a_pos");
    glBindAttribLocation(g_gl_program, 1, "a_uv");
    glLinkProgram(g_gl_program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(g_gl_program, GL_LINK_STATUS, &ok);
    if (!ok) { g_gl_failed = true; return false; }

    // Create indexed texture (8-bit, updated each frame)
    glGenTextures(1, &g_indexed_tex);
    glBindTexture(GL_TEXTURE_2D, g_indexed_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, w, h, 0,
                 GL_LUMINANCE, GL_UNSIGNED_BYTE, nullptr);

    // Create palette texture (256x1 RGB, updated on palette change)
    glGenTextures(1, &g_palette_tex);
    glBindTexture(GL_TEXTURE_2D, g_palette_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 1, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, nullptr);

    // Fullscreen quad VBO: position + UV
    static const float quad[] = {
        -1.0f,  1.0f,  0.0f, 0.0f,  // top-left
         1.0f,  1.0f,  1.0f, 0.0f,  // top-right
        -1.0f, -1.0f,  0.0f, 1.0f,  // bottom-left
         1.0f, -1.0f,  1.0f, 1.0f,  // bottom-right
    };
    glGenBuffers(1, &g_quad_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

    g_tex_w = w;
    g_tex_h = h;
    g_gl_ready = true;

    DBG("GL present init: %dx%d", w, h);
    return true;
}

void GL_Present_Shutdown()
{
    if (g_indexed_tex) { glDeleteTextures(1, &g_indexed_tex); g_indexed_tex = 0; }
    if (g_palette_tex) { glDeleteTextures(1, &g_palette_tex); g_palette_tex = 0; }
    if (g_quad_vbo) { glDeleteBuffers(1, &g_quad_vbo); g_quad_vbo = 0; }
    if (g_gl_program) { glDeleteProgram(g_gl_program); g_gl_program = 0; }
    if (g_gl_ctx) { SDL_GL_DestroyContext(g_gl_ctx); g_gl_ctx = nullptr; }
    g_gl_ready = false;
}

bool GL_Present_Frame(const uint8_t* indexed_pixels, int pitch,
                       int w, int h, const uint8_t* vga_palette)
{
    if (!g_gl_ready) return false;

    SDL_GL_MakeCurrent(g_window, g_gl_ctx);

    // Upload indexed framebuffer
    if (pitch == w) {
        glBindTexture(GL_TEXTURE_2D, g_indexed_tex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h,
                        GL_LUMINANCE, GL_UNSIGNED_BYTE, indexed_pixels);
    } else {
        // Row-by-row upload for non-contiguous pitch
        glBindTexture(GL_TEXTURE_2D, g_indexed_tex);
        for (int row = 0; row < h; row++) {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, row, w, 1,
                            GL_LUMINANCE, GL_UNSIGNED_BYTE,
                            indexed_pixels + row * pitch);
        }
    }

    // Upload palette (VGA 6-bit → 8-bit conversion)
    static uint8_t pal_rgb[256 * 3];
    static const uint8_t* last_pal = nullptr;
    if (vga_palette != last_pal) {
        for (int i = 0; i < 256; i++) {
            pal_rgb[i * 3 + 0] = vga_palette[i * 3 + 0] << 2;
            pal_rgb[i * 3 + 1] = vga_palette[i * 3 + 1] << 2;
            pal_rgb[i * 3 + 2] = vga_palette[i * 3 + 2] << 2;
        }
        glBindTexture(GL_TEXTURE_2D, g_palette_tex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1,
                        GL_RGB, GL_UNSIGNED_BYTE, pal_rgb);
        last_pal = vga_palette;
    }

    // Letterbox: maintain game aspect ratio within window
    int win_w = 0, win_h = 0;
    SDL_GetWindowSizeInPixels(g_window, &win_w, &win_h);

    float game_aspect = static_cast<float>(w) / static_cast<float>(h);
    float win_aspect  = static_cast<float>(win_w) / static_cast<float>(win_h);

    int vp_x = 0, vp_y = 0, vp_w = win_w, vp_h = win_h;
    if (win_aspect > game_aspect) {
        // Window wider than game — pillarbox (black bars on sides)
        vp_w = static_cast<int>(win_h * game_aspect);
        vp_x = (win_w - vp_w) / 2;
    } else {
        // Window taller than game — letterbox (black bars top/bottom)
        vp_h = static_cast<int>(win_w / game_aspect);
        vp_y = (win_h - vp_h) / 2;
    }

    glViewport(0, 0, win_w, win_h); // clear entire window
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Screen shake: offset the viewport
    if (g_shake_remaining > 0) {
        vp_x += (rand() % 5) - 2;
        vp_y += (rand() % 5) - 2;
        g_shake_remaining--;
    }

    glViewport(vp_x, vp_y, vp_w, vp_h); // render into letterboxed area

    glUseProgram(g_gl_program);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_indexed_tex);
    glUniform1i(glGetUniformLocation(g_gl_program, "u_indexed"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, g_palette_tex);
    glUniform1i(glGetUniformLocation(g_gl_program, "u_palette"), 1);

    glBindBuffer(GL_ARRAY_BUFFER, g_quad_vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, (void*)8);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);

    SDL_GL_SwapWindow(g_window);
    return true;
}
