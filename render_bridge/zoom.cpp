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
 *
 * Bridge ownership model:
 * - TacticalCoord = native replay origin used by legacy drawing
 * - g_requested_tac_* = bridge-visible world origin
 * - g_vp_* = source sub-viewport inside the native replay texture
 */

#include "render_bridge.h"
#include "function.h"
#include "dbg.h"
#include <SDL3/SDL.h>
#include <cmath>

extern float g_scroll_zoom_delta;
extern int   g_mouse_x, g_mouse_y;
extern SDL_Window* g_window;
extern bool Render_Bridge_Use_Legacy_Sidebar_Mode();

static constexpr float ZOOM_STEP = 0.1f;
static constexpr float ZOOM_MIN  = 1.0f;
static constexpr float DOS_W     = 320.0f;
static constexpr float DOS_H     = 200.0f;
static constexpr float HIRES_W   = 640.0f;
static constexpr float HIRES_H   = 400.0f;

// Zoom state for the visible sub-viewport inside the native tactical replay.
static float g_zoom         = 1.0f;
static float g_zoom_default = 1.0f;
static float g_zoom_max     = 4.0f;
static int   g_screen_w     = 0;
static int   g_screen_h     = 0;
static float g_vp_x         = 0.0f;  // viewport offset in native buffer pixels
static float g_vp_y         = 0.0f;
static bool  g_user_zoomed  = false;
// Requested visible world origin in map-relative leptons. This is the bridge-owned
// view position; TacticalCoord remains the native replay origin.
static int   g_requested_tac_x = 0;
static int   g_requested_tac_y = 0;
static bool  g_have_requested_tac = false;
static bool  g_explicit_tac_request = false;
static int   g_last_requested_tac_x = 0;
static int   g_last_requested_tac_y = 0;
static float g_last_vp_target_x = 0.0f;
static float g_last_vp_target_y = 0.0f;
static int   g_last_scroll_dx_px = 0;
static int   g_last_scroll_dy_px = 0;

// --- Bridge-owned screen layout cache ---
// Refreshed each frame from game state. All callers go through bridge accessors.
static struct {
    int logical_w, logical_h;       // bridge-owned gameplay logical screen size
    int header_x, header_y, header_w, header_h;
    int sidebar_x, sidebar_y, sidebar_w, sidebar_h;
    int tactical_x, tactical_y, tactical_w, tactical_h;
    // Render tactical rect: sidebar-independent. Spans the full content width
    // below the header regardless of sidebar state. Used for GL world presentation
    // so that toggling the sidebar does not change render scale or visible size.
    int render_tac_x, render_tac_y, render_tac_w, render_tac_h;
    int native_world_w, native_world_h;
    bool sidebar_active;
    bool valid;
} g_layout = {};

void Render_Bridge_Get_Screen_Size(int& w, int& h) { w = g_screen_w; h = g_screen_h; }

/// Default zoom is 1.0 — each native buffer pixel maps to one screen pixel.
/// This gives the widest possible view at native resolution.
static float compute_default_zoom()
{
    return ZOOM_MIN;
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
    g_have_requested_tac = false;
    refresh_default_zoom(false);

    DBG("zoom: range [%.1f, %.1f, %.1f] screen %dx%d",
        ZOOM_MIN, g_zoom_default, g_zoom_max, screen_w, screen_h);

    Render_Bridge_Refresh_Layout();
}

static void clamp_viewport()
{
    if (Render_Bridge_Debug_No_VP_Clamp()) return;
    extern int Render_Bridge_Get_Native_Tac_W();
    extern int Render_Bridge_Get_Native_Tac_H();
    int ntw = Render_Bridge_Get_Native_Tac_W();
    int nth = Render_Bridge_Get_Native_Tac_H();
    if (ntw <= 0 || nth <= 0) { g_vp_x = 0; g_vp_y = 0; return; }

    float vis_w = 0.0f;
    float vis_h = 0.0f;
    Render_Bridge_Get_Visible_Size(vis_w, vis_h);

    // The viewport only moves inside the native tactical replay texture.
    // At the outermost zoom the visible rect already spans the whole fitted source.
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

    // At each zoom level, show as much of the world as the screen can fit.
    // At Z 1.0 each native pixel = 1 screen pixel (render_tac_w screen pixels
    // can hold render_tac_w native pixels). At Z k, each native pixel = k
    // screen pixels, so the screen holds render_tac_w / k native pixels.
    // Cap to native buffer and map extents.
    int map_px_w = Map.MapCellWidth * CELL_PIXEL_W;
    int map_px_h = Map.MapCellHeight * CELL_PIXEL_W;
    int cap_w = (ntw < map_px_w) ? ntw : map_px_w;
    int cap_h = (nth < map_px_h) ? nth : map_px_h;

    float screen_vis_w = static_cast<float>(g_layout.render_tac_w) / g_zoom;
    float screen_vis_h = static_cast<float>(g_layout.render_tac_h) / g_zoom;

    w = (screen_vis_w < cap_w) ? screen_vis_w : static_cast<float>(cap_w);
    h = (screen_vis_h < cap_h) ? screen_vis_h : static_cast<float>(cap_h);
}

void Render_Bridge_Get_Visible_Size_Leptons(int& w, int& h)
{
    extern int Render_Bridge_Get_Native_Tac_W();
    extern int Render_Bridge_Get_Native_Tac_H();
    int native_w = Render_Bridge_Get_Native_Tac_W();
    int native_h = Render_Bridge_Get_Native_Tac_H();
    if (native_w <= 0 || native_h <= 0) {
        w = Map.TacLeptonWidth;
        h = Map.TacLeptonHeight;
        return;
    }

    // Bridge-aware legacy clamp size tracks the native replay window, not the
    // zoomed visible sub-viewport. Old scroll code still uses this path.
    w = Pixel_To_Lepton(native_w);
    h = Pixel_To_Lepton(native_h);

    int map_w = Cell_To_Lepton(Map.MapCellWidth);
    int map_h = Cell_To_Lepton(Map.MapCellHeight);
    if (w > map_w) {
        w = map_w;
    }
    if (h > map_h) {
        h = map_h;
    }
}

static void get_visible_window_leptons(int& w, int& h)
{
    // True visible world size after zoom/aspect fitting. This is the window used
    // for input mapping, visibility checks, and shroud coverage.
    float vis_w = 0.0f;
    float vis_h = 0.0f;
    Render_Bridge_Get_Visible_Size(vis_w, vis_h);
    if (vis_w <= 0.0f || vis_h <= 0.0f) {
        w = Map.TacLeptonWidth;
        h = Map.TacLeptonHeight;
    } else {
        w = Pixel_To_Lepton(static_cast<int>(std::round(vis_w))) + CELL_LEPTON_W;
        h = Pixel_To_Lepton(static_cast<int>(std::round(vis_h))) + CELL_LEPTON_W;
    }

    int map_w = Cell_To_Lepton(Map.MapCellWidth);
    int map_h = Cell_To_Lepton(Map.MapCellHeight);
    if (w > map_w) w = map_w;
    if (h > map_h) h = map_h;
}

/// Bridge-level effective clamp in leptons. When the sidebar is active, the
/// clamp window expands by the sidebar's world coverage. The player still only
/// sees the render tactical rect on screen, but the hidden sidebar-covered area
/// must count toward map-edge clamping so Z 1.0 render scale stays constant.
/// TacticalCoord is confined separately (display.cpp, native size) so the
/// replay buffer never extends past the map edge.
static void get_effective_clamp_leptons(int& w, int& h)
{
    get_visible_window_leptons(w, h);

    if (g_layout.sidebar_active && g_layout.render_tac_w > 0) {
        float vis_w_f = 0.0f;
        float vis_h_unused = 0.0f;
        Render_Bridge_Get_Visible_Size(vis_w_f, vis_h_unused);
        if (vis_w_f > 0.0f) {
            // How many world pixels the sidebar covers at the current render scale.
            float world_per_screen = vis_w_f / static_cast<float>(g_layout.render_tac_w);
            int sidebar_world_px = static_cast<int>(
                static_cast<float>(g_layout.sidebar_w) * world_per_screen);

            // Cap to the viewport room inside the native replay buffer.
            // The viewport can only offset by (native_w - vis_w) pixels;
            // any sidebar adjustment beyond that is unachievable.
            extern int Render_Bridge_Get_Native_Tac_W();
            int native_w = Render_Bridge_Get_Native_Tac_W();
            int vp_room = native_w - static_cast<int>(vis_w_f);
            if (vp_room < 0) vp_room = 0;
            if (sidebar_world_px > vp_room) sidebar_world_px = vp_room;

            w += Pixel_To_Lepton(sidebar_world_px);
        }
    }

    int map_w = Cell_To_Lepton(Map.MapCellWidth);
    int map_h = Cell_To_Lepton(Map.MapCellHeight);
    if (w > map_w) w = map_w;
    if (h > map_h) h = map_h;
}

static void clamp_requested_tac()
{
    if (Render_Bridge_Debug_No_Scroll_Clamp()) return;
    // Bridge-visible origin uses the effective clamp (sidebar-adjusted).
    // TacticalCoord is confined separately in display.cpp using the native
    // replay size, so the buffer stays within the map.
    int eff_w = Map.TacLeptonWidth;
    int eff_h = Map.TacLeptonHeight;
    get_effective_clamp_leptons(eff_w, eff_h);

    int max_x = Cell_To_Lepton(Map.MapCellWidth) - eff_w;
    int max_y = Cell_To_Lepton(Map.MapCellHeight) - eff_h;
    if (max_x < 0) max_x = 0;
    if (max_y < 0) max_y = 0;

    if (g_requested_tac_x < 0) g_requested_tac_x = 0;
    if (g_requested_tac_y < 0) g_requested_tac_y = 0;
    if (g_requested_tac_x > max_x) g_requested_tac_x = max_x;
    if (g_requested_tac_y > max_y) g_requested_tac_y = max_y;
}

void Render_Bridge_Record_Tactical_Request(int x, int y)
{
    g_requested_tac_x = x;
    g_requested_tac_y = y;
    g_have_requested_tac = true;
    g_explicit_tac_request = true;
    clamp_requested_tac();
}

void Render_Bridge_Record_Tactical_Request_Fallback(int x, int y)
{
    if (g_explicit_tac_request) {
        return;
    }
    g_requested_tac_x = x;
    g_requested_tac_y = y;
    g_have_requested_tac = true;
    clamp_requested_tac();
}

void Render_Bridge_Get_Requested_Tactical_Position(int& x, int& y)
{
    x = g_requested_tac_x;
    y = g_requested_tac_y;
}

// Track TacticalCoord and TacLeptonWidth for viewport adjustment
static int g_last_tac_coord_x = -1;
static int g_last_tac_coord_y = -1;
static int g_last_tac_lepton_w = 0;
static int g_last_tac_lepton_h = 0;

static bool apply_zoom_at_point(float new_zoom, int screen_x, int screen_y)
{
    if (new_zoom < ZOOM_MIN)   new_zoom = ZOOM_MIN;
    if (new_zoom > g_zoom_max) new_zoom = g_zoom_max;
    if (std::fabs(new_zoom - g_zoom) < 0.0001f) {
        return false;
    }

    // Zoom pivot is a fraction of the uniform-fitted world destination rect.
    // This must match the GL presentation's fit logic so the native pixel
    // under the cursor stays fixed across zoom changes.
    int rtac_x, rtac_y, rtac_w, rtac_h;
    Render_Bridge_Get_Render_Tactical_Rect(rtac_x, rtac_y, rtac_w, rtac_h);
    float vis_w = 0.0f;
    float vis_h = 0.0f;
    Render_Bridge_Get_Visible_Size(vis_w, vis_h);
    float fit_sx = (vis_w > 0.0f) ? static_cast<float>(rtac_w) / vis_w : 1.0f;
    float fit_sy = (vis_h > 0.0f) ? static_cast<float>(rtac_h) / vis_h : 1.0f;
    float fit_scale = (fit_sx < fit_sy) ? fit_sx : fit_sy;
    int world_w = static_cast<int>(std::round(vis_w * fit_scale));
    int world_h = static_cast<int>(std::round(vis_h * fit_scale));
    int world_x = rtac_x + (rtac_w - world_w) / 2;
    int world_y = rtac_y + (rtac_h - world_h) / 2;

    float old_frac_x = 0.5f;
    float old_frac_y = 0.5f;
    if (world_w > 0 && world_h > 0) {
        bool inside_world =
            screen_x >= world_x && screen_y >= world_y &&
            screen_x < world_x + world_w && screen_y < world_y + world_h;
        if (inside_world) {
            old_frac_x = static_cast<float>(screen_x - world_x) / static_cast<float>(world_w);
            old_frac_y = static_cast<float>(screen_y - world_y) / static_cast<float>(world_h);
            if (old_frac_x < 0.0f) old_frac_x = 0.0f;
            if (old_frac_x > 1.0f) old_frac_x = 1.0f;
            if (old_frac_y < 0.0f) old_frac_y = 0.0f;
            if (old_frac_y > 1.0f) old_frac_y = 1.0f;
        }
    }

    // Keep the native pixel under the pivot stable across the zoom change.
    float old_vis_w = 0.0f;
    float old_vis_h = 0.0f;
    Render_Bridge_Get_Visible_Size(old_vis_w, old_vis_h);
    float mouse_native_x = g_vp_x + old_frac_x * old_vis_w;
    float mouse_native_y = g_vp_y + old_frac_y * old_vis_h;

    g_zoom = new_zoom;
    g_user_zoomed = std::fabs(g_zoom - g_zoom_default) > 0.001f;

    float new_vis_w = 0.0f;
    float new_vis_h = 0.0f;
    Render_Bridge_Get_Visible_Size(new_vis_w, new_vis_h);
    float new_fit_sx = (new_vis_w > 0.0f) ? static_cast<float>(rtac_w) / new_vis_w : 1.0f;
    float new_fit_sy = (new_vis_h > 0.0f) ? static_cast<float>(rtac_h) / new_vis_h : 1.0f;
    float new_fit_scale = (new_fit_sx < new_fit_sy) ? new_fit_sx : new_fit_sy;
    int new_world_w = static_cast<int>(std::round(new_vis_w * new_fit_scale));
    int new_world_h = static_cast<int>(std::round(new_vis_h * new_fit_scale));
    int new_world_x = rtac_x + (rtac_w - new_world_w) / 2;
    int new_world_y = rtac_y + (rtac_h - new_world_h) / 2;

    float new_frac_x = 0.5f;
    float new_frac_y = 0.5f;
    if (new_world_w > 0 && new_world_h > 0) {
        bool inside_world =
            screen_x >= new_world_x && screen_y >= new_world_y &&
            screen_x < new_world_x + new_world_w && screen_y < new_world_y + new_world_h;
        if (inside_world) {
            new_frac_x = static_cast<float>(screen_x - new_world_x) / static_cast<float>(new_world_w);
            new_frac_y = static_cast<float>(screen_y - new_world_y) / static_cast<float>(new_world_h);
            if (new_frac_x < 0.0f) new_frac_x = 0.0f;
            if (new_frac_x > 1.0f) new_frac_x = 1.0f;
            if (new_frac_y < 0.0f) new_frac_y = 0.0f;
            if (new_frac_y > 1.0f) new_frac_y = 1.0f;
        }
    }

    g_vp_x = mouse_native_x - new_frac_x * new_vis_w;
    g_vp_y = mouse_native_y - new_frac_y * new_vis_h;

    if (!g_have_requested_tac) {
        g_requested_tac_x = Coord_X(Map.DesiredTacticalCoord) - Cell_To_Lepton(Map.MapCellX);
        g_requested_tac_y = Coord_Y(Map.DesiredTacticalCoord) - Cell_To_Lepton(Map.MapCellY);
        g_have_requested_tac = true;
    }
    clamp_viewport();

    // After zoom, the visible top-left is the native replay origin (which will be
    // DesiredTacticalCoord after Draw_It runs) plus the viewport offset.
    // Use DesiredTacticalCoord, not TacticalCoord: if Scroll_Map also ran this
    // frame, TacticalCoord is still the pre-scroll value while DesiredTacticalCoord
    // already holds the new position.  Using TacticalCoord here makes
    // g_requested_tac_x stale — the next scroll step would then start from the
    // wrong origin and jump the view.
    int native_origin_x = Coord_X(Map.DesiredTacticalCoord) - Cell_To_Lepton(Map.MapCellX);
    int native_origin_y = Coord_Y(Map.DesiredTacticalCoord) - Cell_To_Lepton(Map.MapCellY);
    g_requested_tac_x = native_origin_x + Pixel_To_Lepton(static_cast<int>(std::round(g_vp_x)));
    g_requested_tac_y = native_origin_y + Pixel_To_Lepton(static_cast<int>(std::round(g_vp_y)));
    clamp_requested_tac();
    return true;
}

void Render_Bridge_Apply_Scroll_Zoom()
{
    extern bool InMainLoop;
    if (!InMainLoop) return;

    Render_Bridge_Refresh_Layout();
    refresh_default_zoom(true);

    extern int Render_Bridge_Get_Native_Tac_W();
    extern int Render_Bridge_Get_Native_Tac_H();

    // Use DesiredTacticalCoord so both Apply_Scroll_Zoom calls per frame (one in
    // Begin_Draw_List before Draw_It, one in TD_SDL_Present after) observe the
    // same intended position.  Draw_It copies Desired→Tactical internally; using
    // TacticalCoord in the first call would see the *old* position, causing a
    // within-frame split: sprites culled for vp_x=delta but presented at vp_x=0.
    int cur_tac_x = Coord_X(Map.DesiredTacticalCoord);
    int cur_tac_y = Coord_Y(Map.DesiredTacticalCoord);
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

    if (!g_have_requested_tac) {
        g_requested_tac_x = cur_tac_x - Cell_To_Lepton(Map.MapCellX);
        g_requested_tac_y = cur_tac_y - Cell_To_Lepton(Map.MapCellY);
        g_have_requested_tac = true;
    }
    clamp_requested_tac();
    bool requested_changed =
        (g_requested_tac_x != g_last_requested_tac_x) ||
        (g_requested_tac_y != g_last_requested_tac_y);
    g_last_requested_tac_x = g_requested_tac_x;
    g_last_requested_tac_y = g_requested_tac_y;

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
            g_last_requested_tac_x = g_requested_tac_x;
            g_last_requested_tac_y = g_requested_tac_y;
            float vis_w = 0.0f;
            float vis_h = 0.0f;
            Render_Bridge_Get_Visible_Size(vis_w, vis_h);

            DBG("zoom: %.2f vp(%.0f,%.0f) vis %.0fx%.0f",
                g_zoom, g_vp_x, g_vp_y, vis_w, vis_h);
        }
    }

    // TacticalCoord still drives native replay. The bridge-visible window is the
    // requested world origin plus a sub-viewport offset inside that replay.
    if (!zoomed && (tac_changed || layout_changed || jumped || requested_changed)) {
        int ntw = Render_Bridge_Get_Native_Tac_W();
        int nth = Render_Bridge_Get_Native_Tac_H();
        if (ntw > 0 && nth > 0) {
            if (jumped || requested_changed) {
                // Explicit repositioning (minimap click, scroll request):
                // re-derive from integer requested position.
                float vis_w = 0.0f, vis_h = 0.0f;
                Render_Bridge_Get_Visible_Size(vis_w, vis_h);
                int tac_x = cur_tac_x - Cell_To_Lepton(Map.MapCellX);
                int tac_y = cur_tac_y - Cell_To_Lepton(Map.MapCellY);
                float vp_max_x = (static_cast<float>(ntw) > vis_w) ? static_cast<float>(ntw) - vis_w : 0.0f;
                float vp_max_y = (static_cast<float>(nth) > vis_h) ? static_cast<float>(nth) - vis_h : 0.0f;
                g_last_vp_target_x = static_cast<float>(Lepton_To_Pixel(g_requested_tac_x - tac_x));
                g_last_vp_target_y = static_cast<float>(Lepton_To_Pixel(g_requested_tac_y - tac_y));
                if (g_last_vp_target_x < 0.0f) g_last_vp_target_x = 0.0f;
                if (g_last_vp_target_y < 0.0f) g_last_vp_target_y = 0.0f;
                if (g_last_vp_target_x > vp_max_x) g_last_vp_target_x = vp_max_x;
                if (g_last_vp_target_y > vp_max_y) g_last_vp_target_y = vp_max_y;
                g_vp_x = g_last_vp_target_x;
                g_vp_y = g_last_vp_target_y;
            } else if (tac_changed) {
                // TacticalCoord moved (normal scroll, post-zoom engine tracking):
                // shift viewport by the exact delta to preserve float precision.
                g_vp_x -= static_cast<float>(g_last_scroll_dx_px);
                g_vp_y -= static_cast<float>(g_last_scroll_dy_px);
            }
            // layout_changed only: existing g_vp_x stays, just reclamp.
            clamp_viewport();
        }
    }

    g_explicit_tac_request = false;
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

    // Gameplay mouse stays in bridge logical screen space. Bridge-aware
    // callers convert into world space explicitly through Pixel_To_Coord /
    // Click_Cell_Calc / Render_Bridge_Tactical_To_World.
    g_mouse_x = raw_x;
    g_mouse_y = raw_y;
}

bool Render_Bridge_Map_Tactical_Point(int screen_x, int screen_y, int& mapped_x, int& mapped_y)
{
    int tac_x, tac_y, tac_w, tac_h;
    Render_Bridge_Get_Tactical_Rect(tac_x, tac_y, tac_w, tac_h);
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

    // Map from tactical screen space into the visible source rect inside the
    // native replay texture, accounting for the uniform-fit centering offset
    // that the GL presentation applies (gl_present_regions.cpp:154-161).
    float vis_w = 0.0f;
    float vis_h = 0.0f;
    Render_Bridge_Get_Visible_Size(vis_w, vis_h);
    if (vis_w <= 0.0f || vis_h <= 0.0f) {
        mapped_x = screen_x;
        mapped_y = screen_y;
        return false;
    }

    // Use the actual tactical viewport in legacy mode. HD mode keeps the
    // sidebar-independent render tactical rect so world scale stays stable.
    int rtac_x, rtac_y, rtac_w, rtac_h;
    if (Render_Bridge_Use_Legacy_Sidebar_Mode()) {
        rtac_x = tac_x;
        rtac_y = tac_y;
        rtac_w = tac_w;
        rtac_h = tac_h;
    } else {
        Render_Bridge_Get_Render_Tactical_Rect(rtac_x, rtac_y, rtac_w, rtac_h);
        if (rtac_w <= 0) rtac_w = tac_w;
        if (rtac_h <= 0) rtac_h = tac_h;
    }

    float fit_sx = static_cast<float>(rtac_w) / vis_w;
    float fit_sy = static_cast<float>(rtac_h) / vis_h;
    float fit_scale = (fit_sx < fit_sy) ? fit_sx : fit_sy;
    int dst_w = static_cast<int>(std::round(vis_w * fit_scale));
    int dst_h = static_cast<int>(std::round(vis_h * fit_scale));
    int dst_x_offset = (rtac_w - dst_w) / 2;
    int dst_y_offset = (rtac_h - dst_h) / 2;

    // Map within the fitted destination area.
    int wx = (screen_x - tac_x) - dst_x_offset;
    int wy = (screen_y - tac_y) - dst_y_offset;
    float frac_x = (dst_w > 0) ? static_cast<float>(wx) / static_cast<float>(dst_w) : 0.0f;
    float frac_y = (dst_h > 0) ? static_cast<float>(wy) / static_cast<float>(dst_h) : 0.0f;
    if (frac_x < 0.0f) frac_x = 0.0f;
    if (frac_x > 1.0f) frac_x = 1.0f;
    if (frac_y < 0.0f) frac_y = 0.0f;
    if (frac_y > 1.0f) frac_y = 1.0f;

    int transformed_x = tac_x + static_cast<int>(g_vp_x + frac_x * vis_w);
    int transformed_y = tac_y + static_cast<int>(g_vp_y + frac_y * vis_h);

    int clamp_w = 0;
    int clamp_h = 0;
    Render_Bridge_Get_Logical_Screen_Size(clamp_w, clamp_h);
    if (clamp_w <= 0 || clamp_h <= 0) {
        clamp_w = ScreenWidth;
        clamp_h = ScreenHeight;
    }
    if (transformed_x < 0) transformed_x = 0;
    if (transformed_y < 0) transformed_y = 0;
    if (transformed_x >= clamp_w) transformed_x = clamp_w - 1;
    if (transformed_y >= clamp_h) transformed_y = clamp_h - 1;

    mapped_x = transformed_x;
    mapped_y = transformed_y;
    return true;
}

// --- Bridge-owned screen layout (Phase 1: Geometry Authority) ---

void Render_Bridge_Refresh_Layout()
{
    extern int Render_Bridge_Get_Native_Tac_W();
    extern int Render_Bridge_Get_Native_Tac_H();

    int base_w = SeenBuff.Get_Width();
    int base_h = SeenBuff.Get_Height();
    if (base_w <= 0) base_w = ScreenWidth;
    if (base_h <= 0) base_h = ScreenHeight;

    g_layout.logical_w = (g_screen_w > 0) ? g_screen_w : base_w;
    g_layout.logical_h = (g_screen_h > 0) ? g_screen_h : base_h;

    float scale_x = (base_w > 0) ? static_cast<float>(g_layout.logical_w) / static_cast<float>(base_w) : 1.0f;
    float scale_y = (base_h > 0) ? static_cast<float>(g_layout.logical_h) / static_cast<float>(base_h) : 1.0f;
    float scale = (scale_x < scale_y) ? scale_x : scale_y;
    int offset_x = static_cast<int>(std::round((g_layout.logical_w - base_w * scale) * 0.5f));
    int offset_y = static_cast<int>(std::round((g_layout.logical_h - base_h * scale) * 0.5f));

    g_layout.tactical_x = offset_x + static_cast<int>(std::round(Map.TacPixelX * scale));
    g_layout.tactical_y = offset_y + static_cast<int>(std::round(Map.TacPixelY * scale));
    g_layout.tactical_w = static_cast<int>(std::round((WindowList[WINDOW_TACTICAL][WINDOWWIDTH] << 3) * scale));
    g_layout.tactical_h = static_cast<int>(std::round(WindowList[WINDOW_TACTICAL][WINDOWHEIGHT] * scale));

    // Header: promoted with the same uniform fit as the rest of the legacy layout.
    g_layout.header_x = offset_x;
    g_layout.header_y = offset_y;
    g_layout.header_w = static_cast<int>(std::round(base_w * scale));
    g_layout.header_h = g_layout.tactical_y - offset_y;

    // Sidebar: use the real legacy sidebar span, not the narrow bookkeeping
    // width field alone. Tactical width and sidebar width must sum back to the
    // full legacy SeenBuff width after promotion.
    g_layout.sidebar_x = offset_x + static_cast<int>(std::round(Map.SideX * scale));
    g_layout.sidebar_y = offset_y;
    g_layout.sidebar_w = static_cast<int>(std::round(Map.SideWidth * scale));
    g_layout.sidebar_h = static_cast<int>(std::round(base_h * scale));

    // Render tactical rect: always full content width, never narrowed by sidebar.
    // The GL world presentation uses this so toggling the sidebar does not change
    // the render scale or the visible world size.
    int full_content_w = static_cast<int>(std::round(base_w * scale));
    g_layout.render_tac_x = g_layout.tactical_x;
    g_layout.render_tac_y = g_layout.tactical_y;
    g_layout.render_tac_w = full_content_w;
    g_layout.render_tac_h = g_layout.tactical_h;

    // Sidebar is active when the tactical area is narrower than the full content.
    // Map.SideWidth is always non-zero after init, so checking sidebar_w alone
    // would give a false positive when the sidebar is closed.
    g_layout.sidebar_active = (g_layout.tactical_w < full_content_w);

    // Native world replay texture.
    g_layout.native_world_w = Render_Bridge_Get_Native_Tac_W();
    g_layout.native_world_h = Render_Bridge_Get_Native_Tac_H();

    g_layout.valid = true;
}

void Render_Bridge_Get_Logical_Screen_Size(int& w, int& h)
{
    w = g_layout.logical_w;
    h = g_layout.logical_h;
}

void Render_Bridge_Get_Header_Rect(int& x, int& y, int& w, int& h)
{
    x = g_layout.header_x;
    y = g_layout.header_y;
    w = g_layout.header_w;
    h = g_layout.header_h;
}

void Render_Bridge_Get_Sidebar_Rect(int& x, int& y, int& w, int& h)
{
    x = g_layout.sidebar_x;
    y = g_layout.sidebar_y;
    w = g_layout.sidebar_w;
    h = g_layout.sidebar_h;
}

void Render_Bridge_Get_Tactical_Rect(int& x, int& y, int& w, int& h)
{
    x = g_layout.tactical_x;
    y = g_layout.tactical_y;
    w = g_layout.tactical_w;
    h = g_layout.tactical_h;
}

void Render_Bridge_Get_Render_Tactical_Rect(int& x, int& y, int& w, int& h)
{
    x = g_layout.render_tac_x;
    y = g_layout.render_tac_y;
    w = g_layout.render_tac_w;
    h = g_layout.render_tac_h;
}

void Render_Bridge_Get_Mouse_Input_Rect(int& x, int& y, int& w, int& h)
{
    if (Render_Bridge_Use_Legacy_Sidebar_Mode()) {
        x = g_layout.tactical_x;
        y = g_layout.tactical_y;
        w = g_layout.tactical_w;
        h = g_layout.tactical_h;
        return;
    }

    // HD input follows the full on-screen tactical presentation, not the
    // narrower legacy tactical window. Sidebar changes clamp behaviour, not click area.
    x = g_layout.render_tac_x;
    y = g_layout.render_tac_y;
    w = g_layout.render_tac_w;
    h = g_layout.render_tac_h;
}

void Render_Bridge_Get_Native_World_Rect(int& w, int& h)
{
    w = g_layout.native_world_w;
    h = g_layout.native_world_h;
}

void Render_Bridge_Get_Visible_World_Rect(int& origin_x, int& origin_y,
                                           int& width, int& height)
{
    get_visible_window_leptons(width, height);

    // The authoritative bridge-visible world window is the current native replay
    // origin plus the sub-viewport offset inside that replay. Using the requested
    // origin directly is only correct at outer zoom when vp == 0; once zoom moves
    // the viewport inside the native replay texture, input/selection/shroud must
    // follow the actual visible top-left.
    int native_origin_x = Coord_X(Map.TacticalCoord) - Cell_To_Lepton(Map.MapCellX);
    int native_origin_y = Coord_Y(Map.TacticalCoord) - Cell_To_Lepton(Map.MapCellY);
    origin_x = native_origin_x + Pixel_To_Lepton(static_cast<int>(std::round(g_vp_x)));
    origin_y = native_origin_y + Pixel_To_Lepton(static_cast<int>(std::round(g_vp_y)));

    int max_x = Cell_To_Lepton(Map.MapCellWidth) - width;
    int max_y = Cell_To_Lepton(Map.MapCellHeight) - height;
    if (max_x < 0) max_x = 0;
    if (max_y < 0) max_y = 0;
    if (origin_x < 0) origin_x = 0;
    if (origin_y < 0) origin_y = 0;
    if (origin_x > max_x) origin_x = max_x;
    if (origin_y > max_y) origin_y = max_y;
}

void Render_Bridge_Get_Clamp_Ranges(int& max_x, int& max_y)
{
    int eff_w = Map.TacLeptonWidth;
    int eff_h = Map.TacLeptonHeight;
    get_effective_clamp_leptons(eff_w, eff_h);

    max_x = Cell_To_Lepton(Map.MapCellWidth) - eff_w;
    max_y = Cell_To_Lepton(Map.MapCellHeight) - eff_h;
    if (max_x < 0) max_x = 0;
    if (max_y < 0) max_y = 0;
}

void Render_Bridge_Get_Effective_Clamp_Size(int& w, int& h)
{
    w = Map.TacLeptonWidth;
    h = Map.TacLeptonHeight;
    get_effective_clamp_leptons(w, h);
}

/// Edge zone in leptons — sprites near the viewport boundary still need rendering.
static constexpr int EDGE_ZONE_L = CELL_LEPTON_W * 2;

bool Render_Bridge_World_To_Tactical(int world_lx, int world_ly,
                                      int& pixel_x, int& pixel_y)
{
    // Project against the NATIVE BUFFER origin (TacticalCoord), not the visible
    // window origin.  The native replay buffer holds the world starting at
    // TacticalCoord; the visible viewport (g_vp_x) is a sub-region of that buffer
    // selected by the GL presenter.  Both the native-buffer replay path and the
    // GL sprites/primitives overlay path subtract g_vp_x themselves when mapping
    // native pixels to screen pixels:
    //   GL formula: screen_x = tac_screen_x + (native_pixel_x - vp_x) * scale
    // Using the visible-window origin here (TacticalCoord + g_vp_x_leptons) would
    // make native_pixel_x already g_vp_x short, causing a double-subtract on every
    // GL overlay draw.  At zoom 1 (g_vp_x = 0) the two are identical; the error
    // only appears when zoomed in, and changes with every zoom step — the visible
    // jump the user sees after each wheel tick.
    int world_origin_x = Coord_X(Map.TacticalCoord);
    int world_origin_y = Coord_Y(Map.TacticalCoord);

    // Cull against the full native buffer extent, not just the visible sub-window.
    // When zoomed in the native buffer is larger than the visible area; sprites
    // just outside the current viewport but still in the buffer must be included
    // in the draw list so the native texture is fully populated for scrolling.
    extern int Render_Bridge_Get_Native_Tac_W();
    extern int Render_Bridge_Get_Native_Tac_H();
    int native_w_px = Render_Bridge_Get_Native_Tac_W();
    int native_h_px = Render_Bridge_Get_Native_Tac_H();
    int cull_w = (native_w_px > 0) ? Pixel_To_Lepton(native_w_px) : Map.TacLeptonWidth;
    int cull_h = (native_h_px > 0) ? Pixel_To_Lepton(native_h_px) : Map.TacLeptonHeight;

    int xoff = (world_lx + EDGE_ZONE_L) - world_origin_x;
    if ((unsigned)xoff > (unsigned)(cull_w + EDGE_ZONE_L * 2)) return false;

    int yoff = (world_ly + EDGE_ZONE_L) - world_origin_y;
    if ((unsigned)yoff > (unsigned)(cull_h + EDGE_ZONE_L * 2)) return false;

    pixel_x = Lepton_To_Pixel(xoff) - CELL_PIXEL_W * 2;
    pixel_y = Lepton_To_Pixel(yoff) - CELL_PIXEL_W * 2;
    return true;
}

bool Render_Bridge_Tactical_To_World(int pixel_x, int pixel_y,
                                      int& world_lx, int& world_ly)
{
    // Convert tactical screen pixels back into absolute world leptons.
    // Replicates the GL uniform-fit centering (gl_present_regions.cpp:154-161)
    // so input mapping matches the presentation geometry exactly.
    int tac_x, tac_y, tac_w, tac_h;
    Render_Bridge_Get_Tactical_Rect(tac_x, tac_y, tac_w, tac_h);

    int sx = pixel_x - tac_x;
    int sy = pixel_y - tac_y;

    if (tac_w <= 0 || tac_h <= 0) return false;
    if ((unsigned)sx >= (unsigned)tac_w || (unsigned)sy >= (unsigned)tac_h) return false;

    // Use the same fitted destination area that presentation uses.
    // Legacy mode presents into the actual tactical viewport; HD mode uses the
    // sidebar-independent render rect.
    int rtac_x, rtac_y, rtac_w, rtac_h;
    if (Render_Bridge_Use_Legacy_Sidebar_Mode()) {
        rtac_x = tac_x;
        rtac_y = tac_y;
        rtac_w = tac_w;
        rtac_h = tac_h;
    } else {
        Render_Bridge_Get_Render_Tactical_Rect(rtac_x, rtac_y, rtac_w, rtac_h);
        if (rtac_w <= 0) rtac_w = tac_w;
        if (rtac_h <= 0) rtac_h = tac_h;
    }

    // Visible size in native pixels (no CELL_LEPTON_W padding).
    float vis_w_px = 0.0f;
    float vis_h_px = 0.0f;
    Render_Bridge_Get_Visible_Size(vis_w_px, vis_h_px);
    if (vis_w_px <= 0.0f || vis_h_px <= 0.0f) return false;

    // Uniform-fit: the visible source is scaled to fill the render tactical area
    // while preserving aspect ratio, centered with bars on the short axis.
    float fit_sx = static_cast<float>(rtac_w) / vis_w_px;
    float fit_sy = static_cast<float>(rtac_h) / vis_h_px;
    float fit_scale = (fit_sx < fit_sy) ? fit_sx : fit_sy;
    int dst_w = static_cast<int>(std::round(vis_w_px * fit_scale));
    int dst_h = static_cast<int>(std::round(vis_h_px * fit_scale));
    int dst_x_offset = (rtac_w - dst_w) / 2;
    int dst_y_offset = (rtac_h - dst_h) / 2;

    // Reject clicks in the centering bars (outside the fitted world area).
    int wx = sx - dst_x_offset;
    int wy = sy - dst_y_offset;
    if (wx < 0 || wx >= dst_w || wy < 0 || wy >= dst_h) return false;

    // Map screen pixel within the fitted destination back to native buffer pixel.
    float native_px_x = static_cast<float>(wx) * vis_w_px / static_cast<float>(dst_w);
    float native_px_y = static_cast<float>(wy) * vis_h_px / static_cast<float>(dst_h);

    // Convert native pixel to lepton (no CELL_LEPTON_W padding).
    int lx = Pixel_To_Lepton(static_cast<int>(native_px_x));
    int ly = Pixel_To_Lepton(static_cast<int>(native_px_y));

    // Add viewport offset and world origin.
    float vp_x = Render_Bridge_Get_Viewport_X();
    float vp_y = Render_Bridge_Get_Viewport_Y();
    world_lx = Coord_X(Map.TacticalCoord) + Pixel_To_Lepton(static_cast<int>(std::round(vp_x))) + lx;
    world_ly = Coord_Y(Map.TacticalCoord) + Pixel_To_Lepton(static_cast<int>(std::round(vp_y))) + ly;
    return true;
}

bool Render_Bridge_Is_Cell_In_View(int cell_x, int cell_y)
{
    if (Render_Bridge_Debug_No_Cell_Cull()) return true;
    // Visibility is evaluated against the bridge-visible window so shroud, hit tests,
    // and selection use the same tactical area.
    int world_lx = Cell_To_Lepton(cell_x) & 0xFF00;
    int world_ly = Cell_To_Lepton(cell_y) & 0xFF00;
    int origin_x = 0;
    int origin_y = 0;
    int vis_w = 0;
    int vis_h = 0;
    Render_Bridge_Get_Visible_World_Rect(origin_x, origin_y, vis_w, vis_h);
    int world_origin_x = (origin_x + Cell_To_Lepton(Map.MapCellX)) & 0xFF00;
    int world_origin_y = (origin_y + Cell_To_Lepton(Map.MapCellY)) & 0xFF00;

    if ((unsigned)(world_lx - world_origin_x) > (unsigned)(vis_w + 255 + CELL_LEPTON_W)) return false;
    if ((unsigned)(world_ly - world_origin_y) > (unsigned)(vis_h + 255 + CELL_LEPTON_W)) return false;
    return true;
}
