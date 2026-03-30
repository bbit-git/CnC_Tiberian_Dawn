/**
 * ui_header.h — Bridge-native header/tab bar rendering.
 *
 * Reads legacy TabClass state and emits UI draw list commands
 * for the header background, tab buttons, and credits display.
 */

#ifndef CNC_UI_HEADER_H
#define CNC_UI_HEADER_H

/// Emit header bar UI into the draw list. Call each frame during UI layout.
void UI_Header_Emit();

#endif // CNC_UI_HEADER_H
