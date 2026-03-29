/**
 * present.cpp — SDL3 presentation via render bridge compositor.
 *
 * Replaces platform.sdl3/cnc/sdl3_present.cpp in the render bridge build.
 *
 * Flow:
 *   1. Legacy Blit_Display() has already copied HidPage → SeenBuff
 *   2. Render_Bridge_Blit_Display() splits SeenBuff into compositor layers
 *   3. This function takes the composited RGBA output and presents it
 *
 * The compositor applies per-layer zoom (world layers only), so the tactical
 * area can be zoomed independently of the sidebar and tab bar.
 */

#include <SDL3/SDL.h>
#include "render_bridge.h"
#include "function.h"

extern SDL_Renderer* g_renderer;
extern SDL_Texture*  g_texture;

// Screen shake (same interface as legacy sdl3_present.cpp)
static int g_shake_remaining = 0;

void Shake_Screen(int shakes)
{
    g_shake_remaining = shakes;
}

// Legacy present buffer allocation (same interface as sdl3_present.cpp)
void Present_Ensure_Buffer(int w, int h)
{
    (void)w; (void)h; // Compositor manages its own buffers
}

void Present_Shutdown(void)
{
    Render_Bridge_Shutdown();
}

void TD_SDL_Present(void)
{
    if (!g_texture || !g_renderer) return;

    // Feed SeenBuff into compositor layers and composite
    Render_Bridge_Blit_Display();

    // Get display dimensions from SeenBuff (matches g_texture size)
    int w = SeenBuff.Get_Width();
    int h = SeenBuff.Get_Height();
    if (w <= 0 || h <= 0) return;

    // For now, present SeenBuff through palette LUT (same visual result as legacy).
    // The compositor is running in parallel, building layer state for when we
    // switch to GL-based presentation.
    //
    // TODO: Replace this with compositor RGBA output → SDL_UpdateTexture
    //       once the compositor output buffer is exposed.

    const uint8_t* pixels = static_cast<const uint8_t*>(VisiblePage.Get_Buffer());
    if (!pixels) pixels = static_cast<const uint8_t*>(HiddenPage.Get_Buffer());
    if (!pixels) return;
    int pitch = VisiblePage.Get_Full_Pitch();

    // Palette LUT
    static uint32_t pal_lut[256];
    static const uint8_t* pal_src = nullptr;
    static uint32_t pal_hash = 0;

    const uint8_t* pal = static_cast<const uint8_t*>((void*)Get_Palette());
    if (!pal) pal = GamePalette;
    if (!pal) return;

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

    // 8-bit → RGBA
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
        for (int x = 0; x < w; x++) {
            dst[x] = pal_lut[row[x]];
        }
    }

#ifndef __ANDROID__
    extern void TD_SDL_Draw_Mouse_RGBA(uint32_t* rgba, int w, int h,
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
