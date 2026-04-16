/**
 * ui_game_controls.cpp — Bridge-native game controls dialog emitter.
 *
 * Game speed slider, scroll rate slider, Visual/Sound Controls buttons, OK.
 */

#include "ui_game_controls.h"
#include "ui_dialog.h"
#include "ui_controls.h"
#include "render_bridge.h"
#include "function.h"
#include <cstring>
#include <cstdio>

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
    if (!Render_Bridge_UI_Has_Active_Game_Controls()) return;

    float game_speed = 0, scroll_rate = 0;
    int screen_w = 0, screen_h = 0;
    UIGameControlsInternalLabels lbl;
    Render_Bridge_UI_Game_Controls_Get_Internal(game_speed, scroll_rate, screen_w, screen_h, lbl);

    if (screen_w <= 0) screen_w = 640;
    if (screen_h <= 0) screen_h = 400;

    UIFontID fnt = UI_FONT_SDF_DEFAULT;
    int lh = UI_Text_Line_Height(fnt);
    if (lh < 6) lh = 6;
    int row = lh + 4;
    int btn_h = lh + 8;
    int slider_h = lh;
    int gap = 6;

    UIDialogStyle style = UI_Default_Dialog_Style();
    int dialog_w = 260;
    int dialog_h = style.title_height + style.padding
                   + row * 2 + slider_h * 2 + gap * 4  // labels + sliders
                   + btn_h + gap                         // visual/sound buttons
                   + btn_h + style.padding;              // OK button

    int cx, cy, cw, ch;
    UI_Dialog_Begin(screen_w, screen_h, dialog_w, dialog_h,
                    lbl.title, style, cx, cy, cw, ch);

    UISliderStyle ss = UI_Default_Slider_Style();
    ss.thumb_r = 0; ss.thumb_g = 100; ss.thumb_b = 0;
    ss.hover_r = 0; ss.hover_g = 140; ss.hover_b = 0;

    // Game Speed
    {
        int spd_val = static_cast<int>(game_speed * 6.0f + 0.5f);
        if (spd_val > 6) spd_val = 6;
        char spd_buf[48];
        snprintf(spd_buf, sizeof(spd_buf), "%s: %d", lbl.speed, spd_val);
        UI_Label(cx, cy, spd_buf, fnt, 180, 180, 180, 255);
    }
    cy += row;
    int slider_x = cx;
    int slider_w = cw;
    UI_Label(cx, cy, lbl.slower, fnt, 100, 100, 100, 255);
    {
        int faster_w = UI_Text_Measure_Width(fnt, lbl.faster, static_cast<int>(strlen(lbl.faster)));
        UI_Label(cx + cw - faster_w, cy, lbl.faster, fnt, 100, 100, 100, 255);
    }
    cy += row;
    UI_Input_Push_String_ID("game_speed");
    game_speed = UI_Slider(slider_x, cy, slider_w, slider_h, game_speed, ss);
    UI_Input_Pop_ID();
    cy += slider_h + gap;

    // Scroll Rate
    {
        int scr_val = static_cast<int>(scroll_rate * 6.0f + 0.5f);
        if (scr_val > 6) scr_val = 6;
        char scr_buf[48];
        snprintf(scr_buf, sizeof(scr_buf), "%s: %d", lbl.scrollrate, scr_val);
        UI_Label(cx, cy, scr_buf, fnt, 180, 180, 180, 255);
    }
    cy += row;
    UI_Label(cx, cy, lbl.slower, fnt, 100, 100, 100, 255);
    {
        int faster_w = UI_Text_Measure_Width(fnt, lbl.faster, static_cast<int>(strlen(lbl.faster)));
        UI_Label(cx + cw - faster_w, cy, lbl.faster, fnt, 100, 100, 100, 255);
    }
    cy += row;
    UI_Input_Push_String_ID("scroll_rate");
    scroll_rate = UI_Slider(slider_x, cy, slider_w, slider_h, scroll_rate, ss);
    UI_Input_Pop_ID();
    cy += slider_h + gap;

    // Sub-dialog buttons
    UIButtonStyle bs = UI_Default_Button_Style();
    bs.normal_r = 0;  bs.normal_g = 50;  bs.normal_b = 0;  bs.normal_a = 240;
    bs.hover_r  = 0;  bs.hover_g  = 80;  bs.hover_b  = 0;  bs.hover_a  = 255;
    bs.press_r  = 0;  bs.press_g  = 30;  bs.press_b  = 0;  bs.press_a  = 255;
    bs.text_r   = 200; bs.text_g  = 200;  bs.text_b  = 200; bs.text_a  = 255;
    bs.border_r = 0;  bs.border_g = 100; bs.border_b = 0;  bs.border_a = 200;
    bs.font = fnt;

    int sub_btn_w = (cw - 6) / 2;
    UIButtonState vis_state = UI_Button(cx, cy, sub_btn_w, btn_h, lbl.visual, bs);
    UIButtonState snd_state = UI_Button(cx + sub_btn_w + 6, cy, sub_btn_w, btn_h, lbl.sound, bs);
    cy += btn_h + gap;

    // OK button (centered)
    int ok_w = UI_Text_Measure_Width(fnt, lbl.ok, static_cast<int>(strlen(lbl.ok))) + 20;
    if (ok_w < 80) ok_w = 80;
    UIButtonState ok_state = UI_Button(cx + (cw - ok_w) / 2, cy, ok_w, btn_h, lbl.ok, bs);

    if (UI_Button_Activated(vis_state)) {
        Render_Bridge_UI_Game_Controls_Set_Result(UI_GCTRL_VISUAL, game_speed, scroll_rate);
    } else if (UI_Button_Activated(snd_state)) {
        Render_Bridge_UI_Game_Controls_Set_Result(UI_GCTRL_SOUND, game_speed, scroll_rate);
    } else if (UI_Button_Activated(ok_state)) {
        Render_Bridge_UI_Game_Controls_Set_Result(UI_GCTRL_OK, game_speed, scroll_rate);
    } else {
        Render_Bridge_UI_Game_Controls_Set_Result(UI_GCTRL_NONE, game_speed, scroll_rate);
    }
}
