/**
 * ui_help.cpp — Bridge-native overlay for the legacy hover help text.
 *
 * Handles both normal hover help (positioned in lower-left tactical corner)
 * and right-click sidebar icon help (positioned relative to cursor, right-aligned).
 */

#include "ui_help.h"
#include "ui_controls.h"
#include "render_bridge.h"
#include "function.h"
#include "help.h"
#include <cstring>

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

/// Scale a SeenBuff-space coordinate to logical screen space.
void scale_to_logical(int buf_x, int buf_y, int& out_x, int& out_y)
{
    int logical_w = 0, logical_h = 0;
    Render_Bridge_Get_Logical_Screen_Size(logical_w, logical_h);
    int base_w = SeenBuff.Get_Width();
    int base_h = SeenBuff.Get_Height();
    float sx = (base_w > 0 && logical_w > 0) ? static_cast<float>(logical_w) / static_cast<float>(base_w) : 1.0f;
    float sy = (base_h > 0 && logical_h > 0) ? static_cast<float>(logical_h) / static_cast<float>(base_h) : 1.0f;
    out_x = static_cast<int>(buf_x * sx);
    out_y = static_cast<int>(buf_y * sy);
}

} // namespace

void UI_Help_Emit()
{
    extern bool InMainLoop;
    if (!InMainLoop) return;

    // Try right-click (sidebar icon) help first.
    const char* right_text = nullptr;
    int right_color = LTGREY;
    int right_cost = 0;
    int draw_x = 0, draw_y = 0, legacy_width = 0;
    if (Map.Get_Render_Bridge_Right_Text(right_text, right_color, right_cost,
                                         draw_x, draw_y, legacy_width)) {
        uint8_t r = 192, g = 192, b = 192;
        map_palette_color(right_color, r, g, b);

        // The legacy cursor position is draw_x + legacy_width (text is right-aligned to cursor).
        int cursor_x = draw_x + legacy_width;
        int lcx = 0, lcy = 0;
        scale_to_logical(cursor_x, draw_y, lcx, lcy);

        // Measure with bridge font for self-consistent sizing.
        int text_len = static_cast<int>(strlen(right_text));
        int tw = UI_Text_Measure_Width(UI_FONT_6PT, right_text, text_len);
        int th = UI_Text_Line_Height(UI_FONT_6PT);
        if (th <= 0) th = 6;

        // Position: right-aligned to cursor, matching legacy DrawX = X - Width.
        int tx = lcx - tw;
        int ty = lcy;

        // 1px outline rect (legacy style: no fill, no shadow).
        g_ui_draw_list.Draw_Rect(tx - 1, ty - 1, tw + 3, th + 2, r, g, b, 255);

        // Text (no shadow — matching legacy TPF_NOSHADOW).
        UI_Label(tx, ty, right_text, UI_FONT_6PT, r, g, b, 255);

        if (right_cost > 0) {
            char cost_buf[16];
            snprintf(cost_buf, sizeof(cost_buf), "$%d", right_cost);
            int cost_len = static_cast<int>(strlen(cost_buf));
            int cw = UI_Text_Measure_Width(UI_FONT_6PT, cost_buf, cost_len);
            int cy = ty + th;

            // Divider line between name and cost (legacy: black line).
            int divider_w = (cw + 1 < tw) ? cw + 1 : tw;
            g_ui_draw_list.Fill_Rect(tx, cy, divider_w, 1, 0, 0, 0, 255);

            // Outline rect around cost.
            g_ui_draw_list.Draw_Rect(tx - 1, cy, cw + 3, th, r, g, b, 255);

            // Cost text.
            UI_Label(tx, cy, cost_buf, UI_FONT_6PT, r, g, b, 255);
        }
        return;
    }

    // Normal hover help — positioned in lower-left tactical corner.
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
