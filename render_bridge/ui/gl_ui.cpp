/**
 * gl_ui.cpp — GL ES 2.0 renderer for the bridge-native UI draw list.
 *
 * Renders UI_CMD_FILL_RECT, UI_CMD_DRAW_RECT, UI_CMD_TEXT, UI_CMD_CLIP_PUSH/POP.
 * Uses scissor test for clipping. Text is rendered as textured quads from
 * font atlas textures. All coordinates are in game-buffer pixel space;
 * the caller provides the screen transform (offset + scale).
 */

#include "ui_draw_list.h"
#include "ui_text.h"
#include "ui_text_sdf.h"
#include "dbg.h"

#include <cmath>
#include <algorithm>

#undef min
#undef max
#include <GLES2/gl2.h>

#include <cstring>
#include <vector>

namespace {

struct UIVertex {
    float x, y;
    float u, v;
    float r, g, b, a;
};

GLuint g_program      = 0;
GLuint g_program_sdf  = 0;
GLint  g_u_viewport   = -1;
GLint  g_u_use_tex    = -1;
GLint  g_u_tex        = -1;
GLint  g_sdf_u_viewport   = -1;
GLint  g_sdf_u_tex        = -1;
GLint  g_sdf_u_sdf_params = -1;
bool   g_initialized  = false;

// Font atlas GL textures (one per UIFontID)
GLuint g_font_textures[UI_FONT_COUNT] = {};
bool   g_font_tex_ready[UI_FONT_COUNT] = {};

// Clip rect stack
struct ClipRect { int x, y, w, h; };
std::vector<ClipRect> g_clip_stack;

static const char* k_vert_src = R"(
    attribute vec2 a_pos;
    attribute vec2 a_uv;
    attribute vec4 a_color;
    varying vec2 v_uv;
    varying vec4 v_color;
    uniform vec2 u_viewport;
    void main() {
        vec2 p = a_pos / u_viewport * 2.0 - 1.0;
        p.y = -p.y;
        gl_Position = vec4(p, 0.0, 1.0);
        v_uv = a_uv;
        v_color = a_color;
    }
)";

static const char* k_frag_src = R"(
    precision mediump float;
    varying vec2 v_uv;
    varying vec4 v_color;
    uniform float u_use_tex;
    uniform sampler2D u_tex;
    void main() {
        if (u_use_tex > 0.5) {
            float a = texture2D(u_tex, v_uv).r;
            if (a < 0.01) discard;
            gl_FragColor = vec4(v_color.rgb, v_color.a * a);
        } else {
            gl_FragColor = v_color;
        }
    }
)";

static const char* k_frag_sdf_src = R"(
    precision mediump float;
    varying vec2 v_uv;
    varying vec4 v_color;
    uniform sampler2D u_tex;
    uniform vec2 u_sdf_params;
    void main() {
        float dist = texture2D(u_tex, v_uv).r;
        float alpha = smoothstep(u_sdf_params.y - u_sdf_params.x,
                                 u_sdf_params.y + u_sdf_params.x,
                                 dist);
        if (alpha < 0.005) discard;
        gl_FragColor = vec4(v_color.rgb, v_color.a * alpha);
    }
)";

bool init_gl()
{
    if (g_initialized) return true;

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &k_vert_src, nullptr);
    glCompileShader(vs);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &k_frag_src, nullptr);
    glCompileShader(fs);

    g_program = glCreateProgram();
    glAttachShader(g_program, vs);
    glAttachShader(g_program, fs);
    glBindAttribLocation(g_program, 0, "a_pos");
    glBindAttribLocation(g_program, 1, "a_uv");
    glBindAttribLocation(g_program, 2, "a_color");
    glLinkProgram(g_program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(g_program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[256];
        glGetProgramInfoLog(g_program, sizeof(log), nullptr, log);
        DBG("gl_ui: link error: %s", log);
        return false;
    }

    g_u_viewport = glGetUniformLocation(g_program, "u_viewport");
    g_u_use_tex  = glGetUniformLocation(g_program, "u_use_tex");
    g_u_tex      = glGetUniformLocation(g_program, "u_tex");

    // SDF program (shares vertex shader, different fragment)
    {
        GLuint vs2 = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs2, 1, &k_vert_src, nullptr);
        glCompileShader(vs2);

        GLuint fs2 = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs2, 1, &k_frag_sdf_src, nullptr);
        glCompileShader(fs2);

        g_program_sdf = glCreateProgram();
        glAttachShader(g_program_sdf, vs2);
        glAttachShader(g_program_sdf, fs2);
        glBindAttribLocation(g_program_sdf, 0, "a_pos");
        glBindAttribLocation(g_program_sdf, 1, "a_uv");
        glBindAttribLocation(g_program_sdf, 2, "a_color");
        glLinkProgram(g_program_sdf);
        glDeleteShader(vs2);
        glDeleteShader(fs2);

        GLint ok2 = 0;
        glGetProgramiv(g_program_sdf, GL_LINK_STATUS, &ok2);
        if (!ok2) {
            char log2[256];
            glGetProgramInfoLog(g_program_sdf, sizeof(log2), nullptr, log2);
            DBG("gl_ui: SDF link error: %s", log2);
        } else {
            g_sdf_u_viewport   = glGetUniformLocation(g_program_sdf, "u_viewport");
            g_sdf_u_tex        = glGetUniformLocation(g_program_sdf, "u_tex");
            g_sdf_u_sdf_params = glGetUniformLocation(g_program_sdf, "u_sdf_params");
        }
    }

    g_initialized = true;
    return true;
}

void ensure_font_texture(UIFontID font_id)
{
    if (font_id < 0 || font_id >= UI_FONT_COUNT) return;
    if (g_font_tex_ready[font_id]) return;

    const UIFontAtlas* atlas = UI_Text_Get_Atlas(font_id);
    if (!atlas) return;

    if (!g_font_textures[font_id]) {
        glGenTextures(1, &g_font_textures[font_id]);
    }
    glBindTexture(GL_TEXTURE_2D, g_font_textures[font_id]);
    GLenum filter = atlas->is_sdf ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,
                 atlas->atlas_w, atlas->atlas_h, 0,
                 GL_LUMINANCE, GL_UNSIGNED_BYTE, atlas->pixels);
    g_font_tex_ready[font_id] = true;
}

void push_quad(std::vector<UIVertex>& verts,
               float x0, float y0, float x1, float y1,
               float u0, float v0, float u1, float v1,
               float r, float g, float b, float a)
{
    verts.push_back({x0, y0, u0, v0, r, g, b, a});
    verts.push_back({x1, y0, u1, v0, r, g, b, a});
    verts.push_back({x0, y1, u0, v1, r, g, b, a});
    verts.push_back({x1, y0, u1, v0, r, g, b, a});
    verts.push_back({x1, y1, u1, v1, r, g, b, a});
    verts.push_back({x0, y1, u0, v1, r, g, b, a});
}

void push_line_rect(std::vector<UIVertex>& verts,
                    float x0, float y0, float x1, float y1,
                    float r, float g, float b, float a)
{
    // Top, bottom, left, right as 1px thick quads
    push_quad(verts, x0, y0, x1, y0 + 1, 0,0,0,0, r,g,b,a);
    push_quad(verts, x0, y1 - 1, x1, y1, 0,0,0,0, r,g,b,a);
    push_quad(verts, x0, y0, x0 + 1, y1, 0,0,0,0, r,g,b,a);
    push_quad(verts, x1 - 1, y0, x1, y1, 0,0,0,0, r,g,b,a);
}

// is_sdf: true when the SDF program is active. The SDF shader always
// samples the texture, so u_use_tex (legacy program only) is skipped.
void flush_verts(std::vector<UIVertex>& verts, bool textured, bool is_sdf = false)
{
    if (verts.empty()) return;

    if (!is_sdf)
        glUniform1f(g_u_use_tex, textured ? 1.0f : 0.0f);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(UIVertex),
                          &verts[0].x);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(UIVertex),
                          &verts[0].u);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(UIVertex),
                          &verts[0].r);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<int>(verts.size()));
    verts.clear();
}

void apply_scissor(int win_w, int win_h, int off_x, int off_y, float scale)
{
    if (g_clip_stack.empty()) {
        glDisable(GL_SCISSOR_TEST);
        return;
    }
    const ClipRect& c = g_clip_stack.back();
    int sx = off_x + static_cast<int>(c.x * scale);
    int sy = win_h - off_y - static_cast<int>((c.y + c.h) * scale);
    int sw = static_cast<int>(c.w * scale);
    int sh = static_cast<int>(c.h * scale);
    glEnable(GL_SCISSOR_TEST);
    glScissor(sx, sy, sw, sh);
}

} // namespace

/// Render the UI draw list. Called from GL_Present_Frame after tactical + overlays.
/// off_x, off_y: screen pixel offset for the game area.
/// scale: game-buffer-to-screen scale factor.
void GL_UI_Render(int win_w, int win_h,
                  int off_x, int off_y, float scale)
{
    if (g_ui_draw_list.Command_Count() == 0) return;
    if (!init_gl()) return;

    glUseProgram(g_program);
    glUniform2f(g_u_viewport, static_cast<float>(win_w),
                static_cast<float>(win_h));
    glUniform1i(g_u_tex, 0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    g_clip_stack.clear();
    std::vector<UIVertex> solid_verts;
    std::vector<UIVertex> text_verts;
    int current_font = -1;
    bool current_sdf = false;

    auto restore_main_program = [&]() {
        if (current_sdf) {
            glUseProgram(g_program);
            glUniform2f(g_u_viewport, static_cast<float>(win_w),
                        static_cast<float>(win_h));
            glUniform1i(g_u_tex, 0);
            current_sdf = false;
            current_font = -1;
        }
    };

    for (int i = 0; i < g_ui_draw_list.Command_Count(); i++) {
        const UIDrawCmd& cmd = g_ui_draw_list.Get(i);

        switch (cmd.type) {
        case UI_CMD_FILL_RECT: {
            flush_verts(text_verts, true, current_sdf);
            restore_main_program();
            float x0 = off_x + cmd.rect.x * scale;
            float y0 = off_y + cmd.rect.y * scale;
            float x1 = x0 + cmd.rect.w * scale;
            float y1 = y0 + cmd.rect.h * scale;
            float r = cmd.rect.r / 255.0f;
            float g = cmd.rect.g / 255.0f;
            float b = cmd.rect.b / 255.0f;
            float a = cmd.rect.a / 255.0f;
            push_quad(solid_verts, x0, y0, x1, y1, 0,0,0,0, r,g,b,a);
            break;
        }

        case UI_CMD_DRAW_RECT: {
            flush_verts(text_verts, true, current_sdf);
            restore_main_program();
            float x0 = off_x + cmd.rect.x * scale;
            float y0 = off_y + cmd.rect.y * scale;
            float x1 = x0 + cmd.rect.w * scale;
            float y1 = y0 + cmd.rect.h * scale;
            float r = cmd.rect.r / 255.0f;
            float g = cmd.rect.g / 255.0f;
            float b = cmd.rect.b / 255.0f;
            float a = cmd.rect.a / 255.0f;
            push_line_rect(solid_verts, x0, y0, x1, y1, r,g,b,a);
            break;
        }

        case UI_CMD_TEXT: {
            // Flush pending solid geometry under the legacy program before
            // any potential switch to the SDF program.
            if (!solid_verts.empty()) {
                restore_main_program();
                flush_verts(solid_verts, false);
            }
            UIFontID fid = static_cast<UIFontID>(cmd.text.font_id);
            const UIFontAtlas* atlas = UI_Text_Get_Atlas(fid);
            if (!atlas) break;

            // Re-setup when font changes OR when the SDF/legacy program
            // doesn't match what this atlas needs (prevents SDF program
            // leaking into subsequent FNT text commands).
            bool need_sdf = atlas->is_sdf && g_program_sdf;
            if (current_font != fid || current_sdf != need_sdf) {
                flush_verts(text_verts, true, current_sdf);
                ensure_font_texture(fid);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, g_font_textures[fid]);

                if (need_sdf) {
                    glUseProgram(g_program_sdf);
                    glUniform2f(g_sdf_u_viewport,
                                static_cast<float>(win_w),
                                static_cast<float>(win_h));
                    glUniform1i(g_sdf_u_tex, 0);

                    float font_scale = UI_Text_Get_Font_Scale();
                    float render_size = atlas->line_height * scale * font_scale;
                    float screen_px_range = atlas->sdf_pixel_range *
                        (render_size / atlas->sdf_font_size);
                    float smoothing = 0.5f / screen_px_range;
                    smoothing = std::max(0.01f, std::min(0.5f, smoothing));
                    glUniform2f(g_sdf_u_sdf_params, smoothing, 0.5f);
                    current_sdf = true;
                } else {
                    if (current_sdf) restore_main_program();
                    current_sdf = false;
                }
                current_font = fid;
            }

            const char* str = g_ui_draw_list.Get_String(
                cmd.text.text_offset, cmd.text.text_length);
            float cr = cmd.text.r / 255.0f;
            float cg = cmd.text.g / 255.0f;
            float cb = cmd.text.b / 255.0f;
            float ca = cmd.text.a / 255.0f;
            float cx = off_x + cmd.text.x * scale;
            float cy = off_y + cmd.text.y * scale;

            float glyph_scale = scale;
            if (atlas->is_sdf) glyph_scale *= UI_Text_Get_Font_Scale();

            for (int c = 0; c < cmd.text.text_length; c++) {
                uint8_t ch = static_cast<uint8_t>(str[c]);
                const UIGlyph& gl = atlas->glyphs[ch];
                if (gl.width <= 0) { cx += gl.advance * glyph_scale; continue; }

                float gx0 = cx;
                float gy0 = atlas->is_sdf ? (cy + gl.top_blank * glyph_scale) : cy;
                float gx1 = cx + gl.width * glyph_scale;
                float gy1 = gy0 + gl.height * glyph_scale;

                push_quad(text_verts, gx0, gy0, gx1, gy1,
                          gl.u0, gl.v0, gl.u1, gl.v1,
                          cr, cg, cb, ca);
                cx += gl.advance * glyph_scale;
            }
            break;
        }

        case UI_CMD_CLIP_PUSH:
            flush_verts(solid_verts, false);
            flush_verts(text_verts, true, current_sdf);
            restore_main_program();
            g_clip_stack.push_back({cmd.clip.x, cmd.clip.y,
                                    cmd.clip.w, cmd.clip.h});
            apply_scissor(win_w, win_h, off_x, off_y, scale);
            break;

        case UI_CMD_CLIP_POP:
            flush_verts(solid_verts, false);
            flush_verts(text_verts, true, current_sdf);
            restore_main_program();
            if (!g_clip_stack.empty()) g_clip_stack.pop_back();
            apply_scissor(win_w, win_h, off_x, off_y, scale);
            break;

        default:
            break;
        }
    }

    flush_verts(text_verts, true, current_sdf);
    restore_main_program();
    flush_verts(solid_verts, false);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
}

void GL_UI_Shutdown()
{
    for (int i = 0; i < UI_FONT_COUNT; i++) {
        if (g_font_textures[i]) {
            glDeleteTextures(1, &g_font_textures[i]);
            g_font_textures[i] = 0;
        }
        g_font_tex_ready[i] = false;
    }
    if (g_program) {
        glDeleteProgram(g_program);
        g_program = 0;
    }
    if (g_program_sdf) {
        glDeleteProgram(g_program_sdf);
        g_program_sdf = 0;
    }
    g_initialized = false;
}
