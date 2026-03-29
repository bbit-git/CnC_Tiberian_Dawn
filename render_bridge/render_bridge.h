/**
 * render_bridge.h — Bridge between legacy engine rendering and libs/render/.
 *
 * Provides compositor-based layer separation for the existing Draw_It() chain.
 * The legacy code continues to render into HidPage as before; the bridge
 * splits HidPage into compositor layers (world, sidebar, radar) and presents
 * via the new rendering pipeline.
 *
 * Build with USE_RENDER_BRIDGE defined to activate.
 * Without it, the legacy Blit_Display → TD_SDL_Present path is used.
 */

#ifndef CNC_RENDER_BRIDGE_H
#define CNC_RENDER_BRIDGE_H

#include "render_compositor.h"
#include "render_layer.h"

/// Initialize the render bridge. Call once after game viewport is configured.
bool Render_Bridge_Init(int output_width, int output_height);

/// Shut down and free bridge resources.
void Render_Bridge_Shutdown();

/// Update layer geometry from current game state (sidebar position, tactical area).
/// Call when viewport dimensions change (e.g. sidebar toggle, resolution change).
void Render_Bridge_Update_Layout();

/// Copy HidPage regions into compositor layers, composite, and present.
/// Replaces GScreenClass::Blit_Display() in the render bridge build.
void Render_Bridge_Blit_Display();

/// Present the composited frame to the window.
/// Replaces TD_SDL_Present() in the render bridge build.
void Render_Bridge_Present();

/// Set world zoom factor (1.0 = normal). Applies to tactical area only.
void Render_Bridge_Set_Zoom(float zoom);

/// Get current world zoom factor.
float Render_Bridge_Get_Zoom();

/// Get the compositor (for direct layer access if needed).
RenderCompositor* Render_Bridge_Get_Compositor();

#endif // CNC_RENDER_BRIDGE_H
