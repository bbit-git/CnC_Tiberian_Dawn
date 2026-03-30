/**
 * ui_tooltip.h — Tooltip and transient status overlay system.
 *
 * Provides cursor-following tooltips and timed status messages
 * rendered via the UI draw list. Tooltips auto-position near
 * the cursor and stay within screen bounds.
 */

#ifndef CNC_UI_TOOLTIP_H
#define CNC_UI_TOOLTIP_H

#include "ui_text.h"
#include <cstdint>

/// Show a tooltip near the cursor. Only one tooltip active at a time.
/// text: tooltip content. Copied internally — pointer need not persist.
/// Tooltip remains visible until UI_Tooltip_Hide() or a new Show call.
void UI_Tooltip_Show(const char* text, UIFontID font = UI_FONT_6PT);

/// Hide the current tooltip.
void UI_Tooltip_Hide();

/// Push a timed status message (bottom-center of screen).
/// Duration in milliseconds. Messages queue and display sequentially.
void UI_Status_Push(const char* text, int duration_ms = 3000,
                    uint8_t r = 200, uint8_t g = 200, uint8_t b = 200);

/// Emit tooltip and status overlays into the UI draw list.
/// Call each frame during UI layout.
/// mouse_x, mouse_y: current mouse position in game-buffer pixels.
/// screen_w, screen_h: game buffer dimensions.
void UI_Tooltip_Emit(int mouse_x, int mouse_y, int screen_w, int screen_h);

#endif // CNC_UI_TOOLTIP_H
