/**
 * zoom_tactical.cpp — Draw list recording with expanded tactical area.
 *
 * Before Draw_It: expand TacLeptonWidth/Height so the game iterates
 * more cells than the 712x400 buffer would normally show. All calls
 * are captured in the draw list (deferred — nothing renders to HidPage).
 *
 * After Draw_It: restore original dimensions. Replay the draw list
 * to an expanded 8-bit buffer, upload to GL as an indexed texture.
 * GL renders at native screen resolution via palette shader.
 *
 * At default zoom: expanded = normal (712x400), identical to legacy.
 * At zoom < default: expanded area is LARGER — more cells visible.
 * At zoom > default: expanded area is SMALLER — zoomed in sub-region.
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

// Saved tactical dimensions (restored after Draw_It)
static int  g_saved_tac_lepton_w = 0;
static int  g_saved_tac_lepton_h = 0;
static int  g_saved_window_w = 0;
static int  g_saved_window_h = 0;
static bool g_expanded = false;

// Expanded buffer for replay (8-bit indexed)
static uint8_t* g_exp_buf = nullptr;
static int      g_exp_w = 0;
static int      g_exp_h = 0;
static int      g_exp_alloc = 0;

// GL texture for expanded tactical area
extern bool GL_Present_Is_Active();
extern void GL_Present_Upload_Tactical(const uint8_t* pixels, int w, int h);

void Render_Bridge_Begin_Draw_List()
{
    g_draw_list.Clear();
    g_draw_list.SetRecording(true);

    // Compute expanded tactical dimensions based on zoom
    extern bool InMainLoop;
    if (!InMainLoop) return;

    float zoom = Render_Bridge_Get_Zoom_Level();
    float default_zoom = 1.0f;
    { extern float Render_Bridge_Get_Default_Zoom(); default_zoom = Render_Bridge_Get_Default_Zoom(); }
    if (default_zoom <= 0.0f) default_zoom = 1.0f;

    float rel_zoom = zoom / default_zoom;

    // At default zoom (rel=1.0): normal dimensions.
    // At zoom < default (rel<1.0): need MORE cells — expand.
    // At zoom > default (rel>1.0): need FEWER cells — could shrink, but keep normal for now.
    if (rel_zoom >= 1.0f) return; // No expansion needed

    int tac_w = Lepton_To_Pixel(Map.TacLeptonWidth);
    int tac_h = Lepton_To_Pixel(Map.TacLeptonHeight);
    if (tac_w <= 0 || tac_h <= 0) return;

    // Expanded size = normal / rel_zoom (shows more cells)
    int exp_w = static_cast<int>(tac_w / rel_zoom);
    int exp_h = static_cast<int>(tac_h / rel_zoom);

    // Align to cell boundaries (24px)
    exp_w = ((exp_w + 23) / 24) * 24;
    exp_h = ((exp_h + 23) / 24) * 24;

    // Save original dimensions
    g_saved_tac_lepton_w = Map.TacLeptonWidth;
    g_saved_tac_lepton_h = Map.TacLeptonHeight;
    g_saved_window_w = WindowList[WINDOW_TACTICAL][WINDOWWIDTH];
    g_saved_window_h = WindowList[WINDOW_TACTICAL][WINDOWHEIGHT];

    // Expand tactical dimensions (game will iterate more cells)
    Map.TacLeptonWidth  = Pixel_To_Lepton(exp_w);
    Map.TacLeptonHeight = Pixel_To_Lepton(exp_h);
    WindowList[WINDOW_TACTICAL][WINDOWWIDTH]  = exp_w >> 3;
    WindowList[WINDOW_TACTICAL][WINDOWHEIGHT] = exp_h;

    g_expanded = true;

    DBG("expand: %dx%d → %dx%d (rel_zoom=%.2f)", tac_w, tac_h, exp_w, exp_h, rel_zoom);
}

/// Replay world commands at 1:1 to the expanded buffer.
static void replay_world_to_buffer()
{
    for (int i = 0; i < g_draw_list.Command_Count(); i++) {
        const DrawCommand& cmd = g_draw_list.Get(i);

        if (cmd.type == CMD_STAMP) {
            const StampCmd& s = cmd.stamp;
            LogicPage->Draw_Stamp(s.icondata, s.icon, s.x, s.y, s.remap, s.window);
        } else if (cmd.type == CMD_SHAPE) {
            const ShapeCmd& s = cmd.shape;
            CC_Draw_Shape(s.shapefile, s.shapenum, s.x, s.y,
                          static_cast<WindowNumberType>(s.window),
                          static_cast<ShapeFlags_Type>(s.flags),
                          s.fadingdata, s.ghostdata);
        }
    }
}

/// Replay overlay commands at 1:1.
static void replay_overlays_to_buffer()
{
    for (int i = 0; i < g_draw_list.Command_Count(); i++) {
        const DrawCommand& cmd = g_draw_list.Get(i);
        switch (cmd.type) {
        case CMD_FILL_RECT:
            LogicPage->Fill_Rect(cmd.prim.x1, cmd.prim.y1,
                                  cmd.prim.x2, cmd.prim.y2, cmd.prim.color);
            break;
        case CMD_DRAW_RECT:
            LogicPage->Draw_Rect(cmd.prim.x1, cmd.prim.y1,
                                  cmd.prim.x2, cmd.prim.y2, cmd.prim.color);
            break;
        case CMD_DRAW_LINE:
            LogicPage->Draw_Line(cmd.prim.x1, cmd.prim.y1,
                                  cmd.prim.x2, cmd.prim.y2, cmd.prim.color);
            break;
        case CMD_PUT_PIXEL:
            LogicPage->Put_Pixel(cmd.prim.x1, cmd.prim.y1, cmd.prim.color);
            break;
        default: break;
        }
    }
}

/// Replay overlay commands with zoom-transformed positions.
static void replay_overlays_zoomed(float zoom, float vp_x, float vp_y)
{
    auto tx = [zoom, vp_x](int x) -> int { return static_cast<int>((x - vp_x) * zoom); };
    auto ty = [zoom, vp_y](int y) -> int { return static_cast<int>((y - vp_y) * zoom); };

    for (int i = 0; i < g_draw_list.Command_Count(); i++) {
        const DrawCommand& cmd = g_draw_list.Get(i);
        switch (cmd.type) {
        case CMD_FILL_RECT:
            LogicPage->Fill_Rect(tx(cmd.prim.x1), ty(cmd.prim.y1),
                                  tx(cmd.prim.x2), ty(cmd.prim.y2), cmd.prim.color);
            break;
        case CMD_DRAW_RECT:
            LogicPage->Draw_Rect(tx(cmd.prim.x1), ty(cmd.prim.y1),
                                  tx(cmd.prim.x2), ty(cmd.prim.y2), cmd.prim.color);
            break;
        case CMD_DRAW_LINE:
            LogicPage->Draw_Line(tx(cmd.prim.x1), ty(cmd.prim.y1),
                                  tx(cmd.prim.x2), ty(cmd.prim.y2), cmd.prim.color);
            break;
        case CMD_PUT_PIXEL:
            LogicPage->Put_Pixel(tx(cmd.prim.x1), ty(cmd.prim.y1), cmd.prim.color);
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

    float zoom = Render_Bridge_Get_Zoom_Level();
    float default_zoom = 1.0f;
    { extern float Render_Bridge_Get_Default_Zoom(); default_zoom = Render_Bridge_Get_Default_Zoom(); }
    if (default_zoom <= 0.0f) default_zoom = 1.0f;
    float rel_zoom = zoom / default_zoom;

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
        DBG("draw_list: %d cmds (%d stamps, %d shapes, %d prims), zoom=%.2f rel=%.2f",
            g_draw_list.Command_Count(), stamps, shapes, prims, zoom, rel_zoom);
    }

    // Check GL status
    extern bool GL_Present_Is_Active();
    bool use_gl = GL_Present_Is_Active();

    if (use_gl && rel_zoom < 1.0f) {
        // EXPANDED PATH: more cells captured than HidPage can hold.
        // Replay to an expanded buffer, GL uploads it as a larger indexed texture.

        int tac_w = Lepton_To_Pixel(Map.TacLeptonWidth);
        int tac_h = Lepton_To_Pixel(Map.TacLeptonHeight);
        int exp_w = static_cast<int>(tac_w / rel_zoom);
        int exp_h = static_cast<int>(tac_h / rel_zoom);
        exp_w = ((exp_w + 23) / 24) * 24;
        exp_h = ((exp_h + 23) / 24) * 24;

        // Ensure expanded buffer
        int needed = exp_w * exp_h;
        if (!g_exp_buf || g_exp_alloc < needed) {
            free(g_exp_buf);
            g_exp_buf = static_cast<uint8_t*>(malloc(needed));
            g_exp_alloc = needed;
        }
        g_exp_w = exp_w;
        g_exp_h = exp_h;
        memset(g_exp_buf, 0, needed);

        // Create temp viewport into expanded buffer
        GraphicBufferClass exp_page;
        exp_page.Init(exp_w, exp_h, g_exp_buf, needed, exp_w);
        GraphicViewPortClass exp_vp(&exp_page, 0, 0, exp_w, exp_h);

        // Redirect rendering to expanded buffer
        GraphicViewPortClass* saved_logic = Set_Logic_Page(exp_vp);
        int saved_win[4];
        saved_win[0] = WindowList[WINDOW_TACTICAL][WINDOWX];
        saved_win[1] = WindowList[WINDOW_TACTICAL][WINDOWY];
        saved_win[2] = WindowList[WINDOW_TACTICAL][WINDOWWIDTH];
        saved_win[3] = WindowList[WINDOW_TACTICAL][WINDOWHEIGHT];

        WindowList[WINDOW_TACTICAL][WINDOWX] = 0;
        WindowList[WINDOW_TACTICAL][WINDOWY] = 0;
        WindowList[WINDOW_TACTICAL][WINDOWWIDTH] = exp_w >> 3;
        WindowList[WINDOW_TACTICAL][WINDOWHEIGHT] = exp_h;

        // Replay all commands to expanded buffer
        replay_world_to_buffer();
        replay_overlays_to_buffer();

        // Restore
        Set_Logic_Page(*saved_logic);
        WindowList[WINDOW_TACTICAL][WINDOWX]      = saved_win[0];
        WindowList[WINDOW_TACTICAL][WINDOWY]       = saved_win[1];
        WindowList[WINDOW_TACTICAL][WINDOWWIDTH]   = saved_win[2];
        WindowList[WINDOW_TACTICAL][WINDOWHEIGHT]  = saved_win[3];

        // Tell GL present to use expanded texture for tactical area
        GL_Present_Upload_Tactical(g_exp_buf, exp_w, exp_h);

        // Also replay to HidPage at normal size (for sidebar/tab via Blit_Display)
        replay_world_to_buffer();
        replay_overlays_to_buffer();

    } else if (use_gl) {
        // GL path, zoom >= default: replay at 1:1, GL handles zoom via UV
        replay_world_to_buffer();
        replay_overlays_to_buffer();

    } else if (zoom != default_zoom) {
        // CPU path: per-element scaled rendering
        float vp_x = Render_Bridge_Get_Viewport_X();
        float vp_y = Render_Bridge_Get_Viewport_Y();

        for (int i = 0; i < g_draw_list.Command_Count(); i++) {
            const DrawCommand& cmd = g_draw_list.Get(i);
            if (cmd.type == CMD_STAMP) {
                if (!Render_Bridge_Draw_Stamp_Scaled(cmd.stamp, rel_zoom, vp_x, vp_y)) {
                    LogicPage->Draw_Stamp(cmd.stamp.icondata, cmd.stamp.icon,
                                           cmd.stamp.x, cmd.stamp.y,
                                           cmd.stamp.remap, cmd.stamp.window);
                }
            }
        }
        for (int i = 0; i < g_draw_list.Command_Count(); i++) {
            const DrawCommand& cmd = g_draw_list.Get(i);
            if (cmd.type == CMD_SHAPE) {
                if (!Render_Bridge_Draw_Shape_Scaled(cmd.shape, rel_zoom, vp_x, vp_y)) {
                    int x = static_cast<int>((cmd.shape.x - vp_x) * rel_zoom);
                    int y = static_cast<int>((cmd.shape.y - vp_y) * rel_zoom);
                    CC_Draw_Shape(cmd.shape.shapefile, cmd.shape.shapenum, x, y,
                                  static_cast<WindowNumberType>(cmd.shape.window),
                                  static_cast<ShapeFlags_Type>(cmd.shape.flags),
                                  cmd.shape.fadingdata, cmd.shape.ghostdata);
                }
            }
        }
        replay_overlays_zoomed(rel_zoom, vp_x, vp_y);

    } else {
        // No zoom: replay at 1:1
        replay_world_to_buffer();
        replay_overlays_to_buffer();
    }
}
