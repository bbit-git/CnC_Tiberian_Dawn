/**
 * ui_visual_controls.cpp — Bridge-native visual controls dialog emitter.
 *
 * Brightness, Color, Contrast, Tint sliders + Reset/OK buttons.
 */

#include "ui_visual_controls.h"
#include "ui_dialog.h"
#include "ui_controls.h"
#include "render_bridge.h"
#include "function.h"
#include <cstring>

extern bool Render_Bridge_UI_Visual_Controls_Get_Internal(
    float& brightness, float& color, float& contrast, float& tint,
    bool& hd_graphics,
    int& screen_w, int& screen_h, UIVisualControlsInternalLabels& labels);
extern void Render_Bridge_UI_Visual_Controls_Set_Result(
    int r, float brightness, float color, float contrast, float tint,
    bool hd_graphics);

void UI_Visual_Controls_Emit()
{
    if (!Render_Bridge_UI_Has_Active_Visual_Controls()) return;

    float brightness = 0, color = 0, contrast = 0, tint = 0;
    bool hd_graphics = false;
    int screen_w = 0, screen_h = 0;
    UIVisualControlsInternalLabels lbl;
    Render_Bridge_UI_Visual_Controls_Get_Internal(brightness, color, contrast, tint,
                                                   hd_graphics,
                                                   screen_w, screen_h, lbl);

    if (screen_w <= 0) screen_w = 640;
    if (screen_h <= 0) screen_h = 400;

    UIFontID fnt = UI_FONT_SDF_DEFAULT;
    int lh = UI_Text_Line_Height(fnt);
    if (lh < 6) lh = 6;
    int row = lh + 4;
    int btn_h = lh + 8;
    int slider_h = lh;
    int gap = 4;

    UIDialogStyle style = UI_Default_Dialog_Style();
    int dialog_w = 240;
    int dialog_h = style.title_height + style.padding
                   + 4 * (row + slider_h + gap)  // 4 slider rows
                   + gap + btn_h + style.padding; // buttons

    int cx, cy, cw, ch;
    UI_Dialog_Begin(screen_w, screen_h, dialog_w, dialog_h,
                    lbl.title, style, cx, cy, cw, ch);

    UISliderStyle ss = UI_Default_Slider_Style();
    ss.thumb_r = 0; ss.thumb_g = 100; ss.thumb_b = 0;
    ss.hover_r = 0; ss.hover_g = 140; ss.hover_b = 0;

    struct SliderRow { const char* label; float* value; };
    SliderRow rows[] = {
        {lbl.brightness, &brightness},
        {lbl.color,      &color},
        {lbl.contrast,   &contrast},
        {lbl.tint,       &tint},
    };

    for (int i = 0; i < 4; i++) {
        UI_Label(cx, cy, rows[i].label, fnt, 180, 180, 180, 255);
        cy += row;
        UI_Input_Push_String_ID(rows[i].label ? rows[i].label : "");
        *rows[i].value = UI_Slider(cx, cy, cw, slider_h, *rows[i].value, ss);
        UI_Input_Pop_ID();
        cy += slider_h + gap;
    }

    hd_graphics = UI_Checkbox(cx, cy, lh, lbl.hd_graphics, hd_graphics, fnt, 180, 180, 180);
    cy += lh + gap;

    // Reset / OK buttons
    UIButtonStyle bs = UI_Default_Button_Style();
    bs.normal_r = 0;  bs.normal_g = 50;  bs.normal_b = 0;  bs.normal_a = 240;
    bs.hover_r  = 0;  bs.hover_g  = 80;  bs.hover_b  = 0;  bs.hover_a  = 255;
    bs.press_r  = 0;  bs.press_g  = 30;  bs.press_b  = 0;  bs.press_a  = 255;
    bs.text_r   = 200; bs.text_g  = 200;  bs.text_b  = 200; bs.text_a  = 255;
    bs.border_r = 0;  bs.border_g = 100; bs.border_b = 0;  bs.border_a = 200;
    bs.font = fnt;

    int half_w = (cw - 6) / 2;
    UIButtonState reset_state = UI_Button(cx, cy, half_w, btn_h, lbl.reset, bs);
    UIButtonState ok_state = UI_Button(cx + half_w + 6, cy, half_w, btn_h, lbl.ok, bs);

    if (UI_Button_Activated(reset_state)) {
        brightness = 0.5f; color = 0.5f; contrast = 0.5f; tint = 0.5f;
        Render_Bridge_UI_Visual_Controls_Set_Result(0, brightness, color, contrast, tint, hd_graphics);
    } else if (UI_Button_Activated(ok_state)) {
        Render_Bridge_UI_Visual_Controls_Set_Result(1, brightness, color, contrast, tint, hd_graphics);
    } else {
        Render_Bridge_UI_Visual_Controls_Set_Result(0, brightness, color, contrast, tint, hd_graphics);
    }
}
