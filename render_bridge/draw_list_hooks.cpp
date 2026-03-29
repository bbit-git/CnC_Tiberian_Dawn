/**
 * draw_list_hooks.cpp — Recording hooks called from engine draw functions.
 *
 * These are called via extern from #ifdef USE_RENDER_BRIDGE blocks in
 * modern.src/. They record to g_draw_list when recording is active.
 * The actual rendering still happens — the draw list is an ADDITIONAL
 * record used for future replay-based rendering.
 */

#include "draw_list.h"
#include "dbg.h"

/// Called from CC_Draw_Shape in conquer.cpp.
/// Records the shape command when draw list is recording.
void Draw_List_Maybe_Record_Shape(
    void const* shapefile, int shapenum, int x, int y,
    int window, int flags, void const* fadingdata, void const* ghostdata)
{
    if (!g_draw_list.IsRecording()) return;

    g_draw_list.Record_Shape(shapefile, shapenum, x, y,
                             window, flags, fadingdata, ghostdata);
}
