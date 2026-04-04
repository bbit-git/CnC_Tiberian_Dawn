/**
 * draw_list_hooks.cpp — Recording hooks called from engine draw functions.
 *
 * Tactical shapes and stamps are recorded directly from their window id.
 * Tactical primitives need coordinate filtering because they do not carry
 * a window id, so the hooks only keep geometry that falls inside the
 * tactical region and normalize it to tactical-local coordinates.
 *
 * The important distinction is:
 * - shapes/stamps are already tactical-world draws
 * - primitives are just raw buffer operations, so the bridge must recover
 *   whether they belong to tactical rendering from buffer identity and bounds
 */

#include "draw_list.h"
#include "render_bridge.h"
#include "function.h"
#include <cstdint>

namespace {

/// Return the legacy tactical record rect used while HidPage primitives are captured.
void get_record_tactical_rect(int& x, int& y, int& w, int& h)
{
    Render_Bridge_Get_Record_Tactical_Rect(x, y, w, h);
}

bool is_tactical_viewport_origin(int origin_x, int origin_y)
{
    int tac_x = 0;
    int tac_y = 0;
    int tac_w = 0;
    int tac_h = 0;
    get_record_tactical_rect(tac_x, tac_y, tac_w, tac_h);
    return origin_x == tac_x && origin_y == tac_y;
}

/// Return true if the target buffer points into HidPage memory.
bool is_hidpage_buffer(const void* buffer)
{
    if (!buffer) {
        return false;
    }

    const uint8_t* hid_begin = static_cast<const uint8_t*>(HidPage.Get_Buffer());
    if (!hid_begin) {
        return false;
    }
    const uint8_t* hid_end = hid_begin + HidPage.Get_Full_Pitch() * HidPage.Get_Height();
    const uint8_t* ptr = static_cast<const uint8_t*>(buffer);
    return ptr >= hid_begin && ptr < hid_end;
}

/// Return true if the absolute rectangle overlaps the tactical region.
bool intersect_tactical(int origin_x, int origin_y, int viewport_w, int viewport_h,
                        int& x1, int& y1, int& x2, int& y2)
{
    int tac_x = 0;
    int tac_y = 0;
    int tac_w = 0;
    int tac_h = 0;
    get_record_tactical_rect(tac_x, tac_y, tac_w, tac_h);
    if (viewport_w > tac_w && viewport_h > tac_h && is_tactical_viewport_origin(origin_x, origin_y)) {
        // Some tactical overlays (selection brackets, health bars) are drawn
        // through a temporary tactical viewport that expands to native size
        // during draw-list capture. Accept that larger tactical-local space,
        // but only when the viewport origin still matches the tactical origin.
        tac_w = viewport_w;
        tac_h = viewport_h;
    }
    if (tac_w <= 0 || tac_h <= 0) {
        return false;
    }

    int min_x = x1 < x2 ? x1 : x2;
    int max_x = x1 > x2 ? x1 : x2;
    int min_y = y1 < y2 ? y1 : y2;
    int max_y = y1 > y2 ? y1 : y2;

    if (max_x < tac_x || max_y < tac_y ||
        min_x >= tac_x + tac_w || min_y >= tac_y + tac_h) {
        return false;
    }

    // Primitives are stored in tactical-local screen space. The presenter later
    // projects them through the world viewport, so capture must first strip the
    // tactical screen offset from HidPage-space coordinates.
    x1 -= tac_x;
    x2 -= tac_x;
    y1 -= tac_y;
    y2 -= tac_y;

    return true;
}

/// Return true if the absolute point lies in the tactical region and normalize it.
bool normalize_tactical_point(int origin_x, int origin_y, int viewport_w, int viewport_h,
                              int& x, int& y)
{
    int tac_x = 0;
    int tac_y = 0;
    int tac_w = 0;
    int tac_h = 0;
    get_record_tactical_rect(tac_x, tac_y, tac_w, tac_h);
    if (viewport_w > tac_w && viewport_h > tac_h && is_tactical_viewport_origin(origin_x, origin_y)) {
        tac_w = viewport_w;
        tac_h = viewport_h;
    }
    if (tac_w <= 0 || tac_h <= 0) {
        return false;
    }

    if (x < tac_x || y < tac_y || x >= tac_x + tac_w || y >= tac_y + tac_h) {
        return false;
    }

    // Record pixels in tactical-local coordinates so they stay independent from
    // whichever sub-viewport or HidPage origin the legacy draw call used.
    x -= tac_x;
    y -= tac_y;
    return true;
}

} // namespace

/// Return the legacy SHADOW.SHP pointer used by Redraw_Shadow.
static const void* get_shadow_shapes()
{
    static const void* shadow_shapes = MixFileClass::Retrieve("SHADOW.SHP");
    return shadow_shapes;
}

/// Called from CC_Draw_Shape in conquer.cpp.
/// Only records tactical window shapes.
bool Draw_List_Maybe_Record_Shape(
    void const* shapefile, int shapenum, int x, int y,
    int window, int flags, void const* fadingdata, void const* ghostdata)
{
    if (!g_draw_list.IsRecording()) return false;
    if (window != WINDOW_TACTICAL) return false;

    DrawLayer layer = LAYER_SPRITE;
    if (shapefile == get_shadow_shapes()) {
        layer = LAYER_SHADOW;
    }

    g_draw_list.Record_Shape(shapefile, shapenum, x, y,
                             window, flags, fadingdata, ghostdata, layer);
    return true;
}

/// Called from GraphicBufferClass::Draw_Stamp in sdl3_render.cpp.
/// Only records tactical window stamps.
bool Draw_List_Maybe_Record_Stamp(
    void const* icondata, int icon, int x, int y,
    void const* remap, int window)
{
    if (!g_draw_list.IsRecording()) return false;
    if (window != WINDOW_TACTICAL) return false;
    g_draw_list.Record_Stamp(icondata, icon, x, y, remap, window);
    return true;
}

/// Record tactical fill rectangles such as health bars and selection fills.
bool Draw_List_Maybe_Record_Fill_Rect(const void* buffer, int origin_x, int origin_y,
                                      int viewport_w, int viewport_h,
                                      int x1, int y1, int x2, int y2,
                                      unsigned char color)
{
    if (!g_draw_list.IsRecording()) return false;
    if (!is_hidpage_buffer(buffer)) return false;
    // Primitive hooks receive coordinates local to the current GraphicViewPort.
    // Convert them back to HidPage absolute space before testing tactical overlap.
    x1 += origin_x; y1 += origin_y;
    x2 += origin_x; y2 += origin_y;
    if (!intersect_tactical(origin_x, origin_y, viewport_w, viewport_h, x1, y1, x2, y2)) return false;
    g_draw_list.Record_Fill_Rect(x1, y1, x2, y2, color);
    return true;
}

/// Record tactical rectangle outlines such as rubber band selection.
bool Draw_List_Maybe_Record_Rect(const void* buffer, int origin_x, int origin_y,
                                 int viewport_w, int viewport_h,
                                 int x1, int y1, int x2, int y2,
                                 unsigned char color)
{
    if (!g_draw_list.IsRecording()) return false;
    if (!is_hidpage_buffer(buffer)) return false;
    x1 += origin_x; y1 += origin_y;
    x2 += origin_x; y2 += origin_y;
    if (!intersect_tactical(origin_x, origin_y, viewport_w, viewport_h, x1, y1, x2, y2)) return false;
    g_draw_list.Record_Draw_Rect(x1, y1, x2, y2, color);
    return true;
}

/// Record tactical overlay lines such as unit corner brackets.
bool Draw_List_Maybe_Record_Line(const void* buffer, int origin_x, int origin_y,
                                 int viewport_w, int viewport_h,
                                 int x1, int y1, int x2, int y2,
                                 unsigned char color)
{
    if (!g_draw_list.IsRecording()) return false;
    if (!is_hidpage_buffer(buffer)) return false;
    // Line commands do not carry a window id, so buffer identity + tactical
    // overlap is the bridge-side filter that separates tactical overlays from UI.
    x1 += origin_x; y1 += origin_y;
    x2 += origin_x; y2 += origin_y;
    if (!intersect_tactical(origin_x, origin_y, viewport_w, viewport_h, x1, y1, x2, y2)) return false;
    g_draw_list.Record_Draw_Line(x1, y1, x2, y2, color);
    return true;
}

/// Record a solid-black shroud fill rect with native-buffer-relative coordinates.
/// Called directly from Redraw_Shadow_Rects in bridge mode, bypassing the generic
/// hook to store LAYER_SHADOW (so the native buffer replay includes these, not the
/// overlay pass which is skipped in GL mode). Returns true when recording; callers
/// fall back to LogicPage->Fill_Rect only when false (CPU path).
bool Render_Bridge_Record_Shroud_Fill_Rect(int x, int y, int w, int h)
{
    if (!g_draw_list.IsRecording()) return false;
    g_draw_list.Record_Fill_Rect(x, y, x + w - 1, y + h - 1, BLACK, LAYER_SHADOW);
    return true;
}

/// Record tactical debug pixels and cursor markers.
bool Draw_List_Maybe_Record_Pixel(const void* buffer, int origin_x, int origin_y,
                                  int viewport_w, int viewport_h,
                                  int x, int y, unsigned char color)
{
    if (!g_draw_list.IsRecording()) return false;
    if (!is_hidpage_buffer(buffer)) return false;
    x += origin_x;
    y += origin_y;
    if (!normalize_tactical_point(origin_x, origin_y, viewport_w, viewport_h, x, y)) return false;
    g_draw_list.Record_Put_Pixel(x, y, color);
    return true;
}
