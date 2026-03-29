/**
 * debug_hud.cpp — On-screen debug overlay for the render bridge.
 *
 * Semi-transparent panel in top-left showing rendering metrics.
 * Uses the engine's Simple_Text_Print for proper font rendering.
 * Always visible in debug builds. Toggle-able via F11 in future.
 *
 * Only compiled in debug builds (DEBUG defined).
 */

#include "render_bridge.h"
#include "draw_list.h"
#include "function.h"
#include <cstdio>
#include <SDL3/SDL.h>

#ifdef DEBUG

extern bool GL_Present_Is_Active();
extern int  GL_Sprites_Atlas_Frame_Count();
extern int  GL_Sprites_Atlas_Page_Count();

static int      g_fps = 0;
static int      g_fps_counter = 0;
static uint64_t g_fps_timer = 0;
static float    g_frame_ms = 0.0f;
static uint64_t g_last_frame = 0;

// Memory tracking
static size_t g_draw_list_peak = 0;

void Render_Bridge_Debug_HUD()
{
    extern bool InMainLoop;
    if (!InMainLoop) return;

    // Frame timing
    uint64_t now = SDL_GetTicks();
    if (g_last_frame > 0) g_frame_ms = static_cast<float>(now - g_last_frame);
    g_last_frame = now;
    g_fps_counter++;
    if (now - g_fps_timer >= 1000) {
        g_fps = g_fps_counter;
        g_fps_counter = 0;
        g_fps_timer = now;
    }

    // Draw list stats
    int cmd_count = g_draw_list.Command_Count();
    int stamps = 0, shapes = 0, prims = 0;
    for (int i = 0; i < cmd_count; i++) {
        switch (g_draw_list.Get(i).type) {
            case CMD_STAMP: stamps++; break;
            case CMD_SHAPE: shapes++; break;
            default: prims++; break;
        }
    }
    if (static_cast<size_t>(cmd_count) > g_draw_list_peak)
        g_draw_list_peak = cmd_count;

    // Zoom info
    float zoom = Render_Bridge_Get_Zoom_Level();
    float vp_x = Render_Bridge_Get_Viewport_X();
    float vp_y = Render_Bridge_Get_Viewport_Y();

    // Screen and buffer info
    int buf_w = SeenBuff.Get_Width();
    int buf_h = SeenBuff.Get_Height();
    int win_w = 0, win_h = 0;
    extern SDL_Window* g_window;
    if (g_window) SDL_GetWindowSizeInPixels(g_window, &win_w, &win_h);

    // Tactical area
    int tac_w = Lepton_To_Pixel(Map.TacLeptonWidth);
    int tac_h = Lepton_To_Pixel(Map.TacLeptonHeight);

    // Atlas info
    int atlas_frames = GL_Sprites_Atlas_Frame_Count();
    int atlas_pages = GL_Sprites_Atlas_Page_Count();

    // Render mode
    const char* mode = GL_Present_Is_Active() ? "GL" : "SW";

    // Draw background panel
    int panel_x = Map.TacPixelX + 2;
    int panel_y = Map.TacPixelY + 2;
    int panel_w = 120;
    int panel_h = 64;

    uint8_t* buf = static_cast<uint8_t*>(LogicPage->Get_Buffer());
    if (!buf) return;
    int pitch = LogicPage->Get_Full_Pitch();
    int pw = LogicPage->Get_Width();
    int ph = LogicPage->Get_Height();

    for (int row = 0; row < panel_h && (panel_y + row) < ph; row++) {
        for (int col = 0; col < panel_w && (panel_x + col) < pw; col++) {
            buf[(panel_y + row) * pitch + (panel_x + col)] = 12;
        }
    }

    // Text lines using Simple_Text_Print
    char line[64];
    int y = panel_y + 2;
    int x = panel_x + 2;
    int fg = 15;  // white
    int bg = 12;  // dark gray (panel background)

    // Line 1: mode + FPS + frame time
    snprintf(line, sizeof(line), "%s %dfps %.0fms", mode, g_fps, g_frame_ms);
    Simple_Text_Print(line, x, y, fg, bg, TPF_6PT_GRAD|TPF_NOSHADOW);
    y += 8;

    // Line 2: screen + buffer
    snprintf(line, sizeof(line), "%dx%d > %dx%d", buf_w, buf_h, win_w, win_h);
    Simple_Text_Print(line, x, y, fg, bg, TPF_6PT_GRAD|TPF_NOSHADOW);
    y += 8;

    // Line 3: zoom + viewport
    snprintf(line, sizeof(line), "z%.1f vp(%d,%d)", zoom,
             static_cast<int>(vp_x), static_cast<int>(vp_y));
    Simple_Text_Print(line, x, y, fg, bg, TPF_6PT_GRAD|TPF_NOSHADOW);
    y += 8;

    // Line 4: tactical area
    snprintf(line, sizeof(line), "tac %dx%d", tac_w, tac_h);
    Simple_Text_Print(line, x, y, fg, bg, TPF_6PT_GRAD|TPF_NOSHADOW);
    y += 8;

    // Line 5: draw list stats
    snprintf(line, sizeof(line), "dl %d(%d/%d/%d)", cmd_count, stamps, shapes, prims);
    Simple_Text_Print(line, x, y, fg, bg, TPF_6PT_GRAD|TPF_NOSHADOW);
    y += 8;

    // Line 6: atlas stats
    snprintf(line, sizeof(line), "atlas %df %dp", atlas_frames, atlas_pages);
    Simple_Text_Print(line, x, y, 14, bg, TPF_6PT_GRAD|TPF_NOSHADOW); // yellow
    y += 8;

    // Line 7: peak draw list
    snprintf(line, sizeof(line), "peak %d", static_cast<int>(g_draw_list_peak));
    Simple_Text_Print(line, x, y, 14, bg, TPF_6PT_GRAD|TPF_NOSHADOW);
}

#else // !DEBUG

void Render_Bridge_Debug_HUD() {}

#endif // DEBUG
