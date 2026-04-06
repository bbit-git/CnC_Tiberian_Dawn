/**
 * ui_sound_controls.cpp — Bridge-native sound controls dialog emitter.
 *
 * Music volume, SFX volume, track list (jukebox), shuffle/repeat, OK.
 */

#include "ui_sound_controls.h"
#include "ui_dialog.h"
#include "ui_controls.h"
#include "render_bridge.h"
#include "function.h"
#include <cstring>

struct UISoundControlsInternalLabels {
    const char* title;
    const char* music_vol;
    const char* sound_vol;
    const char* shuffle;
    const char* repeat;
    const char* on;
    const char* off;
    const char* play;
    const char* stop;
    const char* ok;
};

extern bool Render_Bridge_UI_Sound_Controls_Get_Internal(
    float& music_vol, float& sfx_vol, const char**& tracks, int& track_count,
    int& selected, float& scroll, bool& shuffle, bool& repeat, bool& playing,
    int& screen_w, int& screen_h, UISoundControlsInternalLabels& labels);
extern void Render_Bridge_UI_Sound_Controls_Update(
    int result, float music_vol, float sfx_vol, int selected, float scroll,
    bool shuffle, bool repeat);

void UI_Sound_Controls_Emit()
{
    if (!Render_Bridge_UI_Has_Active_Sound_Controls()) return;

    float music_vol = 0, sfx_vol = 0;
    bool shuffle = false, repeat_on = false, playing = false;
    const char** tracks = nullptr;
    int track_count = 0, selected_track = 0;
    float track_scroll = 0;
    int screen_w = 0, screen_h = 0;
    UISoundControlsInternalLabels lbl;
    Render_Bridge_UI_Sound_Controls_Get_Internal(music_vol, sfx_vol, tracks, track_count,
                                                  selected_track, track_scroll,
                                                  shuffle, repeat_on, playing,
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
    int dialog_w = 280;
    int dialog_h = style.title_height + style.padding
                   + row * 2 + slider_h * 2 + gap * 3
                   + 80
                   + gap + btn_h
                   + gap + btn_h + style.padding;

    int cx, cy, cw, ch;
    UI_Dialog_Begin(screen_w, screen_h, dialog_w, dialog_h,
                    lbl.title, style, cx, cy, cw, ch);

    UISliderStyle ss = UI_Default_Slider_Style();
    ss.thumb_r = 0; ss.thumb_g = 100; ss.thumb_b = 0;
    ss.hover_r = 0; ss.hover_g = 140; ss.hover_b = 0;

    // Music Volume
    UI_Label(cx, cy, lbl.music_vol, fnt, 180, 180, 180, 255);
    cy += row;
    music_vol = UI_Slider(cx, cy, cw, slider_h, music_vol, ss);
    cy += slider_h + gap;

    // SFX Volume
    UI_Label(cx, cy, lbl.sound_vol, fnt, 180, 180, 180, 255);
    cy += row;
    sfx_vol = UI_Slider(cx, cy, cw, slider_h, sfx_vol, ss);
    cy += slider_h + gap;

    // Track list
    int remaining = ch - (cy - cx + style.padding);
    int list_h = remaining - btn_h * 2 - gap * 3;
    if (list_h < 40) list_h = 40;

    UIListBoxStyle lbs = UI_Default_ListBox_Style();
    selected_track = UI_ListBox(cx, cy, cw, list_h, tracks, track_count,
                                 selected_track, track_scroll, lbs);
    cy += list_h + gap;

    // Play / Stop / Shuffle / Repeat
    UIButtonStyle bs = UI_Default_Button_Style();
    bs.normal_r = 0;  bs.normal_g = 50;  bs.normal_b = 0;  bs.normal_a = 240;
    bs.hover_r  = 0;  bs.hover_g  = 80;  bs.hover_b  = 0;  bs.hover_a  = 255;
    bs.press_r  = 0;  bs.press_g  = 30;  bs.press_b  = 0;  bs.press_a  = 255;
    bs.text_r   = 200; bs.text_g  = 200;  bs.text_b  = 200; bs.text_a  = 255;
    bs.border_r = 0;  bs.border_g = 100; bs.border_b = 0;  bs.border_a = 200;
    bs.font = fnt;

    int btn4_w = (cw - 9) / 4;
    if (UI_Button(cx, cy, btn4_w, btn_h, lbl.play, bs) == UI_BTN_PRESSED) {
        Render_Bridge_UI_Sound_Controls_Update(2, music_vol, sfx_vol,
                                               selected_track, track_scroll,
                                               shuffle, repeat_on);
    }
    if (UI_Button(cx + btn4_w + 3, cy, btn4_w, btn_h, lbl.stop, bs) == UI_BTN_PRESSED) {
        Render_Bridge_UI_Sound_Controls_Update(3, music_vol, sfx_vol,
                                               selected_track, track_scroll,
                                               shuffle, repeat_on);
    }
    shuffle   = UI_Checkbox(cx + (btn4_w + 3) * 2, cy, lh, lbl.shuffle,
                             shuffle, fnt, 180, 180, 180);
    repeat_on = UI_Checkbox(cx + (btn4_w + 3) * 3, cy, lh, lbl.repeat,
                             repeat_on, fnt, 180, 180, 180);
    cy += btn_h + gap;

    // OK button
    int ok_w = UI_Text_Measure_Width(fnt, lbl.ok, static_cast<int>(strlen(lbl.ok))) + 20;
    if (ok_w < 80) ok_w = 80;
    if (UI_Button(cx + (cw - ok_w) / 2, cy, ok_w, btn_h, lbl.ok, bs) == UI_BTN_PRESSED) {
        Render_Bridge_UI_Sound_Controls_Update(1, music_vol, sfx_vol,
                                               selected_track, track_scroll,
                                               shuffle, repeat_on);
    } else {
        Render_Bridge_UI_Sound_Controls_Update(0, music_vol, sfx_vol,
                                               selected_track, track_scroll,
                                               shuffle, repeat_on);
    }
}
