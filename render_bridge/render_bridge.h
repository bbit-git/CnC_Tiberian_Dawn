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

/// Set screen pixel dimensions for zoom range and native tactical calculation.
void Render_Bridge_Set_Screen_Size(int screen_w, int screen_h);

/// Get stored screen pixel dimensions.
void Render_Bridge_Get_Screen_Size(int& w, int& h);

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

/// Apply accumulated mouse scroll zoom delta (centered on cursor). Call once per frame.
void Render_Bridge_Apply_Scroll_Zoom();

/// Refresh g_mouse_x/y from raw tactical screen space for bridge input handling.
/// Call after Apply_Scroll_Zoom, before the game reads mouse position.
void Render_Bridge_Transform_Mouse();

/// Map a tactical screen-space point into the bridge-visible source space.
bool Render_Bridge_Map_Tactical_Point(int screen_x, int screen_y, int& mapped_x, int& mapped_y);

/// Record the requested tactical world origin before legacy native-size clamping.
void Render_Bridge_Record_Tactical_Request(int x, int y);

/// Record tactical world origin only if no explicit bridge request is pending.
void Render_Bridge_Record_Tactical_Request_Fallback(int x, int y);

/// Get the requested visible world origin in map-relative pixels.
void Render_Bridge_Get_Requested_Tactical_Position(int& x, int& y);

/// Zoom to a specific level centered on a screen-space point (for pinch zoom).
void Render_Bridge_Zoom_At(float new_zoom, int center_x, int center_y);

/// Current zoom level (1.0 = normal, >1.0 = zoomed in).
float Render_Bridge_Get_Zoom_Level();

/// Get the startup/default zoom level derived from the current setup.
float Render_Bridge_Get_Default_Zoom();

/// Viewport offset in source tactical pixels (top-left of visible region).
float Render_Bridge_Get_Viewport_X();
float Render_Bridge_Get_Viewport_Y();

/// Visible region size in native tactical pixels after zoom/aspect fitting.
void Render_Bridge_Get_Visible_Size(float& w, float& h);

/// Get the native tactical replay size in lepton space for bridge-aware clamps.
void Render_Bridge_Get_Visible_Size_Leptons(int& w, int& h);

/// Dump current render-bridge HUD stats to the debug log.
void Render_Bridge_Debug_Dump();

/// Toggle the render-bridge debug HUD visibility.
void Render_Bridge_Debug_HUD_Toggle();

/// Return whether the render-bridge debug HUD is visible.
bool Render_Bridge_Debug_HUD_Enabled();

/// Toggle the render-bridge debug overlay bars and boxes visibility.
void Render_Bridge_Debug_Bars_Toggle();

/// Return whether the render-bridge debug overlay bars and boxes are visible.
bool Render_Bridge_Debug_Bars_Enabled();

/// Toggle the render-source visualization overlay.
void Render_Bridge_Debug_Sources_Toggle();

/// Return whether the render-source visualization overlay is visible.
bool Render_Bridge_Debug_Sources_Enabled();

/// Returns true when scroll position clamping is bypassed (debug).
bool Render_Bridge_Debug_No_Scroll_Clamp();

/// Returns true when viewport clamping inside the native buffer is bypassed (debug).
bool Render_Bridge_Debug_No_VP_Clamp();

/// Returns true when GL sprite screen-space culling is bypassed (debug).
bool Render_Bridge_Debug_No_GL_Cull();

/// Returns true when cell-in-view culling always returns visible (debug).
bool Render_Bridge_Debug_No_Cell_Cull();

/// Get the last proportional viewport target in native tactical pixels.
void Render_Bridge_Get_Viewport_Target(float& x, float& y);

/// Get the last TacticalCoord delta in pixels.
void Render_Bridge_Get_Scroll_Delta(int& x, int& y);

// --- Bridge-owned screen layout (Phase 1: Geometry Authority) ---
// The bridge caches and owns the logical screen model. Callers must use these
// accessors instead of reading Map.TacPixelX, Map.SideX, WindowList, etc.

/// Refresh the cached screen layout from current game state.
/// Called automatically by Apply_Scroll_Zoom and Set_Screen_Size.
void Render_Bridge_Refresh_Layout();

/// Logical screen size in game-buffer pixels (SeenBuff dimensions).
void Render_Bridge_Get_Logical_Screen_Size(int& w, int& h);

/// Header/tab rect in game-buffer pixels (strip above the tactical area).
void Render_Bridge_Get_Header_Rect(int& x, int& y, int& w, int& h);

/// Sidebar rect in game-buffer pixels.
void Render_Bridge_Get_Sidebar_Rect(int& x, int& y, int& w, int& h);

/// Tactical screen rect in game-buffer pixels.
/// This is the authoritative tactical area — callers must not recompute from Map.*.
void Render_Bridge_Get_Tactical_Rect(int& x, int& y, int& w, int& h);

/// Render tactical rect — sidebar-independent.
/// Always spans the full content width below the header regardless of sidebar state.
/// Used for GL world presentation so toggling the sidebar does not change render
/// scale, visible size, or zoom behaviour.
void Render_Bridge_Get_Render_Tactical_Rect(int& x, int& y, int& w, int& h);

/// Tactical record rect in legacy buffer pixels used while capturing draw-list commands.
void Render_Bridge_Get_Record_Tactical_Rect(int& x, int& y, int& w, int& h);

/// Record a solid-black shroud fill rect with native-buffer-relative coordinates and
/// LAYER_SHADOW, so the native buffer replay includes it (not the overlay pass which
/// is skipped in GL mode). Returns true when recording; caller falls back to
/// LogicPage->Fill_Rect only when false (CPU path).
bool Render_Bridge_Record_Shroud_Fill_Rect(int x, int y, int w, int h);

/// Mouse input rect in game-buffer pixels (area where clicks become tactical actions).
void Render_Bridge_Get_Mouse_Input_Rect(int& x, int& y, int& w, int& h);

/// Native world replay texture size in pixels.
void Render_Bridge_Get_Native_World_Rect(int& w, int& h);

/// Visible world rect in leptons (origin + size of what the bridge shows).
/// Origin is map-relative (0,0 = top-left of playable map).
void Render_Bridge_Get_Visible_World_Rect(int& origin_x, int& origin_y,
                                           int& width, int& height);

/// Maximum scroll position in map-relative leptons. Scrolling beyond this
/// would move the visible window past the map edge.
void Render_Bridge_Get_Clamp_Ranges(int& max_x, int& max_y);

/// Effective clamp window size in leptons. When the sidebar is active this
/// shrinks by the sidebar's world-space coverage, giving more scroll room
/// so the player can reach the map edge behind the sidebar.
void Render_Bridge_Get_Effective_Clamp_Size(int& w, int& h);

/// Map a world coordinate to tactical-relative pixel offset.
/// Returns true if the coordinate falls within the visible tactical area
/// (with EDGE_ZONE margin for sprites that overlap the boundary).
bool Render_Bridge_World_To_Tactical(int world_lepton_x, int world_lepton_y,
                                      int& pixel_x, int& pixel_y);

/// Map a tactical-relative pixel offset to a world coordinate (leptons).
/// Returns true if the pixel is inside the tactical area.
bool Render_Bridge_Tactical_To_World(int pixel_x, int pixel_y,
                                      int& world_lepton_x, int& world_lepton_y);

/// Check whether a cell is visible in the bridge viewport.
bool Render_Bridge_Is_Cell_In_View(int cell_x, int cell_y);

// --- Bridge-native UI (Phase 1: Infrastructure) ---

/// Begin a UI frame — clears the UI draw list and resets input zones.
void Render_Bridge_UI_Begin_Frame();

/// End a UI frame — finalizes input capture state.
void Render_Bridge_UI_End_Frame();

/// Initialize UI font atlases from loaded game font data.
void Render_Bridge_UI_Init_Fonts();

/// Returns true if the UI has captured pointer input this frame.
bool Render_Bridge_UI_Has_Capture();

/// Returns true when bridge-native status messages replace legacy Messages.Draw().
bool Render_Bridge_UI_Use_Native_Messages();

/// Returns true when bridge-native help text replaces HelpClass::Draw_It().
bool Render_Bridge_UI_Use_Native_Help();

#endif // CNC_RENDER_BRIDGE_H
