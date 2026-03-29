/**
 * present.cpp — Render bridge presentation (GL or SDL fallback).
 *
 * Tries GL palette shader first (GPU palette lookup, no CPU conversion).
 * Falls back to SDL_Renderer + CPU palette LUT if GL unavailable.
 *
 * Flow:
 *   1. Apply scroll zoom
 *   2. Blit_Display copies HidPage → SeenBuff (done by engine)
 *   3. Present SeenBuff via GL palette shader or SDL CPU path
 */

#include <SDL3/SDL.h>
#include "render_bridge.h"
#include "function.h"

extern SDL_Window*   g_window;
extern SDL_Renderer* g_renderer;
extern SDL_Texture*  g_texture;

// GL present (gl_present.cpp)
extern bool GL_Present_Init(int w, int h);
extern void GL_Present_Shutdown();
extern bool GL_Present_Frame(const uint8_t* indexed_pixels, int pitch,
                              int w, int h, const uint8_t* vga_palette);

static int  g_shake_remaining = 0;
static bool g_gl_attempted = false;
static bool g_use_gl = false;

void Shake_Screen(int shakes)
{
    g_shake_remaining = shakes;
}

void Present_Ensure_Buffer(int w, int h)
{
    Render_Bridge_Init(w, h);

    if (g_window) {
        int sw = 0, sh = 0;
        SDL_GetWindowSizeInPixels(g_window, &sw, &sh);
        Render_Bridge_Set_Screen_Size(sw, sh);
    }
}

void Present_Shutdown(void)
{
    GL_Present_Shutdown();
    Render_Bridge_Shutdown();
}

/// SDL CPU fallback: palette LUT → RGBA → SDL_UpdateTexture
static void present_sdl(const uint8_t* pixels, int pitch, int w, int h,
                          const uint8_t* pal)
{
    static uint32_t pal_lut[256];
    static const uint8_t* pal_src = nullptr;
    static uint32_t pal_hash = 0;

    uint32_t hash = pal[0] | (pal[3] << 8) | (pal[384] << 16) | (pal[765] << 24);
    if (pal != pal_src || hash != pal_hash) {
        for (int i = 0; i < 256; i++) {
            uint8_t r = pal[i * 3 + 0] << 2;
            uint8_t g = pal[i * 3 + 1] << 2;
            uint8_t b = pal[i * 3 + 2] << 2;
            pal_lut[i] = (0xFFu << 24) | (b << 16) | (g << 8) | r;
        }
        pal_src = pal;
        pal_hash = hash;
    }

    static uint32_t* rgba = nullptr;
    static int rgba_size = 0;
    int needed = w * h;
    if (!rgba || rgba_size < needed) {
        free(rgba);
        rgba = static_cast<uint32_t*>(malloc(needed * sizeof(uint32_t)));
        rgba_size = needed;
    }
    if (!rgba) return;

    for (int y = 0; y < h; y++) {
        const uint8_t* row = pixels + y * pitch;
        uint32_t* dst = rgba + y * w;
        for (int x = 0; x < w; x++)
            dst[x] = pal_lut[row[x]];
    }

#ifndef __ANDROID__
    extern void TD_SDL_Draw_Mouse_RGBA(uint32_t* buf, int w, int h,
                                       const unsigned char* pal);
    TD_SDL_Draw_Mouse_RGBA(rgba, w, h, pal);
#endif

    SDL_UpdateTexture(g_texture, nullptr, rgba, w * 4);
    SDL_RenderClear(g_renderer);

    if (g_shake_remaining > 0) {
        int ox = (rand() % 5) - 2;
        int oy = (rand() % 5) - 2;
        SDL_FRect dst = {(float)ox, (float)oy, (float)w, (float)h};
        SDL_RenderTexture(g_renderer, g_texture, nullptr, &dst);
        g_shake_remaining--;
    } else {
        SDL_RenderTexture(g_renderer, g_texture, nullptr, nullptr);
    }

    SDL_RenderPresent(g_renderer);
}

void TD_SDL_Present(void)
{
    Render_Bridge_Apply_Scroll_Zoom();

    // Source: SeenBuff (8-bit indexed, after Blit_Display)
    const uint8_t* pixels = static_cast<const uint8_t*>(SeenBuff.Get_Buffer());
    if (!pixels) return;
    int w     = SeenBuff.Get_Width();
    int h     = SeenBuff.Get_Height();
    int pitch = SeenBuff.Get_Full_Pitch();

    const uint8_t* pal = static_cast<const uint8_t*>((void*)Get_Palette());
    if (!pal) pal = GamePalette;
    if (!pal) return;

    // Try GL path (once)
    if (!g_gl_attempted) {
        g_gl_attempted = true;
        g_use_gl = GL_Present_Init(w, h);
    }

    if (g_use_gl) {
        if (GL_Present_Frame(pixels, pitch, w, h, pal))
            return; // GL succeeded
    }

    // SDL fallback
    if (g_texture && g_renderer) {
        present_sdl(pixels, pitch, w, h, pal);
    }
}
