/**
 * debug_hud.cpp — On-screen debug overlay for the render bridge.
 *
 * Shows zoom level, viewport offset, draw list stats, and frame timing.
 * Drawn after the tactical zoom but before SDL present, so it's always
 * visible at 1:1 in the top-left of the tactical area.
 *
 * Only active in debug builds (DEBUG defined).
 */

#include "render_bridge.h"
#include "draw_list.h"
#include "function.h"
#include <cstdio>
#include <SDL3/SDL.h>

#ifdef DEBUG

static uint64_t g_last_frame_ms = 0;
static float    g_frame_time_ms = 0.0f;
static int      g_fps = 0;
static int      g_fps_counter = 0;
static uint64_t g_fps_timer = 0;

/// Draw a single character (6x8 font) to an 8-bit buffer.
/// Minimal bitmap font — digits, period, x, letters for labels.
static void draw_char(uint8_t* buf, int pitch, int bw, int bh,
                       int x, int y, char ch, uint8_t color)
{
    // Tiny 4x6 digit glyphs (enough for stats display)
    static const uint8_t digits[10][6] = {
        {0x6,0x9,0x9,0x9,0x9,0x6}, // 0
        {0x2,0x6,0x2,0x2,0x2,0x7}, // 1
        {0x6,0x9,0x2,0x4,0x8,0xF}, // 2
        {0x6,0x9,0x2,0x9,0x9,0x6}, // 3
        {0x9,0x9,0x9,0xF,0x1,0x1}, // 4
        {0xF,0x8,0xE,0x1,0x1,0xE}, // 5
        {0x6,0x8,0xE,0x9,0x9,0x6}, // 6
        {0xF,0x1,0x2,0x4,0x4,0x4}, // 7
        {0x6,0x9,0x6,0x9,0x9,0x6}, // 8
        {0x6,0x9,0x7,0x1,0x1,0x6}, // 9
    };

    const uint8_t* glyph = nullptr;
    if (ch >= '0' && ch <= '9') glyph = digits[ch - '0'];

    if (glyph) {
        for (int row = 0; row < 6; row++) {
            int py = y + row;
            if (py < 0 || py >= bh) continue;
            for (int col = 0; col < 4; col++) {
                int px = x + col;
                if (px < 0 || px >= bw) continue;
                if (glyph[row] & (0x8 >> col))
                    buf[py * pitch + px] = color;
            }
        }
    } else if (ch == '.') {
        if (y + 5 >= 0 && y + 5 < bh && x >= 0 && x < bw)
            buf[(y + 5) * pitch + x] = color;
    } else if (ch == 'x' || ch == 'X') {
        for (int i = 0; i < 4; i++) {
            int py = y + i + 1;
            if (py >= 0 && py < bh) {
                if (x + i >= 0 && x + i < bw) buf[py * pitch + x + i] = color;
                if (x + 3 - i >= 0 && x + 3 - i < bw) buf[py * pitch + x + 3 - i] = color;
            }
        }
    }
}

/// Draw a string of digits/symbols to an 8-bit buffer.
static void draw_string(uint8_t* buf, int pitch, int bw, int bh,
                          int x, int y, const char* str, uint8_t color)
{
    while (*str) {
        draw_char(buf, pitch, bw, bh, x, y, *str, color);
        x += 5;
        str++;
    }
}

void Render_Bridge_Debug_HUD()
{
    float zoom = Render_Bridge_Get_Zoom_Level();
    if (zoom == 1.0f) return; // Only show HUD when zoomed

    // Frame timing
    uint64_t now = SDL_GetTicks();
    if (g_last_frame_ms > 0) {
        g_frame_time_ms = static_cast<float>(now - g_last_frame_ms);
    }
    g_last_frame_ms = now;

    g_fps_counter++;
    if (now - g_fps_timer >= 1000) {
        g_fps = g_fps_counter;
        g_fps_counter = 0;
        g_fps_timer = now;
    }

    // Draw to HidPage in tactical area corner
    uint8_t* buf = static_cast<uint8_t*>(HidPage.Get_Buffer());
    if (!buf) return;
    int pitch = HidPage.Get_Full_Pitch();
    int bw = HidPage.Get_Width();
    int bh = HidPage.Get_Height();

    int tac_x = Map.TacPixelX;
    int tac_y = Map.TacPixelY;

    // Background rect
    int hud_x = tac_x + 2;
    int hud_y = tac_y + 2;
    int hud_w = 45;
    int hud_h = 22;
    for (int row = 0; row < hud_h && (hud_y + row) < bh; row++) {
        for (int col = 0; col < hud_w && (hud_x + col) < bw; col++) {
            buf[(hud_y + row) * pitch + (hud_x + col)] = 12; // dark gray
        }
    }

    uint8_t text_color = 15; // white

    // Line 1: zoom level (e.g. "1.50x")
    char line[32];
    snprintf(line, sizeof(line), "%d.%02dx",
             static_cast<int>(zoom),
             static_cast<int>((zoom - static_cast<int>(zoom)) * 100));
    draw_string(buf, pitch, bw, bh, hud_x + 2, hud_y + 2, line, text_color);

    // Line 2: draw list count
    int count = g_draw_list.Command_Count();
    snprintf(line, sizeof(line), "%d", count);
    draw_string(buf, pitch, bw, bh, hud_x + 2, hud_y + 9, line, text_color);

    // Line 3: FPS
    snprintf(line, sizeof(line), "%d", g_fps);
    draw_string(buf, pitch, bw, bh, hud_x + 2, hud_y + 16, line, 14); // yellow
}

#else // !DEBUG

void Render_Bridge_Debug_HUD() {}

#endif // DEBUG
