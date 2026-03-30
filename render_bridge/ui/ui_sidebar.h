/**
 * ui_sidebar.h — Bridge-native sidebar chrome rendering.
 *
 * Reads legacy SidebarClass state and emits UI draw list commands
 * for the sidebar background, column frames, production slot outlines,
 * scroll indicators, and basic production labels.
 */

#ifndef CNC_UI_SIDEBAR_H
#define CNC_UI_SIDEBAR_H

/// Emit sidebar chrome UI into the draw list. Call each frame during UI layout.
void UI_Sidebar_Emit();

#endif // CNC_UI_SIDEBAR_H
