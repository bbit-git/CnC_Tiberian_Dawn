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
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <SDL3/SDL.h>

#ifdef DEBUG

#undef min
#undef max
#include <GLES2/gl2.h>

extern bool GL_Present_Is_Active();
extern int  GL_Sprites_Atlas_Frame_Count();
extern int  GL_Sprites_Atlas_Page_Count();
extern SDL_Window* g_window;
extern int Render_Bridge_Get_Native_Tac_W();
extern int Render_Bridge_Get_Native_Tac_H();

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

// HUD bitmap: 128x64 RGBA
static constexpr int HUD_W = 128;
static constexpr int HUD_H = 64;
static uint32_t g_hud_pixels[HUD_W * HUD_H];

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
    else if (ch == '(' || ch == ')' || ch == '/' || ch == '>' || ch == ' ') return;
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

/// Render debug HUD as a GL overlay. Call from GL_Present_Frame before SwapWindow.
void Render_Bridge_Debug_HUD_GL(int win_w, int win_h)
{
    extern bool InMainLoop;
    if (!InMainLoop) return;
    if (!init_hud_gl()) return;

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
    int buf_w = SeenBuff.Get_Width(), buf_h = SeenBuff.Get_Height();
    int tac_w = Lepton_To_Pixel(Map.TacLeptonWidth), tac_h = Lepton_To_Pixel(Map.TacLeptonHeight);
    int ntac_w = Render_Bridge_Get_Native_Tac_W(), ntac_h = Render_Bridge_Get_Native_Tac_H();
    int atlas_f = GL_Sprites_Atlas_Frame_Count(), atlas_p = GL_Sprites_Atlas_Page_Count();

    // Clear HUD bitmap
    memset(g_hud_pixels, 0, sizeof(g_hud_pixels));

    // Background
    for (int i = 0; i < HUD_W * HUD_H; i++) g_hud_pixels[i] = 0xCC000000; // semi-transparent black

    uint32_t white  = 0xFFFFFFFF;
    uint32_t yellow = 0xFF00FFFF;
    uint32_t green  = 0xFF00FF00;
    char line[48];
    int y = 2;

    snprintf(line, sizeof(line), "%s %dFPS %.0fMS", GL_Present_Is_Active()?"GL":"SW", g_fps, g_frame_ms);
    hud_puts(2, y, line, white); y += 8;

    snprintf(line, sizeof(line), "%dX%d..%dX%d", buf_w, buf_h, win_w, win_h);
    hud_puts(2, y, line, white); y += 8;

    snprintf(line, sizeof(line), "Z%.1f TAC%dX%d", zoom, tac_w, tac_h);
    hud_puts(2, y, line, white); y += 8;

    if (ntac_w > 0) {
        snprintf(line, sizeof(line), "NAT %dX%d", ntac_w, ntac_h);
        hud_puts(2, y, line, green); y += 8;
    }

    snprintf(line, sizeof(line), "DL%d S%d T%d P%d", cmd_count, shapes, stamps, prims);
    hud_puts(2, y, line, yellow); y += 8;

    snprintf(line, sizeof(line), "ATL%dF %dP PK%d", atlas_f, atlas_p, (int)g_dl_peak);
    hud_puts(2, y, line, yellow); y += 8;

    // Upload and render
    glBindTexture(GL_TEXTURE_2D, g_hud_tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, HUD_W, HUD_H, GL_RGBA, GL_UNSIGNED_BYTE, g_hud_pixels);

    glUseProgram(g_hud_prog);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_hud_tex);
    glUniform1i(glGetUniformLocation(g_hud_prog, "u_tex"), 0);

    // Bottom-left corner, 256x128 screen pixels
    float qw = 256.0f / win_w * 2.0f;
    float qh = 128.0f / win_h * 2.0f;
    float verts[] = {
        -1.0f,        -1.0f + qh,  0.0f, 0.0f,
        -1.0f + qw,   -1.0f + qh,  1.0f, 0.0f,
        -1.0f,        -1.0f,       0.0f, 1.0f,
        -1.0f + qw,   -1.0f,       1.0f, 1.0f,
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

#else
void Render_Bridge_Debug_HUD() {}
void Render_Bridge_Debug_HUD_GL(int, int) {}
#endif
