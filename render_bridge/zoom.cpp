/**
 * zoom.cpp — Zoom with mouse-anchored pivot and viewport tracking.
 *
 * The viewport (vp_x, vp_y) is the top-left corner of the visible
 * region within the native tactical buffer. It's in native buffer pixels.
 *
 * Zoom changes the viewport size (vis = native / zoom) and repositions
 * so the pixel under the mouse stays fixed (pivot zoom).
 *
 * Game scrolling changes TacticalCoord → native buffer re-renders
 * with shifted content → view follows naturally since vp stays fixed.
 *
 * Edge scrolling is NOT affected (mouse coords stay in game buffer space).
 */

#include "render_bridge.h"
#include "function.h"
#include "dbg.h"
#include <SDL3/SDL.h>

extern float g_scroll_zoom_delta;
extern int   g_mouse_x, g_mouse_y;
extern SDL_Window* g_window;

static constexpr float ZOOM_STEP = 0.1f;
static constexpr float ZOOM_MIN  = 1.0f;
static constexpr float DOS_W     = 320.0f;
static constexpr float DOS_H     = 200.0f;

static float g_zoom         = 1.0f;
static float g_zoom_default = 1.0f;
static float g_zoom_max     = 4.0f;
static int   g_screen_w     = 0;
static int   g_screen_h     = 0;
static float g_vp_x         = 0.0f;  // viewport offset in native buffer pixels
static float g_vp_y         = 0.0f;

void Render_Bridge_Get_Screen_Size(int& w, int& h) { w = g_screen_w; h = g_screen_h; }

void Render_Bridge_Set_Screen_Size(int screen_w, int screen_h)
{
    if (screen_w <= 0 || screen_h <= 0) return;
    g_screen_w = screen_w;
    g_screen_h = screen_h;

    g_zoom_default = 1.0f;

    float mx = static_cast<float>(screen_w) / DOS_W;
    float my = static_cast<float>(screen_h) / DOS_H;
    g_zoom_max = (mx < my) ? mx : my;
    if (g_zoom_max < g_zoom_default) g_zoom_max = g_zoom_default;

    g_zoom = g_zoom_default;

    DBG("zoom: range [%.1f, %.1f, %.1f] screen %dx%d",
        ZOOM_MIN, g_zoom_default, g_zoom_max, screen_w, screen_h);
}

static void clamp_viewport()
{
    extern int Render_Bridge_Get_Native_Tac_W();
    extern int Render_Bridge_Get_Native_Tac_H();
    int ntw = Render_Bridge_Get_Native_Tac_W();
    int nth = Render_Bridge_Get_Native_Tac_H();
    if (ntw <= 0 || nth <= 0) { g_vp_x = 0; g_vp_y = 0; return; }

    float vis_w = static_cast<float>(ntw) / g_zoom;
    float vis_h = static_cast<float>(nth) / g_zoom;

    // At zoom 1.0: vis == native → vp must be 0
    if (vis_w >= ntw) g_vp_x = 0.0f;
    else {
        if (g_vp_x < 0.0f) g_vp_x = 0.0f;
        if (g_vp_x + vis_w > ntw) g_vp_x = ntw - vis_w;
    }

    if (vis_h >= nth) g_vp_y = 0.0f;
    else {
        if (g_vp_y < 0.0f) g_vp_y = 0.0f;
        if (g_vp_y + vis_h > nth) g_vp_y = nth - vis_h;
    }
}

// Track TacticalCoord and TacLeptonWidth for viewport adjustment
static int g_last_tac_coord_x = -1;
static int g_last_tac_coord_y = -1;
static int g_last_tac_lepton_w = 0;
static int g_last_tac_lepton_h = 0;

/// Compute proportional viewport target from TacticalCoord position.
///
/// Maps TacticalCoord's fraction through its scroll range to a viewport
/// offset that gives edge-to-edge map coverage:
///   tac at min → vp = 0           (left/top map edge visible)
///   tac at max → vp = T_px - V    (right/bottom map edge visible)
///
/// Uses TacLeptonWidth (which changes with sidebar) so the viewport
/// adapts when the sidebar toggles.
static float proportional_vp(int tac_pos, int tac_range,
                              float tac_px, float vis)
{
    if (tac_range <= 0) return 0.0f;
    float target_max = (tac_px > vis) ? tac_px - vis : 0.0f;
    float frac = static_cast<float>(tac_pos) / tac_range;
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    return frac * target_max;
}

void Render_Bridge_Apply_Scroll_Zoom()
{
    extern bool InMainLoop;
    if (!InMainLoop) return;

    extern int Render_Bridge_Get_Native_Tac_W();
    extern int Render_Bridge_Get_Native_Tac_H();

    int cur_tac_x = Coord_X(Map.TacticalCoord);
    int cur_tac_y = Coord_Y(Map.TacticalCoord);
    int cur_tac_lw = Map.TacLeptonWidth;
    int cur_tac_lh = Map.TacLeptonHeight;

    bool tac_changed = (g_last_tac_coord_x >= 0) &&
                       (cur_tac_x != g_last_tac_coord_x ||
                        cur_tac_y != g_last_tac_coord_y);
    bool layout_changed = (cur_tac_lw != g_last_tac_lepton_w ||
                           cur_tac_lh != g_last_tac_lepton_h);

    // Detect large jumps (minimap click, scenario load) — reset viewport
    bool jumped = false;
    if (g_last_tac_coord_x >= 0 && g_zoom > 1.0f && tac_changed) {
        int dx = cur_tac_x - g_last_tac_coord_x;
        int dy = cur_tac_y - g_last_tac_coord_y;
        if (dx < 0) dx = -dx;
        if (dy < 0) dy = -dy;
        if (dx > CELL_LEPTON_W * 3 || dy > CELL_LEPTON_W * 3)
            jumped = true;
    }

    g_last_tac_coord_x = cur_tac_x;
    g_last_tac_coord_y = cur_tac_y;
    g_last_tac_lepton_w = cur_tac_lw;
    g_last_tac_lepton_h = cur_tac_lh;

    // Handle mouse-wheel zoom (pivot on cursor position)
    float delta = g_scroll_zoom_delta;
    g_scroll_zoom_delta = 0.0f;
    bool zoomed = false;

    if (delta != 0.0f) {
        float new_zoom = g_zoom + g_zoom * ZOOM_STEP * delta;
        if (new_zoom < ZOOM_MIN)   new_zoom = ZOOM_MIN;
        if (new_zoom > g_zoom_max) new_zoom = g_zoom_max;

        if (new_zoom != g_zoom) {
            int tx = Map.TacPixelX, ty = Map.TacPixelY;
            int tw = Lepton_To_Pixel(Map.TacLeptonWidth);
            int th = Lepton_To_Pixel(Map.TacLeptonHeight);

            float frac_x = 0.5f, frac_y = 0.5f;
            if (tw > 0 && th > 0) {
                frac_x = static_cast<float>(g_mouse_x - tx) / tw;
                frac_y = static_cast<float>(g_mouse_y - ty) / th;
                if (frac_x < 0.0f) frac_x = 0.0f; if (frac_x > 1.0f) frac_x = 1.0f;
                if (frac_y < 0.0f) frac_y = 0.0f; if (frac_y > 1.0f) frac_y = 1.0f;
            }

            int ntw = Render_Bridge_Get_Native_Tac_W();
            int nth = Render_Bridge_Get_Native_Tac_H();

            float old_vis_w = static_cast<float>(ntw) / g_zoom;
            float old_vis_h = static_cast<float>(nth) / g_zoom;
            float mouse_native_x = g_vp_x + frac_x * old_vis_w;
            float mouse_native_y = g_vp_y + frac_y * old_vis_h;

            g_zoom = new_zoom;

            float new_vis_w = static_cast<float>(ntw) / g_zoom;
            float new_vis_h = static_cast<float>(nth) / g_zoom;
            g_vp_x = mouse_native_x - frac_x * new_vis_w;
            g_vp_y = mouse_native_y - frac_y * new_vis_h;

            clamp_viewport();
            zoomed = true;

            DBG("zoom: %.2f vp(%.0f,%.0f) vis %.0fx%.0f",
                g_zoom, g_vp_x, g_vp_y, new_vis_w, new_vis_h);
        }
    }

    // Proportional viewport tracking: when TacticalCoord changes (scroll,
    // MMB drag, minimap) or sidebar toggles, set the viewport so the full
    // map is reachable edge-to-edge. Skip on the zoom frame — the pivot
    // position is correct for that instant.
    if (g_zoom > 1.0f && !zoomed && (tac_changed || layout_changed || jumped)) {
        int ntw = Render_Bridge_Get_Native_Tac_W();
        int nth = Render_Bridge_Get_Native_Tac_H();
        if (ntw > 0 && nth > 0) {
            float vis_w = static_cast<float>(ntw) / g_zoom;
            float vis_h = static_cast<float>(nth) / g_zoom;

            float tac_w_px = static_cast<float>(Lepton_To_Pixel(Map.TacLeptonWidth));
            float tac_h_px = static_cast<float>(Lepton_To_Pixel(Map.TacLeptonHeight));

            int tac_x = cur_tac_x - Cell_To_Lepton(Map.MapCellX);
            int tac_y = cur_tac_y - Cell_To_Lepton(Map.MapCellY);
            int range_x = Cell_To_Lepton(Map.MapCellWidth)  - Map.TacLeptonWidth;
            int range_y = Cell_To_Lepton(Map.MapCellHeight) - Map.TacLeptonHeight;

            g_vp_x = proportional_vp(tac_x, range_x, tac_w_px, vis_w);
            g_vp_y = proportional_vp(tac_y, range_y, tac_h_px, vis_h);

            clamp_viewport();
        }
    }
}

void Render_Bridge_Zoom_At(float new_zoom, int cx, int cy)
{
    (void)cx; (void)cy;
    if (new_zoom < ZOOM_MIN)   new_zoom = ZOOM_MIN;
    if (new_zoom > g_zoom_max) new_zoom = g_zoom_max;
    g_zoom = new_zoom;
    clamp_viewport();
}

float Render_Bridge_Get_Zoom_Level()     { return g_zoom; }
float Render_Bridge_Get_Default_Zoom()   { return g_zoom_default; }
float Render_Bridge_Get_Viewport_X()     { return g_vp_x; }
float Render_Bridge_Get_Viewport_Y()     { return g_vp_y; }

void Render_Bridge_Transform_Mouse()
{
    // No transform needed. Game coordinates = game buffer (712x400).
    // The GL maps game buffer → screen via palette shader UV.
    // Edge scrolling, tactical clicks, unit selection all work in
    // the original game buffer coordinate space.
}
