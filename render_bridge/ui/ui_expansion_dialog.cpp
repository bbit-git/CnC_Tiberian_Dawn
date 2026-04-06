/**
 * ui_expansion_dialog.cpp — Bridge-native expansion/bonus dialog emitter.
 */

#include "ui_expansion_dialog.h"
#include "ui_dialog.h"
#include "ui_controls.h"
#include "render_bridge.h"
#include "function.h"

extern bool Render_Bridge_UI_Expansion_Dialog_Get_Internal(
    int& mode, const char*& title, const char**& items, int& count,
    int& selected, float& scroll, int& screen_w, int& screen_h,
    const char*& ok_label, const char*& cancel_label);
extern void Render_Bridge_UI_Expansion_Dialog_Set_Result(int r, int selected, float scroll);

void UI_Expansion_Dialog_Emit()
{
    if (!Render_Bridge_UI_Has_Active_Expansion_Dialog()) return;

    int mode = 0;
    const char* title = nullptr;
    const char** items = nullptr;
    int count = 0, selected = 0;
    float scroll = 0;
    int screen_w = 0, screen_h = 0;
    const char* ok_label = nullptr;
    const char* cancel_label = nullptr;
    Render_Bridge_UI_Expansion_Dialog_Get_Internal(mode, title, items, count,
                                                    selected, scroll,
                                                    screen_w, screen_h,
                                                    ok_label, cancel_label);

    if (screen_w <= 0) screen_w = 640;
    if (screen_h <= 0) screen_h = 400;

    UIFontID fnt = UI_FONT_SDF_DEFAULT;
    int lh = UI_Text_Line_Height(fnt);
    if (lh < 6) lh = 6;
    int btn_h = lh + 8;

    UIDialogStyle style = UI_Default_Dialog_Style();
    int dialog_w = 280;
    int dialog_h = 200;

    int cx, cy, cw, ch;
    UI_Dialog_Begin(screen_w, screen_h, dialog_w, dialog_h,
                    title, style, cx, cy, cw, ch);

    // List box
    UIListBoxStyle lbs = UI_Default_ListBox_Style();
    int list_h = ch - btn_h - style.padding;
    if (list_h < 40) list_h = 40;
    int new_sel = UI_ListBox(cx, cy, cw, list_h, items, count,
                              selected, scroll, lbs);

    // OK / Cancel
    int dx = (screen_w - dialog_w) / 2;
    int dy = (screen_h - dialog_h) / 2;
    UIDialogResult result = UI_Dialog_End(dx, dy, dialog_w, dialog_h,
                                          ok_label, cancel_label, style);

    if (result == UI_DIALOG_OK) {
        Render_Bridge_UI_Expansion_Dialog_Set_Result(1, new_sel, scroll);
    } else if (result == UI_DIALOG_CANCEL) {
        Render_Bridge_UI_Expansion_Dialog_Set_Result(2, new_sel, scroll);
    } else {
        Render_Bridge_UI_Expansion_Dialog_Set_Result(0, new_sel, scroll);
    }
}
