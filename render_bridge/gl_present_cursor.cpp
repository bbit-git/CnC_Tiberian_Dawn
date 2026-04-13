#include "gl_present_internal.h"
#include "render_bridge.h"
#include <cstring>
#include <cstdlib>

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
                       cursor_pixels && cursor_w > 0 && cursor_h > 0;
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
    } else if (ctx.in_main_loop && g_cached_cursor_pixels && g_cached_cursor_w > 0 && g_cached_cursor_h > 0) {
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

    // Compute cursor position and size in window pixels.
    // Cursor shapes are in legacy game-buffer pixels — always scale size
    // by legacy_ui_scale so the cursor matches the visual scale of game content.
    int dst_x, dst_y;
    int dst_w = static_cast<int>(cursor_w * ctx.legacy_ui_scale);
    int dst_h = static_cast<int>(cursor_h * ctx.legacy_ui_scale);
    // Hotspot offset in window pixels (matches cursor image scale).
    int hot_wx = static_cast<int>(cursor_hotx * ctx.legacy_ui_scale);
    int hot_wy = static_cast<int>(cursor_hoty * ctx.legacy_ui_scale);

    if (ctx.in_main_loop) {
        // During gameplay g_mouse_x/y is in bridge logical-screen space.
        // Map mouse position to window, then subtract hotspot in window pixels.
        dst_x = static_cast<int>(g_mouse_x * ctx.ui_scale + ctx.offset_x) - hot_wx;
        dst_y = static_cast<int>(g_mouse_y * ctx.ui_scale + ctx.offset_y) - hot_wy;
    } else {
        // Menus: g_mouse_x/y is in legacy game-buffer space (712x400).
        dst_x = static_cast<int>(g_mouse_x * ctx.legacy_ui_scale + ctx.legacy_offset_x) - hot_wx;
        dst_y = static_cast<int>(g_mouse_y * ctx.legacy_ui_scale + ctx.legacy_offset_y) - hot_wy;
    }

    if (dst_w < 1) dst_w = 1;
    if (dst_h < 1) dst_h = 1;

    glViewport(0, 0, ctx.win_w, ctx.win_h);
    glDisable(GL_SCISSOR_TEST);
    // GL_UI_Render disables vertex attrib 0; re-enable for quad drawing.
    glEnableVertexAttribArray(0);

    GL_Present_Draw_Indexed_RGBA(cursor_pixels, cursor_w, cursor_h,
                                 CurrentPalette, true,
                                 dst_x, dst_y, dst_w, dst_h,
                                 ctx.win_w, ctx.win_h);
}
