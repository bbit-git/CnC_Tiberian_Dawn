/**
 * ui_game_options.cpp — Bridge-native game options menu emitter.
 */

#include "ui_game_options.h"
#include "ui_dialog.h"
#include "ui_controls.h"
#include "render_bridge.h"

struct UIGameOptionsInternalLabels {
    const char* title;
    const char* load;
    const char* save;
    const char* del;
    const char* resign;
    const char* game;
    const char* quit;
    const char* resume;
    const char* restate;
};

extern bool Render_Bridge_UI_Game_Options_Get_Flags(bool& restate, bool& mp);
extern void Render_Bridge_UI_Game_Options_Get_Labels(UIGameOptionsInternalLabels& out);
extern void Render_Bridge_UI_Set_Game_Options_Result(int r);

void UI_Game_Options_Emit()
{
    if (!Render_Bridge_UI_Has_Active_Game_Options()) {
        return;
    }

    bool show_restate = false;
    bool is_multiplayer = false;
    Render_Bridge_UI_Game_Options_Get_Flags(show_restate, is_multiplayer);

    UIGameOptionsInternalLabels lbl;
    Render_Bridge_UI_Game_Options_Get_Labels(lbl);

    extern void Render_Bridge_Get_Logical_Screen_Size(int& w, int& h);
    int screen_w = 0, screen_h = 0;
    Render_Bridge_Get_Logical_Screen_Size(screen_w, screen_h);
    if (screen_w == 0) screen_w = 640;
    if (screen_h == 0) screen_h = 400;

    UIDialogStyle style = UI_Default_Dialog_Style();
    int btn_w = 130;
    int btn_h = 14;
    int btn_spacing = 2;
    int padding = 8;

    int num_buttons = is_multiplayer ? 4 : 7;
    if (!show_restate && !is_multiplayer) num_buttons = 6;

    int dialog_w = btn_w + padding * 2 + 16;
    int dialog_h = style.title_height + padding + num_buttons * (btn_h + btn_spacing)
                   + padding + btn_h + padding;

    int cx, cy, cw, ch;
    UI_Dialog_Begin(screen_w, screen_h, dialog_w, dialog_h,
                    nullptr, style, cx, cy, cw, ch);

    int dx = (screen_w - dialog_w) / 2;
    int dy = (screen_h - dialog_h) / 2;
    UI_Label(dx, dy + 1, lbl.title, UI_FONT_8PT, 220, 220, 220, 255,
             UI_ALIGN_CENTER, dialog_w);

    UIButtonStyle bs = UI_Default_Button_Style();
    bs.normal_r = 0;  bs.normal_g = 50; bs.normal_b = 0;
    bs.hover_r = 0;   bs.hover_g = 80;  bs.hover_b = 0;
    bs.press_r = 0;   bs.press_g = 30;  bs.press_b = 0;
    bs.text_r = 200;  bs.text_g = 200;  bs.text_b = 200;
    bs.border_r = 0;  bs.border_g = 100; bs.border_b = 0;
    bs.font = UI_FONT_6PT;

    int bx = cx + (cw - btn_w) / 2;
    int by = cy;

    struct MenuButton {
        const char* label;
        int result;
        bool visible;
    };

    MenuButton buttons[] = {
        {lbl.load,   UI_GOPTION_LOAD,   !is_multiplayer},
        {lbl.save,   UI_GOPTION_SAVE,   !is_multiplayer},
        {lbl.del,    UI_GOPTION_DELETE,  !is_multiplayer},
        {lbl.resign, UI_GOPTION_DELETE,  is_multiplayer},
        {lbl.game,   UI_GOPTION_GAME,   true},
        {lbl.quit,   UI_GOPTION_QUIT,   true},
    };

    for (int i = 0; i < 6; i++) {
        if (!buttons[i].visible) continue;
        UIButtonState state = UI_Button(bx, by, btn_w, btn_h, buttons[i].label, bs);
        if (state == UI_BTN_PRESSED) {
            Render_Bridge_UI_Set_Game_Options_Result(buttons[i].result);
        }
        by += btn_h + btn_spacing;
    }

    int bottom_y = dy + dialog_h - btn_h - style.padding;
    if (show_restate) {
        int half_w = (dialog_w - padding * 2 - 4) / 2;
        UIButtonState resume_state = UI_Button(dx + padding, bottom_y,
                                                half_w, btn_h, lbl.resume, bs);
        UIButtonState restate_state = UI_Button(dx + padding + half_w + 4, bottom_y,
                                                 half_w, btn_h, lbl.restate, bs);
        if (resume_state == UI_BTN_PRESSED)
            Render_Bridge_UI_Set_Game_Options_Result(UI_GOPTION_RESUME);
        if (restate_state == UI_BTN_PRESSED)
            Render_Bridge_UI_Set_Game_Options_Result(UI_GOPTION_RESTATE);
    } else {
        UIButtonState resume_state = UI_Button(dx + (dialog_w - btn_w) / 2, bottom_y,
                                                btn_w, btn_h, lbl.resume, bs);
        if (resume_state == UI_BTN_PRESSED)
            Render_Bridge_UI_Set_Game_Options_Result(UI_GOPTION_RESUME);
    }
}
