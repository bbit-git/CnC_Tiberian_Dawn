/**
 * ui_sidebar.cpp — Bridge-native sidebar chrome + cameo icon rendering.
 *
 * Reads legacy SidebarClass/StripClass state and emits UI draw list
 * commands for backgrounds, borders, slot outlines, scroll arrows,
 * production labels, and cameo icon textures.
 *
 * Cameo icons are decoded from legacy SHP shapes via LegacySpriteProvider,
 * palette-converted to RGBA, and cached per (shapefile, remap_table) pair.
 * The cache is invalidated on palette change.
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
static constexpr uint8_t READY_R = 0, READY_G = 255, READY_B = 0;
static constexpr uint8_t BUILDING_R = 200, BUILDING_G = 200, BUILDING_B = 0;
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

/// Cache key: combines shapefile pointer with remap table pointer.
static uint64_t cameo_cache_key(const void* shapefile, const uint8_t* remap)
{
    uint64_t key = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(shapefile));
    key ^= static_cast<uint64_t>(reinterpret_cast<uintptr_t>(remap)) << 1;
    return key;
}

static std::unordered_map<uint64_t, CameoCacheEntry> g_cameo_cache;

/// Decode a cameo SHP shape to RGBA and cache the result.
/// Returns pointer to cached RGBA data, or nullptr on failure.
static const CameoCacheEntry* cameo_decode(const void* shapefile,
                                           const uint8_t* remap)
{
    if (!shapefile) return nullptr;

    uint64_t key = cameo_cache_key(shapefile, remap);
    auto it = g_cameo_cache.find(key);
    if (it != g_cameo_cache.end()) return &it->second;

    // Get palette
    const uint8_t* pal = static_cast<const uint8_t*>((void*)Get_Palette());
    if (!pal) pal = GamePalette;
    if (!pal) return nullptr;

    // Decode SHP frame 0
    SpriteFrame frame = {};
    if (!g_cameo_provider.Get_Frame(shapefile, 0, frame))
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
            rgba[i] = (uint32_t(255) << 24) |
                      (uint32_t(b)   << 16) |
                      (uint32_t(g)   << 8)  |
                      uint32_t(r);
        }
    }

    CameoCacheEntry entry = { rgba, frame.width, frame.height };
    auto result = g_cameo_cache.emplace(key, entry);
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
        if (spc >= 1 && spc <= 3) {
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

/// Emit one production slot outline, cameo icon, and label.
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
    const CameoCacheEntry* cameo = cameo_decode(shapefile, remap);
    if (cameo && cameo->rgba) {
        g_ui_draw_list.Draw_Icon(x + 1, y + 1, w - 2, h - 2,
                                 reinterpret_cast<const uint8_t*>(cameo->rgba),
                                 cameo->width, cameo->height);
    }

    // Darken overlay for unavailable items (factory busy, no production started)
    bool darken = is_slot_darkened(strip, actual_index);
    if (darken) {
        g_ui_draw_list.Fill_Rect(x, y, w, h, 0, 0, 0, DARKEN_ALPHA);
    }

    // Check production state
    int factory_id = strip.Buildables[actual_index].Factory;
    if (factory_id != -1) {
        FactoryClass* factory = Factories.Raw_Ptr(factory_id);
        if (factory) {
            if (factory->Has_Completed()) {
                // Ready indicator
                UI_Label(x + 2, y + h - 10, "RDY", UI_FONT_6PT,
                         READY_R, READY_G, READY_B, 255);
            } else if (factory->Is_Building()) {
                // Progress percentage
                int pct = factory->Completion();
                char pct_buf[8];
                snprintf(pct_buf, sizeof(pct_buf), "%d%%", pct);
                UI_Label(x + 2, y + h - 10, pct_buf, UI_FONT_6PT,
                         BUILDING_R, BUILDING_G, BUILDING_B, 255);
            }
        }
    }

    // Flashing indicator
    if (strip.Flasher == actual_index) {
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

    // Column background
    int col_w = strip.StripWidth * factor;
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
        if (strip.BuildableCount > 0 || true) {
            emit_column(strip, factor);
        }
    }

    // Repair/Sell/Map buttons area (below radar, above columns)
    // These are drawn as simple labeled boxes for now
    int btn_y = scale_y_from_legacy(Map.RadY + Map.RadHeight + 2, sy);
    int btn_w = side_w / 3;
    int btn_h = 10 * factor;

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
