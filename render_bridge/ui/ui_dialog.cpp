/**
 * ui_dialog.cpp — Modal dialog and menu rendering.
 */

#include "ui_dialog.h"
#include "ui_layout.h"
#include "render_bridge.h"
#include "function.h"
#include <cstring>

UIDialogStyle UI_Default_Dialog_Style()
{
    UIDialogStyle s = {};
    s.bg_r = 25; s.bg_g = 25; s.bg_b = 25; s.bg_a = 245;
    s.title_bg_r = 0; s.title_bg_g = 60; s.title_bg_b = 0; s.title_bg_a = 255;
    s.title_text_r = 220; s.title_text_g = 220; s.title_text_b = 220; s.title_text_a = 255;
    s.border_r = 0; s.border_g = 100; s.border_b = 0; s.border_a = 255;
    s.scrim_r = 0; s.scrim_g = 0; s.scrim_b = 0; s.scrim_a = 140;
    s.title_font = UI_FONT_SDF_DEFAULT;
    s.title_height = 20;
    s.button_w = 80;
    s.button_h = 20;
    s.padding = 6;
    s.text_scale = 1.0f;
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
        float ts = (style.text_scale > 0.0f) ? style.text_scale : 1.0f;
        int text_h = static_cast<int>(UI_Text_Line_Height(style.title_font) * ts);
        int ty = dy + (style.title_height - text_h) / 2;
        g_ui_draw_list.Draw_Text(dx + style.padding, ty, title,
                                 style.title_font,
                                 style.title_text_r, style.title_text_g,
                                 style.title_text_b, style.title_text_a, ts);
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
                             const UIDialogStyle& style,
                             const char* extra_label)
{
    UIDialogResult result = UI_DIALOG_NONE;

    // ESC key dismisses the dialog (same as Cancel)
    if (cancel_label && Key_Down(KN_ESC)) {
        return UI_DIALOG_CANCEL;
    }

    int btn_y = dialog_y + dialog_h - style.button_h - style.padding;

    UIButtonStyle bs = UI_Default_Button_Style();
    bs.normal_r = 0;  bs.normal_g = 50; bs.normal_b = 0;
    bs.hover_r = 0;   bs.hover_g = 80;  bs.hover_b = 0;
    bs.press_r = 0;   bs.press_g = 30;  bs.press_b = 0;
    bs.text_r = 200;  bs.text_g = 200;  bs.text_b = 200;
    bs.border_r = 0;  bs.border_g = 100; bs.border_b = 0;
    bs.font = style.title_font;
    bs.text_scale = (style.text_scale > 0.0f) ? style.text_scale : 1.0f;

    if (ok_label && cancel_label && extra_label) {
        // Three buttons: OK | Extra (centered) | Cancel
        int btn3_w = (dialog_w - style.padding * 4) / 3;
        int bx = dialog_x + style.padding;

        UIButtonState ok_state = UI_Button(bx, btn_y,
                                           btn3_w, style.button_h,
                                           ok_label, bs);
        UIButtonState extra_state = UI_Button(bx + btn3_w + style.padding, btn_y,
                                              btn3_w, style.button_h,
                                              extra_label, bs);
        UIButtonState cancel_state = UI_Button(bx + (btn3_w + style.padding) * 2, btn_y,
                                               btn3_w, style.button_h,
                                               cancel_label, bs);
        if (UI_Button_Activated(ok_state)) result = UI_DIALOG_OK;
        if (UI_Button_Activated(extra_state)) result = UI_DIALOG_EXTRA;
        if (UI_Button_Activated(cancel_state)) result = UI_DIALOG_CANCEL;
    } else if (ok_label && cancel_label) {
        // Two buttons centered — auto-size to text width
        int ok_tw = UI_Text_Measure_Width(bs.font, ok_label,
                                           static_cast<int>(strlen(ok_label)));
        int cancel_tw = UI_Text_Measure_Width(bs.font, cancel_label,
                                               static_cast<int>(strlen(cancel_label)));
        int ok_w = (ok_tw > style.button_w - style.padding * 2)
                   ? ok_tw + style.padding * 2 : style.button_w;
        int cancel_w = (cancel_tw > style.button_w - style.padding * 2)
                       ? cancel_tw + style.padding * 2 : style.button_w;
        int btn_gap = style.padding * 2;
        int total_w = ok_w + cancel_w + btn_gap;
        int bx = dialog_x + (dialog_w - total_w) / 2;

        UIButtonState ok_state = UI_Button(bx, btn_y,
                                           ok_w, style.button_h,
                                           ok_label, bs);
        UIButtonState cancel_state = UI_Button(bx + ok_w + btn_gap,
                                               btn_y, cancel_w,
                                               style.button_h, cancel_label, bs);
        if (UI_Button_Activated(ok_state)) result = UI_DIALOG_OK;
        if (UI_Button_Activated(cancel_state)) result = UI_DIALOG_CANCEL;
    } else if (ok_label) {
        int bx = dialog_x + (dialog_w - style.button_w) / 2;
        UIButtonState ok_state = UI_Button(bx, btn_y,
                                           style.button_w, style.button_h,
                                           ok_label, bs);
        if (UI_Button_Activated(ok_state)) result = UI_DIALOG_OK;
    } else if (cancel_label) {
        int bx = dialog_x + (dialog_w - style.button_w) / 2;
        UIButtonState cancel_state = UI_Button(bx, btn_y,
                                               style.button_w, style.button_h,
                                               cancel_label, bs);
        if (UI_Button_Activated(cancel_state)) result = UI_DIALOG_CANCEL;
    }

    return result;
}

UIDialogResult UI_Confirm_Dialog(int screen_w, int screen_h,
                                  const char* title, const char* message,
                                  const char* ok_label,
                                  const char* cancel_label)
{
    if (screen_w <= 0) screen_w = 640;
    if (screen_h <= 0) screen_h = 400;

    UIFontID fnt = UI_FONT_SDF_DEFAULT;
    int lh = UI_Text_Line_Height(fnt);
    if (lh < 6) lh = 6;
    int btn_h = lh + 8;

    UIDialogStyle style = UI_Default_Dialog_Style();
    int dialog_w = 240;
    int dialog_h = style.title_height + style.padding
                   + (message && message[0] ? lh + 8 : 0)
                   + btn_h + style.padding;

    int cx, cy, cw, ch;
    UI_Dialog_Begin(screen_w, screen_h, dialog_w, dialog_h,
                    title, style, cx, cy, cw, ch);

    if (message && message[0]) {
        UI_Label(cx, cy, message, fnt, 180, 180, 180, 255,
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

void UI_Dialog_Emit()
{
    if (!Render_Bridge_UI_Has_Active_Dialog()) {
        return;
    }

    int screen_w = 0;
    int screen_h = 0;
    const char* title = nullptr;
    const char* message = nullptr;
    const char* ok_label = nullptr;
    const char* cancel_label = nullptr;
    const char* extra_label = nullptr;
    Render_Bridge_UI_Get_Dialog(screen_w, screen_h, title, message,
                                ok_label, cancel_label, extra_label);

    // Fix coordinate space
    if (screen_w <= 0) screen_w = 640;
    if (screen_h <= 0) screen_h = 400;

    UIFontID fnt = UI_FONT_SDF_DEFAULT;
    int lh = UI_Text_Line_Height(fnt);
    if (lh < 6) lh = 6;
    int btn_h = lh + 8;

    UIDialogStyle style = UI_Default_Dialog_Style();
    int dialog_w = 260;
    if (extra_label && extra_label[0]) dialog_w = 300;
    int dialog_h = style.title_height + style.padding
                   + (message && message[0] ? lh + 8 : 0)
                   + btn_h + style.padding;

    int cx = 0, cy = 0, cw = 0, ch = 0;
    UI_Dialog_Begin(screen_w, screen_h, dialog_w, dialog_h,
                    title, style, cx, cy, cw, ch);

    if (message && message[0]) {
        UI_Label(cx, cy, message, fnt, 180, 180, 180, 255,
                 UI_ALIGN_CENTER, cw);
    }

    int dx = (screen_w - dialog_w) / 2;
    int dy = (screen_h - dialog_h) / 2;
    UIDialogResult result = UI_Dialog_End(dx, dy, dialog_w, dialog_h,
                                          ok_label && ok_label[0] ? ok_label : nullptr,
                                          cancel_label && cancel_label[0] ? cancel_label : nullptr,
                                          style,
                                          extra_label && extra_label[0] ? extra_label : nullptr);
    if (result != UI_DIALOG_NONE) {
        Render_Bridge_UI_Clear_Dialog(result);
    }
}
