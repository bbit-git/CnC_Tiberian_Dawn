/**
 * debug_hud.cpp — On-screen debug overlay for the render bridge.
 *
 * Draws directly to HidPage using a built-in 4x6 bitmap font.
 * No dependency on engine font system (works before fonts are loaded).
 * Visible during gameplay in debug builds.
 */

#include "render_bridge.h"
#include "draw_list.h"
#include "function.h"
#include <cstdio>
#include <cstring>
#include <SDL3/SDL.h>

#ifdef DEBUG

extern bool GL_Present_Is_Active();
extern int  GL_Sprites_Atlas_Frame_Count();
extern int  GL_Sprites_Atlas_Page_Count();
extern SDL_Window* g_window;

static int      g_fps = 0;
static int      g_fps_counter = 0;
static uint64_t g_fps_timer = 0;
static float    g_frame_ms = 0.0f;
static uint64_t g_last_frame = 0;
static size_t   g_dl_peak = 0;

// 4x6 bitmap font: digits 0-9, uppercase A-Z, lowercase a-z, symbols
static const uint8_t font_4x6[][6] = {
    {0x6,0x9,0x9,0x9,0x9,0x6}, // 0
    {0x2,0x6,0x2,0x2,0x2,0x7}, // 1
    {0x6,0x9,0x2,0x4,0x8,0xF}, // 2
    {0xE,0x1,0x6,0x1,0x1,0xE}, // 3
    {0x9,0x9,0xF,0x1,0x1,0x1}, // 4
    {0xF,0x8,0xE,0x1,0x1,0xE}, // 5
    {0x6,0x8,0xE,0x9,0x9,0x6}, // 6
    {0xF,0x1,0x2,0x4,0x4,0x4}, // 7
    {0x6,0x9,0x6,0x9,0x9,0x6}, // 8
    {0x6,0x9,0x7,0x1,0x1,0x6}, // 9
    // A-Z (indices 10-35)
    {0x6,0x9,0x9,0xF,0x9,0x9}, // A
    {0xE,0x9,0xE,0x9,0x9,0xE}, // B
    {0x6,0x9,0x8,0x8,0x9,0x6}, // C
    {0xE,0x9,0x9,0x9,0x9,0xE}, // D
    {0xF,0x8,0xE,0x8,0x8,0xF}, // E
    {0xF,0x8,0xE,0x8,0x8,0x8}, // F
    {0x6,0x9,0x8,0xB,0x9,0x6}, // G
    {0x9,0x9,0xF,0x9,0x9,0x9}, // H
    {0x7,0x2,0x2,0x2,0x2,0x7}, // I
    {0x1,0x1,0x1,0x1,0x9,0x6}, // J
    {0x9,0xA,0xC,0xA,0x9,0x9}, // K
    {0x8,0x8,0x8,0x8,0x8,0xF}, // L
    {0x9,0xF,0xF,0x9,0x9,0x9}, // M
    {0x9,0xD,0xB,0x9,0x9,0x9}, // N
    {0x6,0x9,0x9,0x9,0x9,0x6}, // O
    {0xE,0x9,0xE,0x8,0x8,0x8}, // P
    {0x6,0x9,0x9,0xB,0x9,0x7}, // Q
    {0xE,0x9,0xE,0xA,0x9,0x9}, // R
    {0x7,0x8,0x6,0x1,0x1,0xE}, // S
    {0x7,0x2,0x2,0x2,0x2,0x2}, // T
    {0x9,0x9,0x9,0x9,0x9,0x6}, // U
    {0x9,0x9,0x9,0x9,0x6,0x6}, // V
    {0x9,0x9,0x9,0xF,0xF,0x9}, // W
    {0x9,0x9,0x6,0x6,0x9,0x9}, // X
    {0x9,0x9,0x6,0x2,0x2,0x2}, // Y
    {0xF,0x1,0x2,0x4,0x8,0xF}, // Z
};

static void put_char(uint8_t* buf, int pitch, int bw, int bh,
                      int x, int y, char ch, uint8_t color)
{
    const uint8_t* glyph = nullptr;
    if (ch >= '0' && ch <= '9') glyph = font_4x6[ch - '0'];
    else if (ch >= 'A' && ch <= 'Z') glyph = font_4x6[ch - 'A' + 10];
    else if (ch >= 'a' && ch <= 'z') glyph = font_4x6[ch - 'a' + 10]; // uppercase fallback
    else if (ch == '.') { if (y+5 >= 0 && y+5 < bh && x >= 0 && x < bw) buf[(y+5)*pitch+x] = color; return; }
    else if (ch == ':') { if (y+1 >= 0 && y+1 < bh && x >= 0 && x < bw) buf[(y+1)*pitch+x] = color;
                          if (y+4 >= 0 && y+4 < bh && x >= 0 && x < bw) buf[(y+4)*pitch+x] = color; return; }
    else if (ch == '/') { for (int i=0;i<6;i++) { int px=x+(5-i)/2, py=y+i; if(px>=0&&px<bw&&py>=0&&py<bh) buf[py*pitch+px]=color; } return; }
    else if (ch == '(') { for (int i=0;i<6;i++) { int px=x+(i==0||i==5?1:0), py=y+i; if(px>=0&&px<bw&&py>=0&&py<bh) buf[py*pitch+px]=color; } return; }
    else if (ch == ')') { for (int i=0;i<6;i++) { int px=x+(i==0||i==5?0:1), py=y+i; if(px>=0&&px<bw&&py>=0&&py<bh) buf[py*pitch+px]=color; } return; }
    else if (ch == '>') { for (int i=0;i<3;i++) { int py=y+i; if(x+i>=0&&x+i<bw&&py>=0&&py<bh) buf[py*pitch+x+i]=color; }
                          for (int i=0;i<3;i++) { int py=y+5-i; if(x+i>=0&&x+i<bw&&py>=0&&py<bh) buf[py*pitch+x+i]=color; } return; }
    else if (ch == ',') { if (y+5 >= 0 && y+5 < bh && x >= 0 && x < bw) buf[(y+5)*pitch+x] = color;
                          if (y+6 >= 0 && y+6 < bh && x >= 0 && x < bw) buf[(y+6)*pitch+x] = color; return; }
    else if (ch == ' ') return;
    else return;

    if (!glyph) return;
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
}

static void put_str(uint8_t* buf, int pitch, int bw, int bh,
                     int x, int y, const char* s, uint8_t color)
{
    while (*s) { put_char(buf, pitch, bw, bh, x, y, *s, color); x += 5; s++; }
}

void Render_Bridge_Debug_HUD()
{
    extern bool InMainLoop;
    if (!InMainLoop) return;

    uint64_t now = SDL_GetTicks();
    if (g_last_frame > 0) g_frame_ms = static_cast<float>(now - g_last_frame);
    g_last_frame = now;
    g_fps_counter++;
    if (now - g_fps_timer >= 1000) { g_fps = g_fps_counter; g_fps_counter = 0; g_fps_timer = now; }

    int cmd_count = g_draw_list.Command_Count();
    int stamps=0, shapes=0, prims=0;
    for (int i = 0; i < cmd_count; i++) {
        switch (g_draw_list.Get(i).type) {
            case CMD_STAMP: stamps++; break;
            case CMD_SHAPE: shapes++; break;
            default: prims++; break;
        }
    }
    if (static_cast<size_t>(cmd_count) > g_dl_peak) g_dl_peak = cmd_count;

    float zoom = Render_Bridge_Get_Zoom_Level();
    float vp_x = Render_Bridge_Get_Viewport_X();
    float vp_y = Render_Bridge_Get_Viewport_Y();
    int buf_w = SeenBuff.Get_Width();
    int buf_h = SeenBuff.Get_Height();
    int win_w = 0, win_h = 0;
    if (g_window) SDL_GetWindowSizeInPixels(g_window, &win_w, &win_h);
    int tac_w = Lepton_To_Pixel(Map.TacLeptonWidth);
    int tac_h = Lepton_To_Pixel(Map.TacLeptonHeight);
    int atlas_f = GL_Sprites_Atlas_Frame_Count();
    int atlas_p = GL_Sprites_Atlas_Page_Count();
    const char* mode = GL_Present_Is_Active() ? "GL" : "SW";

    extern int Render_Bridge_Get_Native_Tac_W();
    extern int Render_Bridge_Get_Native_Tac_H();
    int ntac_w = Render_Bridge_Get_Native_Tac_W();
    int ntac_h = Render_Bridge_Get_Native_Tac_H();

    uint8_t* buf = static_cast<uint8_t*>(LogicPage->Get_Buffer());
    if (!buf) return;
    int pitch = LogicPage->Get_Full_Pitch();
    int bw = LogicPage->Get_Width();
    int bh = LogicPage->Get_Height();

    // Panel position: bottom-left of tactical area
    int panel_w = 95;
    int panel_h = 66;
    int panel_x = Map.TacPixelX + 2;
    int panel_y = Map.TacPixelY + Lepton_To_Pixel(Map.TacLeptonHeight) - panel_h - 2;

    // Draw semi-transparent panel background
    for (int r = 0; r < panel_h && (panel_y+r) < bh; r++)
        for (int c = 0; c < panel_w && (panel_x+c) < bw; c++)
            if (panel_y+r >= 0 && panel_x+c >= 0)
                buf[(panel_y+r) * pitch + (panel_x+c)] = 12;

    int y = panel_y + 2;
    int x = panel_x + 2;
    char line[48];

    // Line 1: MODE FPS FRAMETIME
    snprintf(line, sizeof(line), "%s %dFPS %.0fMS", mode, g_fps, g_frame_ms);
    put_str(buf, pitch, bw, bh, x, y, line, 15); y += 8;

    // Line 2: BUFFER > SCREEN
    snprintf(line, sizeof(line), "%dX%d>%dX%d", buf_w, buf_h, win_w, win_h);
    put_str(buf, pitch, bw, bh, x, y, line, 15); y += 8;

    // Line 3: ZOOM VIEWPORT
    snprintf(line, sizeof(line), "Z%.1f VP(%d,%d)", zoom, (int)vp_x, (int)vp_y);
    put_str(buf, pitch, bw, bh, x, y, line, 15); y += 8;

    // Line 4: TACTICAL AREA (game buffer)
    snprintf(line, sizeof(line), "TAC %dX%d", tac_w, tac_h);
    put_str(buf, pitch, bw, bh, x, y, line, 15); y += 8;

    // Line 5: NATIVE TACTICAL BUFFER (screen resolution)
    if (ntac_w > 0 && ntac_h > 0) {
        snprintf(line, sizeof(line), "NAT %dX%d", ntac_w, ntac_h);
        put_str(buf, pitch, bw, bh, x, y, line, 13); y += 8; // green
    }

    // Line 6: DRAW LIST
    snprintf(line, sizeof(line), "DL %d(%d/%d/%d)", cmd_count, stamps, shapes, prims);
    put_str(buf, pitch, bw, bh, x, y, line, 14); y += 8;

    // Line 7: ATLAS + PEAK
    snprintf(line, sizeof(line), "ATL %dF %dP PK%d", atlas_f, atlas_p, (int)g_dl_peak);
    put_str(buf, pitch, bw, bh, x, y, line, 14); y += 8;
}

#else
void Render_Bridge_Debug_HUD() {}
#endif
