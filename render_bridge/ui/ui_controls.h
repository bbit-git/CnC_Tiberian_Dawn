/**
 * ui_controls.h — Bridge-native UI control primitives.
 *
 * Flat, explicit controls that emit draw commands into the UI draw list
 * and register hit zones for input. No inheritance hierarchy.
 *
 * Usage: call the Emit_* functions during UI layout each frame.
 * They write to g_ui_draw_list and register zones via ui_input.
 */

#ifndef CNC_UI_CONTROLS_H
#define CNC_UI_CONTROLS_H

#include "ui_draw_list.h"
#include "ui_input.h"
#include "ui_text.h"
#include <cstdint>

/// Text alignment within a control.
enum UIAlign : uint8_t {
    UI_ALIGN_LEFT   = 0,
    UI_ALIGN_CENTER = 1,
    UI_ALIGN_RIGHT  = 2,
};

/// Button visual state (returned by Emit_Button).
enum UIButtonState : uint8_t {
    UI_BTN_NORMAL  = 0,
    UI_BTN_HOVERED = 1,
    UI_BTN_PRESSED = 2,  // pressed and still held inside the button
    UI_BTN_CLICKED = 3,  // released inside the same button (action trigger)
};

inline bool UI_Button_Activated(UIButtonState state)
{
    return state == UI_BTN_CLICKED;
}

/// Panel style.
struct UIPanelStyle {
    uint8_t bg_r, bg_g, bg_b, bg_a;
    uint8_t border_r, border_g, border_b, border_a;
    bool    has_border;
};

/// Button style.
struct UIButtonStyle {
    uint8_t normal_r, normal_g, normal_b, normal_a;
    uint8_t hover_r,  hover_g,  hover_b,  hover_a;
    uint8_t press_r,  press_g,  press_b,  press_a;
    uint8_t text_r,   text_g,   text_b,   text_a;
    uint8_t border_r, border_g, border_b, border_a;
    UIFontID font;
    float text_scale;  // extra text scale multiplier (1.0 = default)
};

/// Default styles.
UIPanelStyle  UI_Default_Panel_Style();
UIButtonStyle UI_Default_Button_Style();

/// Emit a panel (background + optional border + clip push).
/// Call UI_Panel_End() after emitting child controls.
void UI_Panel_Begin(int x, int y, int w, int h, const UIPanelStyle& style);
void UI_Panel_End();

/// Emit a text label.
void UI_Label(int x, int y, const char* text, UIFontID font,
              uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255,
              UIAlign align = UI_ALIGN_LEFT, int max_width = 0);

/// Emit a clickable button. Returns the button state this frame.
UIButtonState UI_Button(int x, int y, int w, int h,
                        const char* label, const UIButtonStyle& style);

/// Emit an icon/image quad.
void UI_Icon(int x, int y, int w, int h,
             const uint8_t* pixels, int src_w, int src_h);

// --- Phase 4: Interactive controls ---

/// Slider style.
struct UISliderStyle {
    uint8_t track_r, track_g, track_b, track_a;
    uint8_t thumb_r, thumb_g, thumb_b, thumb_a;
    uint8_t hover_r, hover_g, hover_b, hover_a;
    int     thumb_w;   // thumb width in pixels
    int     track_h;   // track height in pixels (slider total height)
};

UISliderStyle UI_Default_Slider_Style();

/// Emit a horizontal slider. Returns the new value (0.0 - 1.0).
/// value: current normalized position (0.0 = left, 1.0 = right).
/// The caller should store and pass back the returned value each frame.
float UI_Slider(int x, int y, int w, int h, float value,
                const UISliderStyle& style);

/// Scrollbar style.
struct UIScrollbarStyle {
    uint8_t track_r, track_g, track_b, track_a;
    uint8_t thumb_r, thumb_g, thumb_b, thumb_a;
    uint8_t hover_r, hover_g, hover_b, hover_a;
    int     min_thumb_h;  // minimum thumb height in pixels
};

UIScrollbarStyle UI_Default_Scrollbar_Style();

/// Emit a vertical scrollbar. Returns the new scroll position (0.0 - 1.0).
/// value: current normalized position (0.0 = top, 1.0 = bottom).
/// visible_frac: fraction of content visible (0.0 - 1.0), controls thumb size.
float UI_Scrollbar(int x, int y, int w, int h, float value,
                   float visible_frac, const UIScrollbarStyle& style);

/// List box style.
struct UIListBoxStyle {
    uint8_t bg_r, bg_g, bg_b, bg_a;
    uint8_t border_r, border_g, border_b, border_a;
    uint8_t item_r, item_g, item_b, item_a;
    uint8_t sel_bg_r, sel_bg_g, sel_bg_b, sel_bg_a;
    uint8_t sel_text_r, sel_text_g, sel_text_b, sel_text_a;
    UIFontID font;
    int      item_height;  // height per item row
    int      scrollbar_w;  // scrollbar width (0 = no scrollbar)
};

UIListBoxStyle UI_Default_ListBox_Style();

/// Emit a list box. Returns the newly selected index, or -1 if unchanged.
/// items: array of string pointers.
/// item_count: total number of items.
/// selected: current selected index (-1 = none).
/// scroll_pos: in/out scroll position (0.0 - 1.0), updated by scrollbar.
int UI_ListBox(int x, int y, int w, int h,
               const char* const* items, int item_count,
               int selected, float& scroll_pos,
               const UIListBoxStyle& style);

/// Emit a checkbox / toggle. Returns the new checked state.
/// checked: current state.
bool UI_Checkbox(int x, int y, int size, const char* label,
                 bool checked, UIFontID font,
                 uint8_t r = 200, uint8_t g = 200, uint8_t b = 200);

/// Emit a single-line text input field.
/// buf: mutable character buffer (edited in place).
/// buf_size: buffer capacity including null terminator.
/// Returns true if the field has focus (is being edited).
bool UI_TextInput(int x, int y, int w, int h, char* buf, int buf_size,
                  UIFontID font, uint8_t r = 220, uint8_t g = 220, uint8_t b = 220);

#endif // CNC_UI_CONTROLS_H
