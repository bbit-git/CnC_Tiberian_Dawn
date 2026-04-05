/**
 * ui_main_menu.h — Bridge-native main menu overlay.
 */

#ifndef CNC_UI_MAIN_MENU_H
#define CNC_UI_MAIN_MENU_H

// Shared between render_bridge.cpp (provider) and ui_main_menu.cpp (consumer).
struct UIMainMenuInternalLabels {
    const char* new_game;
    const char* load_game;
    const char* skirmish;
    const char* options;
    const char* exit;
    const char* gdi;
    const char* nod;
    const char* back;
    const char* campaign_title;
    const char* expansion;
    bool        has_expansion;
    const char* version;
    const char* copyright;
};

bool Render_Bridge_UI_Main_Menu_Get_Internal(int& screen_w, int& screen_h,
                                             int& page, int& focused_button,
                                             float& fade_alpha, bool& dismissing,
                                             UIMainMenuInternalLabels& lbl);
void Render_Bridge_UI_Set_Main_Menu_Result(int r);
void Render_Bridge_UI_Main_Menu_Begin_Dismiss(int result_code);
void Render_Bridge_UI_Main_Menu_Set_Page(int page);
void Render_Bridge_UI_Main_Menu_Set_Focus(int focused);
void Render_Bridge_UI_Main_Menu_Set_Fade(float alpha);
void Render_Bridge_UI_Main_Menu_Commit_Dismiss();
void Render_Bridge_UI_Main_Menu_Signal_Input();   // emitter → poll loop: input activity detected
bool Render_Bridge_UI_Main_Menu_Had_Input();       // poll loop: returns true (and clears) if emitter signalled input

void UI_Main_Menu_Emit();

/// Try loading an HD title screen (TGA from loose files or MEG archive).
/// Returns true if an HD texture was loaded. Safe to call multiple times.
bool UI_Main_Menu_Load_HD_Title();

/// Returns true if an HD title texture is loaded and ready to draw.
bool UI_Main_Menu_Has_HD_Background();

/// Draw the HD title background as a full-screen center-cropped quad.
/// Call from gl_present.cpp INSTEAD of SeenBuff draw when active.
/// Does nothing if no HD texture is loaded.
void UI_Main_Menu_Draw_HD_Background(int win_w, int win_h);

/// Release the HD title texture. Call from GL_Present_Shutdown().
void UI_Main_Menu_Shutdown();

#endif // CNC_UI_MAIN_MENU_H
