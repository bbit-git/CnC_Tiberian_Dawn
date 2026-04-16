/**
 * ui_radar.cpp — Bridge-native radar minimap rendering.
 *
 * Each frame, iterates the visible map cells, builds an RGBA pixel
 * buffer (one pixel per cell), and submits it to the UI draw list as
 * a textured quad.  Draws a green viewport rectangle overlay and
 * handles click-to-move input on the radar area.
 */

#include "ui_radar.h"
#include "ui_draw_list.h"
#include "ui_input.h"
#include "ui_sidebar.h"
#include "hd_sidebar_layout.h"
#include "ui_radar_math.h"
#include "commandbar_atlas.h"
#include "commandbar_sprites.h"
#include "render_bridge.h"
#include "function.h"
#include <cmath>
#include <cstdlib>
#include <cstring>

// ---------------------------------------------------------------------------
// Radar pixel buffer — one RGBA pixel per map cell, rebuilt each frame.
// ---------------------------------------------------------------------------

static uint32_t* g_radar_pixels = nullptr;
static int       g_radar_alloc_w = 0;
static int       g_radar_alloc_h = 0;
static UI_Radar_Frame_Rect_Hook g_radar_frame_rect_hook = nullptr;

static constexpr uint8_t RADAR_VIEWPORT_R = 0;
static constexpr uint8_t RADAR_VIEWPORT_G = 200;
static constexpr uint8_t RADAR_VIEWPORT_B = 0;
static constexpr uint8_t RADAR_VIEWPORT_A = 200;

extern unsigned char* GamePalette;
extern bool Debug_Unshroud;

static int round_i(float v)
{
    return static_cast<int>(std::lround(v));
}

void UI_Radar_Debug_Set_Frame_Rect_Hook(UI_Radar_Frame_Rect_Hook hook)
{
    g_radar_frame_rect_hook = hook;
}

bool UI_Radar_Resolve_Frame_Rect(int logical_w, int logical_h,
                                 int& x, int& y, int& w, int& h)
{
    if (g_radar_frame_rect_hook &&
        g_radar_frame_rect_hook(logical_w, logical_h, x, y, w, h) &&
        w > 0 && h > 0) {
        return true;
    }

    if (Render_Bridge_Get_HD_Graphics()) {
        auto rect = render_bridge::HDSidebarLayout::Instance()
                        .Resolve("radar", logical_w, logical_h);
        if (rect.valid()) {
            x = rect.x;
            y = rect.y;
            w = rect.w;
            h = rect.h;
            return true;
        }
    }

    const int base_w = SeenBuff.Get_Width();
    const int base_h = SeenBuff.Get_Height();
    const float sx = (base_w > 0) ? static_cast<float>(logical_w) / static_cast<float>(base_w) : 1.0f;
    const float sy = (base_h > 0) ? static_cast<float>(logical_h) / static_cast<float>(base_h) : 1.0f;
    x = round_i(static_cast<float>(Map.RadX) * sx);
    y = round_i(static_cast<float>(Map.RadY) * sy);
    w = round_i(static_cast<float>(Map.RadWidth) * sx);
    h = round_i(static_cast<float>(Map.RadHeight) * sy);
    return w > 0 && h > 0;
}

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
    if (!UI_Sidebar_Debug_Is_On(COMP_RADAR)) return;

    int logical_w = 0, logical_h = 0;
    Render_Bridge_Get_Logical_Screen_Size(logical_w, logical_h);
    int frame_x = 0, frame_y = 0, frame_w = 0, frame_h = 0;
    if (!UI_Radar_Resolve_Frame_Rect(logical_w, logical_h,
                                     frame_x, frame_y, frame_w, frame_h)) {
        return;
    }

    if (!Map.Is_Radar_Active()) {
        // When radar is inactive, show faction logo in the radar area (HD only)
        bool atlas_ready = Render_Bridge_Get_HD_Graphics() && Commandbar_Atlas_Is_Ready();
        if (atlas_ready && PlayerPtr) {
            // Draw radar background
            const AtlasSpriteRect* bg = Commandbar_Atlas_Find(ATLAS_SIDEBAR_RADARBG);
            if (bg) {
                g_ui_draw_list.Draw_Atlas_Sprite(frame_x, frame_y, frame_w, frame_h,
                                                 bg->u0, bg->v0, bg->u1, bg->v1);
            }

            // Draw faction logo centered
            const char* logo_name = nullptr;
            HousesType house = PlayerPtr->Class->House;
            if (house == HOUSE_GOOD || house == HOUSE_MULTI1 || house == HOUSE_MULTI3 || house == HOUSE_MULTI5)
                logo_name = ATLAS_SIDEBAR_FACTIONLOGO_GDI;
            else
                logo_name = ATLAS_SIDEBAR_FACTIONLOGO_NOD;

            const AtlasSpriteRect* logo = Commandbar_Atlas_Find(logo_name);
            if (logo) {
                // Center and scale logo within radar frame, maintaining aspect ratio
                float logo_aspect = static_cast<float>(logo->width) / static_cast<float>(logo->height);
                int logo_h = frame_h * 3 / 4;
                int logo_w = static_cast<int>(logo_h * logo_aspect);
                if (logo_w > frame_w * 3 / 4) {
                    logo_w = frame_w * 3 / 4;
                    logo_h = static_cast<int>(logo_w / logo_aspect);
                }
                int logo_x = frame_x + (frame_w - logo_w) / 2;
                int logo_y = frame_y + (frame_h - logo_h) / 2;
                g_ui_draw_list.Draw_Atlas_Sprite(logo_x, logo_y, logo_w, logo_h,
                                                 logo->u0, logo->v0, logo->u1, logo->v1);
            }
        }
        return;
    }

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
    int radar_x = frame_x;
    int radar_y = frame_y;
    int radar_w = frame_w;
    int radar_h = frame_h;

    if (Map.RadWidth > 0 && Map.RadHeight > 0 &&
        Map.RadIWidth > 0 && Map.RadIHeight > 0) {
        radar_x = frame_x + round_i(static_cast<float>(frame_w) *
                                    (static_cast<float>(Map.RadOffX) / static_cast<float>(Map.RadWidth)));
        radar_y = frame_y + round_i(static_cast<float>(frame_h) *
                                    (static_cast<float>(Map.RadOffY) / static_cast<float>(Map.RadHeight)));
        radar_w = round_i(static_cast<float>(frame_w) *
                          (static_cast<float>(Map.RadIWidth) / static_cast<float>(Map.RadWidth)));
        radar_h = round_i(static_cast<float>(frame_h) *
                          (static_cast<float>(Map.RadIHeight) / static_cast<float>(Map.RadHeight)));
    }

    if (radar_x < frame_x) radar_x = frame_x;
    if (radar_y < frame_y) radar_y = frame_y;
    if (radar_x + radar_w > frame_x + frame_w) radar_w = frame_x + frame_w - radar_x;
    if (radar_y + radar_h > frame_y + frame_h) radar_h = frame_y + frame_h - radar_y;

    if (radar_w <= 0 || radar_h <= 0) return;

    bool use_atlas = Render_Bridge_Get_HD_Graphics() && Commandbar_Atlas_Is_Ready();
    if (use_atlas) {
        // HD radar background from atlas
        const AtlasSpriteRect* bg = Commandbar_Atlas_Find(ATLAS_SIDEBAR_RADARBG);
        if (bg) {
            g_ui_draw_list.Draw_Atlas_Sprite(frame_x, frame_y, frame_w, frame_h,
                                             bg->u0, bg->v0, bg->u1, bg->v1);
        }
    } else {
        g_ui_draw_list.Fill_Rect(frame_x, frame_y, frame_w, frame_h,
                                 16, 20, 16, 200);
        g_ui_draw_list.Draw_Rect(frame_x, frame_y, frame_w, frame_h,
                                 60, 80, 60, 255);
    }

    // Submit the radar pixel buffer as a textured icon
    g_ui_draw_list.Draw_Icon(radar_x, radar_y, radar_w, radar_h,
                             reinterpret_cast<const uint8_t*>(g_radar_pixels),
                             map_w, map_h);

    int visible_origin_x = 0;
    int visible_origin_y = 0;
    int visible_w = 0;
    int visible_h = 0;
    Render_Bridge_Get_Visible_World_Rect(visible_origin_x, visible_origin_y,
                                         visible_w, visible_h);

    UIRadarViewportRect viewport_rect;
    if (UI_Radar_Calc_Viewport_Rect(radar_x, radar_y, radar_w, radar_h,
                                    map_w, map_h,
                                    visible_origin_x, visible_origin_y,
                                    visible_w, visible_h,
                                    viewport_rect)) {
        g_ui_draw_list.Draw_Rect(viewport_rect.x, viewport_rect.y,
                                 viewport_rect.w, viewport_rect.h,
                                 RADAR_VIEWPORT_R,
                                 RADAR_VIEWPORT_G,
                                 RADAR_VIEWPORT_B,
                                 RADAR_VIEWPORT_A);
    }

    // --- Click-to-move input ---
    UI_Input_Push_String_ID("radar");
    UIHitZoneID zone = UI_Input_Register_Zone(radar_x, radar_y, radar_w, radar_h,
                                              UI_Input_Make_Widget_ID(0x52414452u));
    UI_Input_Pop_ID();
    bool left_clicked = UI_Input_Was_Clicked(zone);
    bool right_clicked = UI_Input_Was_Right_Clicked(zone);
    if (left_clicked || right_clicked) {
        // Get mouse position
        extern int g_mouse_x, g_mouse_y;
        int mx = g_mouse_x;
        int my = g_mouse_y;

        // Convert screen click to cell coordinates (relative to map origin)
        float px_per_cell_x = static_cast<float>(radar_w) / static_cast<float>(map_w);
        float px_per_cell_y = static_cast<float>(radar_h) / static_cast<float>(map_h);
        float click_cell_x = static_cast<float>(mx - radar_x) / px_per_cell_x;
        float click_cell_y = static_cast<float>(my - radar_y) / px_per_cell_y;

        int clamp_max_x = 0;
        int clamp_max_y = 0;
        Render_Bridge_Get_Clamp_Ranges(clamp_max_x, clamp_max_y);

        UIRadarClickTarget target;
        if (UI_Radar_Calc_Click_Target(Map.MapCellX, Map.MapCellY,
                                       map_w, map_h,
                                       visible_w, visible_h,
                                       clamp_max_x, clamp_max_y,
                                       click_cell_x, click_cell_y,
                                       target)) {
            if (left_clicked) {
                Render_Bridge_Record_Tactical_Request(target.request_x, target.request_y);
                Map.Set_Tactical_Position(XY_Coord(Cell_To_Lepton(Map.MapCellX) + target.request_x,
                                                   Cell_To_Lepton(Map.MapCellY) + target.request_y));
                Map.DisplayClass::IsToRedraw = true;
                Map.Flag_To_Redraw(true);
            }

            if (right_clicked) {
                Map.Zoom_Mode(XY_Cell(target.clicked_cell_x, target.clicked_cell_y));
            }
        }
    }
}

void UI_Radar_Shutdown()
{
    free(g_radar_pixels);
    g_radar_pixels = nullptr;
    g_radar_alloc_w = 0;
    g_radar_alloc_h = 0;
}
