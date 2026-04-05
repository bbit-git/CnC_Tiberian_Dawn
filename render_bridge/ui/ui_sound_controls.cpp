/**
 * ui_sound_controls.cpp — Bridge-native sound controls dialog emitter.
 */

#include "ui_sound_controls.h"
#include "ui_dialog.h"
#include "ui_controls.h"
#include "render_bridge.h"

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
    if (!Render_Bridge_UI_Has_Active_Sound_Controls()) {
        return;
    }

    float music_vol = 0, sfx_vol = 0, scroll = 0;
    const char** tracks = nullptr;
    int track_count = 0, selected = 0;
    bool shuffle = false, repeat = false, playing = false;
    int screen_w = 0, screen_h = 0;
    UISoundControlsInternalLabels lbl;
    Render_Bridge_UI_Sound_Controls_Get_Internal(
        music_vol, sfx_vol, tracks, track_count, selected, scroll,
        shuffle, repeat, playing, screen_w, screen_h, lbl);

    UIDialogStyle style = UI_Default_Dialog_Style();
    int dialog_w = 240;
    int dialog_h = 180;

    int cx, cy, cw, ch;
    UI_Dialog_Begin(screen_w, screen_h, dialog_w, dialog_h,
                    lbl.title, style, cx, cy, cw, ch);

    UISliderStyle ss = UI_Default_Slider_Style();
    ss.thumb_r = 0; ss.thumb_g = 100; ss.thumb_b = 0;
    ss.hover_r = 0; ss.hover_g = 140; ss.hover_b = 0;

    UIButtonStyle bs = UI_Default_Button_Style();
    bs.normal_r = 0; bs.normal_g = 50; bs.normal_b = 0;
    bs.hover_r = 0;  bs.hover_g = 80;  bs.hover_b = 0;
    bs.press_r = 0;  bs.press_g = 30;  bs.press_b = 0;
    bs.text_r = 200; bs.text_g = 200;  bs.text_b = 200;
    bs.border_r = 0; bs.border_g = 100; bs.border_b = 0;
    bs.font = UI_FONT_6PT;

    // Music Volume
    UI_Label(cx, cy, lbl.music_vol, UI_FONT_6PT, 180, 180, 180, 255);
    cy += 10;
    music_vol = UI_Slider(cx, cy, cw, 12, music_vol, ss);
    cy += 14;

    // Sound Volume
    UI_Label(cx, cy, lbl.sound_vol, UI_FONT_6PT, 180, 180, 180, 255);
    cy += 10;
    sfx_vol = UI_Slider(cx, cy, cw, 12, sfx_vol, ss);
    cy += 16;

    // Track list
    int list_h = ch - (cy - (cx - style.padding + style.title_height + style.padding
                         + ((screen_h - dialog_h) / 2))) - 30;
    if (list_h < 36) list_h = 36;

    UIListBoxStyle lbs = UI_Default_ListBox_Style();
    int new_sel = UI_ListBox(cx, cy, cw, list_h, tracks, track_count,
                              selected, scroll, lbs);
    if (new_sel != selected) selected = new_sel;
    cy += list_h + 2;

    // Play/Stop + Shuffle/Repeat row
    int btn4_w = (cw - 12) / 4;
    UIButtonState play_state = UI_Button(cx, cy, btn4_w, 14, lbl.play, bs);
    UIButtonState stop_state = UI_Button(cx + btn4_w + 4, cy, btn4_w, 14, lbl.stop, bs);

    bool new_shuffle = UI_Checkbox(cx + (btn4_w + 4) * 2, cy, 10, lbl.shuffle,
                                    shuffle, UI_FONT_6PT, 180, 180, 180);
    bool new_repeat = UI_Checkbox(cx + (btn4_w + 4) * 3, cy, 10, lbl.repeat,
                                   repeat, UI_FONT_6PT, 180, 180, 180);
    shuffle = new_shuffle;
    repeat = new_repeat;

    // OK button
    int dx = (screen_w - dialog_w) / 2;
    int dy = (screen_h - dialog_h) / 2;
    int btn_y = dy + dialog_h - style.button_h - style.padding;
    UIButtonState ok_state = UI_Button(dx + (dialog_w - style.button_w) / 2, btn_y,
                                        style.button_w, style.button_h, lbl.ok, bs);

    if (play_state == UI_BTN_PRESSED) {
        Render_Bridge_UI_Sound_Controls_Update(UI_SND_PLAY, music_vol, sfx_vol,
                                                selected, scroll, shuffle, repeat);
    } else if (stop_state == UI_BTN_PRESSED) {
        Render_Bridge_UI_Sound_Controls_Update(UI_SND_STOP, music_vol, sfx_vol,
                                                selected, scroll, shuffle, repeat);
    } else if (ok_state == UI_BTN_PRESSED) {
        Render_Bridge_UI_Sound_Controls_Update(UI_SND_OK, music_vol, sfx_vol,
                                                selected, scroll, shuffle, repeat);
    } else {
        Render_Bridge_UI_Sound_Controls_Update(UI_SND_NONE, music_vol, sfx_vol,
                                                selected, scroll, shuffle, repeat);
    }
}
