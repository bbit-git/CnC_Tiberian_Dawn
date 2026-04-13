/**
 * ui_sidebar.h — Bridge-native sidebar chrome + cameo icon rendering.
 *
 * Reads legacy SidebarClass state and emits UI draw list commands
 * for the sidebar background, column frames, production slot outlines,
 * scroll indicators, production labels, and cameo icon textures.
 */

#ifndef CNC_UI_SIDEBAR_H
#define CNC_UI_SIDEBAR_H

/// Emit sidebar chrome UI into the draw list. Call each frame during UI layout.
void UI_Sidebar_Emit();

/// Invalidate the cameo RGBA cache (call on palette change or scene transition).
void UI_Sidebar_Cameo_Invalidate();

/// Free all cameo cache resources (call at shutdown).
void UI_Sidebar_Cameo_Shutdown();

#endif // CNC_UI_SIDEBAR_H
