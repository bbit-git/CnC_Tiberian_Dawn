/**
 * ui_load_dialog.cpp — Bridge-native load/save/delete dialog emitter.
 */

#include "ui_load_dialog.h"
#include "ui_dialog.h"
#include "ui_controls.h"
#include "render_bridge.h"

extern bool Render_Bridge_UI_Load_Dialog_Get_State(
    int& mode, const char**& items, int& count, int& selected,
    float& scroll_pos, const char*& edit_text, int& screen_w, int& screen_h,
    const char*& title, const char*& action_label, const char*& cancel_label);
extern void Render_Bridge_UI_Load_Dialog_Set_Result(int result, int selected, float scroll_pos);

void UI_Load_Dialog_Emit()
{
    if (!Render_Bridge_UI_Has_Active_Load_Dialog()) {
        return;
    }

    int mode = 0;
    const char** items = nullptr;
    int count = 0, selected = 0;
    float scroll_pos = 0.0f;
    const char* edit_text = nullptr;
    int screen_w = 0, screen_h = 0;
    const char* title = nullptr;
    const char* action_label = nullptr;
    const char* cancel_label = nullptr;
    Render_Bridge_UI_Load_Dialog_Get_State(mode, items, count, selected,
                                            scroll_pos, edit_text, screen_w, screen_h,
                                            title, action_label, cancel_label);

    UIDialogStyle style = UI_Default_Dialog_Style();
    int dialog_w = 220;
    int dialog_h = 160;

    int cx, cy, cw, ch;
    UI_Dialog_Begin(screen_w, screen_h, dialog_w, dialog_h,
                    title, style, cx, cy, cw, ch);

    // List box
    UIListBoxStyle lbs = UI_Default_ListBox_Style();
    int list_h = ch - style.button_h - style.padding;
    if (mode == 1) list_h -= 14; // room for edit field

    int new_sel = UI_ListBox(cx, cy, cw, list_h, items, count,
                              selected, scroll_pos, lbs);

    // Edit field for save mode
    if (mode == 1 && edit_text) {
        int ey = cy + list_h + 2;
        UI_Label(cx, ey, edit_text, UI_FONT_6PT, 180, 180, 180, 255);
    }

    // Buttons
    int dx = (screen_w - dialog_w) / 2;
    int dy = (screen_h - dialog_h) / 2;
    UIDialogResult result = UI_Dialog_End(dx, dy, dialog_w, dialog_h,
                                          action_label, cancel_label, style);
    if (result == UI_DIALOG_OK) {
        Render_Bridge_UI_Load_Dialog_Set_Result(1, new_sel, scroll_pos);
    } else if (result == UI_DIALOG_CANCEL) {
        Render_Bridge_UI_Load_Dialog_Set_Result(2, new_sel, scroll_pos);
    } else {
        Render_Bridge_UI_Load_Dialog_Set_Result(0, new_sel, scroll_pos);
    }
}
