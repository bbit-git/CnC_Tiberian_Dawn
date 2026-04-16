/**
 * ui_radar.h — Bridge-native radar minimap rendering.
 *
 * Reads legacy RadarClass / MapClass cell state each frame, builds an
 * RGBA pixel buffer (one pixel per map cell), and emits it as a
 * textured quad via the UI draw list.  Also draws a viewport rectangle
 * and handles click-to-move input on the radar area.
 */

#ifndef CNC_UI_RADAR_H
#define CNC_UI_RADAR_H

typedef bool (*UI_Radar_Frame_Rect_Hook)(int logical_w, int logical_h,
                                         int& x, int& y, int& w, int& h);

/// Emit the radar minimap into the UI draw list. Call each frame during UI layout.
void UI_Radar_Emit();

/// Resolve the current radar frame rect in logical/HD pixels. Prefers any
/// debug hook, then the HD sidebar layout JSON, then the legacy Map.Rad*
/// fields scaled into logical space.
bool UI_Radar_Resolve_Frame_Rect(int logical_w, int logical_h,
                                 int& x, int& y, int& w, int& h);

/// Install an optional debug hook that can override the effective radar frame
/// rect before the built-in HD layout / legacy fallback logic runs.
void UI_Radar_Debug_Set_Frame_Rect_Hook(UI_Radar_Frame_Rect_Hook hook);

/// Free radar pixel buffer resources (call at shutdown).
void UI_Radar_Shutdown();

#endif // CNC_UI_RADAR_H
