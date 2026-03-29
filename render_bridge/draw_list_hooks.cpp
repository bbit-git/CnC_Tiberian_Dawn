/**
 * draw_list_hooks.cpp — Recording hooks called from engine draw functions.
 *
 * Only CC_Draw_Shape and Draw_Stamp are recorded (they have a window
 * parameter to filter by WINDOW_TACTICAL). Primitives (Fill_Rect etc.)
 * are NOT recorded — they render directly to HidPage. This prevents
 * sidebar/tab UI from polluting the native tactical buffer.
 *
 * Health bars and selection boxes (primitives) render to HidPage
 * at 712x400 and are visible via the SeenBuff palette shader path.
 */

#include "draw_list.h"
#include "function.h"

/// Called from CC_Draw_Shape in conquer.cpp.
/// Only records tactical window shapes.
bool Draw_List_Maybe_Record_Shape(
    void const* shapefile, int shapenum, int x, int y,
    int window, int flags, void const* fadingdata, void const* ghostdata)
{
    if (!g_draw_list.IsRecording()) return false;
    if (window != WINDOW_TACTICAL) return false;
    g_draw_list.Record_Shape(shapefile, shapenum, x, y,
                             window, flags, fadingdata, ghostdata);
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

/// Primitives: NOT recorded. They render directly to HidPage.
/// Sidebar/tab/health bars go straight to the buffer.
bool Draw_List_Maybe_Record_Fill_Rect(int, int, int, int, unsigned char)
{ return false; }

bool Draw_List_Maybe_Record_Rect(int, int, int, int, unsigned char)
{ return false; }

bool Draw_List_Maybe_Record_Line(int, int, int, int, unsigned char)
{ return false; }

bool Draw_List_Maybe_Record_Pixel(int, int, unsigned char)
{ return false; }
