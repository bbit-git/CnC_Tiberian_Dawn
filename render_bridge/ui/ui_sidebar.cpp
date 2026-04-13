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
        int pip_y = y + h - pip->height - 2;
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
        g_ui_draw_list.Fill_Rect(x, y, w, h, 0, 0, 0, DARKEN_ALPHA);
    }

    // Production indicators: clock sweep, pip ready/holding
    if (production) {
        if (completed) {
            emit_pip(PIP_READY, x, y, w, h);
        } else if (holding) {
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
static void emit_column(const SidebarClass::StripClass& strip, int factor)
{
    int col_x = strip.X;
    int col_y = strip.Y;
    int obj_w = strip.ObjectWidth;
    int obj_h = strip.ObjectHeight;
    int visible = 4; // MAX_VISIBLE

    // Column background — StripWidth is already scaled by sx, do not multiply by factor
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
    int factor = (logical_w > 800) ? 2 : 1;

    // Keep the legacy sidebar chrome visible and only tint it lightly until
    // cameo art, radar chrome, and button art are fully native.
    g_ui_draw_list.Fill_Rect(side_x, side_y, side_w, side_h,
                             24, 24, 24, 56);
    g_ui_draw_list.Fill_Rect(side_x, side_y, 1, side_h,
                             84, 84, 84, 220);

    // Emit each production column
    for (int c = 0; c < 2; c++) {
        SidebarClass::StripClass strip = Map.Column[c];
        strip.X = scale_x_from_legacy(strip.X, sx);
        strip.Y = scale_y_from_legacy(strip.Y, sy);
        strip.ObjectWidth = scale_x_from_legacy(strip.ObjectWidth, sx);
        strip.ObjectHeight = scale_y_from_legacy(strip.ObjectHeight, sy);
        strip.StripWidth = scale_x_from_legacy(strip.StripWidth, sx);
        strip.LeftEdgeOffset = scale_x_from_legacy(strip.LeftEdgeOffset, sx);
        emit_column(strip, factor);
    }

    // Repair/Sell/Map buttons area — anchored to sidebar bottom
    int btn_w = side_w / 3;
    int btn_h = 10 * factor;
    int btn_y = side_y + side_h - btn_h - 2;

    UIButtonStyle bs = UI_Default_Button_Style();
    bs.normal_r = 54; bs.normal_g = 70; bs.normal_b = 54;
    bs.hover_r = 70; bs.hover_g = 94; bs.hover_b = 70;
    bs.press_r = 44; bs.press_g = 56; bs.press_b = 44;
    bs.text_r = 0; bs.text_g = 200; bs.text_b = 0;
    bs.font = UI_FONT_6PT;

    UI_Button(side_x + 2, btn_y, btn_w - 2, btn_h, "RPR", bs);
    UI_Button(side_x + btn_w + 1, btn_y, btn_w - 2, btn_h, "SEL", bs);
    UI_Button(side_x + btn_w * 2, btn_y, btn_w - 2, btn_h, "MAP", bs);
}
