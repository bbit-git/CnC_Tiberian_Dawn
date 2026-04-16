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

    // Use the logical screen size so the dialog fills the full display
    // when rendered in-game (GL_UI_Render scales by ui_scale which is
    // based on logical screen, not the legacy 640x400 SeenBuff).
    int screen_w = 0, screen_h = 0;
    extern void Render_Bridge_Get_Logical_Screen_Size(int& w, int& h);
    Render_Bridge_Get_Logical_Screen_Size(screen_w, screen_h);
    if (screen_w <= 0 || screen_h <= 0) {
        screen_w = SeenBuff.Get_Width();
        screen_h = SeenBuff.Get_Height();
    }
    if (screen_w <= 0) screen_w = 640;
    if (screen_h <= 0) screen_h = 400;

    // Scale menu metrics relative to a 400px-high reference so the dialog
    // appears the same physical proportion regardless of logical resolution.
    float menu_scale = static_cast<float>(screen_h) / 400.0f;
    if (menu_scale < 1.0f) menu_scale = 1.0f;

    // Font metrics
    UIFontID fnt = UI_FONT_SDF_DEFAULT;
    int lh = UI_Text_Line_Height(fnt);
    if (lh < 6) lh = 6;
    int btn_h = static_cast<int>((lh + 8) * menu_scale);
    int btn_spacing = static_cast<int>(4 * menu_scale);
    int padding = static_cast<int>(10 * menu_scale);

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
    int btn_w = static_cast<int>(120 * menu_scale);
    for (int i = 0; i < btn_count; i++) {
        if (!buttons[i].label) continue;
        int tw = UI_Text_Measure_Width(fnt, buttons[i].label,
                                        static_cast<int>(strlen(buttons[i].label)));
        int scaled_tw = static_cast<int>(tw * menu_scale) + padding * 2;
        if (scaled_tw > btn_w) btn_w = scaled_tw;
    }
    // Also measure resume/restate
    if (lbl.resume) {
        int tw = UI_Text_Measure_Width(fnt, lbl.resume, static_cast<int>(strlen(lbl.resume)));
        int scaled_tw = static_cast<int>(tw * menu_scale) + padding * 2;
        if (scaled_tw > btn_w) btn_w = scaled_tw;
    }

    UIDialogStyle style = UI_Default_Dialog_Style();
    style.title_height = static_cast<int>(style.title_height * menu_scale);
    style.padding = padding;
    style.text_scale = menu_scale;
    int dialog_w = btn_w + padding * 2 + static_cast<int>(8 * menu_scale);
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
    bs.text_scale = menu_scale;

    // Action buttons (centered)
    int bx = cx + (cw - btn_w) / 2;
    int by = cy;

    for (int i = 0; i < btn_count; i++) {
        if (UI_Button_Activated(UI_Button(bx, by, btn_w, btn_h, buttons[i].label, bs))) {
            Render_Bridge_UI_Set_Game_Options_Result(buttons[i].result);
        }
        by += btn_h + btn_spacing;
    }

    // Separator gap
    by += btn_spacing;

    // Resume / Restate buttons at bottom
    if (show_restate && lbl.restate) {
        int gap = static_cast<int>(4 * menu_scale);
        int half_w = (btn_w - gap) / 2;
        if (UI_Button_Activated(UI_Button(bx, by, half_w, btn_h, lbl.resume, bs)))
            Render_Bridge_UI_Set_Game_Options_Result(UI_GOPTION_RESUME);
        if (UI_Button_Activated(UI_Button(bx + half_w + gap, by, half_w, btn_h, lbl.restate, bs)))
            Render_Bridge_UI_Set_Game_Options_Result(UI_GOPTION_RESTATE);
    } else {
        if (UI_Button_Activated(UI_Button(bx, by, btn_w, btn_h, lbl.resume, bs)))
            Render_Bridge_UI_Set_Game_Options_Result(UI_GOPTION_RESUME);
    }

    // ESC dismisses (same as Resume)
    if (Key_Down(KN_ESC)) {
        Render_Bridge_UI_Set_Game_Options_Result(UI_GOPTION_RESUME);
    }
}
