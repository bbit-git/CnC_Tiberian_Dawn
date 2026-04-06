/**
 * ui_special_dialog.cpp — Bridge-native special options dialog emitter.
 */

#include "ui_special_dialog.h"
#include "ui_dialog.h"
#include "ui_controls.h"
#include "render_bridge.h"
#include "function.h"

extern bool Render_Bridge_UI_Special_Dialog_Get_Internal(
    bool options[10], const char* labels[10], int& count,
    int& screen_w, int& screen_h,
    const char*& title, const char*& ok_label, const char*& cancel_label);
extern void Render_Bridge_UI_Special_Dialog_Set_Result(int r, const bool options[10]);

void UI_Special_Dialog_Emit()
{
    if (!Render_Bridge_UI_Has_Active_Special_Dialog()) return;

    bool options[10];
    const char* labels[10];
    int count = 0, screen_w = 0, screen_h = 0;
    const char* title = nullptr;
    const char* ok_label = nullptr;
    const char* cancel_label = nullptr;
    Render_Bridge_UI_Special_Dialog_Get_Internal(options, labels, count,
                                                  screen_w, screen_h,
                                                  title, ok_label, cancel_label);

    if (screen_w <= 0) screen_w = 640;
    if (screen_h <= 0) screen_h = 400;

    UIFontID fnt = UI_FONT_SDF_DEFAULT;
    int lh = UI_Text_Line_Height(fnt);
    if (lh < 6) lh = 6;
    int row = lh + 6;
    int btn_h = lh + 8;

    UIDialogStyle style = UI_Default_Dialog_Style();
    int dialog_w = 260;
    int dialog_h = style.title_height + style.padding
                   + count * row + style.padding
                   + btn_h + style.padding;

    int cx, cy, cw, ch;
    UI_Dialog_Begin(screen_w, screen_h, dialog_w, dialog_h,
                    title, style, cx, cy, cw, ch);

    for (int i = 0; i < count; i++) {
        options[i] = UI_Checkbox(cx, cy, lh, labels[i], options[i],
                                  fnt, 180, 180, 180);
        cy += row;
    }

    int dx = (screen_w - dialog_w) / 2;
    int dy = (screen_h - dialog_h) / 2;
    UIDialogResult result = UI_Dialog_End(dx, dy, dialog_w, dialog_h,
                                          ok_label, cancel_label, style);

    if (result == UI_DIALOG_OK) {
        Render_Bridge_UI_Special_Dialog_Set_Result(1, options);
    } else if (result == UI_DIALOG_CANCEL) {
        Render_Bridge_UI_Special_Dialog_Set_Result(2, options);
    } else {
        Render_Bridge_UI_Special_Dialog_Set_Result(0, options);
    }
}
