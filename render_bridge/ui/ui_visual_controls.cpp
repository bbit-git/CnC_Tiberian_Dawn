/**
 * ui_visual_controls.cpp — Bridge-native visual controls dialog emitter.
 */

#include "ui_visual_controls.h"
#include "ui_dialog.h"
#include "ui_controls.h"
#include "render_bridge.h"

struct UIVisualControlsInternalLabels {
    const char* title;
    const char* brightness;
    const char* color;
    const char* contrast;
    const char* tint;
    const char* reset;
    const char* ok;
};

extern bool Render_Bridge_UI_Visual_Controls_Get_Internal(
    float& brightness, float& color, float& contrast, float& tint,
    int& screen_w, int& screen_h, UIVisualControlsInternalLabels& labels);
extern void Render_Bridge_UI_Visual_Controls_Set_Result(
    int r, float brightness, float color, float contrast, float tint);

void UI_Visual_Controls_Emit()
{
    if (!Render_Bridge_UI_Has_Active_Visual_Controls()) {
        return;
    }

    float brightness = 0, color = 0, contrast = 0, tint = 0;
    int screen_w = 0, screen_h = 0;
    UIVisualControlsInternalLabels lbl;
    Render_Bridge_UI_Visual_Controls_Get_Internal(brightness, color, contrast, tint,
                                                   screen_w, screen_h, lbl);

    UIDialogStyle style = UI_Default_Dialog_Style();
    int dialog_w = 200;
    int dialog_h = 140;

    int cx, cy, cw, ch;
    UI_Dialog_Begin(screen_w, screen_h, dialog_w, dialog_h,
                    lbl.title, style, cx, cy, cw, ch);

    UISliderStyle ss = UI_Default_Slider_Style();
    ss.thumb_r = 0; ss.thumb_g = 100; ss.thumb_b = 0;
    ss.hover_r = 0; ss.hover_g = 140; ss.hover_b = 0;

    int label_h = 10;
    int slider_h = 12;
    int spacing = 2;

    struct SliderRow { const char* label; float* value; };
    SliderRow rows[] = {
        {lbl.brightness, &brightness},
        {lbl.color,      &color},
        {lbl.contrast,   &contrast},
        {lbl.tint,       &tint},
    };

    for (int i = 0; i < 4; i++) {
        UI_Label(cx, cy, rows[i].label, UI_FONT_6PT, 180, 180, 180, 255);
        cy += label_h;
        *rows[i].value = UI_Slider(cx, cy, cw, slider_h, *rows[i].value, ss);
        cy += slider_h + spacing;
    }

    UIButtonStyle bs = UI_Default_Button_Style();
    bs.normal_r = 0; bs.normal_g = 50; bs.normal_b = 0;
    bs.hover_r = 0;  bs.hover_g = 80;  bs.hover_b = 0;
    bs.press_r = 0;  bs.press_g = 30;  bs.press_b = 0;
    bs.text_r = 200; bs.text_g = 200;  bs.text_b = 200;
    bs.border_r = 0; bs.border_g = 100; bs.border_b = 0;
    bs.font = UI_FONT_6PT;

    int dx = (screen_w - dialog_w) / 2;
    int dy = (screen_h - dialog_h) / 2;
    int btn_y = dy + dialog_h - style.button_h - style.padding;
    int half_w = (dialog_w - style.padding * 2 - 4) / 2;

    UIButtonState reset_state = UI_Button(dx + style.padding, btn_y,
                                           half_w, style.button_h, lbl.reset, bs);
    UIButtonState ok_state = UI_Button(dx + style.padding + half_w + 4, btn_y,
                                        half_w, style.button_h, lbl.ok, bs);

    if (reset_state == UI_BTN_PRESSED) {
        brightness = 0.5f; color = 0.5f; contrast = 0.5f; tint = 0.5f;
        Render_Bridge_UI_Visual_Controls_Set_Result(0, brightness, color, contrast, tint);
    } else if (ok_state == UI_BTN_PRESSED) {
        Render_Bridge_UI_Visual_Controls_Set_Result(1, brightness, color, contrast, tint);
    } else {
        Render_Bridge_UI_Visual_Controls_Set_Result(0, brightness, color, contrast, tint);
    }
}
