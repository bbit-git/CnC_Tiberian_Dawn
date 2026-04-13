/**
 * ui_sidebar.cpp — Bridge-native sidebar chrome + cameo icon rendering.
 *
 * Reads legacy SidebarClass/StripClass state and emits UI draw list
 * commands for backgrounds, borders, slot outlines, scroll arrows,
 * cameo icon textures, clock-sweep overlays, and pip indicators.
 *
 * Sprites (cameos, clock shapes, pips) are decoded from legacy SHP data
 * via LegacySpriteProvider, palette-converted to RGBA, and cached per
 * (shapefile, frame, remap_table, opacity) tuple.  The cache is
 * invalidated on palette change.
 */

#include "ui_sidebar.h"
#include "ui_draw_list.h"
#include "ui_controls.h"
#include "ui_layout.h"
#include "ui_text.h"
#include "render_bridge.h"
#include "legacy_sprite_provider.h"
#include "function.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

// td_platform.h defines min/max macros that conflict with STL headers
#undef min
#undef max
#include <unordered_map>

/// Sidebar slot colors.
static constexpr uint8_t SLOT_BG_R = 40, SLOT_BG_G = 44, SLOT_BG_B = 40;
static constexpr uint8_t SLOT_BORDER_R = 36, SLOT_BORDER_G = 120, SLOT_BORDER_B = 36;
static constexpr uint8_t SCROLL_R = 120, SCROLL_G = 120, SCROLL_B = 120;
static constexpr uint8_t DARKEN_ALPHA = 140; // overlay alpha for unavailable items

/// Power bar colors.
static constexpr uint8_t POW_BG_R = 20, POW_BG_G = 20, POW_BG_B = 20;
static constexpr uint8_t POW_GREEN_R = 0, POW_GREEN_G = 180, POW_GREEN_B = 0;
static constexpr uint8_t POW_YELLOW_R = 200, POW_YELLOW_G = 200, POW_YELLOW_B = 0;
static constexpr uint8_t POW_RED_R = 200, POW_RED_G = 40, POW_RED_B = 0;

// ---------------------------------------------------------------------------
// Cameo RGBA cache
// ---------------------------------------------------------------------------

extern unsigned char* GamePalette;

static LegacySpriteProvider g_cameo_provider;

/// Cached RGBA cameo frame.
struct CameoCacheEntry {
    uint32_t* rgba;
    int       width;
    int       height;
};

/// Cache key: combines shapefile, frame index, remap table, and opacity.
static uint64_t sprite_cache_key(const void* shapefile, int frame,
                                 const uint8_t* remap, uint8_t opacity)
{
    uint64_t key = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(shapefile));
    key = key * UINT64_C(0x9e3779b97f4a7c15) +
          static_cast<uint64_t>(reinterpret_cast<uintptr_t>(remap));
    key = key * UINT64_C(0x517cc1b727220a95) + static_cast<uint64_t>(frame);
    key = key * UINT64_C(0x6c62272e07bb0142) + static_cast<uint64_t>(opacity);
    return key;
}

static std::unordered_map<uint64_t, CameoCacheEntry> g_cameo_cache;
static uint8_t g_cameo_pal_snapshot[768]; // palette snapshot for cache invalidation

/// Check if the current palette differs from the cached snapshot.
static bool cameo_palette_changed()
{
    const uint8_t* pal = static_cast<const uint8_t*>((void*)Get_Palette());
    if (!pal) pal = GamePalette;
    if (!pal) return false;
    return memcmp(pal, g_cameo_pal_snapshot, 768) != 0;
}

/// Snapshot the current palette for future change detection.
static void cameo_palette_snapshot()
{
    const uint8_t* pal = static_cast<const uint8_t*>((void*)Get_Palette());
    if (!pal) pal = GamePalette;
    if (pal) memcpy(g_cameo_pal_snapshot, pal, 768);
}

/// Decode a SHP shape frame to RGBA and cache the result.
/// @param shapefile  SHP shape data pointer
/// @param frame_num  Frame index within the shape file
/// @param remap      Optional palette remap table (player colours)
/// @param opacity    Alpha value for non-transparent pixels (255=opaque, ~180=ghost)
/// Returns pointer to cached RGBA data, or nullptr on failure.
static const CameoCacheEntry* sprite_decode(const void* shapefile, int frame_num,
                                            const uint8_t* remap, uint8_t opacity)
{
    if (!shapefile) return nullptr;

    // Auto-invalidate on palette change
    if (!g_cameo_cache.empty() && cameo_palette_changed()) {
        UI_Sidebar_Cameo_Invalidate();
    }

    uint64_t key = sprite_cache_key(shapefile, frame_num, remap, opacity);
    auto it = g_cameo_cache.find(key);
    if (it != g_cameo_cache.end()) return &it->second;

    // Get palette
    const uint8_t* pal = static_cast<const uint8_t*>((void*)Get_Palette());
    if (!pal) pal = GamePalette;
    if (!pal) return nullptr;

    // Decode SHP frame
    SpriteFrame frame = {};
    if (!g_cameo_provider.Get_Frame(shapefile, frame_num, frame))
        return nullptr;
    if (!frame.pixels || frame.width <= 0 || frame.height <= 0)
        return nullptr;

    int pixel_count = frame.width * frame.height;
    uint32_t* rgba = static_cast<uint32_t*>(malloc(pixel_count * 4));
    if (!rgba) return nullptr;

    const uint8_t* src = static_cast<const uint8_t*>(frame.pixels);
    for (int y = 0; y < frame.height; y++) {
        const uint8_t* row = src + y * frame.pitch;
        for (int x = 0; x < frame.width; x++) {
            int i = y * frame.width + x;
            uint8_t idx = row[x];
            if (idx == 0) {
                rgba[i] = 0; // transparent
                continue;
            }
            if (remap) {
                idx = remap[idx];
            }
            uint8_t r = pal[idx * 3 + 0] << 2;
            uint8_t g = pal[idx * 3 + 1] << 2;
            uint8_t b = pal[idx * 3 + 2] << 2;
            rgba[i] = (uint32_t(opacity) << 24) |
                      (uint32_t(b)       << 16) |
                      (uint32_t(g)       << 8)  |
                      uint32_t(r);
        }
    }

    CameoCacheEntry entry = { rgba, frame.width, frame.height };
    auto result = g_cameo_cache.emplace(key, entry);
    cameo_palette_snapshot();
    return &result.first->second;
}

void UI_Sidebar_Cameo_Invalidate()
{
    for (auto& kv : g_cameo_cache) {
        free(kv.second.rgba);
    }
    g_cameo_cache.clear();
}

void UI_Sidebar_Cameo_Shutdown()
{
    UI_Sidebar_Cameo_Invalidate();
}

// ---------------------------------------------------------------------------

static int scale_x_from_legacy(int value, float sx)
{
    return static_cast<int>(value * sx);
}

static int scale_y_from_legacy(int value, float sy)
{
    return static_cast<int>(value * sy);
}

/// Determine the remap table for a buildable item, matching legacy Draw_It logic.
static const uint8_t* get_cameo_remap(const SidebarClass::StripClass& strip,
                                      int index)
{
    if (strip.Buildables[index].BuildableType == RTTI_SPECIAL)
        return nullptr;

    switch (strip.Buildables[index].BuildableType) {
        case RTTI_BUILDINGTYPE:
            if (!BuildingTypeClass::As_Reference(
                    (StructType)strip.Buildables[index].BuildableID).IsWall) {
                return PlayerPtr->Remap_Table(false, false);
            }
            return nullptr;

        case RTTI_UNITTYPE:
            switch (strip.Buildables[index].BuildableID) {
                case UNIT_MCV:
                case UNIT_HARVESTER:
                    return PlayerPtr->Remap_Table(false, false);
                default:
                    return PlayerPtr->Remap_Table(false, true);
            }

        case RTTI_AIRCRAFTTYPE:
            return PlayerPtr->Remap_Table(false, true);

        default:
            return nullptr;
    }
}

/// Fetch the cameo shapefile pointer for a buildable item.
static const void* get_cameo_shape(const SidebarClass::StripClass& strip,
                                   int index)
{
    if (strip.Buildables[index].BuildableType == RTTI_SPECIAL) {
        int spc = strip.Buildables[index].BuildableID;
        constexpr int max_specials = static_cast<int>(
            sizeof(SidebarClass::StripClass::SpecialShapes) /
            sizeof(SidebarClass::StripClass::SpecialShapes[0]));
        if (spc >= 1 && spc <= max_specials) {
            return SidebarClass::StripClass::SpecialShapes[spc - 1];
        }
        return nullptr;
    }

    ObjectTypeClass const* obj = Fetch_Techno_Type(
        strip.Buildables[index].BuildableType,
        strip.Buildables[index].BuildableID);
    return obj ? obj->Get_Cameo_Data() : nullptr;
}

/// Check if a buildable should be darkened (factory of matching type busy).
static bool is_slot_darkened(const SidebarClass::StripClass& strip, int index)
{
    if (strip.Buildables[index].Factory != -1) return false;

    switch (strip.Buildables[index].BuildableType) {
        case RTTI_INFANTRYTYPE:  return PlayerPtr->InfantryFactory != -1;
        case RTTI_BUILDINGTYPE:  return PlayerPtr->BuildingFactory != -1;
        case RTTI_UNITTYPE:      return PlayerPtr->UnitFactory != -1;
        case RTTI_AIRCRAFTTYPE:  return PlayerPtr->AircraftFactory != -1;
        default:                 return false;
    }
}

/// Draw a pip shape (PIP_READY or PIP_HOLDING) centered at the bottom of a slot.
static void emit_pip(int pip_frame, int x, int y, int w, int h)
{
    const CameoCacheEntry* pip = sprite_decode(
        ObjectTypeClass::PipShapes, pip_frame, nullptr, 255);
    if (pip && pip->rgba) {
        int pip_x = x + (w - pip->width) / 2;
        int pip_y = y + h - pip->height - 8;
        g_ui_draw_list.Draw_Icon(pip_x, pip_y, pip->width, pip->height,
                                 reinterpret_cast<const uint8_t*>(pip->rgba),
                                 pip->width, pip->height);
    }
}

/// Draw a clock-sweep overlay for production progress.
/// @param stage  Production stage (0-108 for factories, 0-102 for super weapons)
static void emit_clock(int stage, int x, int y, int w, int h)
{
    if (stage <= 0) return;
    const void* shapes = SidebarClass::StripClass::ClockShapes;
    if (!shapes) return;
    int frame_count = g_cameo_provider.Get_Frame_Count(shapes);
    int frame = stage + 1;
    if (frame >= frame_count) frame = frame_count - 1;
    const CameoCacheEntry* clock = sprite_decode(shapes, frame, nullptr, 180);
    if (clock && clock->rgba) {
        g_ui_draw_list.Draw_Icon(x + 1, y + 1, w - 2, h - 2,
                                 reinterpret_cast<const uint8_t*>(clock->rgba),
                                 clock->width, clock->height);
    }
}

/// Emit one production slot outline, cameo icon, and production indicators.
static void emit_slot(int x, int y, int w, int h, int slot_index,
                      const SidebarClass::StripClass& strip)
{
    int actual_index = strip.TopIndex + slot_index;
    bool has_item = actual_index < strip.BuildableCount;

    // Slot background
    g_ui_draw_list.Fill_Rect(x, y, w, h, SLOT_BG_R, SLOT_BG_G, SLOT_BG_B, 120);
    g_ui_draw_list.Draw_Rect(x, y, w, h, SLOT_BORDER_R, SLOT_BORDER_G, SLOT_BORDER_B, 255);

    if (!has_item) return;

    // Cameo icon
    const void* shapefile = get_cameo_shape(strip, actual_index);
    const uint8_t* remap = get_cameo_remap(strip, actual_index);
    const CameoCacheEntry* cameo = sprite_decode(shapefile, 0, remap, 255);
    if (cameo && cameo->rgba) {
        g_ui_draw_list.Draw_Icon(x + 1, y + 1, w - 2, h - 2,
                                 reinterpret_cast<const uint8_t*>(cameo->rgba),
                                 cameo->width, cameo->height);
    }

    // Determine production state — regular buildables vs special weapons
    bool production = false;
    bool completed  = false;
    bool holding    = false;
    int  stage      = 0;
    bool darken     = false;
    bool is_special = (strip.Buildables[actual_index].BuildableType == RTTI_SPECIAL);

    if (is_special) {
        int spc = strip.Buildables[actual_index].BuildableID;
        switch (spc) {
            case SPC_ION_CANNON:
                production = true;
                completed = PlayerPtr->IonCannon.Is_Ready();
                stage = PlayerPtr->IonCannon.Anim_Stage();
                break;
            case SPC_NUCLEAR_BOMB:
                production = true;
                completed = PlayerPtr->NukeStrike.Is_Ready();
                stage = PlayerPtr->NukeStrike.Anim_Stage();
                break;
            case SPC_AIR_STRIKE:
                production = true;
                completed = PlayerPtr->AirStrike.Is_Ready();
                stage = PlayerPtr->AirStrike.Anim_Stage();
                break;
        }
    } else {
        int factory_id = strip.Buildables[actual_index].Factory;
        if (factory_id != -1) {
            FactoryClass* factory = Factories.Raw_Ptr(factory_id);
            if (factory) {
                production = true;
                completed = factory->Has_Completed();
                stage = factory->Completion();
                holding = !completed && !factory->Is_Building();
            }
        }
        darken = is_slot_darkened(strip, actual_index);
    }

    // Darken overlay for unavailable items (factory busy, no production started)
    if (darken) {
        g_ui_draw_list.Fill_Rect(x + 1, y + 1, w - 2, h - 2, 0, 0, 0, DARKEN_ALPHA);
    }

    // Production indicators: clock sweep, pip ready/holding
    if (production) {
        if (completed) {
            emit_pip(PIP_READY, x, y, w, h);
        } else if (holding) {
            emit_clock(stage, x, y, w, h);
            emit_pip(PIP_HOLDING, x, y, w, h);
        } else {
            emit_clock(stage, x, y, w, h);
        }
    }

    // Flash indicator — pulse on/off matching legacy Fetch_Stage() & 1 toggle
    if (strip.Flasher == actual_index && (strip.Fetch_Stage() & 1)) {
        g_ui_draw_list.Draw_Rect(x + 1, y + 1, w - 2, h - 2,
                                 255, 255, 0, 180);
    }
}

/// Emit scroll arrows for a strip column.
static void emit_scroll_arrows(int x, int y, int strip_w,
                                const SidebarClass::StripClass& strip)
{
    bool can_up   = strip.TopIndex > 0;
    bool can_down = strip.TopIndex + 4 < strip.BuildableCount;

    // Up arrow indicator
    uint8_t up_a = can_up ? (uint8_t)255 : (uint8_t)80;
    g_ui_draw_list.Fill_Rect(x + 2, y, 12, 8,
                             SCROLL_R, SCROLL_R, SCROLL_R, up_a);
    g_ui_draw_list.Draw_Text(x + 4, y, "UP", UI_FONT_6PT,
                             200, 200, 200, up_a);

    // Down arrow indicator
    uint8_t dn_a = can_down ? (uint8_t)255 : (uint8_t)80;
    g_ui_draw_list.Fill_Rect(x + strip_w - 14, y, 12, 8,
                             SCROLL_R, SCROLL_R, SCROLL_R, dn_a);
    g_ui_draw_list.Draw_Text(x + strip_w - 12, y, "DN", UI_FONT_6PT,
                             200, 200, 200, dn_a);
}

/// Emit one sidebar column.
static void emit_column(const SidebarClass::StripClass& strip)
{
    int col_x = strip.X;
    int col_y = strip.Y;
    int obj_w = strip.ObjectWidth;
    int obj_h = strip.ObjectHeight;
    int visible = 4; // MAX_VISIBLE

    // Column background — StripWidth is already scaled by sx in the caller
    int col_w = strip.StripWidth;
    int col_h = obj_h * visible + 14;
    g_ui_draw_list.Fill_Rect(col_x, col_y, col_w, col_h,
                             34, 36, 34, 96);

    // Production slots
    UILayout layout;
    layout.Begin(col_x + strip.LeftEdgeOffset, col_y, obj_w, col_h,
                 UI_DIR_VERTICAL, 0);
    for (int i = 0; i < visible; i++) {
        int sx, sy;
        layout.Next(obj_w, obj_h, sx, sy);
        emit_slot(sx, sy, obj_w, obj_h, i, strip);
    }

    // Scroll arrows below slots
    emit_scroll_arrows(col_x, col_y + obj_h * visible + 1, col_w, strip);
}

// ---------------------------------------------------------------------------
// Power bar rendering
// ---------------------------------------------------------------------------

/// Emit a vertical power bar gauge on the left edge of the sidebar.
/// Shows power output as a filled bar and drain level as an indicator line.
static void emit_power_bar(int x, int y, int w, int h, float sx, float sy)
{
    if (!PlayerPtr) return;

    int power = PlayerPtr->Power;
    int drain = PlayerPtr->Drain;

    // Bar background
    g_ui_draw_list.Fill_Rect(x, y, w, h, POW_BG_R, POW_BG_G, POW_BG_B, 200);
    g_ui_draw_list.Draw_Rect(x, y, w, h, 60, 60, 60, 200);

    if (h < 4) return;

    int inner_x = x + 1;
    int inner_w = w - 2;
    int inner_h = h - 2;
    int inner_y = y + 1;
    int bottom  = inner_y + inner_h;

    // Compute bar heights using the same logarithmic scale as legacy Power_Height
    // (power.h: POWER_STEP_LEVEL=100, POWER_STEP_FACTOR=6).
    auto compute_height = [inner_h](int value) -> int {
        int retval = 0;
        int num = value / 100;         // POWER_STEP_LEVEL
        int remainder = value - num * 100;
        for (int i = 0; i < num; i++) {
            retval = retval + ((inner_h - retval) / 6);  // POWER_STEP_FACTOR
        }
        if (remainder) {
            retval = retval + ((((inner_h - retval) / 6) * remainder) / 100);
        }
        if (retval < 0) retval = 0;
        if (retval > inner_h) retval = inner_h;
        return retval;
    };

    int power_h = compute_height(power);
    int drain_h = compute_height(drain);

    // Choose bar color based on power/drain ratio
    uint8_t bar_r, bar_g, bar_b;
    if (drain == 0 || power >= drain) {
        bar_r = POW_GREEN_R; bar_g = POW_GREEN_G; bar_b = POW_GREEN_B;
    } else if (power * 2 >= drain) {
        bar_r = POW_YELLOW_R; bar_g = POW_YELLOW_G; bar_b = POW_YELLOW_B;
    } else {
        bar_r = POW_RED_R; bar_g = POW_RED_G; bar_b = POW_RED_B;
    }

    // Draw power output bar (fills from bottom upward)
    if (power_h > 0) {
        g_ui_draw_list.Fill_Rect(inner_x, bottom - power_h, inner_w, power_h,
                                 bar_r, bar_g, bar_b, 220);
    }

    // Draw drain indicator line
    if (drain_h > 0 && drain > 0) {
        int drain_y = bottom - drain_h;
        g_ui_draw_list.Fill_Rect(inner_x, drain_y, inner_w, 1,
                                 255, 255, 255, 200);
        // Small arrow marker on left edge
        g_ui_draw_list.Fill_Rect(x, drain_y - 1, 2, 3,
                                 255, 255, 255, 240);
    }

    // Power/Drain text labels at top
    if (power > 0 || drain > 0) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", power);
        g_ui_draw_list.Draw_Text(x + 1, y + 2, buf, UI_FONT_6PT,
                                 bar_r, bar_g, bar_b, 200, 0.8f);
    }
}

// ---------------------------------------------------------------------------
// Button shape loading + rendering
// ---------------------------------------------------------------------------

/// Cached button shape pointers (loaded once from mix files).
static const void* s_repair_shape = nullptr;
static const void* s_sell_shape   = nullptr;
static const void* s_map_shape    = nullptr;
static bool s_btn_shapes_loaded   = false;

static void load_button_shapes()
{
    if (s_btn_shapes_loaded) return;
    s_btn_shapes_loaded = true;
    s_repair_shape = Hires_Retrieve((char*)"REPAIR.SHP");
    s_sell_shape   = Hires_Retrieve((char*)"SELL.SHP");
    s_map_shape    = Hires_Retrieve((char*)"MAP.SHP");
}

/// Emit a sidebar button with SHP art if available, falling back to text.
/// Returns true if clicked this frame.
static bool emit_sidebar_button(int x, int y, int w, int h,
                                const void* shapefile, int frame,
                                const char* fallback_label,
                                bool is_active,
                                const UIButtonStyle& style)
{
    // Draw button background based on active state
    UIButtonState state = UI_Button(x, y, w, h, nullptr, style);

    // Overlay SHP art if available
    const CameoCacheEntry* icon = sprite_decode(shapefile, frame, nullptr, 255);
    if (icon && icon->rgba) {
        g_ui_draw_list.Draw_Icon(x + 1, y + 1, w - 2, h - 2,
                                 reinterpret_cast<const uint8_t*>(icon->rgba),
                                 icon->width, icon->height);
    } else {
        // Text fallback
        int tx = x + 2;
        int ty = y + (h > 8 ? 2 : 1);
        g_ui_draw_list.Draw_Text(tx, ty, fallback_label, style.font,
                                 style.text_r, style.text_g, style.text_b,
                                 style.text_a, style.text_scale);
    }

    // Active mode highlight (repair/sell toggled on)
    if (is_active) {
        g_ui_draw_list.Draw_Rect(x, y, w, h, 255, 255, 0, 180);
    }

    return state == UI_BTN_PRESSED;
}

// ---------------------------------------------------------------------------

void UI_Sidebar_Emit()
{
    extern bool InMainLoop;
    if (!InMainLoop) return;
    if (!Map.IsSidebarActive) return;

    int side_x = 0, side_y = 0, side_w = 0, side_h = 0;
    Render_Bridge_Get_Sidebar_Rect(side_x, side_y, side_w, side_h);
    if (side_w <= 0) return;

    int base_w = SeenBuff.Get_Width();
    int base_h = SeenBuff.Get_Height();
    int logical_w = 0;
    int logical_h = 0;
    Render_Bridge_Get_Logical_Screen_Size(logical_w, logical_h);
    float sx = (base_w > 0) ? static_cast<float>(logical_w) / static_cast<float>(base_w) : 1.0f;
    float sy = (base_h > 0) ? static_cast<float>(logical_h) / static_cast<float>(base_h) : 1.0f;
    // Sidebar background tint
    g_ui_draw_list.Fill_Rect(side_x, side_y, side_w, side_h,
                             24, 24, 24, 56);
    g_ui_draw_list.Fill_Rect(side_x, side_y, 1, side_h,
                             84, 84, 84, 220);

    // --- Power bar ---
    // Position: narrow vertical strip on the left edge of the sidebar,
    // below the radar area, above the buttons.
    int pow_w = scale_x_from_legacy(8, sx);
    int pow_x = side_x + 2;
    int radar_bottom = scale_y_from_legacy(Map.RadY + Map.RadHeight, sy) + scale_y_from_legacy(13, sy);
    int btn_h = scale_y_from_legacy(16, sy);
    int btn_y = side_y + side_h - btn_h - 2;
    int pow_y = radar_bottom;
    int pow_h = btn_y - pow_y - 2;
    if (pow_h > 10) {
        emit_power_bar(pow_x, pow_y, pow_w, pow_h, sx, sy);
    }

    // --- Production columns ---
    for (int c = 0; c < 2; c++) {
        SidebarClass::StripClass strip = Map.Column[c];
        strip.X = scale_x_from_legacy(strip.X, sx);
        strip.Y = scale_y_from_legacy(strip.Y, sy);
        strip.ObjectWidth = scale_x_from_legacy(strip.ObjectWidth, sx);
        strip.ObjectHeight = scale_y_from_legacy(strip.ObjectHeight, sy);
        strip.StripWidth = scale_x_from_legacy(strip.StripWidth, sx);
        strip.LeftEdgeOffset = scale_x_from_legacy(strip.LeftEdgeOffset, sx);
        emit_column(strip);
    }

    // --- Repair / Sell / Map buttons ---
    load_button_shapes();

    int btn_w = side_w / 3;

    UIButtonStyle bs = UI_Default_Button_Style();
    bs.normal_r = 54; bs.normal_g = 70; bs.normal_b = 54;
    bs.hover_r = 70; bs.hover_g = 94; bs.hover_b = 70;
    bs.press_r = 44; bs.press_g = 56; bs.press_b = 44;
    bs.text_r = 0; bs.text_g = 200; bs.text_b = 0;
    bs.font = UI_FONT_6PT;

    // Repair button — frame 0 = normal, frame 1 = pressed (in typical SHP layout)
    bool repair_active = Map.IsRepairMode != 0;
    if (emit_sidebar_button(side_x + 2, btn_y, btn_w - 2, btn_h,
                            s_repair_shape, repair_active ? 1 : 0,
                            "RPR", repair_active, bs)) {
        Map.Repair_Mode_Control(-1);
    }

    // Sell button
    bool sell_active = Map.IsSellMode != 0;
    if (emit_sidebar_button(side_x + btn_w + 1, btn_y, btn_w - 2, btn_h,
                            s_sell_shape, sell_active ? 1 : 0,
                            "SEL", sell_active, bs)) {
        Map.Sell_Mode_Control(-1);
    }

    // Map/Zoom button
    if (emit_sidebar_button(side_x + btn_w * 2, btn_y, btn_w - 2, btn_h,
                            s_map_shape, 0,
                            "MAP", false, bs)) {
        if (Map.Is_Radar_Active()) {
            if (Map.Is_Zoomed() || GameToPlay == GAME_NORMAL) {
                Map.Zoom_Mode(Coord_Cell(Map.TacticalCoord));
            } else {
                if (!Map.Is_Player_Names()) {
                    Map.Player_Names(1);
                } else {
                    Map.Player_Names(0);
                    Map.Zoom_Mode(Coord_Cell(Map.TacticalCoord));
                }
            }
        } else {
            if (GameToPlay != GAME_NORMAL) {
                Map.Player_Names(Map.Is_Player_Names() == 0);
            }
        }
    }
}
