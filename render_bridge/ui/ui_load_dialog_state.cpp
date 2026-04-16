#include "ui_load_dialog_state.h"

#include <cstdio>

namespace {

struct UILoadDialogState {
    bool        active = false;
    int         mode = 0;
    char        items[8][40] = {};
    int         item_count = 0;
    int         selected = 0;
    float       scroll_pos = 0.0f;
    char        edit_text[40] = {};
    int         result = 0;
    int         screen_w = 0;
    int         screen_h = 0;
    char        lbl_title[32] = {};
    char        lbl_action[32] = {};
    char        lbl_cancel[32] = {};
};

UILoadDialogState g_load_dialog_state;
const char* g_load_item_ptrs[8] = {};

void copy_label(char* dst, int size, const char* src)
{
    if (!dst || size <= 0) {
        return;
    }

    std::snprintf(dst, size, "%s", src ? src : "");
}

} // namespace

void Render_Bridge_UI_Set_Load_Dialog(UILoadMode mode, const char* items[], int count,
                                      int selected, const char* edit_text,
                                      int screen_w, int screen_h,
                                      const char* title, const char* action_label,
                                      const char* cancel_label)
{
    g_load_dialog_state.active = true;
    g_load_dialog_state.mode = static_cast<int>(mode);
    g_load_dialog_state.item_count = count > 8 ? 8 : count;
    for (int i = 0; i < g_load_dialog_state.item_count; i++) {
        std::snprintf(g_load_dialog_state.items[i], 40, "%s", items[i] ? items[i] : "");
    }
    g_load_dialog_state.selected = selected;
    g_load_dialog_state.scroll_pos = 0.0f;
    std::snprintf(g_load_dialog_state.edit_text, 40, "%s", edit_text ? edit_text : "");
    g_load_dialog_state.result = 0;
    g_load_dialog_state.screen_w = screen_w;
    g_load_dialog_state.screen_h = screen_h;
    copy_label(g_load_dialog_state.lbl_title, 32, title);
    copy_label(g_load_dialog_state.lbl_action, 32, action_label);
    copy_label(g_load_dialog_state.lbl_cancel, 32, cancel_label);
}

void Render_Bridge_UI_Update_Load_Dialog_Items(const char* items[], int count)
{
    g_load_dialog_state.item_count = count > 8 ? 8 : count;
    for (int i = 0; i < g_load_dialog_state.item_count; i++) {
        std::snprintf(g_load_dialog_state.items[i], 40, "%s", items[i] ? items[i] : "");
    }
}

void Render_Bridge_UI_Get_Load_Dialog_State(int& selected, float& scroll_pos,
                                            char* edit_text, int edit_size)
{
    selected = g_load_dialog_state.selected;
    scroll_pos = g_load_dialog_state.scroll_pos;
    if (edit_text && edit_size > 0) {
        std::snprintf(edit_text, edit_size, "%s", g_load_dialog_state.edit_text);
    }
}

int Render_Bridge_UI_Get_Load_Dialog_Result()
{
    return g_load_dialog_state.result;
}

void Render_Bridge_UI_Clear_Load_Dialog()
{
    g_load_dialog_state.active = false;
    g_load_dialog_state.result = 0;
}

bool Render_Bridge_UI_Has_Active_Load_Dialog()
{
    return g_load_dialog_state.active;
}

bool Render_Bridge_UI_Load_Dialog_Get_State(
    int& mode, const char**& items, int& count, int& selected,
    float& scroll_pos, const char*& edit_text, int& screen_w, int& screen_h,
    const char*& title, const char*& action_label, const char*& cancel_label)
{
    mode = g_load_dialog_state.mode;
    count = g_load_dialog_state.item_count;
    for (int i = 0; i < count; i++) {
        g_load_item_ptrs[i] = g_load_dialog_state.items[i];
    }
    items = g_load_item_ptrs;
    selected = g_load_dialog_state.selected;
    scroll_pos = g_load_dialog_state.scroll_pos;
    edit_text = g_load_dialog_state.edit_text;
    screen_w = g_load_dialog_state.screen_w;
    screen_h = g_load_dialog_state.screen_h;
    title = g_load_dialog_state.lbl_title;
    action_label = g_load_dialog_state.lbl_action;
    cancel_label = g_load_dialog_state.lbl_cancel;
    return g_load_dialog_state.active;
}

void Render_Bridge_UI_Load_Dialog_Set_Result(int result, int selected,
                                             float scroll_pos,
                                             const char* edit_text)
{
    g_load_dialog_state.selected = selected;
    g_load_dialog_state.scroll_pos = scroll_pos;
    std::snprintf(g_load_dialog_state.edit_text, 40, "%s", edit_text ? edit_text : "");
    if (result != 0) {
        g_load_dialog_state.result = result;
        g_load_dialog_state.active = false;
    }
}
