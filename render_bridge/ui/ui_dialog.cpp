/**
 * ui_dialog.cpp — Modal dialog and menu rendering.
 */

#include "ui_dialog.h"
#include "ui_layout.h"
#include <cstring>

UIDialogStyle UI_Default_Dialog_Style()
{
    UIDialogStyle s = {};
    s.bg_r = 25; s.bg_g = 25; s.bg_b = 25; s.bg_a = 245;
    s.title_bg_r = 0; s.title_bg_g = 60; s.title_bg_b = 0; s.title_bg_a = 255;
    s.title_text_r = 220; s.title_text_g = 220; s.title_text_b = 220; s.title_text_a = 255;
    s.border_r = 0; s.border_g = 100; s.border_b = 0; s.border_a = 255;
    s.scrim_r = 0; s.scrim_g = 0; s.scrim_b = 0; s.scrim_a = 140;
    s.title_font = UI_FONT_8PT;
    s.title_height = 14;
    s.button_w = 50;
    s.button_h = 14;
    s.padding = 4;
    return s;
}

void UI_Dialog_Begin(int screen_w, int screen_h,
                     int dialog_w, int dialog_h,
                     const char* title, const UIDialogStyle& style,
                     int& content_x, int& content_y,
                     int& content_w, int& content_h)
{
    // Full-screen scrim
    g_ui_draw_list.Fill_Rect(0, 0, screen_w, screen_h,
                             style.scrim_r, style.scrim_g,
                             style.scrim_b, style.scrim_a);

    // Block tactical input through the scrim
    UI_Input_Register_Zone(0, 0, screen_w, screen_h);

    // Center the dialog
    int dx = (screen_w - dialog_w) / 2;
    int dy = (screen_h - dialog_h) / 2;

    // Dialog background
    g_ui_draw_list.Fill_Rect(dx, dy, dialog_w, dialog_h,
                             style.bg_r, style.bg_g,
                             style.bg_b, style.bg_a);
    g_ui_draw_list.Draw_Rect(dx, dy, dialog_w, dialog_h,
                             style.border_r, style.border_g,
                             style.border_b, style.border_a);

    // Title bar
    g_ui_draw_list.Fill_Rect(dx, dy, dialog_w, style.title_height,
                             style.title_bg_r, style.title_bg_g,
                             style.title_bg_b, style.title_bg_a);
    if (title && title[0]) {
        int text_h = UI_Text_Line_Height(style.title_font);
        int ty = dy + (style.title_height - text_h) / 2;
        UI_Label(dx + style.padding, ty, title, style.title_font,
                 style.title_text_r, style.title_text_g,
                 style.title_text_b, style.title_text_a);
    }

    // Content area
    content_x = dx + style.padding;
    content_y = dy + style.title_height + style.padding;
    content_w = dialog_w - style.padding * 2;
    content_h = dialog_h - style.title_height - style.button_h
                - style.padding * 3;
}

UIDialogResult UI_Dialog_End(int dialog_x, int dialog_y,
                             int dialog_w, int dialog_h,
                             const char* ok_label,
                             const char* cancel_label,
                             const UIDialogStyle& style)
{
    UIDialogResult result = UI_DIALOG_NONE;

    int btn_y = dialog_y + dialog_h - style.button_h - style.padding;

    UIButtonStyle bs = UI_Default_Button_Style();
    bs.normal_r = 0;  bs.normal_g = 50; bs.normal_b = 0;
    bs.hover_r = 0;   bs.hover_g = 80;  bs.hover_b = 0;
    bs.press_r = 0;   bs.press_g = 30;  bs.press_b = 0;
    bs.text_r = 200;  bs.text_g = 200;  bs.text_b = 200;
    bs.border_r = 0;  bs.border_g = 100; bs.border_b = 0;
    bs.font = style.title_font;

    if (ok_label && cancel_label) {
        // Two buttons centered
        int total_w = style.button_w * 2 + style.padding;
        int bx = dialog_x + (dialog_w - total_w) / 2;

        UIButtonState ok_state = UI_Button(bx, btn_y,
                                           style.button_w, style.button_h,
                                           ok_label, bs);
        UIButtonState cancel_state = UI_Button(bx + style.button_w + style.padding,
                                               btn_y, style.button_w,
                                               style.button_h, cancel_label, bs);
        if (ok_state == UI_BTN_PRESSED) result = UI_DIALOG_OK;
        if (cancel_state == UI_BTN_PRESSED) result = UI_DIALOG_CANCEL;
    } else if (ok_label) {
        int bx = dialog_x + (dialog_w - style.button_w) / 2;
        UIButtonState ok_state = UI_Button(bx, btn_y,
                                           style.button_w, style.button_h,
                                           ok_label, bs);
        if (ok_state == UI_BTN_PRESSED) result = UI_DIALOG_OK;
    } else if (cancel_label) {
        int bx = dialog_x + (dialog_w - style.button_w) / 2;
        UIButtonState cancel_state = UI_Button(bx, btn_y,
                                               style.button_w, style.button_h,
                                               cancel_label, bs);
        if (cancel_state == UI_BTN_PRESSED) result = UI_DIALOG_CANCEL;
    }

    return result;
}

UIDialogResult UI_Confirm_Dialog(int screen_w, int screen_h,
                                  const char* title, const char* message,
                                  const char* ok_label,
                                  const char* cancel_label)
{
    UIDialogStyle style = UI_Default_Dialog_Style();
    int dialog_w = 180;
    int dialog_h = 80;

    int cx, cy, cw, ch;
    UI_Dialog_Begin(screen_w, screen_h, dialog_w, dialog_h,
                    title, style, cx, cy, cw, ch);

    if (message && message[0]) {
        UI_Label(cx, cy, message, UI_FONT_6PT, 180, 180, 180, 255,
                 UI_ALIGN_CENTER, cw);
    }

    int dx = (screen_w - dialog_w) / 2;
    int dy = (screen_h - dialog_h) / 2;
    return UI_Dialog_End(dx, dy, dialog_w, dialog_h,
                         ok_label, cancel_label, style);
}

UIDialogResult UI_Message_Box(int screen_w, int screen_h,
                               const char* title, const char* message)
{
    return UI_Confirm_Dialog(screen_w, screen_h, title, message, "OK", nullptr);
}
