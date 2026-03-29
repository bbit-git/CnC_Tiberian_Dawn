/**
 * draw_list_hooks.cpp — Recording hooks called from engine draw functions.
 *
 * When the draw list is recording, draw calls are captured and the
 * function returns true (caller should skip immediate rendering).
 * The draw list is replayed after Draw_It() with zoom transforms.
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
