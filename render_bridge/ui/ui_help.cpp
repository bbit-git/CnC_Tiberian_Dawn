/**
 * ui_help.cpp — Bridge-native overlay for the legacy hover help text.
 */

#include "ui_help.h"
#include "ui_controls.h"
#include "render_bridge.h"
#include "function.h"
#include "help.h"

namespace {

void map_palette_color(int color, uint8_t& r, uint8_t& g, uint8_t& b)
{
    const uint8_t* pal = static_cast<const uint8_t*>((void*)Get_Palette());
    if (!pal) pal = GamePalette;
    if (!pal || color < 0 || color > 255) {
        r = 192; g = 192; b = 192;
        return;
    }

    r = pal[color * 3 + 0] << 2;
    g = pal[color * 3 + 1] << 2;
    b = pal[color * 3 + 2] << 2;
}

} // namespace

void UI_Help_Emit()
{
    extern bool InMainLoop;
    if (!InMainLoop) return;

    const char* help_text = nullptr;
    int help_color = LTGREY;
    int help_cost = 0;
    if (!Map.Get_Render_Bridge_Text(help_text, help_color, help_cost)) {
        return;
    }

    int tac_x = 0, tac_y = 0, tac_w = 0, tac_h = 0;
    Render_Bridge_Get_Tactical_Rect(tac_x, tac_y, tac_w, tac_h);

    uint8_t r = 192, g = 192, b = 192;
    map_palette_color(help_color, r, g, b);

    int line_h = UI_Text_Line_Height(UI_FONT_6PT);
    if (line_h <= 0) line_h = 8;
    int lines = help_cost ? 2 : 1;
    int text_y = tac_y + tac_h - lines * line_h - 4;
    if (text_y < tac_y + 2) text_y = tac_y + 2;
    int text_x = tac_x + 4;

    UI_Label(text_x + 1, text_y + 1, help_text, UI_FONT_6PT, 0, 0, 0, 255);
    UI_Label(text_x, text_y, help_text, UI_FONT_6PT, r, g, b, 255);

    if (help_cost > 0) {
        char cost_buf[16];
        snprintf(cost_buf, sizeof(cost_buf), "$%d", help_cost);
        int cost_y = text_y + line_h;
        UI_Label(text_x + 1, cost_y + 1, cost_buf, UI_FONT_6PT, 0, 0, 0, 255);
        UI_Label(text_x, cost_y, cost_buf, UI_FONT_6PT, r, g, b, 255);
    }
}
