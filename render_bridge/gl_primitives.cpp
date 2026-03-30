/**
 * gl_primitives.cpp — GPU overlay primitive rendering from the draw list.
 *
 * Renders tactical overlay primitives after the world texture so health bars,
 * selection boxes, and debug markers are no longer baked into the native
 * tactical buffer in GL mode.
 *
 * Primitive commands are stored in tactical-local coordinates by the hooks.
 * At present time they are projected through the same visible source viewport
 * as the tactical world texture, then clipped to the final tactical screen rect.
 */

#include "render_bridge.h"
#include "draw_list.h"
#include "dbg.h"

#undef min
#undef max
#include <GLES2/gl2.h>

#include <cstddef>
#include <vector>

namespace {

struct PrimVertex {
    float x;
    float y;
    float r;
    float g;
    float b;
    float a;
};

struct GLPrimitiveState {
    GLuint program = 0;
    GLint a_pos = -1;
    GLint a_color = -1;
    GLint u_viewport = -1;
    bool initialized = false;
};

GLPrimitiveState g_state;
int g_last_primitive_count = 0;

static const char* k_vert_src = R"(
    attribute vec2 a_pos;
    attribute vec4 a_color;
    varying vec4 v_color;
    uniform vec2 u_viewport;
    void main() {
        vec2 p = a_pos / u_viewport * 2.0 - 1.0;
        p.y = -p.y;
        gl_Position = vec4(p, 0.0, 1.0);
        v_color = a_color;
    }
)";

static const char* k_frag_src = R"(
    precision mediump float;
    varying vec4 v_color;
    void main() {
        gl_FragColor = v_color;
    }
)";

GLuint compile_shader(GLenum type, const char* src)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[256];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        DBG("gl_primitives: shader compile failed: %s", log);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

bool init_gl()
{
    if (g_state.initialized) {
        return true;
    }

    GLuint vert = compile_shader(GL_VERTEX_SHADER, k_vert_src);
    GLuint frag = compile_shader(GL_FRAGMENT_SHADER, k_frag_src);
    if (!vert || !frag) {
        return false;
    }

    g_state.program = glCreateProgram();
    glAttachShader(g_state.program, vert);
    glAttachShader(g_state.program, frag);
    glBindAttribLocation(g_state.program, 0, "a_pos");
    glBindAttribLocation(g_state.program, 1, "a_color");
    glLinkProgram(g_state.program);
    glDeleteShader(vert);
    glDeleteShader(frag);

    GLint ok = 0;
    glGetProgramiv(g_state.program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[256];
        glGetProgramInfoLog(g_state.program, sizeof(log), nullptr, log);
        DBG("gl_primitives: program link failed: %s", log);
        glDeleteProgram(g_state.program);
        g_state.program = 0;
        return false;
    }

    g_state.a_pos = glGetAttribLocation(g_state.program, "a_pos");
    g_state.a_color = glGetAttribLocation(g_state.program, "a_color");
    g_state.u_viewport = glGetUniformLocation(g_state.program, "u_viewport");
    g_state.initialized = true;
    return true;
}

void palette_color(uint8_t color, const uint8_t* palette,
                   float& out_r, float& out_g, float& out_b, float& out_a)
{
    out_r = static_cast<float>(palette[color * 3 + 0] << 2) / 255.0f;
    out_g = static_cast<float>(palette[color * 3 + 1] << 2) / 255.0f;
    out_b = static_cast<float>(palette[color * 3 + 2] << 2) / 255.0f;
    out_a = 1.0f;
}

float world_to_screen(float origin, float value, float vp, float scale)
{
    // Primitive coordinates are stored in tactical-local pixels. The tactical
    // presenter samples a zoomed source window from the native replay texture,
    // so primitives must subtract the current source viewport before scaling
    // into final screen space.
    return origin + (value - vp) * scale;
}

void push_line(std::vector<PrimVertex>& lines,
               float x0, float y0, float x1, float y1,
               float r, float g, float b, float a)
{
    lines.push_back({x0, y0, r, g, b, a});
    lines.push_back({x1, y1, r, g, b, a});
}

void push_dashed_line(std::vector<PrimVertex>& lines,
                      float x0, float y0, float x1, float y1,
                      float dash_len, float gap_len,
                      float r, float g, float b, float a)
{
    if (dash_len <= 0.0f) {
        push_line(lines, x0, y0, x1, y1, r, g, b, a);
        return;
    }

    if (x0 == x1) {
        float dir = (y1 >= y0) ? 1.0f : -1.0f;
        float y = y0;
        while ((dir > 0.0f && y < y1) || (dir < 0.0f && y > y1)) {
            float dash_end = y + dir * dash_len;
            if ((dir > 0.0f && dash_end > y1) || (dir < 0.0f && dash_end < y1)) {
                dash_end = y1;
            }
            push_line(lines, x0, y, x1, dash_end, r, g, b, a);
            y = dash_end + dir * gap_len;
        }
        return;
    }

    if (y0 == y1) {
        float dir = (x1 >= x0) ? 1.0f : -1.0f;
        float x = x0;
        while ((dir > 0.0f && x < x1) || (dir < 0.0f && x > x1)) {
            float dash_end = x + dir * dash_len;
            if ((dir > 0.0f && dash_end > x1) || (dir < 0.0f && dash_end < x1)) {
                dash_end = x1;
            }
            push_line(lines, x, y0, dash_end, y1, r, g, b, a);
            x = dash_end + dir * gap_len;
        }
        return;
    }

    push_line(lines, x0, y0, x1, y1, r, g, b, a);
}

void push_rect(std::vector<PrimVertex>& tris,
               float x0, float y0, float x1, float y1,
               float r, float g, float b, float a)
{
    tris.push_back({x0, y0, r, g, b, a});
    tris.push_back({x1, y0, r, g, b, a});
    tris.push_back({x0, y1, r, g, b, a});
    tris.push_back({x0, y1, r, g, b, a});
    tris.push_back({x1, y0, r, g, b, a});
    tris.push_back({x1, y1, r, g, b, a});
}

void push_outline(std::vector<PrimVertex>& lines,
                  float x0, float y0, float x1, float y1,
                  float r, float g, float b, float a)
{
    push_line(lines, x0, y0, x1, y0, r, g, b, a);
    push_line(lines, x1, y0, x1, y1, r, g, b, a);
    push_line(lines, x1, y1, x0, y1, r, g, b, a);
    push_line(lines, x0, y1, x0, y0, r, g, b, a);
}

void draw_vertices(GLenum mode, const std::vector<PrimVertex>& verts)
{
    if (verts.empty()) {
        return;
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glVertexAttribPointer(g_state.a_pos, 2, GL_FLOAT, GL_FALSE, sizeof(PrimVertex),
                          &verts[0].x);
    glVertexAttribPointer(g_state.a_color, 4, GL_FLOAT, GL_FALSE, sizeof(PrimVertex),
                          &verts[0].r);
    glDrawArrays(mode, 0, static_cast<GLsizei>(verts.size()));
}

} // namespace

/// Render overlay primitives from the draw list as GL geometry.
int GL_Primitives_Render(int win_w, int win_h,
                         int tac_screen_x, int tac_screen_y,
                         int tac_screen_w, int tac_screen_h,
                         float scale, float vp_x, float vp_y,
                         const uint8_t* palette)
{
    if (!palette || tac_screen_w <= 0 || tac_screen_h <= 0) {
        g_last_primitive_count = 0;
        return 0;
    }
    if (!init_gl()) {
        g_last_primitive_count = 0;
        return 0;
    }

    std::vector<PrimVertex> tris;
    std::vector<PrimVertex> lines;
    int primitive_count = 0;

    tris.reserve(256);
    lines.reserve(256);

    for (int i = 0; i < g_draw_list.Command_Count(); i++) {
        const DrawCommand& cmd = g_draw_list.Get(i);
        if (cmd.type != CMD_FILL_RECT &&
            cmd.type != CMD_DRAW_RECT &&
            cmd.type != CMD_DRAW_LINE &&
            cmd.type != CMD_PUT_PIXEL) {
            continue;
        }

        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        float a = 0.0f;
        palette_color(cmd.prim.color, palette, r, g, b, a);

        if (cmd.type == CMD_FILL_RECT) {
            // Rect fills are captured as tactical-local pixel bounds. Project
            // them through the same viewport/scale transform as the world quad.
            float x0 = world_to_screen(static_cast<float>(tac_screen_x), static_cast<float>(cmd.prim.x1), vp_x, scale);
            float y0 = world_to_screen(static_cast<float>(tac_screen_y), static_cast<float>(cmd.prim.y1), vp_y, scale);
            float x1 = world_to_screen(static_cast<float>(tac_screen_x), static_cast<float>(cmd.prim.x2 + 1), vp_x, scale);
            float y1 = world_to_screen(static_cast<float>(tac_screen_y), static_cast<float>(cmd.prim.y2 + 1), vp_y, scale);
            push_rect(tris, x0, y0, x1, y1, r, g, b, a);
            primitive_count++;
            continue;
        }

        if (cmd.type == CMD_PUT_PIXEL) {
            // A logical tactical pixel expands to one scaled block in final
            // screen space so debug markers stay aligned with nearest-neighbor art.
            float x0 = world_to_screen(static_cast<float>(tac_screen_x), static_cast<float>(cmd.prim.x1), vp_x, scale);
            float y0 = world_to_screen(static_cast<float>(tac_screen_y), static_cast<float>(cmd.prim.y1), vp_y, scale);
            float x1 = x0 + scale;
            float y1 = y0 + scale;
            push_rect(tris, x0, y0, x1, y1, r, g, b, a);
            primitive_count++;
            continue;
        }

        if (cmd.type == CMD_DRAW_RECT) {
            // Outline rectangles follow the same world-space projection as fills,
            // but stay as GL lines to match selection and bracket overlays.
            float x0 = world_to_screen(static_cast<float>(tac_screen_x), static_cast<float>(cmd.prim.x1), vp_x, scale);
            float y0 = world_to_screen(static_cast<float>(tac_screen_y), static_cast<float>(cmd.prim.y1), vp_y, scale);
            float x1 = world_to_screen(static_cast<float>(tac_screen_x), static_cast<float>(cmd.prim.x2), vp_x, scale);
            float y1 = world_to_screen(static_cast<float>(tac_screen_y), static_cast<float>(cmd.prim.y2), vp_y, scale);
            push_line(lines, x0, y0, x1, y0, r, g, b, a);
            push_line(lines, x1, y0, x1, y1, r, g, b, a);
            push_line(lines, x1, y1, x0, y1, r, g, b, a);
            push_line(lines, x0, y1, x0, y0, r, g, b, a);
            primitive_count++;
            continue;
        }

        // Generic line overlays use the same tactical-local -> visible-source ->
        // final-screen transform as every other tactical primitive.
        float x0 = world_to_screen(static_cast<float>(tac_screen_x), static_cast<float>(cmd.prim.x1), vp_x, scale);
        float y0 = world_to_screen(static_cast<float>(tac_screen_y), static_cast<float>(cmd.prim.y1), vp_y, scale);
        float x1 = world_to_screen(static_cast<float>(tac_screen_x), static_cast<float>(cmd.prim.x2), vp_x, scale);
        float y1 = world_to_screen(static_cast<float>(tac_screen_y), static_cast<float>(cmd.prim.y2), vp_y, scale);
        push_line(lines, x0, y0, x1, y1, r, g, b, a);
        primitive_count++;
    }

    if (primitive_count == 0) {
        g_last_primitive_count = 0;
        return 0;
    }

    glUseProgram(g_state.program);
    glUniform2f(g_state.u_viewport, static_cast<float>(win_w), static_cast<float>(win_h));
    glDisable(GL_BLEND);

    glEnableVertexAttribArray(g_state.a_pos);
    glEnableVertexAttribArray(g_state.a_color);

    // Clamp all tactical primitives to the final tactical screen rect so no
    // overlay geometry can bleed into the header or sidebar.
    glEnable(GL_SCISSOR_TEST);
    glScissor(tac_screen_x, win_h - (tac_screen_y + tac_screen_h), tac_screen_w, tac_screen_h);

    draw_vertices(GL_TRIANGLES, tris);
    draw_vertices(GL_LINES, lines);

    glDisable(GL_SCISSOR_TEST);
    glDisableVertexAttribArray(g_state.a_pos);
    glDisableVertexAttribArray(g_state.a_color);
    g_last_primitive_count = primitive_count;
    return primitive_count;
}

/// Get the number of overlay primitives rendered in the last frame.
int GL_Primitives_Last_Count()
{
    return g_last_primitive_count;
}

/// Render source-tint overlays showing native tactical, SeenBuff chrome, and UI overlay bounds.
void GL_Primitives_Render_Source_Overlay(int win_w, int win_h,
                                         int game_screen_x, int game_screen_y,
                                         int game_screen_w, int game_screen_h,
                                         int header_screen_h,
                                         int tac_screen_x, int tac_screen_y,
                                         int tac_screen_w, int tac_screen_h,
                                         int side_screen_x, int side_screen_w,
                                         bool has_ui_overlay)
{
    if (!init_gl()) {
        return;
    }

    std::vector<PrimVertex> tris;
    std::vector<PrimVertex> lines;
    tris.reserve(48);
    lines.reserve(48);

    const float nat_r = 1.00f, nat_g = 0.18f, nat_b = 0.18f;
    const float seen_r = 0.18f, seen_g = 0.90f, seen_b = 0.28f;
    const float hid_r = 0.18f, hid_g = 0.55f, hid_b = 1.00f;

    // Source overlay colors show which pass owns each screen region:
    // SeenBuff chrome, native tactical, and full-frame UI overlay.
    if (header_screen_h > 0) {
        push_rect(tris,
                  static_cast<float>(game_screen_x),
                  static_cast<float>(game_screen_y),
                  static_cast<float>(game_screen_x + game_screen_w),
                  static_cast<float>(game_screen_y + header_screen_h),
                  seen_r, seen_g, seen_b, 0.16f);
        push_outline(lines,
                     static_cast<float>(game_screen_x) + 0.5f,
                     static_cast<float>(game_screen_y) + 0.5f,
                     static_cast<float>(game_screen_x + game_screen_w) - 0.5f,
                     static_cast<float>(game_screen_y + header_screen_h) - 0.5f,
                     seen_r, seen_g, seen_b, 0.95f);
    }

    if (side_screen_w > 0) {
        push_rect(tris,
                  static_cast<float>(side_screen_x),
                  static_cast<float>(game_screen_y),
                  static_cast<float>(side_screen_x + side_screen_w),
                  static_cast<float>(game_screen_y + game_screen_h),
                  seen_r, seen_g, seen_b, 0.16f);
        push_outline(lines,
                     static_cast<float>(side_screen_x) + 0.5f,
                     static_cast<float>(game_screen_y) + 0.5f,
                     static_cast<float>(side_screen_x + side_screen_w) - 0.5f,
                     static_cast<float>(game_screen_y + game_screen_h) - 0.5f,
                     seen_r, seen_g, seen_b, 0.95f);
    }

    if (tac_screen_w > 0 && tac_screen_h > 0) {
        push_rect(tris,
                  static_cast<float>(tac_screen_x),
                  static_cast<float>(tac_screen_y),
                  static_cast<float>(tac_screen_x + tac_screen_w),
                  static_cast<float>(tac_screen_y + tac_screen_h),
                  nat_r, nat_g, nat_b, 0.14f);
        push_outline(lines,
                     static_cast<float>(tac_screen_x) + 0.5f,
                     static_cast<float>(tac_screen_y) + 0.5f,
                     static_cast<float>(tac_screen_x + tac_screen_w) - 0.5f,
                     static_cast<float>(tac_screen_y + tac_screen_h) - 0.5f,
                     nat_r, nat_g, nat_b, 0.95f);
    }

    if (has_ui_overlay && game_screen_w > 0 && game_screen_h > 0) {
        push_outline(lines,
                     static_cast<float>(game_screen_x) + 2.5f,
                     static_cast<float>(game_screen_y) + 2.5f,
                     static_cast<float>(game_screen_x + game_screen_w) - 2.5f,
                     static_cast<float>(game_screen_y + game_screen_h) - 2.5f,
                     hid_r, hid_g, hid_b, 1.0f);
    }

    glUseProgram(g_state.program);
    glUniform2f(g_state.u_viewport, static_cast<float>(win_w), static_cast<float>(win_h));
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(g_state.a_pos);
    glEnableVertexAttribArray(g_state.a_color);
    draw_vertices(GL_TRIANGLES, tris);
    draw_vertices(GL_LINES, lines);
    glDisableVertexAttribArray(g_state.a_pos);
    glDisableVertexAttribArray(g_state.a_color);
    glDisable(GL_BLEND);
}

/// Render diagnostic rectangles for tactical layout, viewport, and clamp zones.
void GL_Primitives_Render_Debug_Overlay(int win_w, int win_h,
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
                                        int mouse_screen_x, int mouse_screen_y)
{
    if (!init_gl()) {
        return;
    }

    std::vector<PrimVertex> tris;
    std::vector<PrimVertex> lines;
    tris.reserve(96);
    lines.reserve(128);

    const float tac_r = 0.10f, tac_g = 1.00f, tac_b = 0.10f;
    const float vp_r = 0.10f, vp_g = 1.00f, vp_b = 1.00f;
    const float header_r = 1.00f, header_g = 1.00f, header_b = 0.10f;
    const float side_r = 0.10f, side_g = 0.45f, side_b = 1.00f;
    const float mouse_r = 1.00f, mouse_g = 1.00f, mouse_b = 1.00f;
    const float clamp_r = 1.00f, clamp_g = 0.20f, clamp_b = 0.20f;
    const float zone_r = 1.00f, zone_g = 0.60f, zone_b = 0.12f;
    const float raw_r = 1.00f, raw_g = 0.75f, raw_b = 0.10f;
    const float shroud_r = 1.00f, shroud_g = 0.15f, shroud_b = 0.95f;

    if (header_screen_h > 0) {
        push_outline(lines, 0.5f, 0.5f,
                     static_cast<float>(win_w) - 0.5f,
                     static_cast<float>(header_screen_h) - 0.5f,
                     header_r, header_g, header_b, 1.0f);
    }

    if (side_screen_w > 0) {
        push_outline(lines,
                     static_cast<float>(side_screen_x) + 0.5f, 0.5f,
                     static_cast<float>(side_screen_x + side_screen_w) - 0.5f,
                     static_cast<float>(win_h) - 0.5f,
                     side_r, side_g, side_b, 1.0f);
    }

    if (tac_screen_w > 0 && tac_screen_h > 0) {
        push_outline(lines,
                     static_cast<float>(tac_screen_x) + 0.5f,
                     static_cast<float>(tac_screen_y) + 0.5f,
                     static_cast<float>(tac_screen_x + tac_screen_w) - 0.5f,
                     static_cast<float>(tac_screen_y + tac_screen_h) - 0.5f,
                     tac_r, tac_g, tac_b, 1.0f);
    }

    if (mouse_clamp_w > 0 && mouse_clamp_h > 0) {
        push_outline(lines,
                     static_cast<float>(mouse_clamp_x) + 2.5f,
                     static_cast<float>(mouse_clamp_y) + 2.5f,
                     static_cast<float>(mouse_clamp_x + mouse_clamp_w) - 2.5f,
                     static_cast<float>(mouse_clamp_y + mouse_clamp_h) - 2.5f,
                     mouse_r, mouse_g, mouse_b, 1.0f);
    }

    // Magenta rectangle: the current bridge-visible world window projected back
    // into tactical screen space. It is used to compare shroud coverage against
    // the tactical presentation rect.
    if (shroud_screen_w > 0 && shroud_screen_h > 0) {
        push_outline(lines,
                     static_cast<float>(shroud_screen_x) + 4.5f,
                     static_cast<float>(shroud_screen_y) + 4.5f,
                     static_cast<float>(shroud_screen_x + shroud_screen_w) - 4.5f,
                     static_cast<float>(shroud_screen_y + shroud_screen_h) - 4.5f,
                     shroud_r, shroud_g, shroud_b, 1.0f);
    }

    if (scroll_zone_px > 1.0f && tac_screen_w > 0 && tac_screen_h > 0) {
        float sx0 = static_cast<float>(tac_screen_x);
        float sy0 = static_cast<float>(tac_screen_y);
        float sx1 = static_cast<float>(tac_screen_x + tac_screen_w);
        float sy1 = static_cast<float>(tac_screen_y + tac_screen_h);
        float band = scroll_zone_px;
        if (band * 2.0f > tac_screen_w) band = tac_screen_w * 0.5f;
        if (band * 2.0f > tac_screen_h) band = tac_screen_h * 0.5f;

        push_rect(tris, sx0, sy0, sx0 + band, sy1,
                  clamp_left ? clamp_r : zone_r,
                  clamp_left ? clamp_g : zone_g,
                  clamp_left ? clamp_b : zone_b,
                  clamp_left ? 0.32f : 0.07f);
        push_rect(tris, sx1 - band, sy0, sx1, sy1,
                  clamp_right ? clamp_r : zone_r,
                  clamp_right ? clamp_g : zone_g,
                  clamp_right ? clamp_b : zone_b,
                  clamp_right ? 0.32f : 0.07f);
        push_rect(tris, sx0, sy0, sx1, sy0 + band,
                  clamp_top ? clamp_r : zone_r,
                  clamp_top ? clamp_g : zone_g,
                  clamp_top ? clamp_b : zone_b,
                  clamp_top ? 0.32f : 0.07f);
        push_rect(tris, sx0, sy1 - band, sx1, sy1,
                  clamp_bottom ? clamp_r : zone_r,
                  clamp_bottom ? clamp_g : zone_g,
                  clamp_bottom ? clamp_b : zone_b,
                  clamp_bottom ? 0.32f : 0.07f);

        if (clamp_left) {
            push_dashed_line(lines,
                             sx0 + 0.5f, sy0 + 0.5f,
                             sx0 + 0.5f, sy1 - 0.5f,
                             8.0f, 5.0f,
                             clamp_r, clamp_g, clamp_b, 1.0f);
        }
        if (clamp_right) {
            push_dashed_line(lines,
                             sx1 - 0.5f, sy0 + 0.5f,
                             sx1 - 0.5f, sy1 - 0.5f,
                             8.0f, 5.0f,
                             clamp_r, clamp_g, clamp_b, 1.0f);
        }
        if (clamp_top) {
            push_dashed_line(lines,
                             sx0 + 0.5f, sy0 + 0.5f,
                             sx1 - 0.5f, sy0 + 0.5f,
                             8.0f, 5.0f,
                             clamp_r, clamp_g, clamp_b, 1.0f);
        }
        if (clamp_bottom) {
            push_dashed_line(lines,
                             sx0 + 0.5f, sy1 - 0.5f,
                             sx1 - 0.5f, sy1 - 0.5f,
                             8.0f, 5.0f,
                             clamp_r, clamp_g, clamp_b, 1.0f);
        }
    }

    if (tac_screen_w > 0 && tac_screen_h > 0 && native_w > 0 && native_h > 0 && vis_w > 0.0f && vis_h > 0.0f) {
        // Minimap box: native replay texture with the current source viewport highlighted.
        float mini_w = static_cast<float>(tac_screen_w) * 0.22f;
        float mini_h = static_cast<float>(tac_screen_h) * 0.22f;
        if (mini_w > 180.0f) mini_w = 180.0f;
        if (mini_h > 140.0f) mini_h = 140.0f;
        if (mini_w < 80.0f) mini_w = 80.0f;
        if (mini_h < 56.0f) mini_h = 56.0f;

        float mini_x0 = static_cast<float>(tac_screen_x + tac_screen_w) - mini_w - 8.0f;
        float mini_y0 = static_cast<float>(tac_screen_y) + 8.0f;
        float mini_x1 = mini_x0 + mini_w;
        float mini_y1 = mini_y0 + mini_h;

        push_rect(tris, mini_x0, mini_y0, mini_x1, mini_y1, 0.0f, 0.0f, 0.0f, 0.35f);
        push_outline(lines, mini_x0 + 0.5f, mini_y0 + 0.5f, mini_x1 - 0.5f, mini_y1 - 0.5f,
                     tac_r, tac_g, tac_b, 1.0f);

        float vx0 = mini_x0 + (vp_x / native_w) * mini_w;
        float vy0 = mini_y0 + (vp_y / native_h) * mini_h;
        float vx1 = mini_x0 + ((vp_x + vis_w) / native_w) * mini_w;
        float vy1 = mini_y0 + ((vp_y + vis_h) / native_h) * mini_h;
        push_outline(lines, vx0 + 0.5f, vy0 + 0.5f, vx1 - 0.5f, vy1 - 0.5f,
                     vp_r, vp_g, vp_b, 1.0f);
    }

    push_rect(tris,
              static_cast<float>(raw_mouse_screen_x) - 2.0f,
              static_cast<float>(raw_mouse_screen_y) - 2.0f,
              static_cast<float>(raw_mouse_screen_x) + 2.0f,
              static_cast<float>(raw_mouse_screen_y) + 2.0f,
              raw_r, raw_g, raw_b, 0.95f);
    push_rect(tris,
              static_cast<float>(mouse_screen_x) - 2.0f,
              static_cast<float>(mouse_screen_y) - 2.0f,
              static_cast<float>(mouse_screen_x) + 2.0f,
              static_cast<float>(mouse_screen_y) + 2.0f,
              mouse_r, mouse_g, mouse_b, 0.95f);

    glUseProgram(g_state.program);
    glUniform2f(g_state.u_viewport, static_cast<float>(win_w), static_cast<float>(win_h));
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(g_state.a_pos);
    glEnableVertexAttribArray(g_state.a_color);
    draw_vertices(GL_TRIANGLES, tris);
    draw_vertices(GL_LINES, lines);
    glDisableVertexAttribArray(g_state.a_pos);
    glDisableVertexAttribArray(g_state.a_color);
    glDisable(GL_BLEND);
}
