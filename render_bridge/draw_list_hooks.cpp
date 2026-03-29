/**
 * draw_list_hooks.cpp — Recording hooks called from engine draw functions.
 *
 * Only captures calls targeting WINDOW_TACTICAL during recording.
 * Sidebar, radar, power bar, and other UI draws pass through unrecorded.
 * Primitives (Fill_Rect etc.) are filtered by coordinate bounds.
 */

#include "draw_list.h"
#include "function.h"

/// Tactical area bounds for primitive filtering.
static void get_tac_bounds(int& x, int& y, int& w, int& h)
{
    x = Map.TacPixelX;
    y = Map.TacPixelY;
    w = Lepton_To_Pixel(Map.TacLeptonWidth);
    h = Lepton_To_Pixel(Map.TacLeptonHeight);
}

/// Called from CC_Draw_Shape in conquer.cpp.
/// Only records tactical window shapes — sidebar/UI shapes pass through.
bool Draw_List_Maybe_Record_Shape(
    void const* shapefile, int shapenum, int x, int y,
    int window, int flags, void const* fadingdata, void const* ghostdata)
{
    if (!g_draw_list.IsRecording()) return false;
    if (window != WINDOW_TACTICAL) return false; // let non-tactical render normally
    g_draw_list.Record_Shape(shapefile, shapenum, x, y,
                             window, flags, fadingdata, ghostdata);
    return true;
}

/// Called from GraphicBufferClass::Draw_Stamp in sdl3_render.cpp.
bool Draw_List_Maybe_Record_Stamp(
    void const* icondata, int icon, int x, int y,
    void const* remap, int window)
{
    if (!g_draw_list.IsRecording()) return false;
    if (window != WINDOW_TACTICAL) return false;
    g_draw_list.Record_Stamp(icondata, icon, x, y, remap, window);
    return true;
}

/// Check if a rectangle overlaps the tactical area.
static bool is_in_tactical(int x1, int y1, int x2, int y2)
{
    int tx, ty, tw, th;
    get_tac_bounds(tx, ty, tw, th);
    // The primitive coordinates are relative to whatever viewport called them.
    // Tactical viewport primitives have coords within [0, tac_w) x [0, tac_h).
    // Sidebar viewport primitives have coords relative to the sidebar viewport.
    // Since we can't distinguish viewports, accept coords within tactical bounds.
    return (x2 >= 0 && x1 < tw && y2 >= 0 && y1 < th);
}

/// Called from GraphicBufferClass::Fill_Rect.
bool Draw_List_Maybe_Record_Fill_Rect(int x1, int y1, int x2, int y2, unsigned char color)
{
    if (!g_draw_list.IsRecording()) return false;
    if (!is_in_tactical(x1, y1, x2, y2)) return false;
    g_draw_list.Record_Fill_Rect(x1, y1, x2, y2, color);
    return true;
}

/// Called from GraphicBufferClass::Draw_Rect.
bool Draw_List_Maybe_Record_Rect(int x1, int y1, int x2, int y2, unsigned char color)
{
    if (!g_draw_list.IsRecording()) return false;
    if (!is_in_tactical(x1, y1, x2, y2)) return false;
    g_draw_list.Record_Draw_Rect(x1, y1, x2, y2, color);
    return true;
}

/// Called from GraphicBufferClass::Draw_Line.
bool Draw_List_Maybe_Record_Line(int x1, int y1, int x2, int y2, unsigned char color)
{
    if (!g_draw_list.IsRecording()) return false;
    if (!is_in_tactical(x1, y1, x2, y2)) return false;
    g_draw_list.Record_Draw_Line(x1, y1, x2, y2, color);
    return true;
}

/// Called from GraphicBufferClass::Put_Pixel.
bool Draw_List_Maybe_Record_Pixel(int x, int y, unsigned char color)
{
    if (!g_draw_list.IsRecording()) return false;
    if (!is_in_tactical(x, y, x, y)) return false;
    g_draw_list.Record_Put_Pixel(x, y, color);
    return true;
}
