/**
 * present.cpp — SDL3 presentation via render bridge compositor.
 *
 * Replaces platform.sdl3/cnc/sdl3_present.cpp in the render bridge build.
 *
 * Flow:
 *   1. Legacy Blit_Display() copies HidPage → SeenBuff (unchanged)
 *   2. Apply pending scroll zoom
 *   3. Render_Bridge_Blit_Display() splits SeenBuff into compositor layers
 *   4. Compositor composites layers (world with zoom, UI fixed)
 *   5. Composited RGBA output → SDL_UpdateTexture → present
 */

#include <SDL3/SDL.h>
#include "render_bridge.h"
#include "function.h"

extern SDL_Renderer* g_renderer;
extern SDL_Texture*  g_texture;

static int g_shake_remaining = 0;

void Shake_Screen(int shakes)
{
    g_shake_remaining = shakes;
}

void Present_Ensure_Buffer(int w, int h)
{
    Render_Bridge_Init(w, h);
}

void Present_Shutdown(void)
{
    Render_Bridge_Shutdown();
}

void TD_SDL_Present(void)
{
    if (!g_texture || !g_renderer) return;

    // Apply pending scroll zoom (mouse transform moved to input pump)
    Render_Bridge_Apply_Scroll_Zoom();

    // Split SeenBuff into compositor layers and composite
    Render_Bridge_Blit_Display();

    // Get composited RGBA output
    RenderCompositor* comp = Render_Bridge_Get_Compositor();
    if (!comp) return;

    const void* rgba = comp->Get_Output();
    int w = comp->Get_Output_Width();
    int h = comp->Get_Output_Height();
    if (!rgba || w <= 0 || h <= 0) return;

    // Mouse cursor compositing on the RGBA output
#ifndef __ANDROID__
    {
        const uint8_t* pal = static_cast<const uint8_t*>((void*)Get_Palette());
        if (!pal) pal = GamePalette;
        if (pal) {
            extern void TD_SDL_Draw_Mouse_RGBA(uint32_t* rgba, int w, int h,
                                               const unsigned char* pal);
            TD_SDL_Draw_Mouse_RGBA(const_cast<uint32_t*>(
                static_cast<const uint32_t*>(rgba)), w, h, pal);
        }
    }
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
