/**
 * ui_radar.cpp — Bridge-native radar minimap rendering.
 *
 * Each frame, iterates the visible map cells, builds an RGBA pixel
 * buffer (one pixel per cell), and submits it to the UI draw list as
 * a textured quad.  Draws a white viewport rectangle overlay and
 * handles click-to-move input on the radar area.
 */

#include "ui_radar.h"
#include "ui_draw_list.h"
#include "ui_input.h"
#include "render_bridge.h"
#include "function.h"
#include <cstdlib>
#include <cstring>

// ---------------------------------------------------------------------------
// Radar pixel buffer — one RGBA pixel per map cell, rebuilt each frame.
// ---------------------------------------------------------------------------

static uint32_t* g_radar_pixels = nullptr;
static int       g_radar_alloc_w = 0;
static int       g_radar_alloc_h = 0;

extern unsigned char* GamePalette;
extern bool Debug_Unshroud;

/// Convert a VGA palette index to a packed RGBA value (0xAABBGGRR).
static uint32_t pal_index_to_rgba(int index, const uint8_t* pal)
{
    if (index <= 0 || !pal) return 0xFF000000u; // black, opaque
    uint8_t r = pal[index * 3 + 0] << 2;
    uint8_t g = pal[index * 3 + 1] << 2;
    uint8_t b = pal[index * 3 + 2] << 2;
    return (uint32_t(0xFF) << 24) |
           (uint32_t(b)    << 16) |
           (uint32_t(g)    << 8)  |
           uint32_t(r);
}

/// Ensure the pixel buffer is large enough for the current map.
static void ensure_buffer(int w, int h)
{
    if (w <= 0 || h <= 0) return;
    if (g_radar_pixels && g_radar_alloc_w == w && g_radar_alloc_h == h) return;

    free(g_radar_pixels);
    g_radar_pixels = static_cast<uint32_t*>(malloc(w * h * sizeof(uint32_t)));
    g_radar_alloc_w = w;
    g_radar_alloc_h = h;
}

// ---------------------------------------------------------------------------
// Radar pixel generation — mirrors legacy Plot_Radar_Pixel logic.
// ---------------------------------------------------------------------------

/// Build the RGBA pixel buffer from current map cell state.
/// Returns true if the buffer was successfully built.
static bool build_radar_pixels(const uint8_t* pal, int map_w, int map_h)
{
    if (!g_radar_pixels || !pal) return false;

    int mcx = Map.MapCellX;
    int mcy = Map.MapCellY;

    for (int cy = 0; cy < map_h; cy++) {
        for (int cx = 0; cx < map_w; cx++) {
            CELL cell = XY_Cell(mcx + cx, mcy + cy);
            CellClass& cc = Map[cell];
            int pixel_idx = cy * map_w + cx;

            // Shrouded cells → black
            if (!cc.IsVisible && !Debug_Unshroud) {
                g_radar_pixels[pixel_idx] = 0xFF000000u;
                continue;
            }

            // Check for building (house color) first — matches Cell_Color(true)
            int color = cc.Cell_Color(true);
            if (color != TBLACK) {
                g_radar_pixels[pixel_idx] = pal_index_to_rgba(color, pal);

                // Overlay infantry/units on top if present — use bright color
                ObjectClass* obj = cc.Cell_Occupier();
                while (obj) {
                    if (obj->Is_Techno()) {
                        RTTIType what = obj->What_Am_I();
                        if (what == RTTI_INFANTRY || what == RTTI_UNIT || what == RTTI_AIRCRAFT) {
                            TechnoClass* tech = static_cast<TechnoClass*>(obj);
                            if (tech->Cloak != CLOAKED || tech->House->Is_Ally(PlayerPtr)) {
                                g_radar_pixels[pixel_idx] =
                                    pal_index_to_rgba(tech->House->Class->BrightColor, pal);
                            }
                            break;
                        }
                    }
                    obj = obj->Next;
                }
                continue;
            }

            // No building — get terrain ground color
            color = cc.Cell_Color(false);
            g_radar_pixels[pixel_idx] = pal_index_to_rgba(color, pal);

            // Check for units/infantry occupiers
            ObjectClass* obj = cc.Cell_Occupier();
            while (obj) {
                if (obj->Is_Techno()) {
                    RTTIType what = obj->What_Am_I();
                    if (what == RTTI_INFANTRY || what == RTTI_UNIT || what == RTTI_AIRCRAFT) {
                        TechnoClass* tech = static_cast<TechnoClass*>(obj);
                        if (tech->Cloak != CLOAKED || tech->House->Is_Ally(PlayerPtr)) {
                            g_radar_pixels[pixel_idx] =
                                pal_index_to_rgba(tech->House->Class->BrightColor, pal);
                        }
                        break;
                    }
                }
                obj = obj->Next;
            }
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void UI_Radar_Emit()
{
    extern bool InMainLoop;
    if (!InMainLoop) return;
    if (!Map.IsSidebarActive) return;
    if (!Map.Is_Radar_Active()) return;

    // Get palette
    const uint8_t* pal = static_cast<const uint8_t*>((void*)Get_Palette());
    if (!pal) pal = GamePalette;
    if (!pal) return;

    // Map dimensions in cells
    int map_w = Map.MapCellWidth;
    int map_h = Map.MapCellHeight;
    if (map_w <= 0 || map_h <= 0) return;

    // Ensure pixel buffer
    ensure_buffer(map_w, map_h);
    if (!g_radar_pixels) return;

    // Build the radar pixel data
    if (!build_radar_pixels(pal, map_w, map_h)) return;

    // --- Position the radar in the sidebar ---
    // Use legacy radar position members, scaled to HD logical screen space.
    int base_w = SeenBuff.Get_Width();
    int base_h = SeenBuff.Get_Height();
    int logical_w = 0, logical_h = 0;
    Render_Bridge_Get_Logical_Screen_Size(logical_w, logical_h);
    float sx = (base_w > 0) ? static_cast<float>(logical_w) / static_cast<float>(base_w) : 1.0f;
    float sy = (base_h > 0) ? static_cast<float>(logical_h) / static_cast<float>(base_h) : 1.0f;

    // Radar interior position and size in HD space
    int radar_x = static_cast<int>((Map.RadX + Map.RadOffX) * sx);
    int radar_y = static_cast<int>((Map.RadY + Map.RadOffY) * sy);
    int radar_w = static_cast<int>(Map.RadIWidth * sx);
    int radar_h = static_cast<int>(Map.RadIHeight * sy);

    if (radar_w <= 0 || radar_h <= 0) return;

    // Dark background behind the radar
    int frame_x = static_cast<int>(Map.RadX * sx);
    int frame_y = static_cast<int>(Map.RadY * sy);
    int frame_w = static_cast<int>(Map.RadWidth * sx);
    int frame_h = static_cast<int>(Map.RadHeight * sy);
    g_ui_draw_list.Fill_Rect(frame_x, frame_y, frame_w, frame_h,
                             16, 20, 16, 200);
    g_ui_draw_list.Draw_Rect(frame_x, frame_y, frame_w, frame_h,
                             60, 80, 60, 255);

    // Submit the radar pixel buffer as a textured icon
    g_ui_draw_list.Draw_Icon(radar_x, radar_y, radar_w, radar_h,
                             reinterpret_cast<const uint8_t*>(g_radar_pixels),
                             map_w, map_h);

    // --- Viewport rectangle overlay ---
    // Determine which map cells are currently visible in the tactical view.
    float vp_x = Render_Bridge_Get_Viewport_X();
    float vp_y = Render_Bridge_Get_Viewport_Y();
    float vis_w = 0.0f, vis_h = 0.0f;
    Render_Bridge_Get_Visible_Size(vis_w, vis_h);

    // Viewport position in cells (relative to map origin)
    float cell_vp_x = vp_x / static_cast<float>(CELL_PIXEL_W);
    float cell_vp_y = vp_y / static_cast<float>(CELL_PIXEL_W);
    float cell_vis_w = vis_w / static_cast<float>(CELL_PIXEL_W);
    float cell_vis_h = vis_h / static_cast<float>(CELL_PIXEL_W);

    // Convert cell coords to radar pixel coords, then to screen coords
    float px_per_cell_x = static_cast<float>(radar_w) / static_cast<float>(map_w);
    float px_per_cell_y = static_cast<float>(radar_h) / static_cast<float>(map_h);

    int vr_x = radar_x + static_cast<int>(cell_vp_x * px_per_cell_x);
    int vr_y = radar_y + static_cast<int>(cell_vp_y * px_per_cell_y);
    int vr_w = static_cast<int>(cell_vis_w * px_per_cell_x);
    int vr_h = static_cast<int>(cell_vis_h * px_per_cell_y);

    // Clamp to radar bounds
    if (vr_x < radar_x) { vr_w -= (radar_x - vr_x); vr_x = radar_x; }
    if (vr_y < radar_y) { vr_h -= (radar_y - vr_y); vr_y = radar_y; }
    if (vr_x + vr_w > radar_x + radar_w) vr_w = radar_x + radar_w - vr_x;
    if (vr_y + vr_h > radar_y + radar_h) vr_h = radar_y + radar_h - vr_y;
    if (vr_w > 0 && vr_h > 0) {
        g_ui_draw_list.Draw_Rect(vr_x, vr_y, vr_w, vr_h,
                                 255, 255, 255, 200);
    }

    // --- Click-to-move input ---
    UIHitZoneID zone = UI_Input_Register_Zone(radar_x, radar_y, radar_w, radar_h);
    if (UI_Input_Was_Clicked(zone)) {
        // Get mouse position
        extern int g_mouse_x, g_mouse_y;
        int mx = g_mouse_x;
        int my = g_mouse_y;

        // Convert screen click to cell coordinates (relative to map origin)
        float click_cell_x = static_cast<float>(mx - radar_x) / px_per_cell_x;
        float click_cell_y = static_cast<float>(my - radar_y) / px_per_cell_y;

        // Center the viewport on the clicked cell
        int vis_cells_w = 0, vis_cells_h = 0;
        {
            int vis_lep_w = 0, vis_lep_h = 0;
            Render_Bridge_Get_Visible_Size_Leptons(vis_lep_w, vis_lep_h);
            vis_cells_w = Lepton_To_Cell(vis_lep_w);
            vis_cells_h = Lepton_To_Cell(vis_lep_h);
        }

        int cellx = Map.MapCellX + static_cast<int>(click_cell_x) - vis_cells_w / 2;
        int celly = Map.MapCellY + static_cast<int>(click_cell_y) - vis_cells_h / 2;

        // Clamp to map bounds
        if (cellx < Map.MapCellX) cellx = Map.MapCellX;
        if (celly < Map.MapCellY) celly = Map.MapCellY;

        CELL cell = XY_Cell(cellx, celly);

        // Notify the render bridge of the new tactical position
        int rx = Cell_To_Lepton(cellx) - Cell_To_Lepton(Map.MapCellX);
        int ry = Cell_To_Lepton(celly) - Cell_To_Lepton(Map.MapCellY);
        Render_Bridge_Record_Tactical_Request(rx, ry);

        // Update legacy tactical position
        Map.Set_Tactical_Position(Cell_Coord(cell));
        Map.DisplayClass::IsToRedraw = true;
        Map.Flag_To_Redraw(true);
    }
}

void UI_Radar_Shutdown()
{
    free(g_radar_pixels);
    g_radar_pixels = nullptr;
    g_radar_alloc_w = 0;
    g_radar_alloc_h = 0;
}
