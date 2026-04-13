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

/// Emit the radar minimap into the UI draw list. Call each frame during UI layout.
void UI_Radar_Emit();

/// Free radar pixel buffer resources (call at shutdown).
void UI_Radar_Shutdown();

#endif // CNC_UI_RADAR_H
