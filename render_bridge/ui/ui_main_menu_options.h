/**
 * ui_main_menu_options.h — Bridge-native standalone main menu options dialog.
 *
 * Shown when the player clicks "Options" in the main menu.
 * Provides top-level Audio / Video buttons plus Back.
 * Audio opens SoundControls; Video opens VisualControls (with HD/Legacy toggle).
 */

#ifndef CNC_UI_MAIN_MENU_OPTIONS_H
#define CNC_UI_MAIN_MENU_OPTIONS_H

// Result codes returned by Render_Bridge_UI_Get_Main_Menu_Options_Result()
enum UIMainMenuOptionsResult : int {
    UI_MM_OPT_NONE  = 0,   // dialog still open
    UI_MM_OPT_AUDIO = 1,   // player pressed Audio
    UI_MM_OPT_VIDEO = 2,   // player pressed Video
    UI_MM_OPT_BACK  = 3,   // player pressed Back / ESC
};

// Internal label block populated by render_bridge.cpp
struct UIMainMenuOptionsInternalLabels {
    const char* title;
    const char* audio;
    const char* video;
    const char* back;
    const char* hd_label;   // e.g. "HD Textures"
    const char* legacy_label; // e.g. "Legacy Textures"
};

// Called once per frame from gl_ui.cpp while the dialog is active.
void UI_Main_Menu_Options_Emit();

#endif // CNC_UI_MAIN_MENU_OPTIONS_H
