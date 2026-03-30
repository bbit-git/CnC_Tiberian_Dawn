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
#include <cmath>

extern float g_scroll_zoom_delta;
extern int   g_mouse_x, g_mouse_y;
extern SDL_Window* g_window;

static constexpr float ZOOM_STEP = 0.1f;
static constexpr float ZOOM_MIN  = 1.0f;
static constexpr float DOS_W     = 320.0f;
static constexpr float DOS_H     = 200.0f;
static constexpr float HIRES_W   = 640.0f;
static constexpr float HIRES_H   = 400.0f;

static float g_zoom         = 1.0f;
static float g_zoom_default = 1.0f;
static float g_zoom_max     = 4.0f;
static int   g_screen_w     = 0;
static int   g_screen_h     = 0;
static float g_vp_x         = 0.0f;  // viewport offset in native buffer pixels
static float g_vp_y         = 0.0f;
static bool  g_user_zoomed  = false;
static float g_last_vp_target_x = 0.0f;
static float g_last_vp_target_y = 0.0f;
static int   g_last_scroll_dx_px = 0;
static int   g_last_scroll_dy_px = 0;

void Render_Bridge_Get_Screen_Size(int& w, int& h) { w = g_screen_w; h = g_screen_h; }

/// Compute the startup zoom from the classic 640x400 high-resolution baseline.
static float compute_default_zoom()
{
    int base_w = SeenBuff.Get_Width();
    int base_h = SeenBuff.Get_Height();
    if (base_w <= 0 || base_h <= 0) {
        return ZOOM_MIN;
    }

    float zx = static_cast<float>(base_w) / HIRES_W;
    float zy = static_cast<float>(base_h) / HIRES_H;
    float z = (zx > zy) ? zx : zy;
    if (z < ZOOM_MIN) {
        z = ZOOM_MIN;
    }
    return z;
}

/// Refresh default zoom from the live tactical buffer sizing.
static void refresh_default_zoom(bool preserve_current_default)
{
    float new_default = compute_default_zoom();
    if (new_default <= 0.0f) {
        return;
    }

    bool was_at_default = std::fabs(g_zoom - g_zoom_default) < 0.001f;
    g_zoom_default = new_default;

    float mx = static_cast<float>(g_screen_w) / DOS_W;
    float my = static_cast<float>(g_screen_h) / DOS_H;
    g_zoom_max = (mx < my) ? mx : my;
    if (g_zoom_max < g_zoom_default) {
        g_zoom_max = g_zoom_default;
    }

    if (!preserve_current_default || was_at_default || (!g_user_zoomed && g_zoom <= ZOOM_MIN + 0.001f)) {
        g_zoom = g_zoom_default;
    } else {
        if (g_zoom < ZOOM_MIN) g_zoom = ZOOM_MIN;
        if (g_zoom > g_zoom_max) g_zoom = g_zoom_max;
    }
}

void Render_Bridge_Set_Screen_Size(int screen_w, int screen_h)
{
    if (screen_w <= 0 || screen_h <= 0) return;
    g_screen_w = screen_w;
    g_screen_h = screen_h;
    g_user_zoomed = false;
    refresh_default_zoom(false);

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

    float vis_w = 0.0f;
    float vis_h = 0.0f;
    Render_Bridge_Get_Visible_Size(vis_w, vis_h);

    // At outermost zoom: visible rect fills the fitted native viewport.
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

/// Force viewport edges to follow tactical map edges exactly.
static void apply_edge_constraints()
{
    extern int Render_Bridge_Get_Native_Tac_W();
    extern int Render_Bridge_Get_Native_Tac_H();
    int ntw = Render_Bridge_Get_Native_Tac_W();
    int nth = Render_Bridge_Get_Native_Tac_H();
    if (ntw <= 0 || nth <= 0) {
        return;
    }

    float vis_w = 0.0f;
    float vis_h = 0.0f;
    Render_Bridge_Get_Visible_Size(vis_w, vis_h);
    float vp_max_x = (static_cast<float>(ntw) > vis_w) ? static_cast<float>(ntw) - vis_w : 0.0f;
    float vp_max_y = (static_cast<float>(nth) > vis_h) ? static_cast<float>(nth) - vis_h : 0.0f;

    int clamp_w = Map.TacLeptonWidth;
    int clamp_h = Map.TacLeptonHeight;
    Render_Bridge_Get_Visible_Size_Leptons(clamp_w, clamp_h);

    int tac_x = Coord_X(Map.TacticalCoord) - Cell_To_Lepton(Map.MapCellX);
    int tac_y = Coord_Y(Map.TacticalCoord) - Cell_To_Lepton(Map.MapCellY);
    int range_x = Cell_To_Lepton(Map.MapCellWidth)  - clamp_w;
    int range_y = Cell_To_Lepton(Map.MapCellHeight) - clamp_h;

    if (tac_x <= 0) g_vp_x = 0.0f;
    if (tac_y <= 0) g_vp_y = 0.0f;
    if (range_x > 0 && tac_x >= range_x) g_vp_x = vp_max_x;
    if (range_y > 0 && tac_y >= range_y) g_vp_y = vp_max_y;

    clamp_viewport();
}

void Render_Bridge_Get_Visible_Size(float& w, float& h)
{
    extern int Render_Bridge_Get_Native_Tac_W();
    extern int Render_Bridge_Get_Native_Tac_H();
    int ntw = Render_Bridge_Get_Native_Tac_W();
    int nth = Render_Bridge_Get_Native_Tac_H();
    if (ntw <= 0 || nth <= 0) {
        w = 0.0f;
        h = 0.0f;
        return;
    }

    float tac_w = static_cast<float>(Lepton_To_Pixel(Map.TacLeptonWidth));
    float tac_h = static_cast<float>(Lepton_To_Pixel(Map.TacLeptonHeight));
    if (tac_w <= 0.0f || tac_h <= 0.0f) {
        w = static_cast<float>(ntw);
        h = static_cast<float>(nth);
        return;
    }

    float tactical_aspect = tac_w / tac_h;
    float native_aspect = static_cast<float>(ntw) / static_cast<float>(nth);

    float base_w = static_cast<float>(ntw);
    float base_h = static_cast<float>(nth);
    if (native_aspect > tactical_aspect) {
        base_w = base_h * tactical_aspect;
    } else {
        base_h = base_w / tactical_aspect;
    }

    w = base_w / g_zoom;
    h = base_h / g_zoom;
}

void Render_Bridge_Get_Visible_Size_Leptons(int& w, int& h)
{
    float vis_w = 0.0f;
    float vis_h = 0.0f;
    Render_Bridge_Get_Visible_Size(vis_w, vis_h);
    if (vis_w <= 0.0f || vis_h <= 0.0f) {
        w = Map.TacLeptonWidth;
        h = Map.TacLeptonHeight;
        return;
    }

    int vis_px_w = static_cast<int>(std::ceil(vis_w));
    int vis_px_h = static_cast<int>(std::ceil(vis_h));
    w = Pixel_To_Lepton(vis_px_w);
    h = Pixel_To_Lepton(vis_px_h);

    int map_w = Cell_To_Lepton(Map.MapCellWidth);
    int map_h = Cell_To_Lepton(Map.MapCellHeight);
    if (w > map_w) {
        w = map_w;
    }
    if (h > map_h) {
        h = map_h;
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
///   tac at min → vp = 0
///   tac at max → vp = viewport_max
static float proportional_vp(int tac_pos, int tac_range, float viewport_max)
{
    if (tac_range <= 0) return 0.0f;
    float frac = static_cast<float>(tac_pos) / tac_range;
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    if (viewport_max <= 0.0f) return 0.0f;
    return frac * viewport_max;
}

static bool apply_zoom_at_point(float new_zoom, int screen_x, int screen_y)
{
    if (new_zoom < ZOOM_MIN)   new_zoom = ZOOM_MIN;
    if (new_zoom > g_zoom_max) new_zoom = g_zoom_max;
    if (std::fabs(new_zoom - g_zoom) < 0.0001f) {
        return false;
    }

    int tac_x = Map.TacPixelX;
    int tac_y = Map.TacPixelY;
    int tac_w = Lepton_To_Pixel(Map.TacLeptonWidth);
    int tac_h = Lepton_To_Pixel(Map.TacLeptonHeight);

    float frac_x = 0.5f;
    float frac_y = 0.5f;
    if (tac_w > 0 && tac_h > 0) {
        bool inside_tactical =
            screen_x >= tac_x && screen_y >= tac_y &&
            screen_x < tac_x + tac_w && screen_y < tac_y + tac_h;
        if (inside_tactical) {
            frac_x = static_cast<float>(screen_x - tac_x) / static_cast<float>(tac_w);
            frac_y = static_cast<float>(screen_y - tac_y) / static_cast<float>(tac_h);
            if (frac_x < 0.0f) frac_x = 0.0f;
            if (frac_x > 1.0f) frac_x = 1.0f;
            if (frac_y < 0.0f) frac_y = 0.0f;
            if (frac_y > 1.0f) frac_y = 1.0f;
        }
    }

    float old_vis_w = 0.0f;
    float old_vis_h = 0.0f;
    Render_Bridge_Get_Visible_Size(old_vis_w, old_vis_h);
    float mouse_native_x = g_vp_x + frac_x * old_vis_w;
    float mouse_native_y = g_vp_y + frac_y * old_vis_h;

    g_zoom = new_zoom;
    g_user_zoomed = std::fabs(g_zoom - g_zoom_default) > 0.001f;

    float new_vis_w = 0.0f;
    float new_vis_h = 0.0f;
    Render_Bridge_Get_Visible_Size(new_vis_w, new_vis_h);
    g_vp_x = mouse_native_x - frac_x * new_vis_w;
    g_vp_y = mouse_native_y - frac_y * new_vis_h;

    clamp_viewport();
    return true;
}

void Render_Bridge_Apply_Scroll_Zoom()
{
    extern bool InMainLoop;
    if (!InMainLoop) return;

    refresh_default_zoom(true);

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
    g_last_scroll_dx_px = 0;
    g_last_scroll_dy_px = 0;
    if (g_last_tac_coord_x >= 0 && g_zoom > 1.0f && tac_changed) {
        int dx = cur_tac_x - g_last_tac_coord_x;
        int dy = cur_tac_y - g_last_tac_coord_y;
        g_last_scroll_dx_px = Lepton_To_Pixel(dx);
        g_last_scroll_dy_px = Lepton_To_Pixel(dy);
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
        int raw_mouse_x = 0;
        int raw_mouse_y = 0;
        extern void TD_SDL_Get_Raw_Mouse_Position(int& x, int& y);
        TD_SDL_Get_Raw_Mouse_Position(raw_mouse_x, raw_mouse_y);

        if (apply_zoom_at_point(new_zoom, raw_mouse_x, raw_mouse_y)) {
            zoomed = true;
            float vis_w = 0.0f;
            float vis_h = 0.0f;
            Render_Bridge_Get_Visible_Size(vis_w, vis_h);

            DBG("zoom: %.2f vp(%.0f,%.0f) vis %.0fx%.0f",
                g_zoom, g_vp_x, g_vp_y, vis_w, vis_h);
        }
    }

    // Proportional viewport tracking: when TacticalCoord changes (scroll,
    // MMB drag, minimap) or sidebar toggles, set the viewport so the full
    // map is reachable edge-to-edge. Skip on the zoom frame — the pivot
    // position is correct for that instant.
    if (!zoomed && (tac_changed || layout_changed || jumped)) {
        int ntw = Render_Bridge_Get_Native_Tac_W();
        int nth = Render_Bridge_Get_Native_Tac_H();
        if (ntw > 0 && nth > 0) {
            float vis_w = 0.0f;
            float vis_h = 0.0f;
            Render_Bridge_Get_Visible_Size(vis_w, vis_h);

            int clamp_w = Map.TacLeptonWidth;
            int clamp_h = Map.TacLeptonHeight;
            Render_Bridge_Get_Visible_Size_Leptons(clamp_w, clamp_h);

            int tac_x = cur_tac_x - Cell_To_Lepton(Map.MapCellX);
            int tac_y = cur_tac_y - Cell_To_Lepton(Map.MapCellY);
            int range_x = Cell_To_Lepton(Map.MapCellWidth)  - clamp_w;
            int range_y = Cell_To_Lepton(Map.MapCellHeight) - clamp_h;

            float vp_max_x = (static_cast<float>(ntw) > vis_w) ? static_cast<float>(ntw) - vis_w : 0.0f;
            float vp_max_y = (static_cast<float>(nth) > vis_h) ? static_cast<float>(nth) - vis_h : 0.0f;

            g_last_vp_target_x = proportional_vp(tac_x, range_x, vp_max_x);
            g_last_vp_target_y = proportional_vp(tac_y, range_y, vp_max_y);
            g_vp_x = g_last_vp_target_x;
            g_vp_y = g_last_vp_target_y;

            clamp_viewport();
            apply_edge_constraints();
        }
    }
}

void Render_Bridge_Zoom_At(float new_zoom, int cx, int cy)
{
    apply_zoom_at_point(new_zoom, cx, cy);
}

float Render_Bridge_Get_Zoom_Level()     { return g_zoom; }
float Render_Bridge_Get_Default_Zoom()   { return g_zoom_default; }
float Render_Bridge_Get_Viewport_X()     { return g_vp_x; }
float Render_Bridge_Get_Viewport_Y()     { return g_vp_y; }
void Render_Bridge_Get_Viewport_Target(float& x, float& y) { x = g_last_vp_target_x; y = g_last_vp_target_y; }
void Render_Bridge_Get_Scroll_Delta(int& x, int& y) { x = g_last_scroll_dx_px; y = g_last_scroll_dy_px; }

void Render_Bridge_Transform_Mouse()
{
    extern void TD_SDL_Get_Raw_Mouse_Position(int& x, int& y);
    int raw_x = 0;
    int raw_y = 0;
    TD_SDL_Get_Raw_Mouse_Position(raw_x, raw_y);

    g_mouse_x = raw_x;
    g_mouse_y = raw_y;
}

bool Render_Bridge_Map_Tactical_Point(int screen_x, int screen_y, int& mapped_x, int& mapped_y)
{
    int tac_x = Map.TacPixelX;
    int tac_y = Map.TacPixelY;
    int tac_w = Lepton_To_Pixel(Map.TacLeptonWidth);
    int tac_h = Lepton_To_Pixel(Map.TacLeptonHeight);
    if (tac_w <= 0 || tac_h <= 0) {
        mapped_x = screen_x;
        mapped_y = screen_y;
        return false;
    }

    if (screen_x < tac_x || screen_y < tac_y || screen_x >= tac_x + tac_w || screen_y >= tac_y + tac_h) {
        mapped_x = screen_x;
        mapped_y = screen_y;
        return false;
    }

    float frac_x = static_cast<float>(screen_x - tac_x) / static_cast<float>(tac_w);
    float frac_y = static_cast<float>(screen_y - tac_y) / static_cast<float>(tac_h);
    if (frac_x < 0.0f) frac_x = 0.0f;
    if (frac_x > 1.0f) frac_x = 1.0f;
    if (frac_y < 0.0f) frac_y = 0.0f;
    if (frac_y > 1.0f) frac_y = 1.0f;

    float vis_w = 0.0f;
    float vis_h = 0.0f;
    Render_Bridge_Get_Visible_Size(vis_w, vis_h);
    if (vis_w <= 0.0f || vis_h <= 0.0f) {
        mapped_x = screen_x;
        mapped_y = screen_y;
        return false;
    }

    int transformed_x = tac_x + static_cast<int>(g_vp_x + frac_x * vis_w);
    int transformed_y = tac_y + static_cast<int>(g_vp_y + frac_y * vis_h);

    if (transformed_x < 0) transformed_x = 0;
    if (transformed_y < 0) transformed_y = 0;
    if (transformed_x >= ScreenWidth) transformed_x = ScreenWidth - 1;
    if (transformed_y >= ScreenHeight) transformed_y = ScreenHeight - 1;

    mapped_x = transformed_x;
    mapped_y = transformed_y;
    return true;
}
