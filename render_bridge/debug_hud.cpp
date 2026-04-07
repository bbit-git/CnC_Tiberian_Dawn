/**
 * debug_hud.cpp — Debug overlay rendered via GL on top of everything.
 *
 * Outputs stats to stderr via DBG() and renders a small RGBA texture
 * as a GL quad in the bottom-left corner of the screen.
 * Only active in debug builds.
 */

#include "render_bridge.h"
#include "draw_list.h"
#include "function.h"
#include "dbg.h"
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cstdlib>
#include <SDL3/SDL.h>

#undef min
#undef max
#include <GLES2/gl2.h>

extern bool GL_Present_Is_Active();
extern int  GL_Sprites_Atlas_Frame_Count();
extern int  GL_Sprites_Atlas_Page_Count();
extern int  GL_Sprites_Last_Atlas_New_Count();
extern int  GL_Sprites_Last_Sprite_Count();
extern int  GL_Sprites_Last_Draw_Calls();
extern int  GL_Sprites_Last_Fallback_Count();
extern int  GL_Sprites_Last_Terrain_Miss_Count();
extern int  GL_Sprites_Terrain_Atlas_Count();
extern uint32_t GL_Sprites_Hit_Test_Terrain(float native_x, float native_y, int& icon_out,
                                             bool& has_hd_out);
extern int  GL_Primitives_Last_Count();
extern SDL_Window* g_window;
extern void Render_Bridge_Get_Render_Tactical_Rect(int& x, int& y, int& w, int& h);
extern void Render_Bridge_Get_Effective_Clamp_Size(int& w, int& h);

static int      g_fps = 0;
static int      g_fps_counter = 0;
static uint64_t g_fps_timer = 0;
static float    g_frame_ms = 0.0f;
static uint64_t g_last_frame = 0;
static size_t   g_dl_peak = 0;

// GL resources for HUD overlay
static GLuint g_hud_tex = 0;
static GLuint g_hud_prog = 0;
static bool   g_hud_gl_ready = false;
static bool   g_debug_hud_visible = true;
static bool   g_debug_bars_visible = true;
static bool   g_debug_sources_visible = false;
static bool   g_dbg_no_scroll_clamp = false;
static bool   g_dbg_no_vp_clamp = false;
static bool   g_dbg_no_gl_cull = false;
static bool   g_dbg_no_cell_cull = false;
static int    g_hud_screen_x = 0;
static int    g_hud_screen_y = 0;
static bool   g_hud_dragging = false;
static bool   g_hud_mouse_was_down = false;
static int    g_hud_drag_off_x = 0;
static int    g_hud_drag_off_y = 0;

// HUD bitmap
static constexpr int HUD_W = 160;
static constexpr int HUD_H = 408;
static constexpr int HUD_SCREEN_W = 320;
static constexpr int HUD_SCREEN_H = 680;
static uint32_t g_hud_pixels[HUD_W * HUD_H];
static int      g_cb_row[4] = {HUD_H, HUD_H, HUD_H, HUD_H};

// 4x6 bitmap font
static const uint8_t font_4x6[][6] = {
    {0x6,0x9,0x9,0x9,0x9,0x6},{0x2,0x6,0x2,0x2,0x2,0x7},
    {0x6,0x9,0x2,0x4,0x8,0xF},{0xE,0x1,0x6,0x1,0x1,0xE},
    {0x9,0x9,0xF,0x1,0x1,0x1},{0xF,0x8,0xE,0x1,0x1,0xE},
    {0x6,0x8,0xE,0x9,0x9,0x6},{0xF,0x1,0x2,0x4,0x4,0x4},
    {0x6,0x9,0x6,0x9,0x9,0x6},{0x6,0x9,0x7,0x1,0x1,0x6},
    {0x6,0x9,0x9,0xF,0x9,0x9},{0xE,0x9,0xE,0x9,0x9,0xE},
    {0x6,0x9,0x8,0x8,0x9,0x6},{0xE,0x9,0x9,0x9,0x9,0xE},
    {0xF,0x8,0xE,0x8,0x8,0xF},{0xF,0x8,0xE,0x8,0x8,0x8},
    {0x6,0x9,0x8,0xB,0x9,0x6},{0x9,0x9,0xF,0x9,0x9,0x9},
    {0x7,0x2,0x2,0x2,0x2,0x7},{0x1,0x1,0x1,0x1,0x9,0x6},
    {0x9,0xA,0xC,0xA,0x9,0x9},{0x8,0x8,0x8,0x8,0x8,0xF},
    {0x9,0xF,0xF,0x9,0x9,0x9},{0x9,0xD,0xB,0x9,0x9,0x9},
    {0x6,0x9,0x9,0x9,0x9,0x6},{0xE,0x9,0xE,0x8,0x8,0x8},
    {0x6,0x9,0x9,0xB,0x9,0x7},{0xE,0x9,0xE,0xA,0x9,0x9},
    {0x7,0x8,0x6,0x1,0x1,0xE},{0x7,0x2,0x2,0x2,0x2,0x2},
    {0x9,0x9,0x9,0x9,0x9,0x6},{0x9,0x9,0x9,0x9,0x6,0x6},
    {0x9,0x9,0x9,0xF,0xF,0x9},{0x9,0x9,0x6,0x6,0x9,0x9},
    {0x9,0x9,0x6,0x2,0x2,0x2},{0xF,0x1,0x2,0x4,0x8,0xF},
};

static void hud_putc(int x, int y, char ch, uint32_t color)
{
    const uint8_t* glyph = nullptr;
    if (ch >= '0' && ch <= '9') glyph = font_4x6[ch - '0'];
    else if (ch >= 'A' && ch <= 'Z') glyph = font_4x6[ch - 'A' + 10];
    else if (ch >= 'a' && ch <= 'z') glyph = font_4x6[ch - 'a' + 10];
    else if (ch == '.') { if (x>=0&&x<HUD_W&&y+5>=0&&y+5<HUD_H) g_hud_pixels[(y+5)*HUD_W+x]=color; return; }
    else if (ch == ':') { if (x>=0&&x<HUD_W) { if(y+1>=0&&y+1<HUD_H) g_hud_pixels[(y+1)*HUD_W+x]=color; if(y+4>=0&&y+4<HUD_H) g_hud_pixels[(y+4)*HUD_W+x]=color; } return; }
    else if (ch == '-') { for(int c=0;c<4;c++){int px=x+c,py=y+3;if(px>=0&&px<HUD_W&&py>=0&&py<HUD_H)g_hud_pixels[py*HUD_W+px]=color;} return; }
    else if (ch == '(' || ch == ')' || ch == '/' || ch == '>' || ch == ' ') return;
    else if (ch == '[') {
        static const uint8_t g[] = {0xE,0x8,0x8,0x8,0x8,0xE};
        glyph = g;
    }
    else if (ch == ']') {
        static const uint8_t g[] = {0x7,0x1,0x1,0x1,0x1,0x7};
        glyph = g;
    }
    if (!glyph) return;
    for (int r=0;r<6;r++) for (int c=0;c<4;c++)
        if (glyph[r]&(0x8>>c)) { int px=x+c,py=y+r; if(px>=0&&px<HUD_W&&py>=0&&py<HUD_H) g_hud_pixels[py*HUD_W+px]=color; }
}

static void hud_puts(int x, int y, const char* s, uint32_t color)
{
    while (*s) { hud_putc(x, y, *s, color); x += 5; s++; }
}

static bool init_hud_gl()
{
    if (g_hud_gl_ready) return true;
    const char* vs = "attribute vec2 a_pos; attribute vec2 a_uv; varying vec2 v_uv;"
                     "void main() { gl_Position = vec4(a_pos, 0.0, 1.0); v_uv = a_uv; }";
    const char* fs = "precision mediump float; varying vec2 v_uv; uniform sampler2D u_tex;"
                     "void main() { vec4 c = texture2D(u_tex, v_uv); if (c.a < 0.1) discard; gl_FragColor = c; }";
    GLuint v = glCreateShader(GL_VERTEX_SHADER); glShaderSource(v,1,&vs,0); glCompileShader(v);
    GLuint f = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(f,1,&fs,0); glCompileShader(f);
    g_hud_prog = glCreateProgram();
    glAttachShader(g_hud_prog,v); glAttachShader(g_hud_prog,f);
    glBindAttribLocation(g_hud_prog,0,"a_pos"); glBindAttribLocation(g_hud_prog,1,"a_uv");
    glLinkProgram(g_hud_prog); glDeleteShader(v); glDeleteShader(f);

    glGenTextures(1, &g_hud_tex);
    glBindTexture(GL_TEXTURE_2D, g_hud_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, HUD_W, HUD_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    g_hud_gl_ready = true;
    return true;
}

static void dump_line(const char* fmt, ...)
{
    char line[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);

    DBG("%s", line);

    FILE* fp = fopen("/tmp/ea_render_bridge_dump.log", "a");
    if (!fp) {
        return;
    }
    fprintf(fp, "%s\n", line);
    fclose(fp);
}

static void update_hud_drag(int win_w, int win_h)
{
    float mouse_x = 0.0f;
    float mouse_y = 0.0f;
    uint32_t buttons = SDL_GetMouseState(&mouse_x, &mouse_y);
    bool left_down = (buttons & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) != 0;

    bool inside = mouse_x >= g_hud_screen_x &&
                  mouse_x < g_hud_screen_x + HUD_SCREEN_W &&
                  mouse_y >= g_hud_screen_y &&
                  mouse_y < g_hud_screen_y + HUD_SCREEN_H;

    bool consumed = false;
    if (left_down && !g_hud_mouse_was_down && inside) {
        int hud_rel_x = static_cast<int>(mouse_x) - g_hud_screen_x;
        int hud_rel_y = static_cast<int>(mouse_y) - g_hud_screen_y;
        int tex_x = hud_rel_x * HUD_W / HUD_SCREEN_W;
        int tex_y = hud_rel_y * HUD_H / HUD_SCREEN_H;

        if (tex_x >= 0 && tex_x < HUD_W) {
            // tex_x comes from hud_rel_x ∈ [0, HUD_SCREEN_W) so this guard is defensive.
            bool* cb_flags[4] = {
                &g_dbg_no_scroll_clamp,
                &g_dbg_no_vp_clamp,
                &g_dbg_no_gl_cull,
                &g_dbg_no_cell_cull,
            };
            for (int i = 0; i < 4; i++) {
                if (tex_y >= g_cb_row[i] && tex_y < g_cb_row[i] + 8) {
                    *cb_flags[i] = !*cb_flags[i];
                    consumed = true;
                    break;
                }
            }
        }

        if (!consumed) {
            g_hud_dragging = true;
            g_hud_drag_off_x = static_cast<int>(mouse_x) - g_hud_screen_x;
            g_hud_drag_off_y = static_cast<int>(mouse_y) - g_hud_screen_y;
        }
    } else if (!left_down) {
        g_hud_dragging = false;
    }

    if (g_hud_dragging) {
        g_hud_screen_x = static_cast<int>(mouse_x) - g_hud_drag_off_x;
        g_hud_screen_y = static_cast<int>(mouse_y) - g_hud_drag_off_y;
    }

    if (g_hud_screen_x < 0) g_hud_screen_x = 0;
    if (g_hud_screen_y < 0) g_hud_screen_y = 0;
    if (g_hud_screen_x > win_w - HUD_SCREEN_W) g_hud_screen_x = win_w - HUD_SCREEN_W;
    if (g_hud_screen_y > win_h - HUD_SCREEN_H) g_hud_screen_y = win_h - HUD_SCREEN_H;
    if (g_hud_screen_x < 0) g_hud_screen_x = 0;
    if (g_hud_screen_y < 0) g_hud_screen_y = 0;

    g_hud_mouse_was_down = left_down;
}

void Render_Bridge_Debug_Dump()
{
    int cmd_count = g_draw_list.Command_Count();
    int stamps = 0;
    int shapes = 0;
    int prims = 0;
    for (int i = 0; i < cmd_count; i++) {
        switch (g_draw_list.Get(i).type) {
        case CMD_STAMP: stamps++; break;
        case CMD_SHAPE: shapes++; break;
        default: prims++; break;
        }
    }

    float zoom = Render_Bridge_Get_Zoom_Level();
    float zoom_default = Render_Bridge_Get_Default_Zoom();
    int log_w = 0, log_h = 0;
    Render_Bridge_Get_Logical_Screen_Size(log_w, log_h);
    int tac_x, tac_y, tac_w, tac_h;
    Render_Bridge_Get_Tactical_Rect(tac_x, tac_y, tac_w, tac_h);
    int hdr_x, hdr_y, hdr_w, hdr_h;
    Render_Bridge_Get_Header_Rect(hdr_x, hdr_y, hdr_w, hdr_h);
    int side_x, side_y, side_w, side_h;
    Render_Bridge_Get_Sidebar_Rect(side_x, side_y, side_w, side_h);
    int ntac_w = 0, ntac_h = 0;
    Render_Bridge_Get_Native_World_Rect(ntac_w, ntac_h);
    int atlas_f = GL_Sprites_Atlas_Frame_Count();
    int atlas_p = GL_Sprites_Atlas_Page_Count();
    int atlas_new = GL_Sprites_Last_Atlas_New_Count();
    int gl_sprites = GL_Sprites_Last_Sprite_Count();
    int gl_draws = GL_Sprites_Last_Draw_Calls();
    int gl_fallback = GL_Sprites_Last_Fallback_Count();
    int gl_prims = GL_Primitives_Last_Count();
    float vp_x = Render_Bridge_Get_Viewport_X();
    float vp_y = Render_Bridge_Get_Viewport_Y();
    float vp_target_x = 0.0f;
    float vp_target_y = 0.0f;
    Render_Bridge_Get_Viewport_Target(vp_target_x, vp_target_y);
    int scroll_dx = 0;
    int scroll_dy = 0;
    Render_Bridge_Get_Scroll_Delta(scroll_dx, scroll_dy);
    float vis_w = 0.0f;
    float vis_h = 0.0f;
    Render_Bridge_Get_Visible_Size(vis_w, vis_h);
    int vp_w = (vis_w > 0.0f) ? static_cast<int>(vis_w) : tac_w;
    int vp_h = (vis_h > 0.0f) ? static_cast<int>(vis_h) : tac_h;
    if (vp_w > ntac_w && ntac_w > 0) vp_w = ntac_w;
    if (vp_h > ntac_h && ntac_h > 0) vp_h = ntac_h;
    float vp_max_x = (ntac_w > vp_w) ? ntac_w - vp_w : 0;
    float vp_max_y = (ntac_h > vp_h) ? ntac_h - vp_h : 0;

    int tac_coord_px = Lepton_To_Pixel(Coord_X(Map.TacticalCoord) - Cell_To_Lepton(Map.MapCellX));
    int tac_coord_py = Lepton_To_Pixel(Coord_Y(Map.TacticalCoord) - Cell_To_Lepton(Map.MapCellY));
    int req_x = 0;
    int req_y = 0;
    Render_Bridge_Get_Requested_Tactical_Position(req_x, req_y);
    int clamp_max_x = 0;
    int clamp_max_y = 0;
    Render_Bridge_Get_Clamp_Ranges(clamp_max_x, clamp_max_y);
    int tac_range_x = Lepton_To_Pixel(clamp_max_x);
    int tac_range_y = Lepton_To_Pixel(clamp_max_y);

    bool at_L = req_x <= 0;
    bool at_R = tac_range_x > 0 && req_x >= clamp_max_x;
    bool at_T = req_y <= 0;
    bool at_B = tac_range_y > 0 && req_y >= clamp_max_y;

    extern int g_mouse_x, g_mouse_y;
    bool mL = g_mouse_x <= tac_x + 1;
    bool mR = g_mouse_x >= tac_x + tac_w - 2;
    bool mT = g_mouse_y <= tac_y + 1;
    bool mB = g_mouse_y >= tac_y + tac_h - 2;

    int map_cw = Map.MapCellWidth;
    int map_ch = Map.MapCellHeight;
    int win_w = 0;
    int win_h = 0;
    if (g_window) {
        SDL_GetWindowSizeInPixels(g_window, &win_w, &win_h);
    }

    int rtac_x = 0, rtac_y = 0, rtac_w = 0, rtac_h = 0;
    Render_Bridge_Get_Render_Tactical_Rect(rtac_x, rtac_y, rtac_w, rtac_h);
    int eff_clamp_w = 0, eff_clamp_h = 0;
    Render_Bridge_Get_Effective_Clamp_Size(eff_clamp_w, eff_clamp_h);

    dump_line("bridge: LOG %dx%d NAT %dx%d", log_w, log_h, ntac_w, ntac_h);
    dump_line("bridge: TAC %d,%d %dx%d", tac_x, tac_y, tac_w, tac_h);
    dump_line("bridge: TAC R %d,%d %dx%d", rtac_x, rtac_y, rtac_w, rtac_h);
    dump_line("bridge: CLAMP %dx%d", Lepton_To_Pixel(eff_clamp_w), Lepton_To_Pixel(eff_clamp_h));
    dump_line("bridge: HDR %d,%d %dx%d", hdr_x, hdr_y, hdr_w, hdr_h);
    dump_line("bridge: SID %d,%d %dx%d", side_x, side_y, side_w, side_h);
    dump_line("bridge: Z %.2f DEF %.2f VIS %dx%d VP %.0f-%.0f", zoom, zoom_default, vp_w, vp_h, vp_x, vp_y);
    dump_line("bridge: VP MAX %.0f-%.0f", vp_max_x, vp_max_y);
    dump_line("bridge: VP TG %.0f-%.0f SD %d-%d", vp_target_x, vp_target_y, scroll_dx, scroll_dy);
    dump_line("bridge: TC %d-%d REQ %d-%d RNG %d-%d", tac_coord_px, tac_coord_py, req_x, req_y, tac_range_x, tac_range_y);
    dump_line("bridge: WORLD %d-%d", req_x, req_y);
    dump_line("bridge: BND %c%c%c%c MSE %c%c%c%c",
              at_L ? 'L' : '.', at_R ? 'R' : '.', at_T ? 'T' : '.', at_B ? 'B' : '.',
              mL ? 'L' : '.', mR ? 'R' : '.', mT ? 'T' : '.', mB ? 'B' : '.');
    dump_line("bridge: DL %d S %d T %d P %d", cmd_count, shapes, stamps, prims);
    dump_line("bridge: ATL %dF %dP GL S %d D %d CPU S %d GL P %d",
              atlas_f, atlas_p, gl_sprites, gl_draws, gl_fallback, gl_prims);
    dump_line("bridge: ATL NEW %d", atlas_new);
    dump_line("bridge: MAP %dx%d SCR %dx%d MSE %d-%d", map_cw, map_ch, win_w, win_h,
              g_mouse_x, g_mouse_y);
}

/// Render debug HUD as a GL overlay. Call from GL_Present_Frame before SwapWindow.
void Render_Bridge_Debug_HUD_GL(int win_w, int win_h)
{
    extern bool InMainLoop;
    if (!InMainLoop) return;
    if (!g_debug_hud_visible) return;
    if (!init_hud_gl()) return;
    update_hud_drag(win_w, win_h);

    // Update timing
    uint64_t now = SDL_GetTicks();
    if (g_last_frame > 0) g_frame_ms = static_cast<float>(now - g_last_frame);
    g_last_frame = now;
    g_fps_counter++;
    if (now - g_fps_timer >= 1000) { g_fps = g_fps_counter; g_fps_counter = 0; g_fps_timer = now; }

    // Gather stats
    int cmd_count = g_draw_list.Command_Count();
    int stamps=0, shapes=0, prims=0;
    for (int i=0; i<cmd_count; i++) {
        switch(g_draw_list.Get(i).type) { case CMD_STAMP:stamps++;break; case CMD_SHAPE:shapes++;break; default:prims++;break; }
    }
    if ((size_t)cmd_count > g_dl_peak) g_dl_peak = cmd_count;

    float zoom = Render_Bridge_Get_Zoom_Level();
    float zoom_default = Render_Bridge_Get_Default_Zoom();
    int log_w = 0, log_h = 0;
    Render_Bridge_Get_Logical_Screen_Size(log_w, log_h);
    int tac_x, tac_y, tac_w, tac_h;
    Render_Bridge_Get_Tactical_Rect(tac_x, tac_y, tac_w, tac_h);
    int hdr_x, hdr_y, hdr_w, hdr_h;
    Render_Bridge_Get_Header_Rect(hdr_x, hdr_y, hdr_w, hdr_h);
    int side_x, side_y, side_w, side_h;
    Render_Bridge_Get_Sidebar_Rect(side_x, side_y, side_w, side_h);
    int ntac_w = 0, ntac_h = 0;
    Render_Bridge_Get_Native_World_Rect(ntac_w, ntac_h);
    int rtac_x2 = 0, rtac_y2 = 0, rtac_w2 = 0, rtac_h2 = 0;
    Render_Bridge_Get_Render_Tactical_Rect(rtac_x2, rtac_y2, rtac_w2, rtac_h2);
    int eff_clamp_w2 = 0, eff_clamp_h2 = 0;
    Render_Bridge_Get_Effective_Clamp_Size(eff_clamp_w2, eff_clamp_h2);
    int atlas_f = GL_Sprites_Atlas_Frame_Count(), atlas_p = GL_Sprites_Atlas_Page_Count();
    int atlas_new = GL_Sprites_Last_Atlas_New_Count();
    int gl_sprites = GL_Sprites_Last_Sprite_Count(), gl_draws = GL_Sprites_Last_Draw_Calls();
    int gl_fallback = GL_Sprites_Last_Fallback_Count();
    int gl_prims = GL_Primitives_Last_Count();

    // Viewport = visible portion of native buffer at current zoom
    float vp_x = Render_Bridge_Get_Viewport_X();
    float vp_y = Render_Bridge_Get_Viewport_Y();
    float vp_target_x = 0.0f;
    float vp_target_y = 0.0f;
    Render_Bridge_Get_Viewport_Target(vp_target_x, vp_target_y);
    int scroll_dx = 0;
    int scroll_dy = 0;
    Render_Bridge_Get_Scroll_Delta(scroll_dx, scroll_dy);
    float vis_w = 0.0f;
    float vis_h = 0.0f;
    Render_Bridge_Get_Visible_Size(vis_w, vis_h);
    int vp_w = (vis_w > 0.0f) ? static_cast<int>(vis_w) : tac_w;
    int vp_h = (vis_h > 0.0f) ? static_cast<int>(vis_h) : tac_h;
    if (vp_w > ntac_w && ntac_w > 0) vp_w = ntac_w;
    if (vp_h > ntac_h && ntac_h > 0) vp_h = ntac_h;
    float vp_max_x = (ntac_w > vp_w) ? ntac_w - vp_w : 0;
    float vp_max_y = (ntac_h > vp_h) ? ntac_h - vp_h : 0;

    // TacticalCoord in pixels relative to map origin
    int tac_coord_px = Lepton_To_Pixel(Coord_X(Map.TacticalCoord) - Cell_To_Lepton(Map.MapCellX));
    int tac_coord_py = Lepton_To_Pixel(Coord_Y(Map.TacticalCoord) - Cell_To_Lepton(Map.MapCellY));
    int req_x = 0;
    int req_y = 0;
    Render_Bridge_Get_Requested_Tactical_Position(req_x, req_y);
    int clamp_max_x = 0;
    int clamp_max_y = 0;
    Render_Bridge_Get_Clamp_Ranges(clamp_max_x, clamp_max_y);
    int tac_range_x = Lepton_To_Pixel(clamp_max_x);
    int tac_range_y = Lepton_To_Pixel(clamp_max_y);

    // Map size in cells
    int map_cw = Map.MapCellWidth, map_ch = Map.MapCellHeight;

    // Boundary flags
    bool at_L = req_x <= 0;
    bool at_R = tac_range_x > 0 && req_x >= clamp_max_x;
    bool at_T = req_y <= 0;
    bool at_B = tac_range_y > 0 && req_y >= clamp_max_y;

    // Mouse edge flags
    extern int g_mouse_x, g_mouse_y;
    bool mL = g_mouse_x <= tac_x + 1;
    bool mR = g_mouse_x >= tac_x + tac_w - 2;
    bool mT = g_mouse_y <= tac_y + 1;
    bool mB = g_mouse_y >= tac_y + tac_h - 2;

    // Clear HUD bitmap
    memset(g_hud_pixels, 0, sizeof(g_hud_pixels));
    for (int i = 0; i < HUD_W * HUD_H; i++) g_hud_pixels[i] = 0xCC000000;

    uint32_t white  = 0xFFFFFFFF;
    uint32_t yellow = 0xFF00FFFF;
    uint32_t green  = 0xFF00FF00;
    uint32_t cyan   = 0xFFFFFF00;
    uint32_t red    = 0xFF0000FF;
    uint32_t blue   = 0xFFFF0000;
    uint32_t orange = 0xFF0080FF;
    uint32_t violet = 0xFFFF40A0;
    uint32_t magenta = 0xFFF040FF;
    char line[48];
    int y = 2;

    snprintf(line, sizeof(line), "%s %dFPS %.0fMS", GL_Present_Is_Active()?"GL":"SW", g_fps, g_frame_ms);
    hud_puts(2, y, line, white); y += 8;

    snprintf(line, sizeof(line), "LOG%dX%d SCR%dX%d", log_w, log_h, win_w, win_h);
    hud_puts(2, y, line, white); y += 8;

    snprintf(line, sizeof(line), "MAP %dX%d", map_cw, map_ch);
    hud_puts(2, y, line, white); y += 8;

    snprintf(line, sizeof(line), "NAT %dX%d TAC%dX%d", ntac_w, ntac_h, tac_w, tac_h);
    hud_puts(2, y, line, green); y += 8;

    snprintf(line, sizeof(line), "TACR %dX%d", rtac_w2, rtac_h2);
    hud_puts(2, y, line, cyan); y += 8;

    snprintf(line, sizeof(line), "CLAMP %dX%d", Lepton_To_Pixel(eff_clamp_w2), Lepton_To_Pixel(eff_clamp_h2));
    hud_puts(2, y, line, red); y += 8;

    snprintf(line, sizeof(line), "HDR%d.%d SID%d.%d", hdr_w, hdr_h, side_x, side_w);
    hud_puts(2, y, line, yellow); y += 8;

    snprintf(line, sizeof(line), "Z%.2f D%.2f V%dX%d", zoom, zoom_default, vp_w, vp_h);
    hud_puts(2, y, line, cyan); y += 8;

    // VP top-left and range
    snprintf(line, sizeof(line), "VP %d-%d MAX%d-%d",
             (int)vp_x, (int)vp_y, (int)vp_max_x, (int)vp_max_y);
    hud_puts(2, y, line, cyan); y += 8;

    snprintf(line, sizeof(line), "TG %d-%d SD%d-%d",
             (int)vp_target_x, (int)vp_target_y, scroll_dx, scroll_dy);
    hud_puts(2, y, line, yellow); y += 8;

    // TacticalCoord position and range
    snprintf(line, sizeof(line), "TC %d-%d RQ%d-%d",
             tac_coord_px, tac_coord_py, req_x, req_y);
    hud_puts(2, y, line, white); y += 8;

    snprintf(line, sizeof(line), "RNG %d-%d W %d-%d",
             tac_range_x, tac_range_y, req_x, req_y);
    hud_puts(2, y, line, green); y += 8;

    // Boundary and mouse-edge flags
    snprintf(line, sizeof(line), "BND %c%c%c%c MSE %c%c%c%c",
             at_L?'L':'.', at_R?'R':'.', at_T?'T':'.', at_B?'B':'.',
             mL?'L':'.', mR?'R':'.', mT?'T':'.', mB?'B':'.');
    hud_puts(2, y, line, (at_L||at_R||at_T||at_B) ? red : white); y += 8;

    // Draw list stats
    snprintf(line, sizeof(line), "DL%d S%d T%d P%d", cmd_count, shapes, stamps, prims);
    hud_puts(2, y, line, yellow); y += 8;

    snprintf(line, sizeof(line), "ATL%dF %dP N%d", atlas_f, atlas_p, atlas_new);
    hud_puts(2, y, line, yellow); y += 8;

    snprintf(line, sizeof(line), "PK%d", (int)g_dl_peak);
    hud_puts(2, y, line, yellow); y += 8;

    snprintf(line, sizeof(line), "GL S%d D%d", gl_sprites, gl_draws);
    hud_puts(2, y, line, green); y += 8;

    snprintf(line, sizeof(line), "CPU S%d", gl_fallback);
    hud_puts(2, y, line, gl_fallback > 0 ? red : green); y += 8;

    snprintf(line, sizeof(line), "GL P%d", gl_prims);
    hud_puts(2, y, line, green); y += 8;

    // Terrain tile stats: HD hits vs misses
    int terrain_miss = GL_Sprites_Last_Terrain_Miss_Count();
    int terrain_cached = GL_Sprites_Terrain_Atlas_Count();
    snprintf(line, sizeof(line), "TRN %dHD %dMIS", terrain_cached, terrain_miss);
    hud_puts(2, y, line, terrain_miss > 0 ? orange : green); y += 8;

    // Mouse position: mapped (bridge-transformed) and raw (window-scaled)
    extern void TD_SDL_Get_Raw_Mouse_Position(int& x, int& y);
    int raw_mx = 0, raw_my = 0;
    TD_SDL_Get_Raw_Mouse_Position(raw_mx, raw_my);
    snprintf(line, sizeof(line), "MSE %d-%d", g_mouse_x, g_mouse_y);
    hud_puts(2, y, line, white); y += 8;

    snprintf(line, sizeof(line), "RAW %d-%d", raw_mx, raw_my);
    hud_puts(2, y, line, orange); y += 8;

    // Mouse-over terrain tile: convert mouse to native buffer coords and hit-test.
    {
        // Mouse is in game-buffer coords (g_mouse_x/y). Tactical window origin
        // gives the buffer offset. Stamps are stored in window-local coords.
        int tac_ox = WindowList[WINDOW_TACTICAL][WINDOWX] << 3;
        int tac_oy = WindowList[WINDOW_TACTICAL][WINDOWY];
        float native_mx = static_cast<float>(g_mouse_x - tac_ox) + vp_x;
        float native_my = static_cast<float>(g_mouse_y - tac_oy) + vp_y;

        int tile_icon = 0;
        bool tile_has_hd = false;
        uint32_t tile_hash = GL_Sprites_Hit_Test_Terrain(native_mx, native_my,
                                                          tile_icon, tile_has_hd);
        if (tile_hash) {
            snprintf(line, sizeof(line), "TILE %08X I%d", tile_hash, tile_icon);
            hud_puts(2, y, line, tile_has_hd ? green : red); y += 8;
            snprintf(line, sizeof(line), "%s", tile_has_hd ? "HD" : "NO HD");
            hud_puts(2, y, line, tile_has_hd ? green : red); y += 8;
        } else {
            hud_puts(2, y, "TILE NONE", white); y += 8;
        }
    }

    // Debug rect legend — matches gl_present_debug.cpp
    y += 2;
    hud_puts(2, y, "RECT LEGEND", white); y += 8;

    hud_puts(2, y, "BUF", violet);
    hud_puts(30, y, "GAME BUFFER", white); y += 8;

    hud_puts(2, y, "HDR", yellow);
    hud_puts(30, y, "HEADER STRIP", white); y += 8;

    hud_puts(2, y, "SID", blue);
    hud_puts(30, y, "SIDEBAR", white); y += 8;

    hud_puts(2, y, "TAC", green);
    hud_puts(30, y, "TACTICAL VIS", white); y += 8;

    hud_puts(2, y, "RTAC", 0xFF006600);
    hud_puts(30, y, "RENDER TAC", white); y += 8;

    hud_puts(2, y, "WORLD", red);
    hud_puts(40, y, "FIT QUAD", white); y += 8;

    hud_puts(2, y, "NATIVE", cyan);
    hud_puts(45, y, "REPLAY BUF", white); y += 8;

    hud_puts(2, y, "VP", white);
    hud_puts(20, y, "VIEWPORT", white); y += 8;

    hud_puts(2, y, "VPMAX", orange);
    hud_puts(40, y, "VP LIMIT", white); y += 8;

    hud_puts(2, y, "REQ", yellow);
    hud_puts(25, y, "SCROLL POS", white); y += 8;

    hud_puts(2, y, "RMAX", 0xFF00B0B0);
    hud_puts(35, y, "SCROLL LIM", white); y += 8;

    hud_puts(2, y, "MAP", white);
    hud_puts(25, y, "FULL MAP", white); y += 8;

    hud_puts(2, y, "VIS", magenta);
    hud_puts(25, y, "WORLD RECT", white); y += 8;

    {
        uint32_t sfill_col = 0xFFFF6633; // R=0x33,G=0x66,B=0xFF — matches GL_Debug_Rect 0.2,0.4,1.0
        hud_puts(2, y, "SFILL", sfill_col);
        hud_puts(40, y, "SHROUD FILL", white); y += 8;
    }

    y += 2;
    hud_puts(2, y, "TOGGLES", white); y += 8;

    const bool* cb_flags[4] = {
        &g_dbg_no_scroll_clamp,
        &g_dbg_no_vp_clamp,
        &g_dbg_no_gl_cull,
        &g_dbg_no_cell_cull,
    };
    const char* cb_labels[4] = {
        "NO SCLAMP",
        "NO VPCLAMP",
        "NO GLCULL",
        "NO CELLCULL",
    };
    for (int i = 0; i < 4; i++) {
        g_cb_row[i] = y;
        uint32_t color = *cb_flags[i] ? red : white;
        hud_puts(2, y, *cb_flags[i] ? "[X]" : "[ ]", color);
        hud_puts(22, y, cb_labels[i], color);
        y += 8;
    }

    // Upload and render
    glBindTexture(GL_TEXTURE_2D, g_hud_tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, HUD_W, HUD_H, GL_RGBA, GL_UNSIGNED_BYTE, g_hud_pixels);

    glUseProgram(g_hud_prog);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_hud_tex);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glUniform1i(glGetUniformLocation(g_hud_prog, "u_tex"), 0);

    // Draggable fixed-size debug panel in window pixel space.
    float x0 = static_cast<float>(g_hud_screen_x) / win_w * 2.0f - 1.0f;
    float y0 = 1.0f - static_cast<float>(g_hud_screen_y) / win_h * 2.0f;
    float x1 = static_cast<float>(g_hud_screen_x + HUD_SCREEN_W) / win_w * 2.0f - 1.0f;
    float y1 = 1.0f - static_cast<float>(g_hud_screen_y + HUD_SCREEN_H) / win_h * 2.0f;
    float verts[] = {
        x0, y0, 0.0f, 0.0f,
        x1, y0, 1.0f, 0.0f,
        x0, y1, 0.0f, 1.0f,
        x1, y1, 1.0f, 1.0f,
    };

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, &verts[0]);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, &verts[2]);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisable(GL_BLEND);
}

/// Legacy stub — no longer draws to game buffer.
void Render_Bridge_Debug_HUD() {}

void Render_Bridge_Debug_HUD_Toggle()
{
    g_debug_hud_visible = !g_debug_hud_visible;
    DBG("bridge: debug hud %s", g_debug_hud_visible ? "on" : "off");
}

bool Render_Bridge_Debug_HUD_Enabled()
{
    return g_debug_hud_visible;
}

void Render_Bridge_Debug_Bars_Toggle()
{
    g_debug_bars_visible = !g_debug_bars_visible;
    DBG("bridge: debug bars %s", g_debug_bars_visible ? "on" : "off");
}

bool Render_Bridge_Debug_Bars_Enabled()
{
    return g_debug_bars_visible;
}

void Render_Bridge_Debug_Sources_Toggle()
{
    g_debug_sources_visible = !g_debug_sources_visible;
    DBG("bridge: debug sources %s", g_debug_sources_visible ? "on" : "off");
}

bool Render_Bridge_Debug_Sources_Enabled()
{
    return g_debug_sources_visible;
}

bool Render_Bridge_Debug_No_Scroll_Clamp()
{
    return g_dbg_no_scroll_clamp;
}

bool Render_Bridge_Debug_No_VP_Clamp()
{
    return g_dbg_no_vp_clamp;
}

bool Render_Bridge_Debug_No_GL_Cull()
{
    return g_dbg_no_gl_cull;
}

bool Render_Bridge_Debug_No_Cell_Cull()
{
    return g_dbg_no_cell_cull;
}
