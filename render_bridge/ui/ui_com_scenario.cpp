/**
 * ui_com_scenario.cpp — Bridge-native multiplayer/skirmish setup dialog emitter.
 */

#include "ui_com_scenario.h"
#include "ui_dialog.h"
#include "ui_controls.h"
#include "render_bridge.h"
#include <cstdio>
#include <cstring>

#ifndef MPLAYER_BUILD_LEVEL_MAX
#define MPLAYER_BUILD_LEVEL_MAX 7
#endif

extern bool Render_Bridge_UI_Com_Scenario_Get_Internal(UIComScenarioState& state);
extern void Render_Bridge_UI_Com_Scenario_Update(const UIComScenarioState& state);

void UI_Com_Scenario_Emit()
{
    if (!Render_Bridge_UI_Has_Active_Com_Scenario()) {
        return;
    }

    UIComScenarioState st;
    Render_Bridge_UI_Com_Scenario_Get_Internal(st);

    UIDialogStyle style = UI_Default_Dialog_Style();

    // Derive layout metrics from font so the dialog scales with font_scale.
    UIFontID fnt = UI_FONT_SDF_DEFAULT;
    int lh = UI_Text_Line_Height(fnt);
    if (lh < 6) lh = 6;
    int row   = lh + 4;          // label row height
    int btn_h = lh + 6;          // button height
    int slider_h = lh;           // slider track height
    int gap   = 4;               // inter-section gap

    int dialog_w = 460;
    int dialog_h = row * 14 + style.title_height + style.padding * 3 + btn_h;
    if (dialog_h > st.screen_h - 20) dialog_h = st.screen_h - 20;

    int cx, cy, cw, ch;
    UI_Dialog_Begin(st.screen_w, st.screen_h, dialog_w, dialog_h,
                    st.lbl_title, style, cx, cy, cw, ch);

    UIButtonStyle bs = UI_Default_Button_Style();
    bs.normal_r = 0; bs.normal_g = 50; bs.normal_b = 0;
    bs.hover_r = 0;  bs.hover_g = 80;  bs.hover_b = 0;
    bs.press_r = 0;  bs.press_g = 30;  bs.press_b = 0;
    bs.text_r = 200; bs.text_g = 200;  bs.text_b = 200;
    bs.border_r = 0; bs.border_g = 100; bs.border_b = 0;
    bs.font = fnt;

    UISliderStyle ss = UI_Default_Slider_Style();
    ss.thumb_r = 0; ss.thumb_g = 100; ss.thumb_b = 0;
    ss.hover_r = 0; ss.hover_g = 140; ss.hover_b = 0;

    int col_gap = 12;
    int left_w = cw * 55 / 100;
    int right_x = cx + left_w + col_gap;
    int right_w = cw - left_w - col_gap;
    int y = cy;

    // --- Left column ---

    // Player name
    UI_Label(cx, y, st.lbl_name, fnt, 180, 180, 180, 255);
    y += row;
    UI_TextInput(cx, y, left_w, btn_h, st.player_name, sizeof(st.player_name), fnt);
    y += btn_h + gap;

    // Faction
    UI_Label(cx, y, st.lbl_side, fnt, 180, 180, 180, 255);
    y += row;
    int fac_w = left_w / 2 - 2;
    UIButtonStyle gdi_bs = bs;
    UIButtonStyle nod_bs = bs;
    if (st.faction == 0) { gdi_bs.normal_r = 0; gdi_bs.normal_g = 80; gdi_bs.normal_b = 0; }
    else                  { nod_bs.normal_r = 80; nod_bs.normal_g = 0; nod_bs.normal_b = 0; }
    UIButtonState gdi_state = UI_Button(cx, y, fac_w, btn_h, st.lbl_gdi, gdi_bs);
    UIButtonState nod_state = UI_Button(cx + fac_w + 4, y, fac_w, btn_h, st.lbl_nod, nod_bs);
    if (UI_Button_Activated(gdi_state)) st.faction = 0;
    if (UI_Button_Activated(nod_state)) st.faction = 1;
    y += btn_h + gap;

    // Credits (slider: 0–10000 in steps of 500)
    UI_Label(cx, y, st.lbl_credits, fnt, 180, 180, 180, 255);
    {
        char credit_buf[16];
        std::snprintf(credit_buf, sizeof(credit_buf), "%d", st.credits);
        int lbl_w = UI_Text_Measure_Width(fnt, st.lbl_credits,
                                           static_cast<int>(strlen(st.lbl_credits)));
        UI_Label(cx + lbl_w + 4, y, credit_buf, fnt, 220, 220, 220, 255);
    }
    y += row;
    {
        float cred_norm = static_cast<float>(st.credits) / 10000.0f;
        if (cred_norm < 0.0f) cred_norm = 0.0f;
        if (cred_norm > 1.0f) cred_norm = 1.0f;
        UI_Input_Push_String_ID("credits");
        cred_norm = UI_Slider(cx, y, left_w, slider_h, cred_norm, ss);
        UI_Input_Pop_ID();
        st.credits = static_cast<int>(cred_norm * 20.0f + 0.5f) * 500;  // snap to 500s
    }
    y += slider_h + gap;

    // Scenario list (fills remaining content height)
    int list_h = ch - (y - cy) - 4;
    if (list_h < 40) list_h = 40;
    static const char* scenario_ptrs[64];
    for (int i = 0; i < st.scenario_count && i < 64; i++) {
        scenario_ptrs[i] = st.scenarios[i];
    }
    UIListBoxStyle lbs = UI_Default_ListBox_Style();
    UI_Input_Push_String_ID("scenario_list");
    int new_scen = UI_ListBox(cx, y, left_w, list_h, scenario_ptrs,
                              st.scenario_count, st.selected_scenario,
                              st.scenario_scroll, lbs);
    UI_Input_Pop_ID();
    st.selected_scenario = new_scen;

    // --- Right column ---
    int ry = cy;

    // Build Level (1–MPLAYER_BUILD_LEVEL_MAX mapped to 0.0–1.0)
    {
        int bl_val = 1 + static_cast<int>(st.build_level * (MPLAYER_BUILD_LEVEL_MAX - 1));
        if (bl_val < 1) bl_val = 1;
        if (bl_val > MPLAYER_BUILD_LEVEL_MAX) bl_val = MPLAYER_BUILD_LEVEL_MAX;
        char bl_buf[32];
        std::snprintf(bl_buf, sizeof(bl_buf), "%s %d", st.lbl_build_level, bl_val);
        UI_Label(right_x, ry, bl_buf, fnt, 180, 180, 180, 255);
    }
    ry += row;
    UI_Input_Push_String_ID("build_level");
    st.build_level = UI_Slider(right_x, ry, right_w, slider_h, st.build_level, ss);
    UI_Input_Pop_ID();
    ry += slider_h + gap;

    // AI Players (0–5 mapped to 0.0–1.0)
    {
        int ai_val = static_cast<int>(st.ai_players * 5.0f + 0.5f);
        if (ai_val < 0) ai_val = 0;
        if (ai_val > 5) ai_val = 5;
        char ai_buf[32];
        std::snprintf(ai_buf, sizeof(ai_buf), "%s %d", st.lbl_ai_players, ai_val);
        UI_Label(right_x, ry, ai_buf, fnt, 180, 180, 180, 255);
    }
    ry += row;
    UI_Input_Push_String_ID("ai_players");
    st.ai_players = UI_Slider(right_x, ry, right_w, slider_h, st.ai_players, ss);
    UI_Input_Pop_ID();
    ry += slider_h + gap;

    // Unit Count (depends on bases toggle)
    {
        int count_min = 1;
        int count_max = st.options[0] ? 50 : 12;
        int uc_val = count_min + static_cast<int>(st.unit_count * (count_max - count_min) + 0.5f);
        if (uc_val < count_min) uc_val = count_min;
        if (uc_val > count_max) uc_val = count_max;
        char uc_buf[32];
        std::snprintf(uc_buf, sizeof(uc_buf), "%s %d", st.lbl_unit_count, uc_val);
        UI_Label(right_x, ry, uc_buf, fnt, 180, 180, 180, 255);
    }
    ry += row;
    UI_Input_Push_String_ID("unit_count");
    st.unit_count = UI_Slider(right_x, ry, right_w, slider_h, st.unit_count, ss);
    UI_Input_Pop_ID();
    ry += slider_h + gap;

    // AI Skill / Difficulty (0–2: Easy, Normal, Hard)
    {
        int sk_val = static_cast<int>(st.ai_skill * 2.0f + 0.5f);
        if (sk_val > 2) sk_val = 2;
        const char* sk_names[] = {"Easy", "Normal", "Hard"};
        char sk_buf[32];
        std::snprintf(sk_buf, sizeof(sk_buf), "%s %s", st.lbl_ai_skill, sk_names[sk_val]);
        UI_Label(right_x, ry, sk_buf, fnt, 180, 180, 180, 255);
    }
    ry += row;
    UI_Input_Push_String_ID("ai_skill");
    st.ai_skill = UI_Slider(right_x, ry, right_w, slider_h, st.ai_skill, ss);
    UI_Input_Pop_ID();
    ry += slider_h + gap;

    // Option checkboxes
    for (int i = 0; i < 4; i++) {
        if (st.option_labels[i][0]) {
            st.options[i] = UI_Checkbox(right_x, ry, lh, st.option_labels[i],
                                         st.options[i], fnt, 180, 180, 180);
            ry += row;
        }
    }

    // OK / Cancel buttons
    int dx = (st.screen_w - dialog_w) / 2;
    int dy = (st.screen_h - dialog_h) / 2;
    UIDialogResult result = UI_Dialog_End(dx, dy, dialog_w, dialog_h,
                                          st.lbl_ok, st.lbl_cancel, style);

    if (result == UI_DIALOG_OK) {
        st.result = 1;
        st.active = false;
    } else if (result == UI_DIALOG_CANCEL) {
        st.result = 2;
        st.active = false;
    }

    Render_Bridge_UI_Com_Scenario_Update(st);
}
