/**
 * ui_action_menu.cpp — Bridge-native right-click action menu overlay.
 *
 * Reads ActionMenu state and renders it through the UI draw list
 * instead of drawing to HidPage via LogicPage.
 */

#include "ui_action_menu.h"
#include "ui_draw_list.h"
#include "ui_controls.h"
#include "render_bridge.h"
#include "function.h"
#include "actmenu.h"

void UI_ActionMenu_Emit()
{
    extern ActionMenuClass ActionMenu;
    auto state = ActionMenu.Get_Render_State();
    if (!state.active) return;

    // Scale SeenBuff-space coordinates to logical screen space.
    int logical_w = 0, logical_h = 0;
    Render_Bridge_Get_Logical_Screen_Size(logical_w, logical_h);
    int base_w = SeenBuff.Get_Width();
    int base_h = SeenBuff.Get_Height();
    float sx = (base_w > 0 && logical_w > 0) ? static_cast<float>(logical_w) / static_cast<float>(base_w) : 1.0f;
    float sy = (base_h > 0 && logical_h > 0) ? static_cast<float>(logical_h) / static_cast<float>(base_h) : 1.0f;

    int mx = static_cast<int>(state.menu_x * sx);
    int my = static_cast<int>(state.menu_y * sy);
    int mw = static_cast<int>(state.menu_w * sx);
    int mh = static_cast<int>(state.menu_h * sy);
    int item_h = static_cast<int>(state.item_height * sy);

    // Background + border (matches legacy: black fill, white border)
    g_ui_draw_list.Fill_Rect(mx, my, mw, mh, 0, 0, 0, 240);
    g_ui_draw_list.Draw_Rect(mx, my, mw, mh, 255, 255, 255, 255);

    // Menu items
    int text_x = mx + static_cast<int>(5 * sx);
    for (int i = 0; i < state.num_items; i++) {
        if (!state.item_names[i]) continue;
        int text_y = my + static_cast<int>(2 * sy) + (i * item_h) + static_cast<int>(2 * sy);
        UI_Label(text_x, text_y, state.item_names[i], UI_FONT_6PT,
                 255, 255, 255, 255);
    }
}
