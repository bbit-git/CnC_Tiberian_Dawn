/**
 * ui_tooltip.cpp — Tooltip and status message rendering.
 */

#include "ui_tooltip.h"
#include "ui_draw_list.h"
#include "ui_text.h"
#include <SDL3/SDL.h>
#include <cstring>

static constexpr int TOOLTIP_PAD     = 3;
static constexpr int TOOLTIP_OFFSET  = 12;
static constexpr int STATUS_MAX_MSGS = 4;
static constexpr int STATUS_MAX_LEN  = 80;

// Tooltip state
static char     g_tooltip_text[128] = {};
static UIFontID g_tooltip_font = UI_FONT_6PT;
static bool     g_tooltip_active = false;

// Status message queue
struct StatusMsg {
    char     text[STATUS_MAX_LEN];
    uint8_t  r, g, b;
    uint64_t expire_time;
    bool     active;
};
static StatusMsg g_status_msgs[STATUS_MAX_MSGS] = {};

void UI_Tooltip_Show(const char* text, UIFontID font)
{
    if (!text || !text[0]) {
        g_tooltip_active = false;
        return;
    }
    strncpy(g_tooltip_text, text, sizeof(g_tooltip_text) - 1);
    g_tooltip_text[sizeof(g_tooltip_text) - 1] = '\0';
    g_tooltip_font = font;
    g_tooltip_active = true;
}

void UI_Tooltip_Hide()
{
    g_tooltip_active = false;
}

void UI_Status_Push(const char* text, int duration_ms,
                    uint8_t r, uint8_t g, uint8_t b)
{
    if (!text || !text[0]) return;
    uint64_t now = SDL_GetTicks();

    // Find a free slot or overwrite oldest
    int slot = -1;
    uint64_t oldest = UINT64_MAX;
    int oldest_slot = 0;
    for (int i = 0; i < STATUS_MAX_MSGS; i++) {
        if (!g_status_msgs[i].active) { slot = i; break; }
        if (g_status_msgs[i].expire_time < oldest) {
            oldest = g_status_msgs[i].expire_time;
            oldest_slot = i;
        }
    }
    if (slot < 0) slot = oldest_slot;

    StatusMsg& m = g_status_msgs[slot];
    strncpy(m.text, text, STATUS_MAX_LEN - 1);
    m.text[STATUS_MAX_LEN - 1] = '\0';
    m.r = r; m.g = g; m.b = b;
    m.expire_time = now + duration_ms;
    m.active = true;
}

void UI_Tooltip_Emit(int mouse_x, int mouse_y, int screen_w, int screen_h)
{
    // --- Tooltip ---
    if (g_tooltip_active && g_tooltip_text[0]) {
        int len = static_cast<int>(strlen(g_tooltip_text));
        int tw = UI_Text_Measure_Width(g_tooltip_font, g_tooltip_text, len);
        int th = UI_Text_Line_Height(g_tooltip_font);
        int bw = tw + TOOLTIP_PAD * 2;
        int bh = th + TOOLTIP_PAD * 2;

        // Position below-right of cursor, clamped to screen
        int tx = mouse_x + TOOLTIP_OFFSET;
        int ty = mouse_y + TOOLTIP_OFFSET;
        if (tx + bw > screen_w) tx = mouse_x - bw - 2;
        if (ty + bh > screen_h) ty = mouse_y - bh - 2;
        if (tx < 0) tx = 0;
        if (ty < 0) ty = 0;

        g_ui_draw_list.Fill_Rect(tx, ty, bw, bh, 10, 10, 10, 230);
        g_ui_draw_list.Draw_Rect(tx, ty, bw, bh, 80, 80, 80, 255);
        g_ui_draw_list.Draw_Text(tx + TOOLTIP_PAD, ty + TOOLTIP_PAD,
                                 g_tooltip_text, g_tooltip_font,
                                 220, 220, 220, 255);
    }

    // --- Status messages ---
    uint64_t now = SDL_GetTicks();
    int sy = screen_h - 16;

    for (int i = 0; i < STATUS_MAX_MSGS; i++) {
        StatusMsg& m = g_status_msgs[i];
        if (!m.active) continue;
        if (now >= m.expire_time) { m.active = false; continue; }

        // Fade out in last 500ms
        uint64_t remaining = m.expire_time - now;
        uint8_t alpha = 255;
        if (remaining < 500) {
            alpha = static_cast<uint8_t>(remaining * 255 / 500);
        }

        int len = static_cast<int>(strlen(m.text));
        int tw = UI_Text_Measure_Width(UI_FONT_6PT, m.text, len);
        int tx = (screen_w - tw) / 2;

        g_ui_draw_list.Fill_Rect(tx - 4, sy - 1, tw + 8,
                                 UI_Text_Line_Height(UI_FONT_6PT) + 2,
                                 0, 0, 0, alpha / 2);
        g_ui_draw_list.Draw_Text(tx, sy, m.text, UI_FONT_6PT,
                                 m.r, m.g, m.b, alpha);
        sy -= UI_Text_Line_Height(UI_FONT_6PT) + 4;
    }
}
