/**
 * draw_list_hooks.cpp — Recording hooks called from engine draw functions.
 *
 * When recording is active (during Draw_It), all tactical draw calls
 * are captured in g_draw_list and skipped. They're replayed after
 * Draw_It with zoom transforms applied.
 */

#include "draw_list.h"

/// Called from CC_Draw_Shape in conquer.cpp.
bool Draw_List_Maybe_Record_Shape(
    void const* shapefile, int shapenum, int x, int y,
    int window, int flags, void const* fadingdata, void const* ghostdata)
{
    if (!g_draw_list.IsRecording()) return false;
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
    g_draw_list.Record_Stamp(icondata, icon, x, y, remap, window);
    return true;
}

/// Called from GraphicBufferClass::Fill_Rect.
bool Draw_List_Maybe_Record_Fill_Rect(int x1, int y1, int x2, int y2, unsigned char color)
{
    if (!g_draw_list.IsRecording()) return false;
    g_draw_list.Record_Fill_Rect(x1, y1, x2, y2, color);
    return true;
}

/// Called from GraphicBufferClass::Draw_Rect.
bool Draw_List_Maybe_Record_Rect(int x1, int y1, int x2, int y2, unsigned char color)
{
    if (!g_draw_list.IsRecording()) return false;
    g_draw_list.Record_Draw_Rect(x1, y1, x2, y2, color);
    return true;
}

/// Called from GraphicBufferClass::Draw_Line.
bool Draw_List_Maybe_Record_Line(int x1, int y1, int x2, int y2, unsigned char color)
{
    if (!g_draw_list.IsRecording()) return false;
    g_draw_list.Record_Draw_Line(x1, y1, x2, y2, color);
    return true;
}

/// Called from GraphicBufferClass::Put_Pixel.
bool Draw_List_Maybe_Record_Pixel(int x, int y, unsigned char color)
{
    if (!g_draw_list.IsRecording()) return false;
    g_draw_list.Record_Put_Pixel(x, y, color);
    return true;
}
