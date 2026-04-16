/**
 * ui_main_menu_options.cpp — Bridge-native standalone main menu options emitter.
 *
 * Shown from the main menu "Options" button (result code 5 in init.cpp).
 * Provides: Audio | Video | Back.
 * Does NOT show game-speed/scroll sliders — those are in-game controls only.
 *
 * The HD/Legacy texture toggle is surfaced directly on this dialog as a
 * prominently-labeled toggle button so it's one tap away from the main menu.
 */

#include "ui_main_menu_options.h"
#include "ui_dialog.h"
#include "ui_controls.h"
#include "render_bridge.h"
#include "function.h"
#include <cstring>

// ---- Bridge-side state accessors (defined in render_bridge.cpp) ----
extern bool Render_Bridge_UI_Has_Active_Main_Menu_Options();
extern void Render_Bridge_UI_Main_Menu_Options_Get_Internal(
    int& screen_w, int& screen_h,
    bool& hd_graphics,
    UIMainMenuOptionsInternalLabels& lbl);
extern void Render_Bridge_UI_Set_Main_Menu_Options_Result(int r);
extern void Render_Bridge_UI_Main_Menu_Options_Set_HD(bool hd);

void UI_Main_Menu_Options_Emit()
{
    if (!Render_Bridge_UI_Has_Active_Main_Menu_Options()) return;

    int screen_w = 0, screen_h = 0;
    bool hd_graphics = false;
    UIMainMenuOptionsInternalLabels lbl;
    Render_Bridge_UI_Main_Menu_Options_Get_Internal(screen_w, screen_h, hd_graphics, lbl);

    if (screen_w <= 0) screen_w = 640;
    if (screen_h <= 0) screen_h = 400;

    UIFontID fnt = UI_FONT_SDF_DEFAULT;
    int lh = UI_Text_Line_Height(fnt);
    if (lh < 6) lh = 6;
    int btn_h  = lh + 10;
    int tog_h  = lh + 8;   // texture toggle row height
    int gap    = 6;
    int pad    = 12;

    UIDialogStyle style = UI_Default_Dialog_Style();

    // Dialog: title + [Audio] [Video] + separator + [HD | Legacy] toggle + [Back]
    int dialog_w = 220;
    int dialog_h = style.title_height + pad
                   + btn_h + gap          // Audio button
                   + btn_h + gap          // Video button
                   + gap                  // separator
                   + tog_h + gap          // HD/Legacy toggle row
                   + btn_h + pad;         // Back button

    int cx, cy, cw, ch;
    UI_Dialog_Begin(screen_w, screen_h, dialog_w, dialog_h,
                    lbl.title, style, cx, cy, cw, ch);

    // ---- Button style (C&C green) ----
    UIButtonStyle bs = UI_Default_Button_Style();
    bs.normal_r = 0;   bs.normal_g = 50;  bs.normal_b = 0;   bs.normal_a = 240;
    bs.hover_r  = 0;   bs.hover_g  = 80;  bs.hover_b  = 0;   bs.hover_a  = 255;
    bs.press_r  = 0;   bs.press_g  = 30;  bs.press_b  = 0;   bs.press_a  = 255;
    bs.text_r   = 200; bs.text_g   = 200; bs.text_b   = 200; bs.text_a   = 255;
    bs.border_r = 0;   bs.border_g = 100; bs.border_b = 0;   bs.border_a = 200;
    bs.font     = fnt;

    // ---- Audio button ----
    UIButtonState audio_state = UI_Button(cx, cy, cw, btn_h, lbl.audio, bs);
    cy += btn_h + gap;

    // ---- Video button ----
    UIButtonState video_state = UI_Button(cx, cy, cw, btn_h, lbl.video, bs);
    cy += btn_h + gap * 2;  // extra gap before toggle

    // ---- HD / Legacy texture toggle ----
    // Two side-by-side buttons acting as a mutually-exclusive toggle.
    // Active selection gets a brighter border to indicate "selected".
    int half_w = (cw - 4) / 2;

    UIButtonStyle hd_bs = bs;
    UIButtonStyle legacy_bs = bs;

    // Highlight the currently active mode
    if (hd_graphics) {
        hd_bs.border_r = 0; hd_bs.border_g = 200; hd_bs.border_b = 80;
        hd_bs.border_a = 255;
        hd_bs.normal_r = 0; hd_bs.normal_g = 80;  hd_bs.normal_b = 20;
        hd_bs.normal_a = 255;
    } else {
        legacy_bs.border_r = 0; legacy_bs.border_g = 200; legacy_bs.border_b = 80;
        legacy_bs.border_a = 255;
        legacy_bs.normal_r = 0; legacy_bs.normal_g = 80;  legacy_bs.normal_b = 20;
        legacy_bs.normal_a = 255;
    }

    UIButtonState hd_state     = UI_Button(cx,            cy, half_w, tog_h, lbl.hd_label,     hd_bs);
    UIButtonState legacy_state = UI_Button(cx + half_w + 4, cy, half_w, tog_h, lbl.legacy_label, legacy_bs);
    cy += tog_h + gap * 2;

    if (UI_Button_Activated(hd_state) && !hd_graphics) {
        hd_graphics = true;
        Render_Bridge_UI_Main_Menu_Options_Set_HD(true);
    }
    if (UI_Button_Activated(legacy_state) && hd_graphics) {
        hd_graphics = false;
        Render_Bridge_UI_Main_Menu_Options_Set_HD(false);
    }

    // ---- Back button ----
    int back_w = UI_Text_Measure_Width(fnt, lbl.back, static_cast<int>(strlen(lbl.back))) + 24;
    if (back_w < 80) back_w = 80;
    UIButtonState back_state = UI_Button(cx + (cw - back_w) / 2, cy, back_w, btn_h, lbl.back, bs);

    // ---- Commit result ----
    if (UI_Button_Activated(audio_state)) {
        Render_Bridge_UI_Set_Main_Menu_Options_Result(UI_MM_OPT_AUDIO);
    } else if (UI_Button_Activated(video_state)) {
        Render_Bridge_UI_Set_Main_Menu_Options_Result(UI_MM_OPT_VIDEO);
    } else if (UI_Button_Activated(back_state)) {
        Render_Bridge_UI_Set_Main_Menu_Options_Result(UI_MM_OPT_BACK);
    } else if (Key_Down(KN_ESC)) {
        Render_Bridge_UI_Set_Main_Menu_Options_Result(UI_MM_OPT_BACK);
    } else {
        Render_Bridge_UI_Set_Main_Menu_Options_Result(UI_MM_OPT_NONE);
    }
}
