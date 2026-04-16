/**
 * ui_sidebar.h — Bridge-native sidebar chrome + cameo icon rendering.
 *
 * Reads legacy SidebarClass state and emits UI draw list commands
 * for the sidebar background, column frames, production slot outlines,
 * scroll indicators, production labels, and cameo icon textures.
 */

#ifndef CNC_UI_SIDEBAR_H
#define CNC_UI_SIDEBAR_H

#include <cstdint>

/// Emit sidebar chrome UI into the draw list. Call each frame during UI layout.
void UI_Sidebar_Emit();

/// Invalidate the legacy cameo RGBA cache (call on palette change or scene transition).
void UI_Sidebar_Cameo_Invalidate();

/// Free all cameo cache resources (call at shutdown).
void UI_Sidebar_Cameo_Shutdown();

// ---------------------------------------------------------------------------
// Debug component mask
// ---------------------------------------------------------------------------
//
// Bitmask toggles for individual sidebar components. Default mask = ~0u, so
// production is unaffected. The HD sidebar preview tool flips bits via the
// ImGui panel to isolate components when auditing sizing/layout.

enum UISidebarComponent : uint32_t {
    COMP_SIDEBAR_FRAME    = 1u << 0,   ///< Background frame (atlas or legacy tile)
    COMP_TOP_BAR          = 1u << 1,   ///< Top strip background
    COMP_CREDITS          = 1u << 2,   ///< $nnn centred text
    COMP_MENU_BTN         = 1u << 3,   ///< ≡ options button
    COMP_MAP_BTN          = 1u << 4,   ///< Radar/zoom toggle (top strip)
    COMP_MODE_TABS        = 1u << 5,   ///< Repair / Sell / Build
    COMP_CATEGORY_ROW     = 1u << 6,   ///< Infantry / Vehicle / Structure / Support
    COMP_POWER_BAR        = 1u << 7,
    COMP_GRID_BG          = 1u << 8,   ///< Slot backgrounds / borders
    COMP_CAMEOS           = 1u << 9,   ///< Icon textures in grid
    COMP_CLOCK_OVERLAY    = 1u << 10,  ///< Production-progress clock sweep
    COMP_PIPS             = 1u << 11,  ///< Queue-count pip indicators
    COMP_QUEUE_COUNT      = 1u << 12,  ///< Numeric queue-count label
    COMP_SCROLL_ARROWS    = 1u << 13,
    COMP_LEGACY_BOTTOM    = 1u << 14,  ///< Legacy-only Repair/Sell/Map row
    COMP_RADAR            = 1u << 15,  ///< Radar frame + minimap/faction logo

    COMP_ALL              = 0xFFFFFFFFu,
};

/// Replace the current mask. Default is COMP_ALL. Bits not listed above are
/// reserved for future components.
void     UI_Sidebar_Debug_Set_Mask(uint32_t mask);
uint32_t UI_Sidebar_Debug_Get_Mask();

/// True iff the given component bit is currently enabled.
bool     UI_Sidebar_Debug_Is_On(UISidebarComponent comp);

#endif // CNC_UI_SIDEBAR_H
