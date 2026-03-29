/**
 * init.cpp — Render bridge initialization hook for TD.
 *
 * Called from the game's init sequence after viewport dimensions are known.
 * Sets up the compositor with layers matching the game layout.
 */

#include "render_bridge.h"
#include "function.h"

/// Call after Set_Video_Mode / viewport configuration completes.
void Render_Bridge_Game_Init(void)
{
    int w = SeenBuff.Get_Width();
    int h = SeenBuff.Get_Height();
    if (w <= 0 || h <= 0) return;

    Render_Bridge_Init(w, h);
}

/// Call on resolution change or sidebar toggle.
void Render_Bridge_Game_Layout_Changed(void)
{
    Render_Bridge_Update_Layout();
}
