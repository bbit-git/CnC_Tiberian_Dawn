/**
 * zoom.cpp — Zoom control for the render bridge.
 *
 * Zoom = screen pixels per game pixel.
 *   1.0 = native (each cell = 24 screen pixels, max cells visible)
 *   >1.0 = zoomed in (fewer cells, each larger on screen)
 *   max = min(screen/320, screen/200) — DOS closeup
 *
 * Panning is handled by the game's TacticalCoord scroll system.
 * The zoom viewport always shows the CENTER of the native buffer.
 * No separate viewport offset — zoom just scales the rendered content.
 */

#include "render_bridge.h"
#include "function.h"
#include "dbg.h"
#include <SDL3/SDL.h>

extern float g_scroll_zoom_delta;
extern int   g_mouse_x, g_mouse_y;
extern SDL_Window* g_window;

static constexpr float ZOOM_STEP  = 0.1f;
static constexpr float ZOOM_MIN  = 1.0f;
static constexpr float DOS_W     = 320.0f;
static constexpr float DOS_H     = 200.0f;

static float g_zoom         = 1.0f;
static float g_zoom_default = 1.0f;
static float g_zoom_max     = 4.0f;
static int   g_screen_w     = 0;
static int   g_screen_h     = 0;
static int   g_raw_mouse_x = 0;
static int   g_raw_mouse_y = 0;

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

    g_zoom_default = 1.0f;

    float mx = static_cast<float>(screen_w) / DOS_W;
    float my = static_cast<float>(screen_h) / DOS_H;
    g_zoom_max = (mx < my) ? mx : my;
    if (g_zoom_max < g_zoom_default) g_zoom_max = g_zoom_default;

    g_zoom = g_zoom_default;

    DBG("zoom: range [%.1f, %.1f, %.1f] screen %dx%d buffer %dx%d",
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

    float new_zoom = g_zoom + g_zoom * ZOOM_STEP * delta;
    if (new_zoom < ZOOM_MIN)   new_zoom = ZOOM_MIN;
    if (new_zoom > g_zoom_max) new_zoom = g_zoom_max;

    if (new_zoom != g_zoom) {
        g_zoom = new_zoom;
        DBG("zoom: %.2f", g_zoom);
    }
}

void Render_Bridge_Zoom_At(float new_zoom, int center_x, int center_y)
{
    (void)center_x; (void)center_y;
    if (new_zoom < ZOOM_MIN)   new_zoom = ZOOM_MIN;
    if (new_zoom > g_zoom_max) new_zoom = g_zoom_max;
    g_zoom = new_zoom;
}

float Render_Bridge_Get_Zoom_Level()     { return g_zoom; }
float Render_Bridge_Get_Default_Zoom()   { return g_zoom_default; }
float Render_Bridge_Get_Viewport_X()     { return 0.0f; } // no separate viewport — game scrolls
float Render_Bridge_Get_Viewport_Y()     { return 0.0f; }

void Render_Bridge_Transform_Mouse()
{
    extern bool InMainLoop;
    if (!InMainLoop || g_zoom == 1.0f) return;

    // At zoom > 1.0, the native buffer shows more cells than the zoomed
    // viewport displays. The GL UV crops to the center portion.
    // Mouse coords need to map from screen tactical area to the
    // corresponding native buffer position.
    int tx = Map.TacPixelX, ty = Map.TacPixelY;
    int tw = Lepton_To_Pixel(Map.TacLeptonWidth);
    int th = Lepton_To_Pixel(Map.TacLeptonHeight);
    if (tw <= 0 || th <= 0) return;

    extern int Render_Bridge_Get_Native_Tac_W();
    extern int Render_Bridge_Get_Native_Tac_H();
    int ntw = Render_Bridge_Get_Native_Tac_W();
    int nth = Render_Bridge_Get_Native_Tac_H();
    if (ntw <= 0 || nth <= 0) return;

    if (g_raw_mouse_x >= tx && g_raw_mouse_x < tx + tw &&
        g_raw_mouse_y >= ty && g_raw_mouse_y < ty + th) {

        // Screen fraction within tactical area
        float frac_x = static_cast<float>(g_raw_mouse_x - tx) / tw;
        float frac_y = static_cast<float>(g_raw_mouse_y - ty) / th;

        // Visible viewport within native buffer (centered)
        float vis_w = static_cast<float>(ntw) / g_zoom;
        float vis_h = static_cast<float>(nth) / g_zoom;
        float vp_x = (ntw - vis_w) * 0.5f;
        float vp_y = (nth - vis_h) * 0.5f;

        // Map to native buffer position
        float native_x = vp_x + frac_x * vis_w;
        float native_y = vp_y + frac_y * vis_h;

        // Convert back to game buffer coords (fraction of native → fraction of game tac)
        float game_frac_x = native_x / ntw;
        float game_frac_y = native_y / nth;

        g_mouse_x = tx + static_cast<int>(game_frac_x * tw);
        g_mouse_y = ty + static_cast<int>(game_frac_y * th);

        // Clamp
        if (g_mouse_x < tx) g_mouse_x = tx;
        if (g_mouse_y < ty) g_mouse_y = ty;
        if (g_mouse_x >= tx + tw) g_mouse_x = tx + tw - 1;
        if (g_mouse_y >= ty + th) g_mouse_y = ty + th - 1;
    }
}
