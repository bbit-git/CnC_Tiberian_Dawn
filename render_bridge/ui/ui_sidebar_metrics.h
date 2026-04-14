/**
 * ui_sidebar_metrics.h — Single source of truth for HD sidebar layout sizes.
 *
 * Centralises the legacy pixel constants (top bar, tab row, category row,
 * power bar, grid spacing, etc.) that UI_Sidebar_Emit previously scattered as
 * inline scale_y_from_legacy(literal, sy) / scale_x_from_legacy calls.
 *
 * One Compute() call per frame; every emit_* helper reads named fields.
 * Allows the preview tool to show live values, and lets regression fixtures
 * assert against expected metrics without grepping for magic numbers.
 */

#ifndef CNC_UI_SIDEBAR_METRICS_H
#define CNC_UI_SIDEBAR_METRICS_H

struct SidebarMetrics {
    /* --- Inputs (populated by Compute) --------------------------------- */
    int   side_x, side_y, side_w, side_h;   ///< Sidebar rect (logical/HD px)
    float sx, sy;                            ///< Legacy→HD scale factors
    bool  hd_mode;

    /* --- Derived chrome sizes (legacy constants × sx/sy) --------------- */
    int   top_bar_h;          ///< Legacy 16
    int   top_btn_inset;      ///< Legacy 2 (unscaled, pixel-exact)
    int   mode_tab_h;         ///< Legacy 18
    int   mode_tab_gap;       ///< Legacy 3 (below tabs)
    int   category_h;         ///< Legacy 16
    int   category_gap;       ///< Legacy 2 (below category row)
    int   power_bar_w;        ///< Legacy 8
    int   power_bar_x_inset;  ///< Legacy 2 (from left edge)
    int   radar_gap_below;    ///< Legacy 13 (between radar & mode tabs)
    int   btn_row_h;          ///< Legacy 16 (legacy-only bottom row)
    int   credits_text_pad;   ///< Legacy 6 (gap between credits & tiberium)

    /* --- Grid --- */
    int   grid_cols;          ///< 3 (HD)
    int   grid_icon_pad;      ///< 2
    int   grid_text_h;        ///< Legacy 14 (label strip below cameo)
    int   grid_x_inset;       ///< Legacy 6 (between power bar & grid)
    int   grid_x_pad;         ///< Legacy 8 (grid right padding)

    /* --- Anchors (computed from above + Map.RadX/Y/W/H) ---------------- */
    int   radar_bottom_y;     ///< y at which the mode-tab row starts
    int   tab_row_y;
    int   cat_row_y;
    int   power_bar_x;
    int   power_bar_y;        ///< top of power bar
    int   power_bar_h;        ///< height = btn_y - power_bar_y - 2
    int   prod_area_x;        ///< left edge of production grid
    int   prod_area_y;        ///< top of production grid
    int   prod_area_w;
    int   prod_area_h;
    int   bottom_btn_y;       ///< legacy-only Repair/Sell/Map row

    bool valid() const { return side_w > 0; }
};

/// Populate a SidebarMetrics struct from current Render_Bridge / SeenBuff
/// / Map state. Returns a struct with side_w=0 if the sidebar rect is
/// invalid (caller should early-out).
SidebarMetrics UI_Sidebar_Compute_Metrics();

#endif // CNC_UI_SIDEBAR_METRICS_H
