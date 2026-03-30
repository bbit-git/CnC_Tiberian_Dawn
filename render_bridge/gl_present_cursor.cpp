#include "gl_present_internal.h"
#include <cstring>
#include <cstdlib>

extern bool TD_SDL_Get_Mouse_Cursor(const uint8_t*& pixels, int& w, int& h,
                                    int& hotx, int& hoty, bool& visible);
extern void TD_SDL_Get_Raw_Mouse_Position(int& x, int& y);

void GL_Present_Draw_Cursor(const GLPresentFrameContext& ctx)
{
    const uint8_t* cursor_pixels = nullptr;
    int cursor_w = 0;
    int cursor_h = 0;
    int cursor_hotx = 0;
    int cursor_hoty = 0;
    bool cursor_visible = false;
    if (!(TD_SDL_Get_Mouse_Cursor(cursor_pixels, cursor_w, cursor_h,
                                  cursor_hotx, cursor_hoty, cursor_visible) &&
          cursor_visible && cursor_pixels && cursor_w > 0 && cursor_h > 0 && g_ui_program)) {
        return;
    }

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

    int raw_mouse_x = 0;
    int raw_mouse_y = 0;
    TD_SDL_Get_Raw_Mouse_Position(raw_mouse_x, raw_mouse_y);

    int cursor_x = raw_mouse_x - cursor_hotx;
    int cursor_y = raw_mouse_y - cursor_hoty;
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
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, overlay_w, overlay_h, 0,
                     GL_LUMINANCE, GL_UNSIGNED_BYTE, g_cursor_overlay);
        g_cursor_tex_w = overlay_w;
        g_cursor_tex_h = overlay_h;
    } else {
        glBindTexture(GL_TEXTURE_2D, g_cursor_tex);
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
    float nx0 = static_cast<float>(ctx.offset_x) / ctx.win_w * 2.0f - 1.0f;
    float ny0 = 1.0f - static_cast<float>(ctx.offset_y) / ctx.win_h * 2.0f;
    float nx1 = static_cast<float>(ctx.offset_x + static_cast<int>(ctx.buffer_w * ctx.ui_scale)) / ctx.win_w * 2.0f - 1.0f;
    float ny1 = 1.0f - static_cast<float>(ctx.offset_y + static_cast<int>(ctx.buffer_h * ctx.ui_scale)) / ctx.win_h * 2.0f;
    glUniform4f(g_ui_u_dst_rect, nx0, ny0, nx1, ny1);

    static const float quad[] = { 0,0, 1,0, 0,1, 1,1 };
    GL_Present_Bind_Client_Quad(quad);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    GL_Present_Bind_Palette_Program();
}
