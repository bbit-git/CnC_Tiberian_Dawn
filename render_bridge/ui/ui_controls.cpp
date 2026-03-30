/**
 * ui_controls.cpp — Bridge-native UI control rendering.
 */

#include "ui_controls.h"
#include <cstring>

UIPanelStyle UI_Default_Panel_Style()
{
    UIPanelStyle s = {};
    s.bg_r = 20; s.bg_g = 20; s.bg_b = 20; s.bg_a = 220;
    s.border_r = 80; s.border_g = 80; s.border_b = 80; s.border_a = 255;
    s.has_border = true;
    return s;
}

UIButtonStyle UI_Default_Button_Style()
{
    UIButtonStyle s = {};
    s.normal_r = 40;  s.normal_g = 40;  s.normal_b = 40;  s.normal_a = 255;
    s.hover_r  = 60;  s.hover_g  = 60;  s.hover_b  = 60;  s.hover_a  = 255;
    s.press_r  = 30;  s.press_g  = 30;  s.press_b  = 30;  s.press_a  = 255;
    s.text_r   = 200; s.text_g   = 200; s.text_b   = 200; s.text_a   = 255;
    s.border_r = 100; s.border_g = 100; s.border_b = 100; s.border_a = 255;
    s.font = UI_FONT_8PT;
    return s;
}

void UI_Panel_Begin(int x, int y, int w, int h, const UIPanelStyle& style)
{
    g_ui_draw_list.Fill_Rect(x, y, w, h,
                             style.bg_r, style.bg_g, style.bg_b, style.bg_a);
    if (style.has_border) {
        g_ui_draw_list.Draw_Rect(x, y, w, h,
                                 style.border_r, style.border_g,
                                 style.border_b, style.border_a);
    }
    g_ui_draw_list.Clip_Push(x, y, w, h);
}

void UI_Panel_End()
{
    g_ui_draw_list.Clip_Pop();
}

void UI_Label(int x, int y, const char* text, UIFontID font,
              uint8_t r, uint8_t g, uint8_t b, uint8_t a,
              UIAlign align, int max_width)
{
    if (!text || !text[0]) return;

    if (align != UI_ALIGN_LEFT && max_width > 0) {
        int text_w = UI_Text_Measure_Width(font, text,
                                            static_cast<int>(strlen(text)));
        if (align == UI_ALIGN_CENTER) {
            x += (max_width - text_w) / 2;
        } else if (align == UI_ALIGN_RIGHT) {
            x += max_width - text_w;
        }
    }

    g_ui_draw_list.Draw_Text(x, y, text, font, r, g, b, a);
}

UIButtonState UI_Button(int x, int y, int w, int h,
                        const char* label, const UIButtonStyle& style)
{
    UIHitZoneID zone = UI_Input_Register_Zone(x, y, w, h);
    UIHitZoneID hovered = UI_Input_Get_Hovered();
    UIHitZoneID pressed = UI_Input_Get_Pressed();

    bool is_hovered = (zone == hovered);
    bool is_pressed = (zone == pressed);

    // Determine visual state
    UIButtonState state = UI_BTN_NORMAL;
    uint8_t bg_r = style.normal_r, bg_g = style.normal_g;
    uint8_t bg_b = style.normal_b, bg_a = style.normal_a;

    if (is_pressed && is_hovered) {
        state = UI_BTN_PRESSED;
        bg_r = style.press_r; bg_g = style.press_g;
        bg_b = style.press_b; bg_a = style.press_a;
    } else if (is_pressed && !is_hovered) {
        // Pressed but dragged away — show normal
        state = UI_BTN_NORMAL;
    } else if (is_hovered) {
        state = UI_BTN_HOVERED;
        bg_r = style.hover_r; bg_g = style.hover_g;
        bg_b = style.hover_b; bg_a = style.hover_a;
    }

    // Detect click: was pressed on this zone, now released while hovered
    // (UI_Input tracks this — pressed goes to NONE on release)
    if (pressed == UI_HIT_NONE && is_hovered && !UI_Input_Has_Capture()) {
        // The input system clears pressed on release. If we were the
        // last pressed zone and mouse is still over us, that's a click.
        // This requires the caller to check state == UI_BTN_CLICKED.
    }

    // Draw background
    g_ui_draw_list.Fill_Rect(x, y, w, h, bg_r, bg_g, bg_b, bg_a);
    g_ui_draw_list.Draw_Rect(x, y, w, h,
                             style.border_r, style.border_g,
                             style.border_b, style.border_a);

    // Draw label centered
    if (label && label[0]) {
        int text_w = UI_Text_Measure_Width(style.font, label,
                                            static_cast<int>(strlen(label)));
        int text_h = UI_Text_Line_Height(style.font);
        int tx = x + (w - text_w) / 2;
        int ty = y + (h - text_h) / 2;
        g_ui_draw_list.Draw_Text(tx, ty, label, style.font,
                                 style.text_r, style.text_g,
                                 style.text_b, style.text_a);
    }

    return state;
}

void UI_Icon(int x, int y, int w, int h,
             const uint8_t* pixels, int src_w, int src_h)
{
    g_ui_draw_list.Draw_Icon(x, y, w, h, pixels, src_w, src_h);
}

// --- Phase 4: Interactive controls ---

UISliderStyle UI_Default_Slider_Style()
{
    UISliderStyle s = {};
    s.track_r = 30; s.track_g = 30; s.track_b = 30; s.track_a = 255;
    s.thumb_r = 120; s.thumb_g = 120; s.thumb_b = 120; s.thumb_a = 255;
    s.hover_r = 160; s.hover_g = 160; s.hover_b = 160; s.hover_a = 255;
    s.thumb_w = 10;
    s.track_h = 12;
    return s;
}

float UI_Slider(int x, int y, int w, int h, float value,
                const UISliderStyle& style)
{
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;

    UIHitZoneID zone = UI_Input_Register_Zone(x, y, w, h);
    UIHitZoneID pressed = UI_Input_Get_Pressed();
    UIHitZoneID hovered = UI_Input_Get_Hovered();

    // Track background
    int track_y = y + (h - style.track_h) / 2;
    g_ui_draw_list.Fill_Rect(x, track_y, w, style.track_h,
                             style.track_r, style.track_g,
                             style.track_b, style.track_a);

    // Thumb position
    int usable = w - style.thumb_w;
    if (usable < 1) usable = 1;
    int thumb_x = x + static_cast<int>(value * usable);

    bool is_active = (zone == pressed);
    bool is_hovered = (zone == hovered);

    uint8_t tr = is_active || is_hovered ? style.hover_r : style.thumb_r;
    uint8_t tg = is_active || is_hovered ? style.hover_g : style.thumb_g;
    uint8_t tb = is_active || is_hovered ? style.hover_b : style.thumb_b;

    g_ui_draw_list.Fill_Rect(thumb_x, y, style.thumb_w, h, tr, tg, tb, 255);
    g_ui_draw_list.Draw_Rect(thumb_x, y, style.thumb_w, h, 80, 80, 80, 255);

    // Drag interaction: when pressed, update value from mouse position
    if (is_active) {
        extern int g_mouse_x;
        // Mouse position is in game-buffer space
        int mx = g_mouse_x - x - style.thumb_w / 2;
        float new_val = static_cast<float>(mx) / static_cast<float>(usable);
        if (new_val < 0.0f) new_val = 0.0f;
        if (new_val > 1.0f) new_val = 1.0f;
        value = new_val;
    }

    return value;
}

UIScrollbarStyle UI_Default_Scrollbar_Style()
{
    UIScrollbarStyle s = {};
    s.track_r = 20; s.track_g = 20; s.track_b = 20; s.track_a = 255;
    s.thumb_r = 100; s.thumb_g = 100; s.thumb_b = 100; s.thumb_a = 255;
    s.hover_r = 140; s.hover_g = 140; s.hover_b = 140; s.hover_a = 255;
    s.min_thumb_h = 12;
    return s;
}

float UI_Scrollbar(int x, int y, int w, int h, float value,
                   float visible_frac, const UIScrollbarStyle& style)
{
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;
    if (visible_frac < 0.0f) visible_frac = 0.0f;
    if (visible_frac > 1.0f) visible_frac = 1.0f;

    UIHitZoneID zone = UI_Input_Register_Zone(x, y, w, h);
    UIHitZoneID pressed = UI_Input_Get_Pressed();
    UIHitZoneID hovered = UI_Input_Get_Hovered();

    // Track
    g_ui_draw_list.Fill_Rect(x, y, w, h,
                             style.track_r, style.track_g,
                             style.track_b, style.track_a);

    // Thumb size proportional to visible fraction
    int thumb_h = static_cast<int>(visible_frac * h);
    if (thumb_h < style.min_thumb_h) thumb_h = style.min_thumb_h;
    int usable = h - thumb_h;
    if (usable < 1) usable = 1;
    int thumb_y = y + static_cast<int>(value * usable);

    bool is_active = (zone == pressed);
    bool is_hovered = (zone == hovered);

    uint8_t tr = is_active || is_hovered ? style.hover_r : style.thumb_r;
    uint8_t tg = is_active || is_hovered ? style.hover_g : style.thumb_g;
    uint8_t tb = is_active || is_hovered ? style.hover_b : style.thumb_b;

    g_ui_draw_list.Fill_Rect(x, thumb_y, w, thumb_h, tr, tg, tb, 255);

    // Drag interaction
    if (is_active) {
        extern int g_mouse_y;
        int my = g_mouse_y - y - thumb_h / 2;
        float new_val = static_cast<float>(my) / static_cast<float>(usable);
        if (new_val < 0.0f) new_val = 0.0f;
        if (new_val > 1.0f) new_val = 1.0f;
        value = new_val;
    }

    return value;
}

UIListBoxStyle UI_Default_ListBox_Style()
{
    UIListBoxStyle s = {};
    s.bg_r = 15; s.bg_g = 15; s.bg_b = 15; s.bg_a = 240;
    s.border_r = 80; s.border_g = 80; s.border_b = 80; s.border_a = 255;
    s.item_r = 180; s.item_g = 180; s.item_b = 180; s.item_a = 255;
    s.sel_bg_r = 0; s.sel_bg_g = 80; s.sel_bg_b = 0; s.sel_bg_a = 255;
    s.sel_text_r = 255; s.sel_text_g = 255; s.sel_text_b = 255; s.sel_text_a = 255;
    s.font = UI_FONT_8PT;
    s.item_height = 12;
    s.scrollbar_w = 8;
    return s;
}

int UI_ListBox(int x, int y, int w, int h,
               const char* const* items, int item_count,
               int selected, float& scroll_pos,
               const UIListBoxStyle& style)
{
    // Background and border
    g_ui_draw_list.Fill_Rect(x, y, w, h,
                             style.bg_r, style.bg_g, style.bg_b, style.bg_a);
    g_ui_draw_list.Draw_Rect(x, y, w, h,
                             style.border_r, style.border_g,
                             style.border_b, style.border_a);

    int content_w = w;
    int total_h = item_count * style.item_height;
    bool needs_scroll = total_h > h && style.scrollbar_w > 0;

    if (needs_scroll) {
        content_w = w - style.scrollbar_w;
        float vis_frac = static_cast<float>(h) / static_cast<float>(total_h);
        UIScrollbarStyle sbs = UI_Default_Scrollbar_Style();
        scroll_pos = UI_Scrollbar(x + content_w, y, style.scrollbar_w, h,
                                  scroll_pos, vis_frac, sbs);
    }

    // Clip items to list area
    g_ui_draw_list.Clip_Push(x + 1, y + 1, content_w - 2, h - 2);

    int scroll_offset = 0;
    if (needs_scroll) {
        int max_scroll = total_h - h;
        scroll_offset = static_cast<int>(scroll_pos * max_scroll);
    }

    int new_selected = selected;

    for (int i = 0; i < item_count; i++) {
        int iy = y + i * style.item_height - scroll_offset;
        if (iy + style.item_height < y || iy > y + h) continue;

        bool is_sel = (i == selected);

        // Hit zone for this item
        UIHitZoneID zone = UI_Input_Register_Zone(x + 1, iy,
                                                   content_w - 2,
                                                   style.item_height);
        UIHitZoneID hovered = UI_Input_Get_Hovered();
        UIHitZoneID pressed = UI_Input_Get_Pressed();

        if (zone == pressed && zone == hovered) {
            new_selected = i;
        }

        if (is_sel) {
            g_ui_draw_list.Fill_Rect(x + 1, iy, content_w - 2,
                                     style.item_height,
                                     style.sel_bg_r, style.sel_bg_g,
                                     style.sel_bg_b, style.sel_bg_a);
        } else if (zone == hovered) {
            g_ui_draw_list.Fill_Rect(x + 1, iy, content_w - 2,
                                     style.item_height,
                                     40, 40, 40, 200);
        }

        if (items[i]) {
            uint8_t tr = is_sel ? style.sel_text_r : style.item_r;
            uint8_t tg = is_sel ? style.sel_text_g : style.item_g;
            uint8_t tb = is_sel ? style.sel_text_b : style.item_b;
            g_ui_draw_list.Draw_Text(x + 4, iy + 1, items[i], style.font,
                                     tr, tg, tb, 255);
        }
    }

    g_ui_draw_list.Clip_Pop();
    return new_selected;
}

bool UI_Checkbox(int x, int y, int size, const char* label,
                 bool checked, UIFontID font,
                 uint8_t r, uint8_t g, uint8_t b)
{
    UIHitZoneID zone = UI_Input_Register_Zone(x, y, size, size);
    UIHitZoneID hovered = UI_Input_Get_Hovered();
    UIHitZoneID pressed = UI_Input_Get_Pressed();

    bool is_hovered = (zone == hovered);
    bool is_pressed = (zone == pressed);

    // Box background
    uint8_t bg = is_hovered ? (uint8_t)50 : (uint8_t)30;
    g_ui_draw_list.Fill_Rect(x, y, size, size, bg, bg, bg, 255);
    g_ui_draw_list.Draw_Rect(x, y, size, size, r, g, b, 255);

    // Check mark
    if (checked) {
        int m = size / 4;
        g_ui_draw_list.Fill_Rect(x + m, y + m, size - m * 2, size - m * 2,
                                 r, g, b, 255);
    }

    // Toggle on press
    if (is_pressed && is_hovered) {
        checked = !checked;
    }

    // Label next to checkbox
    if (label && label[0]) {
        int text_h = UI_Text_Line_Height(font);
        int ty = y + (size - text_h) / 2;
        g_ui_draw_list.Draw_Text(x + size + 4, ty, label, font, r, g, b, 255);
    }

    return checked;
}
