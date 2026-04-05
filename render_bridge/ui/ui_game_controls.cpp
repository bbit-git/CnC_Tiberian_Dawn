/**
 * ui_game_controls.cpp — Bridge-native game controls dialog emitter.
 */

#include "ui_game_controls.h"
#include "ui_dialog.h"
#include "ui_controls.h"
#include "render_bridge.h"

struct UIGameControlsInternalLabels {
    const char* title;
    const char* speed;
    const char* slower;
    const char* faster;
    const char* scrollrate;
    const char* visual;
    const char* sound;
    const char* ok;
};

extern void Render_Bridge_UI_Game_Controls_Set_Result(int r, float speed, float scroll);
extern bool Render_Bridge_UI_Game_Controls_Get_Internal(
    float& speed, float& scroll, int& screen_w, int& screen_h,
    UIGameControlsInternalLabels& labels);

void UI_Game_Controls_Emit()
{
    if (!Render_Bridge_UI_Has_Active_Game_Controls()) {
        return;
    }

    float game_speed = 0, scroll_rate = 0;
    int screen_w = 0, screen_h = 0;
    UIGameControlsInternalLabels lbl;
    Render_Bridge_UI_Game_Controls_Get_Internal(game_speed, scroll_rate, screen_w, screen_h, lbl);

    UIDialogStyle style = UI_Default_Dialog_Style();
    int dialog_w = 200;
    int dialog_h = 130;

    int cx, cy, cw, ch;
    UI_Dialog_Begin(screen_w, screen_h, dialog_w, dialog_h,
                    lbl.title, style, cx, cy, cw, ch);

    int label_h = 10;
    int slider_h = 12;
    int row_spacing = 4;

    // Game Speed
    UI_Label(cx, cy, lbl.speed, UI_FONT_6PT, 180, 180, 180, 255);
    cy += label_h;
    UI_Label(cx, cy + 1, lbl.slower, UI_FONT_6PT, 140, 140, 140, 255);
    UI_Label(cx + cw - 30, cy + 1, lbl.faster, UI_FONT_6PT, 140, 140, 140, 255);
    UISliderStyle ss = UI_Default_Slider_Style();
    ss.thumb_r = 0; ss.thumb_g = 100; ss.thumb_b = 0;
    ss.hover_r = 0; ss.hover_g = 140; ss.hover_b = 0;
    game_speed = UI_Slider(cx + 32, cy, cw - 64, slider_h, game_speed, ss);
    cy += slider_h + row_spacing;

    // Scroll Rate
    UI_Label(cx, cy, lbl.scrollrate, UI_FONT_6PT, 180, 180, 180, 255);
    cy += label_h;
    UI_Label(cx, cy + 1, lbl.slower, UI_FONT_6PT, 140, 140, 140, 255);
    UI_Label(cx + cw - 30, cy + 1, lbl.faster, UI_FONT_6PT, 140, 140, 140, 255);
    scroll_rate = UI_Slider(cx + 32, cy, cw - 64, slider_h, scroll_rate, ss);
    cy += slider_h + row_spacing * 2;

    // Sub-dialog buttons
    UIButtonStyle bs = UI_Default_Button_Style();
    bs.normal_r = 0;  bs.normal_g = 50; bs.normal_b = 0;
    bs.hover_r = 0;   bs.hover_g = 80;  bs.hover_b = 0;
    bs.press_r = 0;   bs.press_g = 30;  bs.press_b = 0;
    bs.text_r = 200;  bs.text_g = 200;  bs.text_b = 200;
    bs.border_r = 0;  bs.border_g = 100; bs.border_b = 0;
    bs.font = UI_FONT_6PT;

    int sub_btn_w = (cw - 4) / 2;
    UIButtonState vis_state = UI_Button(cx, cy, sub_btn_w, 14, lbl.visual, bs);
    UIButtonState snd_state = UI_Button(cx + sub_btn_w + 4, cy, sub_btn_w, 14, lbl.sound, bs);

    // Options menu button at bottom
    int dx = (screen_w - dialog_w) / 2;
    int dy = (screen_h - dialog_h) / 2;
    int btn_y = dy + dialog_h - style.button_h - style.padding;
    UIButtonState options_state = UI_Button(dx + (dialog_w - style.button_w) / 2, btn_y,
                                             style.button_w, style.button_h, lbl.ok, bs);

    if (vis_state == UI_BTN_PRESSED) {
        Render_Bridge_UI_Game_Controls_Set_Result(UI_GCTRL_VISUAL, game_speed, scroll_rate);
    } else if (snd_state == UI_BTN_PRESSED) {
        Render_Bridge_UI_Game_Controls_Set_Result(UI_GCTRL_SOUND, game_speed, scroll_rate);
    } else if (options_state == UI_BTN_PRESSED) {
        Render_Bridge_UI_Game_Controls_Set_Result(UI_GCTRL_OK, game_speed, scroll_rate);
    } else {
        Render_Bridge_UI_Game_Controls_Set_Result(UI_GCTRL_NONE, game_speed, scroll_rate);
    }
}
