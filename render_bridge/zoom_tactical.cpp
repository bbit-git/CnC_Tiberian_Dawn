/**
 * zoom_tactical.cpp — Native resolution tactical rendering via draw list.
 *
 * The game buffer (HidPage) stays at its viewport size for UI.
 * When zoomed out, the tactical area expands to show more cells:
 *   - Buffer size = min(screen_resolution, map_pixel_size) per axis
 *   - Before Draw_It: TacLeptonWidth/Height expanded to buffer size
 *   - Draw_It: game iterates more cells, all captured in draw list
 *   - After Draw_It: restore original dimensions
 *   - Replay to native buffer, upload to GL as tactical texture
 *   - Also replay to HidPage for sidebar/tab
 *
 * At default zoom: no expansion, SeenBuff used for everything.
 * At zoom < default: native buffer active, more cells visible.
 * See BUFFERS.md for complete buffer architecture.
 */

#include "render_bridge.h"
#include "draw_list.h"
#include "function.h"
#include "dbg.h"
#include <cstdlib>
#include <cstring>

extern void CC_Draw_Shape(void const* shapefile, int shapenum, int x, int y,
                          WindowNumberType window, ShapeFlags_Type flags,
                          void const* fadingdata, void const* ghostdata);

extern bool Render_Bridge_Draw_Shape_Scaled(const ShapeCmd& cmd, float zoom,
                                             float vp_x, float vp_y);
extern bool Render_Bridge_Draw_Stamp_Scaled(const StampCmd& cmd, float zoom,
                                             float vp_x, float vp_y);
extern bool GL_Present_Is_Active();
extern void GL_Present_Upload_Tactical(const uint8_t* pixels, int w, int h);
extern void Render_Bridge_Get_Screen_Size(int& w, int& h);

// Saved tactical dimensions
static int  g_saved_tac_lepton_w = 0;
static int  g_saved_tac_lepton_h = 0;
static int  g_saved_window_w = 0;
static int  g_saved_window_h = 0;
static bool g_expanded = false;

// Native tactical buffer (8-bit indexed)
static uint8_t* g_native_buf = nullptr;
static int      g_native_w = 0;
static int      g_native_h = 0;
static int      g_native_alloc = 0;

/// Compute tactical buffer dimensions: min(screen, map) per axis.
/// This is the maximum content that could ever be visible at 1:1 zoom.
/// Compute native tactical buffer dimensions.
/// STABLE: based on screen and map only, NOT sidebar state.
/// The buffer covers the maximum possible tactical area regardless
/// of sidebar toggle. The GL viewport selects the visible portion.
static void get_native_tactical(int& out_w, int& out_h)
{
    int screen_w = 0, screen_h = 0;
    Render_Bridge_Get_Screen_Size(screen_w, screen_h);
    if (screen_w <= 0 || screen_h <= 0) { out_w = 0; out_h = 0; return; }

    int map_px_w = Map.MapCellWidth * ICON_PIXEL_W;
    int map_px_h = Map.MapCellHeight * ICON_PIXEL_W;
    if (map_px_w <= 0 || map_px_h <= 0) { out_w = 0; out_h = 0; return; }

    // Native buffer = min(screen, map) per axis.
    // Does NOT depend on sidebar/tab state — always the max possible.
    out_w = (screen_w < map_px_w) ? screen_w : map_px_w;
    out_h = (screen_h < map_px_h) ? screen_h : map_px_h;

    // Align to cell boundaries (24px)
    out_w = ((out_w + 23) / 24) * 24;
    out_h = ((out_h + 23) / 24) * 24;
}

void Render_Bridge_Begin_Draw_List()
{
    g_draw_list.Clear();
    g_draw_list.SetRecording(true);

    extern bool InMainLoop;
    if (!InMainLoop || !GL_Present_Is_Active()) return;

    int native_w = 0, native_h = 0;
    get_native_tactical(native_w, native_h);
    if (native_w <= 0 || native_h <= 0) return;

    int tac_w = Lepton_To_Pixel(Map.TacLeptonWidth);
    int tac_h = Lepton_To_Pixel(Map.TacLeptonHeight);

    // Only expand if native is larger than current
    if (native_w <= tac_w && native_h <= tac_h) return;

    // Save original dimensions
    g_saved_tac_lepton_w = Map.TacLeptonWidth;
    g_saved_tac_lepton_h = Map.TacLeptonHeight;
    g_saved_window_w = WindowList[WINDOW_TACTICAL][WINDOWWIDTH];
    g_saved_window_h = WindowList[WINDOW_TACTICAL][WINDOWHEIGHT];

    // Expand for Draw_It — game iterates more cells
    Map.TacLeptonWidth  = Pixel_To_Lepton(native_w);
    Map.TacLeptonHeight = Pixel_To_Lepton(native_h);
    WindowList[WINDOW_TACTICAL][WINDOWWIDTH]  = native_w >> 3;
    WindowList[WINDOW_TACTICAL][WINDOWHEIGHT] = native_h;
    g_expanded = true;

    static bool logged = false;
    if (!logged) {
        DBG("native tactical: %dx%d (from %dx%d buffer, expanded from %dx%d)",
            native_w, native_h, SeenBuff.Get_Width(), SeenBuff.Get_Height(), tac_w, tac_h);
        logged = true;
    }
}

/// Replay world commands (terrain + sprites).
static void replay_world()
{
    for (int i = 0; i < g_draw_list.Command_Count(); i++) {
        const DrawCommand& cmd = g_draw_list.Get(i);
        if (cmd.type == CMD_STAMP) {
            LogicPage->Draw_Stamp(cmd.stamp.icondata, cmd.stamp.icon,
                                   cmd.stamp.x, cmd.stamp.y, cmd.stamp.remap, cmd.stamp.window);
        } else if (cmd.type == CMD_SHAPE) {
            CC_Draw_Shape(cmd.shape.shapefile, cmd.shape.shapenum, cmd.shape.x, cmd.shape.y,
                          static_cast<WindowNumberType>(cmd.shape.window),
                          static_cast<ShapeFlags_Type>(cmd.shape.flags),
                          cmd.shape.fadingdata, cmd.shape.ghostdata);
        }
    }
}

/// Replay overlay commands (health bars, selection, etc.).
static void replay_overlays()
{
    for (int i = 0; i < g_draw_list.Command_Count(); i++) {
        const DrawCommand& cmd = g_draw_list.Get(i);
        switch (cmd.type) {
        case CMD_FILL_RECT:
            LogicPage->Fill_Rect(cmd.prim.x1, cmd.prim.y1, cmd.prim.x2, cmd.prim.y2, cmd.prim.color);
            break;
        case CMD_DRAW_RECT:
            LogicPage->Draw_Rect(cmd.prim.x1, cmd.prim.y1, cmd.prim.x2, cmd.prim.y2, cmd.prim.color);
            break;
        case CMD_DRAW_LINE:
            LogicPage->Draw_Line(cmd.prim.x1, cmd.prim.y1, cmd.prim.x2, cmd.prim.y2, cmd.prim.color);
            break;
        case CMD_PUT_PIXEL:
            LogicPage->Put_Pixel(cmd.prim.x1, cmd.prim.y1, cmd.prim.color);
            break;
        default: break;
        }
    }
}

void Render_Bridge_End_Draw_List(GraphicViewPortClass& page)
{
    g_draw_list.SetRecording(false);

    // Restore tactical dimensions if expanded
    if (g_expanded) {
        Map.TacLeptonWidth  = g_saved_tac_lepton_w;
        Map.TacLeptonHeight = g_saved_tac_lepton_h;
        WindowList[WINDOW_TACTICAL][WINDOWWIDTH]  = g_saved_window_w;
        WindowList[WINDOW_TACTICAL][WINDOWHEIGHT] = g_saved_window_h;
        g_expanded = false;
    }

    static int frame_count = 0;
    if (++frame_count % 300 == 1) {
        int stamps = 0, shapes = 0, prims = 0;
        for (int i = 0; i < g_draw_list.Command_Count(); i++) {
            switch (g_draw_list.Get(i).type) {
                case CMD_STAMP: stamps++; break;
                case CMD_SHAPE: shapes++; break;
                default: prims++; break;
            }
        }
        DBG("draw_list: %d cmds (%d/%d/%d) zoom=%.2f",
            g_draw_list.Command_Count(), stamps, shapes, prims,
            Render_Bridge_Get_Zoom_Level());
    }

    bool use_gl = GL_Present_Is_Active();
    float zoom = Render_Bridge_Get_Zoom_Level();
    float default_zoom = 1.0f;
    { extern float Render_Bridge_Get_Default_Zoom(); default_zoom = Render_Bridge_Get_Default_Zoom(); }

    if (use_gl) {
        // Always use native buffer when GL is active — the tactical viewport
        // is native-sized (more cells than 712x400 buffer provides).
        int native_w = 0, native_h = 0;
        get_native_tactical(native_w, native_h);

        int tac_w = Lepton_To_Pixel(Map.TacLeptonWidth);
        int tac_h = Lepton_To_Pixel(Map.TacLeptonHeight);

        bool use_native = (native_w > tac_w || native_h > tac_h) && native_w > 0;

        if (use_native) {
            // NATIVE PATH: replay to native-sized buffer, upload to GL

            int needed = native_w * native_h;
            if (!g_native_buf || g_native_alloc < needed) {
                free(g_native_buf);
                g_native_buf = static_cast<uint8_t*>(malloc(needed));
                g_native_alloc = needed;
            }
            g_native_w = native_w;
            g_native_h = native_h;
            memset(g_native_buf, 0, needed);

            // Temp viewport for native buffer
            GraphicBufferClass native_page;
            native_page.Init(native_w, native_h, g_native_buf, needed, native_w);
            GraphicViewPortClass native_vp(&native_page, 0, 0, native_w, native_h);

            // Redirect to native buffer
            GraphicViewPortClass* saved_logic = Set_Logic_Page(native_vp);
            int saved_win[4] = {
                WindowList[WINDOW_TACTICAL][WINDOWX],
                WindowList[WINDOW_TACTICAL][WINDOWY],
                WindowList[WINDOW_TACTICAL][WINDOWWIDTH],
                WindowList[WINDOW_TACTICAL][WINDOWHEIGHT]
            };
            WindowList[WINDOW_TACTICAL][WINDOWX] = 0;
            WindowList[WINDOW_TACTICAL][WINDOWY] = 0;
            WindowList[WINDOW_TACTICAL][WINDOWWIDTH] = native_w >> 3;
            WindowList[WINDOW_TACTICAL][WINDOWHEIGHT] = native_h;

            // Replay to native buffer
            replay_world();
            replay_overlays();

            // Restore
            Set_Logic_Page(*saved_logic);
            WindowList[WINDOW_TACTICAL][WINDOWX]     = saved_win[0];
            WindowList[WINDOW_TACTICAL][WINDOWY]      = saved_win[1];
            WindowList[WINDOW_TACTICAL][WINDOWWIDTH]  = saved_win[2];
            WindowList[WINDOW_TACTICAL][WINDOWHEIGHT] = saved_win[3];

            // Upload native tactical texture to GL
            GL_Present_Upload_Tactical(g_native_buf, native_w, native_h);
        }

        // DO NOT replay to HidPage — tactical content lives in native buffer only.
        // HidPage has sidebar/tab/UI content from Draw_It (not captured by draw list).
        // This prevents tactical content from leaking into SeenBuff UI quads.

    } else {
        // CPU-only path (no GL): replay to HidPage at 712x400
        replay_world();
        replay_overlays();
    }
    // Debug HUD is now a GL overlay — drawn in GL_Present_Frame before SwapWindow
}

int Render_Bridge_Get_Native_Tac_W() { return g_native_w; }
int Render_Bridge_Get_Native_Tac_H() { return g_native_h; }

uint8_t* Render_Bridge_Get_Native_Buffer(int& w, int& h)
{
    w = g_native_w;
    h = g_native_h;
    return g_native_buf;
}
