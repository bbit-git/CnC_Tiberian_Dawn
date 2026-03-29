/**
 * zoom.cpp — Pixel-zoom on the tactical viewport only.
 *
 * The tactical viewport is defined by Map.TacPixelX/Y and
 * Map.TacLeptonWidth/Height. It changes dynamically when the
 * sidebar toggles. Zoom magnifies the world INSIDE the viewport
 * without resizing the viewport itself.
 *
 * Does NOT modify game state — reads live tactical dimensions each frame.
 */

#include "render_bridge.h"
#include "function.h"
#include "dbg.h"
#include <SDL3/SDL.h>

extern float g_scroll_zoom_delta;
extern int   g_mouse_x, g_mouse_y;
extern SDL_Window* g_window;

static constexpr float ZOOM_STEP  = 0.1f;
static constexpr float ZOOM_MIN  = 1.0f;  // 1:1 — each game pixel = 1 screen pixel
static constexpr float DOS_W     = 320.0f;
static constexpr float DOS_H     = 200.0f;

static float g_zoom         = 1.0f;  // current zoom (screen px per game px)
static float g_zoom_default = 1.0f;  // fills screen = min(screen/buffer)
static float g_zoom_max     = 4.0f;  // computed: min(screen/320, screen/200)
static int   g_screen_w     = 0;
static int   g_screen_h     = 0;
static float g_viewport_x = 0.0f;
static float g_viewport_y = 0.0f;
static int   g_raw_mouse_x = 0;
static int   g_raw_mouse_y = 0;

/// Get live tactical area dimensions from the game.
static void get_tac(int& x, int& y, int& w, int& h)
{
    x = Map.TacPixelX;
    y = Map.TacPixelY;
    w = Lepton_To_Pixel(Map.TacLeptonWidth);
    h = Lepton_To_Pixel(Map.TacLeptonHeight);
}

static void clamp_viewport()
{
    // Viewport operates within the native tactical buffer,
    // not the game buffer tactical dimensions.
    extern int Render_Bridge_Get_Native_Tac_W();
    extern int Render_Bridge_Get_Native_Tac_H();
    int tw = Render_Bridge_Get_Native_Tac_W();
    int th = Render_Bridge_Get_Native_Tac_H();

    // Fall back to game buffer if no native buffer
    if (tw <= 0 || th <= 0) {
        tw = Lepton_To_Pixel(Map.TacLeptonWidth);
        th = Lepton_To_Pixel(Map.TacLeptonHeight);
    }
    if (tw <= 0 || th <= 0) return;

    float vis_w = static_cast<float>(tw) / g_zoom;
    float vis_h = static_cast<float>(th) / g_zoom;

    if (g_viewport_x < 0.0f) g_viewport_x = 0.0f;
    if (g_viewport_y < 0.0f) g_viewport_y = 0.0f;
    if (g_viewport_x + vis_w > tw) g_viewport_x = static_cast<float>(tw) - vis_w;
    if (g_viewport_y + vis_h > th) g_viewport_y = static_cast<float>(th) - vis_h;
    if (g_viewport_x < 0.0f) g_viewport_x = 0.0f;
    if (g_viewport_y < 0.0f) g_viewport_y = 0.0f;
}

static void zoom_at(float new_zoom, int anchor_x, int anchor_y)
{
    int tx, ty, tw, th;
    get_tac(tx, ty, tw, th);
    if (tw <= 0 || th <= 0) return;

    extern int Render_Bridge_Get_Native_Tac_W();
    extern int Render_Bridge_Get_Native_Tac_H();
    int ntw = Render_Bridge_Get_Native_Tac_W();
    int nth = Render_Bridge_Get_Native_Tac_H();
    if (ntw <= 0) ntw = tw;
    if (nth <= 0) nth = th;

    if (new_zoom < ZOOM_MIN)  new_zoom = ZOOM_MIN;
    if (new_zoom > g_zoom_max) new_zoom = g_zoom_max;
    if (new_zoom == g_zoom) return;

    // Anchor in game buffer coords → fraction of tactical area (0..1)
    float frac_x = static_cast<float>(anchor_x - tx) / static_cast<float>(tw);
    float frac_y = static_cast<float>(anchor_y - ty) / static_cast<float>(th);
    if (frac_x < 0.0f) frac_x = 0.0f; if (frac_x > 1.0f) frac_x = 1.0f;
    if (frac_y < 0.0f) frac_y = 0.0f; if (frac_y > 1.0f) frac_y = 1.0f;

    // Fraction → native buffer pixel position within visible viewport
    float vis_w = static_cast<float>(ntw) / g_zoom;
    float vis_h = static_cast<float>(nth) / g_zoom;
    float ax = frac_x * vis_w;
    float ay = frac_y * vis_h;

    // Source pixel in native buffer under the anchor
    float src_x = g_viewport_x + ax;
    float src_y = g_viewport_y + ay;

    g_zoom = new_zoom;

    // New visible area at new zoom
    float new_vis_w = static_cast<float>(ntw) / g_zoom;
    float new_vis_h = static_cast<float>(nth) / g_zoom;

    // Reposition so the anchor fraction maps to the same source pixel
    g_viewport_x = src_x - frac_x * new_vis_w;
    g_viewport_y = src_y - frac_y * new_vis_h;
    clamp_viewport();

    DBG("zoom: %.2f viewport [%.1f, %.1f] tac %dx%d", g_zoom, g_viewport_x, g_viewport_y, tw, th);
}

void Render_Bridge_Get_Screen_Size(int& w, int& h)
{
    w = g_screen_w;
    h = g_screen_h;
}

void Render_Bridge_Set_Screen_Size(int screen_w, int screen_h)
{
    if (screen_w <= 0 || screen_h <= 0) return;
    g_screen_w = screen_w;
    g_screen_h = screen_h;

    int bw = SeenBuff.Get_Width();
    int bh = SeenBuff.Get_Height();
    if (bw <= 0 || bh <= 0) return;

    // Default zoom = 1.0: native 1:1 pixels. Each game pixel = 1 screen pixel.
    // The tactical viewport expands to show min(screen, map) cells at 24x24px.
    // UI (sidebar/tab) scales independently via ui_scale in GL present.
    g_zoom_default = 1.0f;

    // Max zoom: 320x200 DOS view fills the screen (accounting for sidebar).
    // Sidebar width in screen pixels = SideBarWidth * g_zoom_default.
    // Tactical screen area = screen_w - sidebar_screen_w.
    // Max zoom = tactical_screen_w / 320 (or screen_h / 200, whichever is less).
    float mx = static_cast<float>(screen_w) / DOS_W;
    float my = static_cast<float>(screen_h) / DOS_H;
    g_zoom_max = (mx < my) ? mx : my;
    if (g_zoom_max < g_zoom_default) g_zoom_max = g_zoom_default;

    // Start at default zoom (game fills screen)
    g_zoom = g_zoom_default;

    DBG("zoom: range [%.1f, %.1f, %.1f] (1:1, default, max) screen %dx%d buffer %dx%d",
        ZOOM_MIN, g_zoom_default, g_zoom_max, screen_w, screen_h, bw, bh);
}

void Render_Bridge_Apply_Scroll_Zoom()
{
    g_raw_mouse_x = g_mouse_x;
    g_raw_mouse_y = g_mouse_y;

    float delta = g_scroll_zoom_delta;
    g_scroll_zoom_delta = 0.0f;

    extern bool InMainLoop;
    if (!InMainLoop || delta == 0.0f) return;

    float step = g_zoom * ZOOM_STEP * delta;
    zoom_at(g_zoom + step, g_raw_mouse_x, g_raw_mouse_y);
}

void Render_Bridge_Zoom_At(float new_zoom, int center_x, int center_y)
{
    zoom_at(new_zoom, center_x, center_y);
}

float Render_Bridge_Get_Zoom_Level()     { return g_zoom; }
float Render_Bridge_Get_Default_Zoom()   { return g_zoom_default; }
float Render_Bridge_Get_Viewport_X()     { return g_viewport_x; }
float Render_Bridge_Get_Viewport_Y()     { return g_viewport_y; }

void Render_Bridge_Transform_Mouse()
{
    extern bool InMainLoop;
    if (!InMainLoop || g_zoom == 1.0f) return;

    int tx, ty, tw, th;
    get_tac(tx, ty, tw, th);
    if (tw <= 0 || th <= 0) return;

    // Only transform if raw mouse is over the tactical viewport
    if (g_raw_mouse_x >= tx && g_raw_mouse_x < tx + tw &&
        g_raw_mouse_y >= ty && g_raw_mouse_y < ty + th) {

        float rel_x = static_cast<float>(g_raw_mouse_x - tx);
        float rel_y = static_cast<float>(g_raw_mouse_y - ty);

        float src_x = g_viewport_x + rel_x / g_zoom;
        float src_y = g_viewport_y + rel_y / g_zoom;

        if (src_x < 0.0f) src_x = 0.0f;
        if (src_y < 0.0f) src_y = 0.0f;
        if (src_x >= tw) src_x = static_cast<float>(tw - 1);
        if (src_y >= th) src_y = static_cast<float>(th - 1);

        g_mouse_x = tx + static_cast<int>(src_x);
        g_mouse_y = ty + static_cast<int>(src_y);
    }
}
