/**
 * ui_sidebar.cpp — Bridge-native sidebar chrome rendering.
 *
 * Reads legacy SidebarClass/StripClass state and emits UI draw list
 * commands. This is the chrome layer: backgrounds, borders, slot
 * outlines, scroll arrows, and production labels. Cameo icons are
 * NOT yet rendered natively — the legacy SeenBuff quad still provides
 * those until a full icon atlas is built.
 */

#include "ui_sidebar.h"
#include "ui_draw_list.h"
#include "ui_controls.h"
#include "ui_layout.h"
#include "ui_text.h"
#include "render_bridge.h"
#include "function.h"
#include <cstdio>

/// Sidebar slot colors.
static constexpr uint8_t SLOT_BG_R = 40, SLOT_BG_G = 44, SLOT_BG_B = 40;
static constexpr uint8_t SLOT_BORDER_R = 36, SLOT_BORDER_G = 120, SLOT_BORDER_B = 36;
static constexpr uint8_t READY_R = 0, READY_G = 255, READY_B = 0;
static constexpr uint8_t BUILDING_R = 200, BUILDING_G = 200, BUILDING_B = 0;
static constexpr uint8_t SCROLL_R = 120, SCROLL_G = 120, SCROLL_B = 120;

/// Emit one production slot outline and label.
static void emit_slot(int x, int y, int w, int h, int slot_index,
                      const SidebarClass::StripClass& strip)
{
    int actual_index = strip.TopIndex + slot_index;
    bool has_item = actual_index < strip.BuildableCount;

    // Slot background
    g_ui_draw_list.Fill_Rect(x, y, w, h, SLOT_BG_R, SLOT_BG_G, SLOT_BG_B, 120);
    g_ui_draw_list.Draw_Rect(x, y, w, h, SLOT_BORDER_R, SLOT_BORDER_G, SLOT_BORDER_B, 255);

    if (!has_item) return;

    // Check production state
    int factory_id = strip.Buildables[actual_index].Factory;
    if (factory_id != -1) {
        FactoryClass* factory = Factories.Raw_Ptr(factory_id);
        if (factory) {
            if (factory->Has_Completed()) {
                // Ready indicator
                UI_Label(x + 2, y + h - 10, "RDY", UI_FONT_6PT,
                         READY_R, READY_G, READY_B, 255);
            } else if (factory->Is_Building()) {
                // Progress percentage
                int pct = factory->Completion();
                char pct_buf[8];
                snprintf(pct_buf, sizeof(pct_buf), "%d%%", pct);
                UI_Label(x + 2, y + h - 10, pct_buf, UI_FONT_6PT,
                         BUILDING_R, BUILDING_G, BUILDING_B, 255);
            }
        }
    }

    // Flashing indicator
    if (strip.Flasher == actual_index) {
        g_ui_draw_list.Draw_Rect(x + 1, y + 1, w - 2, h - 2,
                                 255, 255, 0, 180);
    }
}

/// Emit scroll arrows for a strip column.
static void emit_scroll_arrows(int x, int y, int strip_w,
                                const SidebarClass::StripClass& strip)
{
    bool can_up   = strip.TopIndex > 0;
    bool can_down = strip.TopIndex + 4 < strip.BuildableCount;

    // Up arrow indicator
    uint8_t up_a = can_up ? (uint8_t)255 : (uint8_t)80;
    g_ui_draw_list.Fill_Rect(x + 2, y, 12, 8,
                             SCROLL_R, SCROLL_R, SCROLL_R, up_a);
    g_ui_draw_list.Draw_Text(x + 4, y, "UP", UI_FONT_6PT,
                             200, 200, 200, up_a);

    // Down arrow indicator
    uint8_t dn_a = can_down ? (uint8_t)255 : (uint8_t)80;
    g_ui_draw_list.Fill_Rect(x + strip_w - 14, y, 12, 8,
                             SCROLL_R, SCROLL_R, SCROLL_R, dn_a);
    g_ui_draw_list.Draw_Text(x + strip_w - 12, y, "DN", UI_FONT_6PT,
                             200, 200, 200, dn_a);
}

/// Emit one sidebar column.
static void emit_column(const SidebarClass::StripClass& strip, int factor)
{
    int col_x = strip.X;
    int col_y = strip.Y;
    int obj_w = strip.ObjectWidth;
    int obj_h = strip.ObjectHeight;
    int visible = 4; // MAX_VISIBLE

    // Column background
    int col_w = strip.StripWidth * factor;
    int col_h = obj_h * visible + 14;
    g_ui_draw_list.Fill_Rect(col_x, col_y, col_w, col_h,
                             34, 36, 34, 96);

    // Production slots
    UILayout layout;
    layout.Begin(col_x + strip.LeftEdgeOffset, col_y, obj_w, col_h,
                 UI_DIR_VERTICAL, 0);
    for (int i = 0; i < visible; i++) {
        int sx, sy;
        layout.Next(obj_w, obj_h, sx, sy);
        emit_slot(sx, sy, obj_w, obj_h, i, strip);
    }

    // Scroll arrows below slots
    emit_scroll_arrows(col_x, col_y + obj_h * visible + 1, col_w, strip);
}

void UI_Sidebar_Emit()
{
    extern bool InMainLoop;
    if (!InMainLoop) return;
    if (!Map.IsSidebarActive) return;

    int side_x = Map.SideX;
    int side_w = Map.SideBarWidth;
    int buf_h  = SeenBuff.Get_Height();
    if (side_w <= 0) return;

    int factor = (SeenBuff.Get_Width() > 400) ? 2 : 1;

    // Keep the legacy sidebar chrome visible and only tint it lightly until
    // cameo art, radar chrome, and button art are fully native.
    g_ui_draw_list.Fill_Rect(side_x, 0, side_w, buf_h,
                             24, 24, 24, 56);
    g_ui_draw_list.Fill_Rect(side_x, 0, 1, buf_h,
                             84, 84, 84, 220);

    // Emit each production column
    for (int c = 0; c < 2; c++) {
        const SidebarClass::StripClass& strip = Map.Column[c];
        if (strip.BuildableCount > 0 || true) {
            emit_column(strip, factor);
        }
    }

    // Repair/Sell/Map buttons area (below radar, above columns)
    // These are drawn as simple labeled boxes for now
    int btn_y = Map.RadY + Map.RadHeight + 2;
    int btn_w = side_w / 3;
    int btn_h = 10 * factor;

    UIButtonStyle bs = UI_Default_Button_Style();
    bs.normal_r = 54; bs.normal_g = 70; bs.normal_b = 54;
    bs.hover_r = 70; bs.hover_g = 94; bs.hover_b = 70;
    bs.press_r = 44; bs.press_g = 56; bs.press_b = 44;
    bs.text_r = 0; bs.text_g = 200; bs.text_b = 0;
    bs.font = UI_FONT_6PT;

    UI_Button(side_x + 2, btn_y, btn_w - 2, btn_h, "RPR", bs);
    UI_Button(side_x + btn_w + 1, btn_y, btn_w - 2, btn_h, "SEL", bs);
    UI_Button(side_x + btn_w * 2, btn_y, btn_w - 2, btn_h, "MAP", bs);
}
