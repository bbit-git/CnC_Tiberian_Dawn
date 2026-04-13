#include "gl_present_internal.h"
#include "render_bridge.h"
#include <cstring>
#include <cstdlib>
#include <cmath>

extern bool TD_SDL_Get_Mouse_Cursor(const uint8_t*& pixels, int& w, int& h,
                                    int& hotx, int& hoty, bool& visible);
extern int g_mouse_x, g_mouse_y;
extern unsigned char CurrentPalette[768];

static uint8_t* g_cached_cursor_pixels = nullptr;
static int g_cached_cursor_alloc = 0;
static int g_cached_cursor_w = 0;
static int g_cached_cursor_h = 0;
static int g_cached_cursor_hotx = 0;
static int g_cached_cursor_hoty = 0;
static uint8_t* g_cursor_rgba = nullptr;
static int g_cursor_rgba_alloc = 0;

void GL_Present_Draw_Cursor(const GLPresentFrameContext& ctx)
{
    const uint8_t* cursor_pixels = nullptr;
    int cursor_w = 0;
    int cursor_h = 0;
    int cursor_hotx = 0;
    int cursor_hoty = 0;
    bool cursor_visible = false;
    bool have_cursor = TD_SDL_Get_Mouse_Cursor(cursor_pixels, cursor_w, cursor_h,
                                               cursor_hotx, cursor_hoty, cursor_visible) &&
                       cursor_pixels && cursor_w > 0 && cursor_h > 0 && g_ui_program;
    if (have_cursor) {
        int pixel_count = cursor_w * cursor_h;
        if (!g_cached_cursor_pixels || g_cached_cursor_alloc < pixel_count) {
            free(g_cached_cursor_pixels);
            g_cached_cursor_pixels = static_cast<uint8_t*>(malloc(pixel_count));
            g_cached_cursor_alloc = pixel_count;
        }
        if (g_cached_cursor_pixels) {
            memcpy(g_cached_cursor_pixels, cursor_pixels, pixel_count);
            g_cached_cursor_w = cursor_w;
            g_cached_cursor_h = cursor_h;
            g_cached_cursor_hotx = cursor_hotx;
            g_cached_cursor_hoty = cursor_hoty;
        }
    } else if (ctx.in_main_loop && g_cached_cursor_pixels && g_cached_cursor_w > 0 && g_cached_cursor_h > 0 && g_ui_program) {
        cursor_pixels = g_cached_cursor_pixels;
        cursor_w = g_cached_cursor_w;
        cursor_h = g_cached_cursor_h;
        cursor_hotx = g_cached_cursor_hotx;
        cursor_hoty = g_cached_cursor_hoty;
        cursor_visible = true;
        have_cursor = true;
    }

    if (!have_cursor) {
        return;
    }

    // Legacy Hide_Mouse()/Show_Mouse() toggles are still used during gameplay
    // buffer work, but the bridge cursor is a final GL overlay. Honor that
    // visibility only outside gameplay.
    if (!ctx.in_main_loop && !cursor_visible) {
        return;
    }
    if (ctx.in_main_loop) {
        cursor_visible = true;
    }

    // Gameplay cursor is now rendered as a dedicated RGBA quad. The cursor
    // data and placement are valid; the remaining failure was in the indexed
    // full-screen overlay path only.
    if (ctx.in_main_loop) {
        int rgba_needed = cursor_w * cursor_h * 4;
        if (!g_cursor_rgba || g_cursor_rgba_alloc < rgba_needed) {
            free(g_cursor_rgba);
            g_cursor_rgba = static_cast<uint8_t*>(malloc(rgba_needed));
            g_cursor_rgba_alloc = rgba_needed;
        }
        if (!g_cursor_rgba) {
            return;
        }

        for (int i = 0; i < cursor_w * cursor_h; ++i) {
            uint8_t idx = cursor_pixels[i];
            g_cursor_rgba[i * 4 + 0] = CurrentPalette[idx * 3 + 0] << 2;
            g_cursor_rgba[i * 4 + 1] = CurrentPalette[idx * 3 + 1] << 2;
            g_cursor_rgba[i * 4 + 2] = CurrentPalette[idx * 3 + 2] << 2;
            g_cursor_rgba[i * 4 + 3] = (idx == 0) ? 0 : 255;
        }

        if (!g_cursor_tex || g_cursor_tex_w != cursor_w || g_cursor_tex_h != cursor_h) {
            if (g_cursor_tex) glDeleteTextures(1, &g_cursor_tex);
            glGenTextures(1, &g_cursor_tex);
            glBindTexture(GL_TEXTURE_2D, g_cursor_tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, cursor_w, cursor_h, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, g_cursor_rgba);
            g_cursor_tex_w = cursor_w;
            g_cursor_tex_h = cursor_h;
        } else {
            glBindTexture(GL_TEXTURE_2D, g_cursor_tex);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, cursor_w, cursor_h,
                            GL_RGBA, GL_UNSIGNED_BYTE, g_cursor_rgba);
        }

        int cursor_x = g_mouse_x - cursor_hotx;
        int cursor_y = g_mouse_y - cursor_hoty;

        // Map game-buffer coordinates to window pixels via the same
        // scale+offset used by the input path (legacy_ui_scale).
        float s = ctx.legacy_ui_scale;
        float wx0 = cursor_x * s + ctx.legacy_offset_x;
        float wy0 = cursor_y * s + ctx.legacy_offset_y;
        float wx1 = (cursor_x + cursor_w) * s + ctx.legacy_offset_x;
        float wy1 = (cursor_y + cursor_h) * s + ctx.legacy_offset_y;

        glViewport(0, 0, ctx.win_w, ctx.win_h);
        glDisable(GL_SCISSOR_TEST);
        glUseProgram(g_rgba_program);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_cursor_tex);
        glUniform1i(g_rgba_u_tex, 0);

        float x0 = wx0 / ctx.win_w * 2.0f - 1.0f;
        float y0 = 1.0f - wy0 / ctx.win_h * 2.0f;
        float x1 = wx1 / ctx.win_w * 2.0f - 1.0f;
        float y1 = 1.0f - wy1 / ctx.win_h * 2.0f;
        glUniform4f(g_rgba_u_src_rect, 0.0f, 0.0f, 1.0f, 1.0f);
        glUniform4f(g_rgba_u_dst_rect, x0, y0, x1, y1);

        static const float quad[] = { 0,0, 1,0, 0,1, 1,1 };
        GL_Present_Bind_Client_Quad(quad);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glDisable(GL_BLEND);

        GL_Present_Bind_Palette_Program();
        return;
    }

    // Use the legacy buffer size elsewhere. Stamping the indexed cursor into a
    // full-screen overlay keeps menu/UI cursor behavior aligned with the
    // original small-buffer presentation path.
    int overlay_w = ctx.buffer_w;
    int overlay_h = ctx.buffer_h;
    // Cursor pixels are stamped into a temporary full-frame overlay in
    // game-buffer space so nearest-neighbor scaling matches the rest of the UI.
    int overlay_size = overlay_w * overlay_h;
    if (!g_cursor_overlay || g_cursor_overlay_alloc < overlay_size) {
        free(g_cursor_overlay);
        g_cursor_overlay = static_cast<uint8_t*>(malloc(overlay_size));
        g_cursor_overlay_alloc = overlay_size;
    }
    if (!g_cursor_overlay) {
        return;
    }

    memset(g_cursor_overlay, 0, overlay_size);

    int cursor_x = g_mouse_x - cursor_hotx;
    int cursor_y = g_mouse_y - cursor_hoty;
    for (int row = 0; row < cursor_h; row++) {
        int dy = cursor_y + row;
        if (dy < 0 || dy >= overlay_h) {
            continue;
        }
        for (int col = 0; col < cursor_w; col++) {
            int dx = cursor_x + col;
            if (dx < 0 || dx >= overlay_w) {
                continue;
            }
            uint8_t idx = cursor_pixels[row * cursor_w + col];
            if (idx != 0) {
                g_cursor_overlay[dy * overlay_w + dx] = idx;
            }
        }
    }

    if (!g_cursor_tex || g_cursor_tex_w != overlay_w || g_cursor_tex_h != overlay_h) {
        if (g_cursor_tex) glDeleteTextures(1, &g_cursor_tex);
        glGenTextures(1, &g_cursor_tex);
        glBindTexture(GL_TEXTURE_2D, g_cursor_tex);
        // Cursor overlay is a sparse indexed mask. Linear filtering against a
        // mostly-zero full-frame texture can wash the cursor out entirely, so
        // keep nearest sampling here.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, overlay_w, overlay_h, 0,
                     GL_LUMINANCE, GL_UNSIGNED_BYTE, g_cursor_overlay);
        g_cursor_tex_w = overlay_w;
        g_cursor_tex_h = overlay_h;
    } else {
        glBindTexture(GL_TEXTURE_2D, g_cursor_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, overlay_w, overlay_h,
                        GL_LUMINANCE, GL_UNSIGNED_BYTE, g_cursor_overlay);
    }

    glUseProgram(g_ui_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_cursor_tex);
    glUniform1i(g_ui_u_indexed, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, g_palette_tex);
    glUniform1i(g_ui_u_palette, 1);

    glUniform4f(g_ui_u_src_rect, 0.0f, 0.0f, 1.0f, 1.0f);
    int dst_x = ctx.legacy_offset_x;
    int dst_y = ctx.legacy_offset_y;
    int dst_w = static_cast<int>(overlay_w * ctx.legacy_ui_scale);
    int dst_h = static_cast<int>(overlay_h * ctx.legacy_ui_scale);
    float nx0 = static_cast<float>(dst_x) / ctx.win_w * 2.0f - 1.0f;
    float ny0 = 1.0f - static_cast<float>(dst_y) / ctx.win_h * 2.0f;
    float nx1 = static_cast<float>(dst_x + dst_w) / ctx.win_w * 2.0f - 1.0f;
    float ny1 = 1.0f - static_cast<float>(dst_y + dst_h) / ctx.win_h * 2.0f;
    glUniform4f(g_ui_u_dst_rect, nx0, ny0, nx1, ny1);

    static const float quad[] = { 0,0, 1,0, 0,1, 1,1 };
    GL_Present_Bind_Client_Quad(quad);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    GL_Present_Bind_Palette_Program();
}
