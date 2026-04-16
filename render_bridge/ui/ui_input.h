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

/// Stable widget identity for persistent UI state across frame rebuilds.
using UIWidgetID = std::uint32_t;
static constexpr UIWidgetID UI_WIDGET_NONE = 0;

/// Maximum number of registered hit zones per frame.
static constexpr int UI_MAX_HIT_ZONES = 128;

/// Register a rectangular hit zone. Returns a zone id.
/// Call each frame during UI layout — zones are cleared at frame start.
UIHitZoneID UI_Input_Register_Zone(int x, int y, int w, int h,
                                   UIWidgetID widget_id = UI_WIDGET_NONE);

/// Push a stable ID scope for the current UI subtree.
void UI_Input_Push_ID(int id);
void UI_Input_Push_String_ID(const char* id);
void UI_Input_Push_Pointer_ID(const void* ptr);
void UI_Input_Pop_ID();

/// Build a stable widget id from the current ID scope and a local salt.
/// Returns UI_WIDGET_NONE when there is no stable scope.
UIWidgetID UI_Input_Make_Widget_ID(std::uint32_t local_salt = 0);

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
/// released to released-inside on the same widget.
bool UI_Input_Was_Clicked(UIHitZoneID zone);

/// Returns true on the single frame the right button transitioned from
/// released to released-inside on the same widget.
bool UI_Input_Was_Right_Clicked(UIHitZoneID zone);

/// Latch a mouse-button down edge from the SDL event pump.
/// screen_x / screen_y are in bridge logical screen coordinates.
void UI_Input_On_Mouse_Button_Down(int screen_x, int screen_y, bool right_button = false);

/// Latch a mouse-button up edge from the SDL event pump.
/// screen_x / screen_y are in bridge logical screen coordinates.
void UI_Input_On_Mouse_Button_Up(int screen_x, int screen_y, bool right_button = false);

/// Update input state. Call once per frame with the current logical mouse position.
/// Mouse button held/edge state comes from the SDL event pump latches above.
void UI_Input_Update(int screen_x, int screen_y);

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
