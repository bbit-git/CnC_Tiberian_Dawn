/**
 * gl_present_debug.cpp — Debug overlay rectangles for render bridge diagnostics.
 *
 * Each rectangle corresponds to a specific coordinate rect computed elsewhere
 * in the bridge. The label in the four corners identifies it; the source
 * location where the value is computed is noted in the comment above.
 *
 * Rectangle reference (toggle with Render_Bridge_Debug_Bars_Toggle):
 *
 * ┌─ Layout ──────────────────────────────────────────────────────────┐
 * │  BUF     Purple     SeenBuff boundary in window pixels            │
 * │                     gl_present.cpp: ctx.buffer_w * ctx.ui_scale   │
 * │  HDR     Yellow     Header strip above tactical area              │
 * │                     gl_present.cpp: ctx.tactical_game_y           │
 * │  SID     Blue       Sidebar column                                │
 * │                     zoom.cpp: Render_Bridge_Get_Sidebar_Rect      │
 * │  TAC     Green      Visible tactical screen rect (sidebar-adj)    │
 * │                     zoom.cpp: Render_Bridge_Get_Tactical_Rect     │
 * │  RTAC    Dk green   Render tactical rect (sidebar-independent)    │
 * │                     zoom.cpp: Render_Bridge_Get_Render_Tac_Rect   │
 * │  WORLD   Red        Fitted world quad (uniform-fit into RTAC)     │
 * │                     gl_present_regions.cpp: fit_sx/fit_sy/fit     │
 * └───────────────────────────────────────────────────────────────────┘
 *
 * ┌─ Clamp level 1: viewport inside native buffer ────────────────────┐
 * │  NATIVE  Cyan       Native replay buffer projected to screen      │
 * │                     zoom_tactical.cpp: get_native_tactical()       │
 * │  VP      White      Current viewport window inside native buffer  │
 * │                     zoom.cpp: g_vp_x, g_vp_y                     │
 * │  VPMAX   Orange     VP at maximum offset (native - vis)           │
 * │                     zoom.cpp: clamp_viewport()                    │
 * └───────────────────────────────────────────────────────────────────┘
 *
 * ┌─ Clamp level 2: bridge scroll range ──────────────────────────────┐
 * │  REQ     Yellow     Current g_requested_tac position + vis size   │
 * │                     zoom.cpp: g_requested_tac_x/y                 │
 * │  RMAX    Dk yellow  Max g_requested_tac + vis size                │
 * │                     zoom.cpp: clamp_requested_tac()               │
 * └───────────────────────────────────────────────────────────────────┘
 *
 * ┌─ Clamp level 3: TacticalCoord confine ────────────────────────────┐
 * │  MAP     White      Full map extent (MapCellW × MapCellH × 24)   │
 * │                     engine: Map.MapCellWidth/Height               │
 * │  (NATIVE rect also shows this — TC confined by native size        │
 * │   in display.cpp Set_Tactical_Position via Visible_Size_Leptons)  │
 * └───────────────────────────────────────────────────────────────────┘
 *
 * ┌─ Other ────────────────────────────────────────────────────────────┐
 * │  VIS     Magenta    Bridge-visible world rect in leptons          │
 * │                     zoom.cpp: Render_Bridge_Get_Visible_World_Rect│
 * │  SFILL   Blue       Shroud fill coverage (native buf at TacCoord) │
 * │                     display.cpp: Redraw_Shadow_Rects iteration    │
 * └───────────────────────────────────────────────────────────────────┘
 */

#include "gl_present_internal.h"
#include "render_bridge.h"
#include "function.h"
#include <cmath>

extern void GL_Debug_Rect(int win_w, int win_h,
                          int x, int y, int w, int h,
                          float r, float g, float b,
                          const char* label);
extern bool Render_Bridge_Debug_Bars_Enabled();
extern void Render_Bridge_Debug_HUD_GL(int win_w, int win_h);
extern void Render_Bridge_Get_Render_Tactical_Rect(int& x, int& y, int& w, int& h);
extern void Render_Bridge_Get_Effective_Clamp_Size(int& w, int& h);
extern int  Render_Bridge_Get_Native_Tac_W();
extern int  Render_Bridge_Get_Native_Tac_H();

void GL_Present_Draw_Debug_Overlays(const GLPresentFrameContext& ctx)
{
    if (!ctx.in_main_loop || !Render_Bridge_Debug_Bars_Enabled()) {
        Render_Bridge_Debug_HUD_GL(ctx.win_w, ctx.win_h);
        return;
    }

    // --- Gather bridge state ---
    float vis_w = 0.0f, vis_h = 0.0f;
    Render_Bridge_Get_Visible_Size(vis_w, vis_h);       // zoom.cpp: native_w/h / g_zoom
    float vp_x = Render_Bridge_Get_Viewport_X();        // zoom.cpp: g_vp_x
    float vp_y = Render_Bridge_Get_Viewport_Y();        // zoom.cpp: g_vp_y
    int native_w = Render_Bridge_Get_Native_Tac_W();    // zoom_tactical.cpp: get_native_tactical()
    int native_h = Render_Bridge_Get_Native_Tac_H();

    // Compute the fitted world rect — must match gl_present_regions.cpp.
    float fit_scale = 1.0f;
    int wld_x = ctx.render_tac_screen_x;
    int wld_y = ctx.render_tac_screen_y;
    int wld_w = ctx.render_tac_screen_w;
    int wld_h = ctx.render_tac_screen_h;
    if (vis_w > 0.0f && vis_h > 0.0f && wld_w > 0 && wld_h > 0) {
        float fit_sx = static_cast<float>(ctx.render_tac_screen_w) / vis_w;
        float fit_sy = static_cast<float>(ctx.render_tac_screen_h) / vis_h;
        fit_scale = (fit_sx < fit_sy) ? fit_sx : fit_sy;
        wld_w = static_cast<int>(std::round(vis_w * fit_scale));
        wld_h = static_cast<int>(std::round(vis_h * fit_scale));
        wld_x = ctx.render_tac_screen_x + (ctx.render_tac_screen_w - wld_w) / 2;
        wld_y = ctx.render_tac_screen_y + (ctx.render_tac_screen_h - wld_h) / 2;
    }

    // base_scale: fits the full native buffer into the render tactical area.
    // Constant across zoom — used for NATIVE, MAP, and other fixed-size rects.
    // fit_scale is zoom-dependent — used for VP, WORLD, REQ (visible content).
    float base_scale = 1.0f;
    if (native_w > 0 && native_h > 0) {
        float bs_x = static_cast<float>(ctx.render_tac_screen_w) / static_cast<float>(native_w);
        float bs_y = static_cast<float>(ctx.render_tac_screen_h) / static_cast<float>(native_h);
        base_scale = (bs_x < bs_y) ? bs_x : bs_y;
    }

    // Native buffer origin in screen space (using base_scale for position).
    // The viewport offset (vp_x/y) maps native pixels to the fitted world quad,
    // so use fit_scale for that offset, then anchor the native rect.
    int nat_scr_x = wld_x - static_cast<int>(std::round(vp_x * fit_scale));
    int nat_scr_y = wld_y - static_cast<int>(std::round(vp_y * fit_scale));

    // =====================================================================
    // Layout rects
    // =====================================================================

    // BUF — SeenBuff boundary.  Source: gl_present.cpp ctx.buffer_w/h
    GL_Debug_Rect(ctx.win_w, ctx.win_h,
                  ctx.offset_x, ctx.offset_y,
                  static_cast<int>(std::round(ctx.buffer_w * ctx.ui_scale)),
                  static_cast<int>(std::round(ctx.buffer_h * ctx.ui_scale)),
                  0.65f, 0.20f, 1.00f, "BUF");

    // HDR — Header strip.  Source: gl_present.cpp ctx.tactical_game_y
    int hdr_h = static_cast<int>(std::round(ctx.tactical_game_y * ctx.ui_scale));
    if (hdr_h > 0)
        GL_Debug_Rect(ctx.win_w, ctx.win_h,
                      ctx.offset_x, ctx.offset_y,
                      static_cast<int>(std::round(ctx.buffer_w * ctx.ui_scale)), hdr_h,
                      1.00f, 1.00f, 0.10f, "HDR");

    // SID — Sidebar.  Source: zoom.cpp Render_Bridge_Get_Sidebar_Rect
    if (ctx.side_game_w > 0)
        GL_Debug_Rect(ctx.win_w, ctx.win_h,
                      ctx.offset_x + static_cast<int>(std::round(ctx.side_game_x * ctx.ui_scale)),
                      ctx.offset_y,
                      static_cast<int>(std::round(ctx.side_game_w * ctx.ui_scale)),
                      static_cast<int>(std::round(ctx.buffer_h * ctx.ui_scale)),
                      0.10f, 0.45f, 1.00f, "SID");

    // TAC — Visible tactical rect.  Source: zoom.cpp Render_Bridge_Get_Tactical_Rect
    GL_Debug_Rect(ctx.win_w, ctx.win_h,
                  ctx.tactical_screen_x, ctx.tactical_screen_y,
                  ctx.tactical_screen_w, ctx.tactical_screen_h,
                  0.10f, 1.00f, 0.10f, "TAC");

    // RTAC — Render tactical rect.  Source: zoom.cpp Render_Bridge_Get_Render_Tactical_Rect
    GL_Debug_Rect(ctx.win_w, ctx.win_h,
                  ctx.render_tac_screen_x, ctx.render_tac_screen_y,
                  ctx.render_tac_screen_w, ctx.render_tac_screen_h,
                  0.10f, 0.60f, 0.10f, "RTAC");

    // WORLD — Fitted world quad.  Source: gl_present_regions.cpp uniform-fit
    GL_Debug_Rect(ctx.win_w, ctx.win_h, wld_x, wld_y, wld_w, wld_h,
                  1.00f, 0.20f, 0.20f, "WORLD");

    // =====================================================================
    // Clamp level 1 — viewport inside native buffer
    //   zoom.cpp: clamp_viewport() caps g_vp to [0, native - vis]
    // =====================================================================
    if (native_w > 0 && native_h > 0 && fit_scale > 0.0f) {
        // NATIVE — Full native replay buffer.  Source: zoom_tactical.cpp get_native_tactical
        // Uses base_scale (fixed) so the rect doesn't grow with zoom.
        int nat_w_scr = static_cast<int>(std::round(native_w * base_scale));
        int nat_h_scr = static_cast<int>(std::round(native_h * base_scale));
        int nat_cx = ctx.render_tac_screen_x + (ctx.render_tac_screen_w - nat_w_scr) / 2;
        int nat_cy = ctx.render_tac_screen_y + (ctx.render_tac_screen_h - nat_h_scr) / 2;
        GL_Debug_Rect(ctx.win_w, ctx.win_h, nat_cx, nat_cy, nat_w_scr, nat_h_scr,
                      0.10f, 1.00f, 1.00f, "NATIVE");

        float vp_max_x = (native_w > vis_w) ? native_w - vis_w : 0.0f;
        float vp_max_y = (native_h > vis_h) ? native_h - vis_h : 0.0f;
        // VP size in the native rect uses base_scale (vis_w native pixels → screen)
        int vis_base_w = static_cast<int>(std::round(vis_w * base_scale));
        int vis_base_h = static_cast<int>(std::round(vis_h * base_scale));

        // VPMAX — VP at max offset.  Source: zoom.cpp clamp_viewport()
        GL_Debug_Rect(ctx.win_w, ctx.win_h,
                      nat_cx + static_cast<int>(std::round(vp_max_x * base_scale)),
                      nat_cy + static_cast<int>(std::round(vp_max_y * base_scale)),
                      vis_base_w, vis_base_h,
                      1.00f, 0.50f, 0.00f, "VPMAX");

        // VP — Current viewport.  Source: zoom.cpp g_vp_x/y
        GL_Debug_Rect(ctx.win_w, ctx.win_h,
                      nat_cx + static_cast<int>(std::round(vp_x * base_scale)),
                      nat_cy + static_cast<int>(std::round(vp_y * base_scale)),
                      vis_base_w, vis_base_h,
                      1.00f, 1.00f, 1.00f, "VP");
    }

    // =====================================================================
    // Clamp level 2 — bridge scroll range
    //   zoom.cpp: clamp_requested_tac() caps g_requested_tac to
    //   [0, map_w - vis_w_leptons]  (via get_visible_window_leptons)
    // =====================================================================
    {
        int clamp_max_x = 0, clamp_max_y = 0;
        Render_Bridge_Get_Clamp_Ranges(clamp_max_x, clamp_max_y); // zoom.cpp
        int req_x = 0, req_y = 0;
        Render_Bridge_Get_Requested_Tactical_Position(req_x, req_y); // zoom.cpp: g_requested_tac

        if (native_w > 0 && native_h > 0 && base_scale > 0.0f) {
            int nat_cx2 = ctx.render_tac_screen_x + (ctx.render_tac_screen_w -
                          static_cast<int>(std::round(native_w * base_scale))) / 2;
            int nat_cy2 = ctx.render_tac_screen_y + (ctx.render_tac_screen_h -
                          static_cast<int>(std::round(native_h * base_scale))) / 2;
            int vis_base_w = static_cast<int>(std::round(vis_w * base_scale));
            int vis_base_h = static_cast<int>(std::round(vis_h * base_scale));

            // REQ — Current requested position.  Source: zoom.cpp g_requested_tac_x/y
            GL_Debug_Rect(ctx.win_w, ctx.win_h,
                          nat_cx2 + static_cast<int>(std::round(Lepton_To_Pixel(req_x) * base_scale)),
                          nat_cy2 + static_cast<int>(std::round(Lepton_To_Pixel(req_y) * base_scale)),
                          vis_base_w, vis_base_h,
                          1.00f, 1.00f, 0.10f, "REQ");

            // RMAX — Max requested position.  Source: zoom.cpp clamp_requested_tac
            GL_Debug_Rect(ctx.win_w, ctx.win_h,
                          nat_cx2 + static_cast<int>(std::round(Lepton_To_Pixel(clamp_max_x) * base_scale)),
                          nat_cy2 + static_cast<int>(std::round(Lepton_To_Pixel(clamp_max_y) * base_scale)),
                          vis_base_w, vis_base_h,
                          0.70f, 0.70f, 0.00f, "RMAX");
        }
    }

    // =====================================================================
    // Clamp level 3 — TacticalCoord confine
    //   display.cpp Set_Tactical_Position: Confine_Rect with
    //   Render_Bridge_Get_Visible_Size_Leptons (= native buffer size)
    //   → shown by NATIVE rect above (TC can go 0..map-native per axis)
    // =====================================================================

    // MAP — Full map extent.  Source: engine Map.MapCellWidth/Height
    // Uses base_scale (fixed size, doesn't grow with zoom).
    if (native_w > 0 && native_h > 0 && base_scale > 0.0f) {
        int nat_cx2 = ctx.render_tac_screen_x + (ctx.render_tac_screen_w -
                      static_cast<int>(std::round(native_w * base_scale))) / 2;
        int nat_cy2 = ctx.render_tac_screen_y + (ctx.render_tac_screen_h -
                      static_cast<int>(std::round(native_h * base_scale))) / 2;
        int tc_off_x = Lepton_To_Pixel(Coord_X(Map.TacticalCoord) - Cell_To_Lepton(Map.MapCellX));
        int tc_off_y = Lepton_To_Pixel(Coord_Y(Map.TacticalCoord) - Cell_To_Lepton(Map.MapCellY));
        GL_Debug_Rect(ctx.win_w, ctx.win_h,
                      nat_cx2 - static_cast<int>(std::round(tc_off_x * base_scale)),
                      nat_cy2 - static_cast<int>(std::round(tc_off_y * base_scale)),
                      static_cast<int>(std::round(Map.MapCellWidth * CELL_PIXEL_W * base_scale)),
                      static_cast<int>(std::round(Map.MapCellHeight * CELL_PIXEL_W * base_scale)),
                      1.00f, 1.00f, 1.00f, "MAP");

        // SFILL — Shroud fill coverage: native buffer at TacticalCoord origin.
        // After Bug 1/4 fixes this matches the terrain iteration exactly and
        // should align with the NATIVE rect. Source: display.cpp Redraw_Shadow_Rects.
        GL_Debug_Rect(ctx.win_w, ctx.win_h,
                      nat_cx2,
                      nat_cy2,
                      static_cast<int>(std::round(native_w * base_scale)),
                      static_cast<int>(std::round(native_h * base_scale)),
                      0.20f, 0.40f, 1.00f, "SFILL");
    }

    // VIS — Bridge visible world rect.  Source: zoom.cpp Render_Bridge_Get_Visible_World_Rect
    {
        int vw_x = 0, vw_y = 0, vw_w = 0, vw_h = 0;
        Render_Bridge_Get_Visible_World_Rect(vw_x, vw_y, vw_w, vw_h);
        if (vw_w > 0 && vw_h > 0)
            GL_Debug_Rect(ctx.win_w, ctx.win_h, wld_x, wld_y,
                          static_cast<int>(std::round(Lepton_To_Pixel(vw_w) * fit_scale)),
                          static_cast<int>(std::round(Lepton_To_Pixel(vw_h) * fit_scale)),
                          1.00f, 0.15f, 0.95f, "VIS");
    }

    GL_Present_Bind_Palette_Program();
    Render_Bridge_Debug_HUD_GL(ctx.win_w, ctx.win_h);
}
