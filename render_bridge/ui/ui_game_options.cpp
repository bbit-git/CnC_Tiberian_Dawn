/**
 * ui_game_options.cpp — Bridge-native game options menu emitter.
 *
 * In-game pause menu: Load, Save, Delete, Game Controls, Quit, Resume.
 * Uses SDF fonts, game-buffer coordinates (640x400).
 */

#include "ui_game_options.h"
#include "ui_dialog.h"
#include "ui_controls.h"
#include "render_bridge.h"
#include "function.h"

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
    if (!Render_Bridge_UI_Has_Active_Game_Options()) return;

    bool show_restate = false;
    bool is_multiplayer = false;
    Render_Bridge_UI_Game_Options_Get_Flags(show_restate, is_multiplayer);

    UIGameOptionsInternalLabels lbl;
    Render_Bridge_UI_Game_Options_Get_Labels(lbl);

    // Game-buffer coordinates — GL_UI_Render maps via legacy_ui_scale
    int screen_w = SeenBuff.Get_Width();
    int screen_h = SeenBuff.Get_Height();
    if (screen_w <= 0) screen_w = 640;
    if (screen_h <= 0) screen_h = 400;

    // Font metrics
    UIFontID fnt = UI_FONT_SDF_DEFAULT;
    int lh = UI_Text_Line_Height(fnt);
    if (lh < 6) lh = 6;
    int btn_h = lh + 8;
    int btn_spacing = 4;
    int padding = 10;

    // Build button list
    struct MenuButton { const char* label; int result; bool visible; };
    MenuButton buttons[8];
    int btn_count = 0;

    if (!is_multiplayer) {
        buttons[btn_count++] = {lbl.load,   UI_GOPTION_LOAD,   true};
        buttons[btn_count++] = {lbl.save,   UI_GOPTION_SAVE,   true};
        buttons[btn_count++] = {lbl.del,    UI_GOPTION_DELETE,  true};
    } else {
        buttons[btn_count++] = {lbl.resign, UI_GOPTION_DELETE,  true};
    }
    buttons[btn_count++] = {lbl.game,   UI_GOPTION_GAME,   true};
    buttons[btn_count++] = {lbl.quit,   UI_GOPTION_QUIT,   true};

    // Measure widest button text to size the dialog
    int btn_w = 120;
    for (int i = 0; i < btn_count; i++) {
        if (!buttons[i].label) continue;
        int tw = UI_Text_Measure_Width(fnt, buttons[i].label,
                                        static_cast<int>(strlen(buttons[i].label)));
        if (tw + padding * 2 > btn_w) btn_w = tw + padding * 2;
    }
    // Also measure resume/restate
    if (lbl.resume) {
        int tw = UI_Text_Measure_Width(fnt, lbl.resume, static_cast<int>(strlen(lbl.resume)));
        if (tw + padding * 2 > btn_w) btn_w = tw + padding * 2;
    }

    UIDialogStyle style = UI_Default_Dialog_Style();
    int dialog_w = btn_w + padding * 2 + 8;
    int dialog_h = style.title_height + padding
                   + btn_count * (btn_h + btn_spacing)
                   + btn_spacing * 2
                   + btn_h  // resume button
                   + padding;

    int cx, cy, cw, ch;
    UI_Dialog_Begin(screen_w, screen_h, dialog_w, dialog_h,
                    lbl.title, style, cx, cy, cw, ch);

    // Button style
    UIButtonStyle bs = UI_Default_Button_Style();
    bs.normal_r = 0;  bs.normal_g = 50;  bs.normal_b = 0;  bs.normal_a = 240;
    bs.hover_r  = 0;  bs.hover_g  = 80;  bs.hover_b  = 0;  bs.hover_a  = 255;
    bs.press_r  = 0;  bs.press_g  = 30;  bs.press_b  = 0;  bs.press_a  = 255;
    bs.text_r   = 200; bs.text_g  = 200;  bs.text_b  = 200; bs.text_a  = 255;
    bs.border_r = 0;  bs.border_g = 100; bs.border_b = 0;  bs.border_a = 200;
    bs.font = fnt;

    // Action buttons (centered)
    int bx = cx + (cw - btn_w) / 2;
    int by = cy;

    for (int i = 0; i < btn_count; i++) {
        if (UI_Button(bx, by, btn_w, btn_h, buttons[i].label, bs) == UI_BTN_PRESSED) {
            Render_Bridge_UI_Set_Game_Options_Result(buttons[i].result);
        }
        by += btn_h + btn_spacing;
    }

    // Separator gap
    by += btn_spacing;

    // Resume / Restate buttons at bottom
    if (show_restate && lbl.restate) {
        int half_w = (btn_w - 4) / 2;
        if (UI_Button(bx, by, half_w, btn_h, lbl.resume, bs) == UI_BTN_PRESSED)
            Render_Bridge_UI_Set_Game_Options_Result(UI_GOPTION_RESUME);
        if (UI_Button(bx + half_w + 4, by, half_w, btn_h, lbl.restate, bs) == UI_BTN_PRESSED)
            Render_Bridge_UI_Set_Game_Options_Result(UI_GOPTION_RESTATE);
    } else {
        if (UI_Button(bx, by, btn_w, btn_h, lbl.resume, bs) == UI_BTN_PRESSED)
            Render_Bridge_UI_Set_Game_Options_Result(UI_GOPTION_RESUME);
    }

    // ESC dismisses (same as Resume)
    if (Key_Down(KN_ESC)) {
        Render_Bridge_UI_Set_Game_Options_Result(UI_GOPTION_RESUME);
    }
}
