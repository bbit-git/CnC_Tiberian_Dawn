#ifndef CNC_UI_LOAD_DIALOG_STATE_H
#define CNC_UI_LOAD_DIALOG_STATE_H

#include "render_bridge.h"

bool Render_Bridge_UI_Load_Dialog_Get_State(
    int& mode, const char**& items, int& count, int& selected,
    float& scroll_pos, const char*& edit_text, int& screen_w, int& screen_h,
    const char*& title, const char*& action_label, const char*& cancel_label);

void Render_Bridge_UI_Load_Dialog_Set_Result(int result, int selected,
                                             float scroll_pos,
                                             const char* edit_text);

#endif // CNC_UI_LOAD_DIALOG_STATE_H
