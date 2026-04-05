/**
 * ui_input.h — UI hit-testing and input capture for bridge-native UI.
 *
 * Tracks rectangular hit zones, determines hover/press state,
 * and manages pointer capture so UI events do not leak into tactical input.
 */

#ifndef CNC_UI_INPUT_H
#define CNC_UI_INPUT_H

#include <cstdint>

/// Opaque hit zone handle.
using UIHitZoneID = int;
static constexpr UIHitZoneID UI_HIT_NONE = -1;

/// Maximum number of registered hit zones per frame.
static constexpr int UI_MAX_HIT_ZONES = 128;

/// Register a rectangular hit zone. Returns a zone id.
/// Call each frame during UI layout — zones are cleared at frame start.
UIHitZoneID UI_Input_Register_Zone(int x, int y, int w, int h);

/// Perform hit test at the given screen position.
/// Returns the topmost zone id, or UI_HIT_NONE.
UIHitZoneID UI_Input_Hit_Test(int screen_x, int screen_y);

/// Get the currently hovered zone (updated by UI_Input_Update).
UIHitZoneID UI_Input_Get_Hovered();

/// Get the currently pressed zone (mouse down on a zone).
UIHitZoneID UI_Input_Get_Pressed();

/// Returns true if the UI owns the current pointer event.
/// When true, tactical input should be suppressed.
bool UI_Input_Has_Capture();

/// Returns true on the single frame the left button transitioned from
/// released to pressed while hovering over the given zone.
bool UI_Input_Was_Clicked(UIHitZoneID zone);

/// Update input state. Call once per frame with current mouse state.
/// left_down: left mouse button is currently held.
void UI_Input_Update(int screen_x, int screen_y, bool left_down);

/// Feed mouse wheel delta into the UI input system.
/// Called from the event pump before UI_Input_Update.
void UI_Input_Set_Scroll_Delta(float delta);

/// Get accumulated scroll delta and reset it.
/// Returns positive for scroll-up, negative for scroll-down.
float UI_Input_Consume_Scroll_Delta();

/// Clear all zones and reset state. Call at UI frame start.
void UI_Input_Begin_Frame();

/// End frame. Finalizes capture state.
void UI_Input_End_Frame();

/// Push a raw key code into the text-input key buffer.
/// Call before UI_Input_Begin_Frame to pre-populate keys from the engine
/// key queue without re-entering the event pump.
void UI_Input_Push_Key(unsigned short key);

/// Pop one key from the text-input key buffer.
/// Returns 0 when the buffer is empty.
unsigned short UI_Input_Pop_Key();

#endif // CNC_UI_INPUT_H
