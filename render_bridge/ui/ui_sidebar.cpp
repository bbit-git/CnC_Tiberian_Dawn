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
 *
 * When HD graphics are enabled, cameo icons are loaded from the command
 * bar atlas (MTD/TGA) and the sidebar uses a 3-column grid layout.
 */

#include "ui_sidebar.h"
#include "ui_sidebar_metrics.h"
#include "ui_draw_list.h"
#include "ui_controls.h"
#include "ui_input.h"
#include "ui_layout.h"
#include "ui_radar.h"
#include "ui_text.h"
#include "commandbar_atlas.h"
#include "commandbar_sprites.h"
#include "hd_sidebar_layout.h"
#include "render_bridge.h"
#include "legacy_sprite_provider.h"
#include "function.h"
#include "dbg.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

// td_platform.h defines min/max macros that conflict with STL headers
#undef min
#undef max
#include <unordered_map>

/// HD command bar atlas init state (used early in Cameo_Shutdown).
static bool g_atlas_init_attempted = false;

/// HD grid scroll state (reset on shutdown, used by emit_hd_grid).
static int g_hd_grid_top_index = 0;

// ---------------------------------------------------------------------------
// Debug component mask (see ui_sidebar.h)
// ---------------------------------------------------------------------------
static uint32_t g_sidebar_debug_mask = COMP_ALL;

void     UI_Sidebar_Debug_Set_Mask(uint32_t mask) { g_sidebar_debug_mask = mask; }
uint32_t UI_Sidebar_Debug_Get_Mask()              { return g_sidebar_debug_mask; }
bool     UI_Sidebar_Debug_Is_On(UISidebarComponent comp) {
    return (g_sidebar_debug_mask & comp) != 0;
}

static UI_Sidebar_Metrics_Hook g_sidebar_metrics_hook = nullptr;
void UI_Sidebar_Debug_Set_Metrics_Hook(UI_Sidebar_Metrics_Hook hook) {
    g_sidebar_metrics_hook = hook;
}

/// HD sidebar build-category filter (selected by the row below the mode tabs).
/// Defaults to CAT_STRUCTURE so fresh games (no factories yet) show buildings
/// rather than an empty grid.
enum SidebarCategory {
    CAT_INFANTRY,
    CAT_VEHICLE,
    CAT_STRUCTURE,
    CAT_SUPPORT,
};
static SidebarCategory g_hd_grid_category = CAT_STRUCTURE;

/// True if the given buildable RTTI belongs to the selected category.
static bool category_matches(SidebarCategory cat, RTTIType rtti)
{
    switch (cat) {
        case CAT_INFANTRY:  return rtti == RTTI_INFANTRYTYPE;
        case CAT_VEHICLE:   return rtti == RTTI_UNITTYPE
                                || rtti == RTTI_AIRCRAFTTYPE;
        case CAT_STRUCTURE: return rtti == RTTI_BUILDINGTYPE;
        case CAT_SUPPORT:   return rtti == RTTI_SPECIAL;
    }
    return true;
}

/// Sidebar slot colors.
static constexpr uint8_t SLOT_BG_R = 40, SLOT_BG_G = 44, SLOT_BG_B = 40;
static constexpr uint8_t SLOT_BORDER_R = 36, SLOT_BORDER_G = 120, SLOT_BORDER_B = 36;
static constexpr uint8_t SCROLL_R = 120, SCROLL_G = 120, SCROLL_B = 120;
static constexpr uint8_t DARKEN_ALPHA = 80; // overlay alpha for unavailable items (legacy uses ClockTranslucentTable)

/// Power bar colors.
static constexpr uint8_t POW_BG_R = 20, POW_BG_G = 20, POW_BG_B = 20;
static constexpr uint8_t POW_GREEN_R = 0, POW_GREEN_G = 180, POW_GREEN_B = 0;
static constexpr uint8_t POW_YELLOW_R = 200, POW_YELLOW_G = 200, POW_YELLOW_B = 0;
static constexpr uint8_t POW_RED_R = 200, POW_RED_G = 40, POW_RED_B = 0;

/// HD grid layout constants.
static constexpr int HD_GRID_COLS = 3;
static constexpr int HD_GRID_ICON_PAD = 2;
static constexpr int HD_GRID_TEXT_H = 14; // text label area below icon

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
    Commandbar_Atlas_Shutdown();
    g_atlas_init_attempted = false;
    g_hd_grid_top_index = 0;
}

// ---------------------------------------------------------------------------
// HD command bar atlas — lazy initialization
// ---------------------------------------------------------------------------

/// Discover the TEXTURES_SRGB.MEG path and load the command bar atlas.
/// Called once on first HD sidebar frame. Returns true if atlas is usable.
static bool ensure_commandbar_atlas()
{
    if (Commandbar_Atlas_Is_Ready()) return true;
    if (g_atlas_init_attempted) return false;
    g_atlas_init_attempted = true;

    // Replicate the data directory discovery from gl_sprites.cpp
    const char* env = std::getenv("CNC_REMASTERED_DATA");
    char meg_path[1024];

    auto try_open = [&](const char* dir) -> bool {
        std::snprintf(meg_path, sizeof(meg_path), "%s/TEXTURES_SRGB.MEG", dir);
        FILE* f = fopen(meg_path, "rb");
        if (!f) return false;
        fclose(f);
        return Commandbar_Atlas_Init(meg_path);
    };

    if (env && env[0] && try_open(env)) return true;
    if (try_open("data")) return true;
    if (try_open("Data")) return true;

    const char* home = std::getenv("HOME");
    if (home && home[0]) {
        char steam_dir[1024];
        std::snprintf(steam_dir, sizeof(steam_dir),
                      "%s/.local/share/Steam/steamapps/common/CnCRemastered/Data", home);
        if (try_open(steam_dir)) return true;
    }

    DBG("[HD-SIDEBAR] TEXTURES_SRGB.MEG not found — HD sidebar disabled");
    return false;
}

/// Emit an atlas sprite into the UI draw list.
/// @return true if the sprite was found and emitted.
static bool emit_atlas_sprite(const char* name,
                              int dst_x, int dst_y, int dst_w, int dst_h,
                              uint8_t r = 255, uint8_t g = 255,
                              uint8_t b = 255, uint8_t a = 255)
{
    const AtlasSpriteRect* rect = Commandbar_Atlas_Find(name);
    if (!rect) {
        static int miss_count = 0;
        if (miss_count < 20) {
            DBG("[HD-SIDEBAR] atlas sprite MISS: '%s'", name);
            miss_count++;
        }
        return false;
    }
    g_ui_draw_list.Draw_Atlas_Sprite(dst_x, dst_y, dst_w, dst_h,
                                     rect->u0, rect->v0, rect->u1, rect->v1,
                                     r, g, b, a);
    return true;
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

/// Determine the remap table for a buildable item.
/// Legacy Draw_It (sidebar.cpp:1943) zeroes the remapper before drawing, so
/// cameo icons are always rendered with their natural palette colours — no
/// player-colour remap is applied.
static const uint8_t* get_cameo_remap(const SidebarClass::StripClass& /*strip*/,
                                      int /*index*/)
{
    return nullptr;
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

/// Map from a TD IniName (short code) to the atlas sprite's display-name
/// suffix (e.g. "NUKE" -> "POWERPLANT" so the atlas key becomes
/// "BUILDICON_TD_POWERPLANT.TGA").  Returns nullptr if not mapped — caller
/// falls back to uppercasing IniName directly (works for APC, JEEP, MCV,
/// ORCA, A10, SILO — names that already match the atlas form).
static const char* ininame_to_atlas_suffix(const char* ini)
{
    if (!ini) return nullptr;
    // Buildings
    if (!strcmp(ini, "TMPL")) return "TEMPLEOFNOD";
    if (!strcmp(ini, "EYE"))  return "ADVCOMMCTR";
    if (!strcmp(ini, "WEAP")) return "WEAPONSFACTORY";
    if (!strcmp(ini, "GTWR")) return "GUARDTOWER";
    if (!strcmp(ini, "ATWR")) return "ADVGUARDTOWER";
    if (!strcmp(ini, "OBLI")) return "OBELISK";
    if (!strcmp(ini, "GUN"))  return "TURRET";
    if (!strcmp(ini, "FACT")) return "CONYARD";
    if (!strcmp(ini, "PROC")) return "REFINERY";
    if (!strcmp(ini, "HPAD")) return "HELIPAD";
    if (!strcmp(ini, "HQ"))   return "COMMCENTER";
    if (!strcmp(ini, "SAM"))  return "SAMSITE";
    if (!strcmp(ini, "AFLD")) return "AIRSTRIP";
    if (!strcmp(ini, "NUKE")) return "POWERPLANT";
    if (!strcmp(ini, "NUK2")) return "ADVPOWERPLANT";
    if (!strcmp(ini, "PYLE")) return "BARRACKS";
    if (!strcmp(ini, "HAND")) return "HANDOFNOD";
    if (!strcmp(ini, "FIX"))  return "REPAIRFACILITY";
    if (!strcmp(ini, "SBAG")) return "SANDBAGS";
    if (!strcmp(ini, "CYCL")) return "CHAINLINKFENCE";
    if (!strcmp(ini, "BRIK")) return "CONCRETEWALL";
    if (!strcmp(ini, "BARB")) return "BARBEDWIRE";
    if (!strcmp(ini, "WOOD")) return "WOODENFENCE";
    // Infantry
    if (!strcmp(ini, "E1"))   return "MINIGUNNER";
    if (!strcmp(ini, "E2"))   return "GRENADIER";
    if (!strcmp(ini, "E3"))   return "ROCKETSOLDIER";
    if (!strcmp(ini, "E4"))   return "FLAMETHROWER";
    if (!strcmp(ini, "E5"))   return "CHEMWARRIOR";
    if (!strcmp(ini, "E6"))   return "ENGINEER";
    if (!strcmp(ini, "RMBO")) return "COMMANDO";
    // Units
    if (!strcmp(ini, "FTNK")) return "FLAMETANK";
    if (!strcmp(ini, "STNK")) return "STEALTHTANK";
    if (!strcmp(ini, "LTNK")) return "LIGHTTANK";
    if (!strcmp(ini, "MTNK")) return "MEDIUMTANK";
    if (!strcmp(ini, "HTNK")) return "MAMMOTHTANK";
    if (!strcmp(ini, "MLRS")) return "SSMLAUNCHER";
    if (!strcmp(ini, "ARTY")) return "ARTILLERY";
    if (!strcmp(ini, "HARV")) return "HARVESTER";
    if (!strcmp(ini, "BGGY")) return "NODBUGGY";
    if (!strcmp(ini, "BIKE")) return "RECONBIKE";
    if (!strcmp(ini, "MSAM")) return "ROCKETLAUNCHER";
    if (!strcmp(ini, "BOAT")) return "GUNBOAT";
    // Aircraft
    if (!strcmp(ini, "TRAN")) return "CHINOOK";
    if (!strcmp(ini, "HELI")) return "APACHE";
    // Names that already match verbatim: APC, JEEP, MCV, ORCA, A10, SILO
    return nullptr;
}

/// Build the atlas sprite name for a buildable item's cameo icon.
/// Returns true if the name was built, false if the item has no atlas cameo.
/// Special weapons: BUILDICON_TD_IONCANNON.TGA, etc.
/// Regular units/buildings: BUILDICON_TD_{ATLAS_NAME}.TGA where ATLAS_NAME
/// comes from ininame_to_atlas_suffix() or (fallback) uppercased IniName.
static bool get_atlas_cameo_name(const SidebarClass::StripClass& strip, int index,
                                 char* buf, int buf_size)
{
    if (strip.Buildables[index].BuildableType == RTTI_SPECIAL) {
        int spc = strip.Buildables[index].BuildableID;
        switch (spc) {
            case SPC_ION_CANNON:
                snprintf(buf, buf_size, "BUILDICON_TD_IONCANNON.TGA");
                return true;
            case SPC_NUCLEAR_BOMB:
                snprintf(buf, buf_size, "BUILDICON_TD_NUCLEARSTRIKE.TGA");
                return true;
            case SPC_AIR_STRIKE:
                snprintf(buf, buf_size, "BUILDICON_TD_AIRSTRIKE.TGA");
                return true;
            default:
                return false;
        }
    }

    ObjectTypeClass const* obj = Fetch_Techno_Type(
        strip.Buildables[index].BuildableType,
        strip.Buildables[index].BuildableID);
    if (!obj || !obj->IniName) return false;

    const char* atlas_suffix = ininame_to_atlas_suffix(obj->IniName);
    if (atlas_suffix) {
        snprintf(buf, buf_size, "BUILDICON_TD_%s.TGA", atlas_suffix);
        return true;
    }

    // Fallback: uppercase IniName directly (works for APC, JEEP, MCV, ORCA, A10, SILO)
    snprintf(buf, buf_size, "BUILDICON_TD_%s.TGA", obj->IniName);
    for (char* p = buf + 13; *p && *p != '.'; p++) {
        *p = static_cast<char>(toupper(static_cast<unsigned char>(*p)));
    }
    return true;
}

/// Try to render a cameo from the atlas. Returns true if successful.
/// Caller must verify Commandbar_Atlas_Is_Ready() before calling.
static bool emit_atlas_cameo(const SidebarClass::StripClass& strip, int index,
                             int x, int y, int w, int h)
{
    char name[80];
    if (!get_atlas_cameo_name(strip, index, name, sizeof(name))) {
        static int no_name_count = 0;
        if (no_name_count < 5) {
            DBG("[HD-SIDEBAR] cameo: no atlas name for type=%d id=%d",
                strip.Buildables[index].BuildableType,
                strip.Buildables[index].BuildableID);
            no_name_count++;
        }
        return false;
    }

    bool ok = emit_atlas_sprite(name, x, y, w, h);
    static int cameo_log_count = 0;
    if (cameo_log_count < 10) {
        DBG("[HD-SIDEBAR] cameo lookup: '%s' → %s", name, ok ? "HIT" : "MISS");
        cameo_log_count++;
    }
    return ok;
}

static const CameoCacheEntry* get_best_cameo(const SidebarClass::StripClass& strip,
                                             int index)
{
    const void* shapefile = get_cameo_shape(strip, index);
    const uint8_t* remap = get_cameo_remap(strip, index);
    return sprite_decode(shapefile, 0, remap, 255);
}

/// Get the display name for a buildable item.
static const char* get_buildable_name(const SidebarClass::StripClass& strip, int index)
{
    if (strip.Buildables[index].BuildableType == RTTI_SPECIAL) {
        int spc = strip.Buildables[index].BuildableID;
        switch (spc) {
            case SPC_ION_CANNON:  return Text_String(TXT_ION_CANNON);
            case SPC_NUCLEAR_BOMB: return Text_String(TXT_NUKE_STRIKE);
            case SPC_AIR_STRIKE:  return Text_String(TXT_AIR_STRIKE);
            default: return "";
        }
    }

    ObjectTypeClass const* obj = Fetch_Techno_Type(
        strip.Buildables[index].BuildableType,
        strip.Buildables[index].BuildableID);
    if (!obj) return "";
    return Text_String(obj->Full_Name());
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

// ---------------------------------------------------------------------------
// Production click dispatch — mirrors SelectClass::Action() logic
// ---------------------------------------------------------------------------

/// Handle a left-click on a production slot.
static void handle_slot_left_click(SidebarClass::StripClass& strip, int actual_index)
{
    if (actual_index >= strip.BuildableCount) return;
    DBG("sidebar: left_click index=%d count=%d", actual_index, strip.BuildableCount);

    RTTIType otype = strip.Buildables[actual_index].BuildableType;
    int oid = strip.Buildables[actual_index].BuildableID;
    int fnumber = strip.Buildables[actual_index].Factory;

    // Determine generic factory for this type
    int genfactory = -1;
    switch (otype) {
        case RTTI_INFANTRYTYPE:  genfactory = PlayerPtr->InfantryFactory; break;
        case RTTI_UNITTYPE:      genfactory = PlayerPtr->UnitFactory; break;
        case RTTI_AIRCRAFTTYPE:  genfactory = PlayerPtr->AircraftFactory; break;
        case RTTI_BUILDINGTYPE:  genfactory = PlayerPtr->BuildingFactory; break;
        default: break;
    }

    Map.Override_Mouse_Shape(MOUSE_NORMAL);

    // Special weapons
    if (otype == RTTI_SPECIAL) {
        int spc = oid;
        switch (spc) {
            case SPC_ION_CANNON:
                if (PlayerPtr->IonCannon.Is_Ready()) {
                    Map.IsTargettingMode = spc;
                    Unselect_All();
                    Speak(VOX_SELECT_TARGET);
                } else {
                    PlayerPtr->IonCannon.Impatient_Click();
                }
                break;
            case SPC_AIR_STRIKE:
                if (PlayerPtr->AirStrike.Is_Ready()) {
                    Map.IsTargettingMode = spc;
                    Unselect_All();
                    Speak(VOX_SELECT_TARGET);
                } else {
                    PlayerPtr->AirStrike.Impatient_Click();
                }
                break;
            case SPC_NUCLEAR_BOMB:
                if (PlayerPtr->NukeStrike.Is_Ready()) {
                    Map.IsTargettingMode = spc;
                    Unselect_All();
                    Speak(VOX_SELECT_TARGET);
                } else {
                    PlayerPtr->NukeStrike.Impatient_Click();
                }
                break;
        }
        return;
    }

    // Normal production
    ObjectTypeClass const* choice = Fetch_Techno_Type(otype, oid);
    if (!choice) return;

    FactoryClass* factory = nullptr;
    if (fnumber != -1) {
        factory = Factories.Raw_Ptr(fnumber);
    }

    // If this type's factory slot is busy with a different item, reject
    if (fnumber == -1 && genfactory != -1) {
        Speak(VOX_NO_FACTORY);
        return;
    }

    DBG("sidebar: click type=%d id=%d factory=%d genfactory=%d", (int)otype, oid, fnumber, genfactory);

    if (factory) {
        if (factory->Is_Building()) {
            DBG("sidebar: factory busy — VOX_NO_FACTORY");
            Speak(VOX_NO_FACTORY);
        } else if (factory->Has_Completed()) {
            TechnoClass* pending = factory->Get_Object();
            if (!pending && factory->Get_Special_Item()) {
                DBG("sidebar: completed special — entering target mode");
                Map.IsTargettingMode = true;
            } else if (pending) {
                BuildingClass* builder = pending->Who_Can_Build_Me(false, false);
                if (!builder) {
                    DBG("sidebar: completed but no builder — ABANDON");
                    OutList.Add(EventClass(EventClass::ABANDON, otype, oid));
                    Speak(VOX_NO_FACTORY);
                } else if (pending->What_Am_I() == RTTI_BUILDING) {
                    DBG("sidebar: completed building — Manual_Place");
                    PlayerPtr->Manual_Place(builder, (BuildingClass*)pending);
                } else {
                    DBG("sidebar: completed unit — PLACE");
                    OutList.Add(EventClass(EventClass::PLACE, otype, (CELL)-1));
                }
            }
        } else {
            // Suspended — resume
            DBG("sidebar: resuming — PRODUCE event queued");
            Speak(VOX_BUILDING);
            OutList.Add(EventClass(EventClass::PRODUCE, otype, oid));
        }
    } else {
        // No factory — start production
        DBG("sidebar: new production — PRODUCE event queued");
        Speak(VOX_BUILDING);
        OutList.Add(EventClass(EventClass::PRODUCE, otype, oid));
    }
}

/// Handle a right-click on a production slot (cancel/suspend).
static void handle_slot_right_click(SidebarClass::StripClass& strip, int actual_index)
{
    if (actual_index >= strip.BuildableCount) return;

    RTTIType otype = strip.Buildables[actual_index].BuildableType;
    int oid = strip.Buildables[actual_index].BuildableID;
    int fnumber = strip.Buildables[actual_index].Factory;

    // Special weapons — cancel targeting mode
    if (otype == RTTI_SPECIAL) {
        Map.IsTargettingMode = false;
        return;
    }

    if (fnumber == -1) return;

    FactoryClass* factory = Factories.Raw_Ptr(fnumber);
    if (!factory) return;

    // Cancel placement mode if active
    if (Map.PendingObjectPtr && Map.PendingObjectPtr->Is_Techno()) {
        Map.PendingObjectPtr = 0;
        Map.PendingObject = 0;
        Map.PendingHouse = HOUSE_NONE;
        Map.Set_Cursor_Shape(0);
    }

    if (!factory->Is_Building()) {
        Speak(VOX_CANCELED);
        OutList.Add(EventClass(EventClass::ABANDON, otype, oid));
    } else {
        Speak(VOX_SUSPENDED);
        OutList.Add(EventClass(EventClass::SUSPEND, otype, oid));
    }
}

// ---------------------------------------------------------------------------
// Pip / Clock / Slot rendering
// ---------------------------------------------------------------------------

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

/// Determine production state for a buildable slot.
struct SlotState {
    bool production;
    bool completed;
    bool holding;
    int  stage;
    bool darken;
};

static SlotState get_slot_state(const SidebarClass::StripClass& strip, int actual_index)
{
    SlotState s = {};
    bool is_special = (strip.Buildables[actual_index].BuildableType == RTTI_SPECIAL);

    if (is_special) {
        int spc = strip.Buildables[actual_index].BuildableID;
        switch (spc) {
            case SPC_ION_CANNON:
                s.production = true;
                s.completed = PlayerPtr->IonCannon.Is_Ready();
                s.stage = PlayerPtr->IonCannon.Anim_Stage();
                break;
            case SPC_NUCLEAR_BOMB:
                s.production = true;
                s.completed = PlayerPtr->NukeStrike.Is_Ready();
                s.stage = PlayerPtr->NukeStrike.Anim_Stage();
                break;
            case SPC_AIR_STRIKE:
                s.production = true;
                s.completed = PlayerPtr->AirStrike.Is_Ready();
                s.stage = PlayerPtr->AirStrike.Anim_Stage();
                break;
        }
    } else {
        int factory_id = strip.Buildables[actual_index].Factory;
        if (factory_id != -1) {
            FactoryClass* factory = Factories.Raw_Ptr(factory_id);
            if (factory) {
                s.production = true;
                s.completed = factory->Has_Completed();
                s.stage = factory->Completion();
                s.holding = !s.completed && !factory->Is_Building();
            }
        } else {
            // Also check the real Map.Column to see if Factory was linked there
            // but not in our copy
            for (int c = 0; c < 2; c++) {
                if (actual_index < Map.Column[c].BuildableCount &&
                    Map.Column[c].Buildables[actual_index].BuildableType == strip.Buildables[actual_index].BuildableType &&
                    Map.Column[c].Buildables[actual_index].BuildableID == strip.Buildables[actual_index].BuildableID &&
                    Map.Column[c].Buildables[actual_index].Factory != -1) {
                    DBG("slot_state: MISMATCH — strip copy has Factory=-1 but Map.Column[%d] has Factory=%d",
                        c, Map.Column[c].Buildables[actual_index].Factory);
                }
            }
        }
        s.darken = is_slot_darkened(strip, actual_index);
    }
    return s;
}

/// Render production overlays (clock, pip, darken, flash) on a slot.
/// @param show_progress_text  If true, draw a % text label for building items (HD grid).
static void emit_production_overlays(int x, int y, int w, int h,
                                     const SlotState& state,
                                     const SidebarClass::StripClass& strip,
                                     int actual_index,
                                     bool show_progress_text = false)
{
    if (state.darken) {
        g_ui_draw_list.Fill_Rect(x + 1, y + 1, w - 2, h - 2, 0, 0, 0, DARKEN_ALPHA);
    }

    const bool show_clock = UI_Sidebar_Debug_Is_On(COMP_CLOCK_OVERLAY);
    const bool show_pips  = UI_Sidebar_Debug_Is_On(COMP_PIPS);
    const bool show_text  = show_progress_text && UI_Sidebar_Debug_Is_On(COMP_QUEUE_COUNT);

    if (state.production) {
        if (state.completed) {
            if (show_pips) emit_pip(PIP_READY, x, y, w, h);
            if (show_text) {
                g_ui_draw_list.Draw_Text(x + 2, y + h / 2 - 4, "READY", UI_FONT_6PT,
                                         0, 255, 0, 255, 0.9f);
            }
        } else if (state.holding) {
            if (show_clock) emit_clock(state.stage, x, y, w, h);
            if (show_pips)  emit_pip(PIP_HOLDING, x, y, w, h);
            if (show_text) {
                g_ui_draw_list.Draw_Text(x + 2, y + h / 2 - 4, "HOLD", UI_FONT_6PT,
                                         255, 200, 0, 255, 0.9f);
            }
        } else {
            if (show_clock) emit_clock(state.stage, x, y, w, h);
            if (show_text && state.stage > 0) {
                // stage is 0-54 for normal production (Completion() returns percentage)
                int pct_val = state.stage;
                if (pct_val > 100) pct_val = 100;
                char pct[16];  // generous, avoids -Wformat-truncation on INT_MIN
                snprintf(pct, sizeof(pct), "%d%%", pct_val);
                g_ui_draw_list.Draw_Text(x + 2, y + h / 2 - 4, pct, UI_FONT_6PT,
                                         200, 200, 200, 240, 0.9f);
            }
        }
        // Active production border highlight
        g_ui_draw_list.Draw_Rect(x, y, w, h, 0, 200, 0, 120);
    }

    if (strip.Flasher == actual_index && (strip.Fetch_Stage() & 1)) {
        g_ui_draw_list.Draw_Rect(x + 1, y + 1, w - 2, h - 2, 255, 255, 0, 180);
    }
}

/// Emit one production slot outline, cameo icon, production indicators, and hit zone.
/// Returns the hit zone ID for click handling in the caller (for non-HD legacy path).
static void emit_slot(int x, int y, int w, int h, int slot_index,
                      SidebarClass::StripClass& strip, bool hd_mode,
                      bool use_atlas)
{
    int actual_index = strip.TopIndex + slot_index;
    bool has_item = actual_index < strip.BuildableCount;

    // Slot background
    g_ui_draw_list.Fill_Rect(x, y, w, h, SLOT_BG_R, SLOT_BG_G, SLOT_BG_B, 120);
    g_ui_draw_list.Draw_Rect(x, y, w, h, SLOT_BORDER_R, SLOT_BORDER_G, SLOT_BORDER_B, 255);

    // Register hit zone for the slot (even if empty, so clicks are captured)
    UI_Input_Push_Pointer_ID(&strip);
    UI_Input_Push_ID(actual_index);
    UIHitZoneID zone = UI_Input_Register_Zone(x, y, w, h,
                                              UI_Input_Make_Widget_ID(0x534c4f54u));
    UI_Input_Pop_ID();
    UI_Input_Pop_ID();

    if (!has_item) return;
    if (use_atlas) {
        emit_atlas_sprite(ATLAS_SIDEBAR_BUILDFRAME, x, y, w, h);
    }

    // Prefer atlas cameos, fall back to legacy SHP decode
    bool drew_cameo = false;
    if (use_atlas) {
        drew_cameo = emit_atlas_cameo(strip, actual_index, x + 1, y + 1, w - 2, h - 2);
    }
    if (!drew_cameo) {
        const CameoCacheEntry* cameo = get_best_cameo(strip, actual_index);
        if (cameo && cameo->rgba) {
            g_ui_draw_list.Draw_Icon(x + 1, y + 1, w - 2, h - 2,
                                     reinterpret_cast<const uint8_t*>(cameo->rgba),
                                     cameo->width, cameo->height);
        }
    }

    // Production overlays
    SlotState state = get_slot_state(strip, actual_index);
    emit_production_overlays(x, y, w, h, state, strip, actual_index);

    // Click handling
    if (UI_Input_Was_Clicked(zone)) {
        handle_slot_left_click(strip, actual_index);
    }
    if (UI_Input_Was_Right_Clicked(zone)) {
        handle_slot_right_click(strip, actual_index);
    }
}

/// Emit scroll arrows for a strip column, with click handling.
/// @param col_index  Index into Map.Column[] (0 or 1) — needed to scroll the original, not a copy.
static void emit_scroll_arrows(int x, int y, int strip_w,
                                const SidebarClass::StripClass& strip,
                                int col_index)
{
    bool can_up   = strip.TopIndex > 0;
    bool can_down = strip.TopIndex + 4 < strip.BuildableCount;

    // Up arrow indicator
    uint8_t up_a = can_up ? (uint8_t)255 : (uint8_t)80;
    int up_w = 16, up_h = 10;
    g_ui_draw_list.Fill_Rect(x + 2, y, up_w, up_h,
                             SCROLL_R, SCROLL_R, SCROLL_R, up_a);
    g_ui_draw_list.Draw_Text(x + 4, y + 1, "UP", UI_FONT_6PT,
                             200, 200, 200, up_a);
    UI_Input_Push_ID(col_index);
    UI_Input_Push_String_ID("scroll_up");
    UIHitZoneID up_zone = UI_Input_Register_Zone(x + 2, y, up_w, up_h,
                                                 UI_Input_Make_Widget_ID(0x555031u));
    UI_Input_Pop_ID();
    UI_Input_Pop_ID();
    if (UI_Input_Was_Clicked(up_zone) && can_up) {
        Map.Column[col_index].Scroll(true);
    }

    // Down arrow indicator
    uint8_t dn_a = can_down ? (uint8_t)255 : (uint8_t)80;
    int dn_x = x + strip_w - up_w - 2;
    g_ui_draw_list.Fill_Rect(dn_x, y, up_w, up_h,
                             SCROLL_R, SCROLL_R, SCROLL_R, dn_a);
    g_ui_draw_list.Draw_Text(dn_x + 2, y + 1, "DN", UI_FONT_6PT,
                             200, 200, 200, dn_a);
    UI_Input_Push_ID(col_index);
    UI_Input_Push_String_ID("scroll_down");
    UIHitZoneID dn_zone = UI_Input_Register_Zone(dn_x, y, up_w, up_h,
                                                 UI_Input_Make_Widget_ID(0x444e31u));
    UI_Input_Pop_ID();
    UI_Input_Pop_ID();
    if (UI_Input_Was_Clicked(dn_zone) && can_down) {
        Map.Column[col_index].Scroll(false);
    }
}

/// Emit one sidebar column (legacy 2-column layout).
/// @param col_index  Index into Map.Column[] (0 or 1) — scroll ops use the original.
static void emit_column(SidebarClass::StripClass& strip, bool hd_mode, int col_index)
{
    int col_x = strip.X;
    int col_y = strip.Y;
    int obj_w = strip.ObjectWidth;
    int obj_h = strip.ObjectHeight;
    int visible = 4; // MAX_VISIBLE

    int col_w = strip.StripWidth;
    int col_h = obj_h * visible + 14;
    g_ui_draw_list.Fill_Rect(col_x, col_y, col_w, col_h,
                             34, 36, 34, 96);

    // Register a hit zone for the whole column (for hover-gated scrolling)
    UIHitZoneID col_zone = UI_Input_Register_Zone(col_x, col_y, col_w, col_h);

    // Production slots
    UILayout layout;
    layout.Begin(col_x + strip.LeftEdgeOffset, col_y, obj_w, col_h,
                 UI_DIR_VERTICAL, 0);
    for (int i = 0; i < visible; i++) {
        int sx, sy;
        layout.Next(obj_w, obj_h, sx, sy);
        emit_slot(sx, sy, obj_w, obj_h, i, strip, hd_mode, false);
    }

    // Scroll arrows below slots
    if (UI_Sidebar_Debug_Is_On(COMP_SCROLL_ARROWS)) {
        emit_scroll_arrows(col_x, col_y + obj_h * visible + 1, col_w, strip, col_index);
    }

    // Mouse wheel scrolling — only consume when hovering over this column
    if (UI_Input_Get_Hovered() == col_zone) {
        float scroll_delta = UI_Input_Consume_Scroll_Delta();
        if (scroll_delta > 0.0f && strip.TopIndex > 0) {
            Map.Column[col_index].Scroll(true);
        } else if (scroll_delta < 0.0f && strip.TopIndex + 4 < strip.BuildableCount) {
            Map.Column[col_index].Scroll(false);
        }
    }
}

// ---------------------------------------------------------------------------
// HD 3-column grid layout
// ---------------------------------------------------------------------------

/// Emit the HD 3-column grid sidebar layout, merging both strip columns.
static void emit_hd_grid(int grid_x, int grid_y, int grid_w, int grid_h,
                          float sx, float sy, bool use_atlas)
{

    // Compute cell dimensions
    int cell_w = (grid_w - HD_GRID_ICON_PAD * (HD_GRID_COLS + 1)) / HD_GRID_COLS;
    int icon_h = cell_w * 3 / 4; // 4:3 aspect for icon
    int cell_h = icon_h + HD_GRID_TEXT_H + HD_GRID_ICON_PAD;
    if (cell_w < 20 || cell_h < 20) return;

    int visible_rows = (grid_h - HD_GRID_ICON_PAD) / (cell_h + HD_GRID_ICON_PAD);
    if (visible_rows < 1) visible_rows = 1;
    int visible_slots = visible_rows * HD_GRID_COLS;

    // Build a merged list of all buildable items from both strip columns.
    // Column 0 = structures, Column 1 = units (preserve ordering).
    struct MergedItem {
        int strip_col;
        int strip_index; // index into strip.Buildables[]
    };
    MergedItem merged[60]; // MAX_BUILDABLES * 2
    int merged_count = 0;

    for (int c = 0; c < 2; c++) {
        SidebarClass::StripClass& strip = Map.Column[c];
        for (int i = 0; i < strip.BuildableCount && merged_count < 60; i++) {
            if (!category_matches(g_hd_grid_category,
                                  strip.Buildables[i].BuildableType)) {
                continue;
            }
            merged[merged_count].strip_col = c;
            merged[merged_count].strip_index = i;
            merged_count++;
        }
    }

    // Clamp scroll position
    int max_top = merged_count - visible_slots;
    if (max_top < 0) max_top = 0;
    if (g_hd_grid_top_index > max_top) g_hd_grid_top_index = max_top;
    if (g_hd_grid_top_index < 0) g_hd_grid_top_index = 0;

    // Register a hit zone for the whole grid (for mouse wheel)
    UIHitZoneID grid_zone = UI_Input_Register_Zone(grid_x, grid_y, grid_w, grid_h);

    const bool show_bg      = UI_Sidebar_Debug_Is_On(COMP_GRID_BG);
    const bool show_cameos  = UI_Sidebar_Debug_Is_On(COMP_CAMEOS);
    const bool show_arrows  = UI_Sidebar_Debug_Is_On(COMP_SCROLL_ARROWS);

    // Render visible items
    for (int slot = 0; slot < visible_slots; slot++) {
        int item_idx = g_hd_grid_top_index + slot;
        int row = slot / HD_GRID_COLS;
        int col = slot % HD_GRID_COLS;

        int cx = grid_x + HD_GRID_ICON_PAD + col * (cell_w + HD_GRID_ICON_PAD);
        int cy = grid_y + HD_GRID_ICON_PAD + row * (cell_h + HD_GRID_ICON_PAD);

        if (item_idx >= merged_count) {
            // Empty slot — just draw faint outline
            if (show_bg) {
                g_ui_draw_list.Draw_Rect(cx, cy, cell_w, icon_h,
                                         SLOT_BORDER_R, SLOT_BORDER_G, SLOT_BORDER_B, 60);
            }
            continue;
        }

        SidebarClass::StripClass& strip = Map.Column[merged[item_idx].strip_col];
        int si = merged[item_idx].strip_index;

        // Slot background — use atlas build frame when available
        if (show_bg) {
            if (use_atlas) {
                emit_atlas_sprite(ATLAS_SIDEBAR_BUILDFRAME, cx, cy, cell_w, icon_h);
            } else {
                g_ui_draw_list.Fill_Rect(cx, cy, cell_w, icon_h,
                                         SLOT_BG_R, SLOT_BG_G, SLOT_BG_B, 200);
                g_ui_draw_list.Draw_Rect(cx, cy, cell_w, icon_h,
                                         SLOT_BORDER_R, SLOT_BORDER_G, SLOT_BORDER_B, 255);
            }
        }

        // Hit zone for this cell
        UI_Input_Push_ID(merged[item_idx].strip_col);
        UI_Input_Push_ID(si);
        UIHitZoneID cell_zone = UI_Input_Register_Zone(cx, cy, cell_w, cell_h,
                                                       UI_Input_Make_Widget_ID(0x43454c4cu));
        UI_Input_Pop_ID();
        UI_Input_Pop_ID();

        // Prefer atlas cameos, fall back to legacy SHP decode
        bool drew_icon = false;
        if (show_cameos) {
            if (use_atlas) {
                drew_icon = emit_atlas_cameo(strip, si, cx + 1, cy + 1, cell_w - 2, icon_h - 2);
            }
            if (!drew_icon) {
                const CameoCacheEntry* cameo = get_best_cameo(strip, si);
                if (cameo && cameo->rgba) {
                    g_ui_draw_list.Draw_Icon(cx + 1, cy + 1, cell_w - 2, icon_h - 2,
                                             reinterpret_cast<const uint8_t*>(cameo->rgba),
                                             cameo->width, cameo->height);
                    drew_icon = true;
                }
            }
        }

        // Production overlays — with progress text for HD grid
        SlotState state = get_slot_state(strip, si);
        emit_production_overlays(cx, cy, cell_w, icon_h, state, strip, si, true);

        // Text label below icon — always show name, even if icon failed
        const char* name = get_buildable_name(strip, si);
        if (name && name[0]) {
            // Use brighter text for items with no icon so they're still identifiable
            uint8_t text_a = drew_icon ? (uint8_t)220 : (uint8_t)255;
            float text_scale = drew_icon ? 0.7f : 0.85f;
            g_ui_draw_list.Draw_Text(cx + 2, cy + icon_h + 1, name, UI_FONT_6PT,
                                     180, 220, 180, text_a, text_scale);
        }

        // Click handling
        if (UI_Input_Was_Clicked(cell_zone)) {
            handle_slot_left_click(strip, si);
        }
        if (UI_Input_Was_Right_Clicked(cell_zone)) {
            handle_slot_right_click(strip, si);
        }
    }

    // Scroll arrows at bottom of grid
    bool can_up = g_hd_grid_top_index > 0;
    bool can_down = g_hd_grid_top_index + visible_slots < merged_count;

    if ((can_up || can_down) && show_arrows) {
        int arrow_y = grid_y + grid_h - 14;
        int arrow_w = grid_w / 3;

        if (can_up) {
            g_ui_draw_list.Fill_Rect(grid_x + 4, arrow_y, arrow_w, 12,
                                     SCROLL_R, SCROLL_R, SCROLL_R, 200);
            g_ui_draw_list.Draw_Text(grid_x + 8, arrow_y + 1, "UP", UI_FONT_6PT,
                                     200, 200, 200, 255);
            UI_Input_Push_String_ID("hd_grid_up");
            UIHitZoneID up_zone = UI_Input_Register_Zone(grid_x + 4, arrow_y, arrow_w, 12,
                                                         UI_Input_Make_Widget_ID(0x48475550u));
            UI_Input_Pop_ID();
            if (UI_Input_Was_Clicked(up_zone)) {
                g_hd_grid_top_index -= HD_GRID_COLS;
                if (g_hd_grid_top_index < 0) g_hd_grid_top_index = 0;
            }
        }
        if (can_down) {
            int dn_x = grid_x + grid_w - arrow_w - 4;
            g_ui_draw_list.Fill_Rect(dn_x, arrow_y, arrow_w, 12,
                                     SCROLL_R, SCROLL_R, SCROLL_R, 200);
            g_ui_draw_list.Draw_Text(dn_x + 4, arrow_y + 1, "DN", UI_FONT_6PT,
                                     200, 200, 200, 255);
            UI_Input_Push_String_ID("hd_grid_down");
            UIHitZoneID dn_zone = UI_Input_Register_Zone(dn_x, arrow_y, arrow_w, 12,
                                                         UI_Input_Make_Widget_ID(0x4847444eu));
            UI_Input_Pop_ID();
            if (UI_Input_Was_Clicked(dn_zone)) {
                g_hd_grid_top_index += HD_GRID_COLS;
            }
        }
    }

    // Mouse wheel scrolling — only consume when hovering over the grid
    UIHitZoneID hovered = UI_Input_Get_Hovered();
    if (hovered == grid_zone) {
        float scroll_delta = UI_Input_Consume_Scroll_Delta();
        if (scroll_delta > 0.0f && can_up) {
            g_hd_grid_top_index -= HD_GRID_COLS;
            if (g_hd_grid_top_index < 0) g_hd_grid_top_index = 0;
        } else if (scroll_delta < 0.0f && can_down) {
            g_hd_grid_top_index += HD_GRID_COLS;
        }
    }
}

// ---------------------------------------------------------------------------
// Power bar rendering
// ---------------------------------------------------------------------------

/// Emit a vertical power bar gauge on the left edge of the sidebar.
static void emit_power_bar(int x, int y, int w, int h, bool use_atlas)
{
    if (!PlayerPtr) return;

    int power = PlayerPtr->Power;
    int drain = PlayerPtr->Drain;

    // Bar background — use atlas chrome if available
    if (use_atlas) {
        emit_atlas_sprite(ATLAS_SIDEBAR_POWERBG, x, y, w, h);
        emit_atlas_sprite(ATLAS_SIDEBAR_POWERFRAMING, x, y, w, h);
    } else {
        g_ui_draw_list.Fill_Rect(x, y, w, h, POW_BG_R, POW_BG_G, POW_BG_B, 200);
        g_ui_draw_list.Draw_Rect(x, y, w, h, 60, 60, 60, 200);
    }

    if (h < 4) return;

    int inner_x = x + 1;
    int inner_w = w - 2;
    int inner_h = h - 2;
    int inner_y = y + 1;
    int bottom  = inner_y + inner_h;

    // Logarithmic scale matching legacy Power_Height
    auto compute_height = [inner_h](int value) -> int {
        int retval = 0;
        int num = value / 100;
        int remainder = value - num * 100;
        for (int i = 0; i < num; i++) {
            retval = retval + ((inner_h - retval) / 6);
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

    // Bar color
    uint8_t bar_r, bar_g, bar_b;
    if (drain == 0 || power >= drain) {
        bar_r = POW_GREEN_R; bar_g = POW_GREEN_G; bar_b = POW_GREEN_B;
    } else if (power * 2 >= drain) {
        bar_r = POW_YELLOW_R; bar_g = POW_YELLOW_G; bar_b = POW_YELLOW_B;
    } else {
        bar_r = POW_RED_R; bar_g = POW_RED_G; bar_b = POW_RED_B;
    }

    if (power_h > 0) {
        g_ui_draw_list.Fill_Rect(inner_x, bottom - power_h, inner_w, power_h,
                                 bar_r, bar_g, bar_b, 220);
    }

    if (drain_h > 0 && drain > 0) {
        int drain_y = bottom - drain_h;
        g_ui_draw_list.Fill_Rect(inner_x, drain_y, inner_w, 1,
                                 255, 255, 255, 200);
        g_ui_draw_list.Fill_Rect(x, drain_y - 1, 2, 3,
                                 255, 255, 255, 240);
    }

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

static const void* s_repair_shape = nullptr;
static const void* s_sell_shape   = nullptr;
static const void* s_map_shape    = nullptr;
static bool s_btn_shapes_loaded   = false;

/// Cached sidebar frame shape pointers.
static const void* s_sidebar_shape1 = nullptr;
static const void* s_sidebar_shape2 = nullptr;
static bool s_frame_shapes_loaded   = false;

static void load_button_shapes()
{
    if (s_btn_shapes_loaded) return;
    s_btn_shapes_loaded = true;
    s_repair_shape = Hires_Retrieve((char*)"REPAIR.SHP");
    s_sell_shape   = Hires_Retrieve((char*)"SELL.SHP");
    s_map_shape    = Hires_Retrieve((char*)"MAP.SHP");
}

static void load_frame_shapes()
{
    if (s_frame_shapes_loaded) return;
    s_frame_shapes_loaded = true;
    s_sidebar_shape1 = SidebarClass::SidebarShape1;
    s_sidebar_shape2 = SidebarClass::SidebarShape2;
}

/// Atlas sprite names for a multi-state button (off/on/hover/press).
struct AtlasButtonSprites {
    const char* off;
    const char* on;
    const char* hover;
    const char* press;
};

/// Atlas sprite names for an HD mode-tab button in the row below the radar.
struct AtlasModeTabSprites {
    const char* bg_off;
    const char* bg_on;
    const char* icon;
};

/// Draw a sprite centered inside a target rect while preserving its aspect ratio.
static bool emit_atlas_sprite_centered(const char* name,
                                       int x, int y, int w, int h,
                                       int pad_x, int pad_y,
                                       uint8_t r = 255, uint8_t g = 255,
                                       uint8_t b = 255, uint8_t a = 255)
{
    const AtlasSpriteRect* rect = Commandbar_Atlas_Find(name);
    if (!rect) return false;

    int avail_w = w - pad_x * 2;
    int avail_h = h - pad_y * 2;
    if (avail_w <= 0 || avail_h <= 0 || rect->width <= 0 || rect->height <= 0) {
        return false;
    }

    int draw_w = avail_w;
    int draw_h = (draw_w * rect->height) / rect->width;
    if (draw_h > avail_h) {
        draw_h = avail_h;
        draw_w = (draw_h * rect->width) / rect->height;
    }
    if (draw_w <= 0 || draw_h <= 0) return false;

    int draw_x = x + (w - draw_w) / 2;
    int draw_y = y + (h - draw_h) / 2;
    g_ui_draw_list.Draw_Atlas_Sprite(draw_x, draw_y, draw_w, draw_h,
                                     rect->u0, rect->v0, rect->u1, rect->v1,
                                     r, g, b, a);
    return true;
}

/// Emit a sidebar button with SHP art if available, falling back to text.
/// When use_atlas is true, atlas sprites provide the button chrome.
static bool emit_sidebar_button(int x, int y, int w, int h,
                                const void* shapefile, int frame,
                                const char* fallback_label,
                                bool is_active,
                                const UIButtonStyle& style,
                                bool use_atlas = false,
                                const AtlasButtonSprites* atlas = nullptr)
{
    UI_Input_Push_Pointer_ID(shapefile);
    UI_Input_Push_ID(frame);
    UIButtonState state = UI_Button(x, y, w, h, nullptr, style);
    UI_Input_Pop_ID();
    UI_Input_Pop_ID();

    if (use_atlas && atlas) {
        // Pick sprite based on interaction state
        const char* sprite = atlas->off;
        if (is_active)                              sprite = atlas->on;
        if (state == UI_BTN_HOVERED && atlas->hover) sprite = atlas->hover;
        if (state == UI_BTN_PRESSED && atlas->press) sprite = atlas->press;
        if (sprite) {
            emit_atlas_sprite(sprite, x, y, w, h);
        }
    } else {
        const CameoCacheEntry* icon = sprite_decode(shapefile, frame, nullptr, 255);
        if (icon && icon->rgba) {
            g_ui_draw_list.Draw_Icon(x + 1, y + 1, w - 2, h - 2,
                                     reinterpret_cast<const uint8_t*>(icon->rgba),
                                     icon->width, icon->height);
        } else {
            int tx = x + 2;
            int ty = y + (h > 8 ? 2 : 1);
            g_ui_draw_list.Draw_Text(tx, ty, fallback_label, style.font,
                                     style.text_r, style.text_g, style.text_b,
                                     style.text_a, style.text_scale);
        }

        if (is_active) {
            g_ui_draw_list.Draw_Rect(x, y, w, h, 255, 255, 0, 180);
        }
    }

    return UI_Button_Activated(state);
}

/// Emit a small atlas-backed icon button with text fallback.
static bool emit_icon_button(int x, int y, int w, int h,
                             const char* fallback_label,
                             bool use_atlas,
                             const char* off_sprite,
                             const char* hover_sprite,
                             const char* press_sprite)
{
    UIButtonStyle style = UI_Default_Button_Style();
    style.normal_r = 0; style.normal_g = 0; style.normal_b = 0; style.normal_a = 0;
    style.hover_r  = 0; style.hover_g  = 0; style.hover_b  = 0; style.hover_a  = 0;
    style.press_r  = 0; style.press_g  = 0; style.press_b  = 0; style.press_a  = 0;
    style.border_r = 0; style.border_g = 0; style.border_b = 0; style.border_a = 0;

    UI_Input_Push_String_ID(off_sprite ? off_sprite : fallback_label);
    UIButtonState state = UI_Button(x, y, w, h, nullptr, style);
    UI_Input_Pop_ID();
    if (use_atlas) {
        const char* sprite = off_sprite;
        if (state == UI_BTN_HOVERED && hover_sprite) sprite = hover_sprite;
        if (state == UI_BTN_PRESSED && press_sprite) sprite = press_sprite;
        if (sprite) {
            emit_atlas_sprite(sprite, x, y, w, h);
        }
    } else if (fallback_label && fallback_label[0]) {
        g_ui_draw_list.Draw_Text(x + 2, y + 1, fallback_label, UI_FONT_6PT,
                                 0, 220, 0, 240, 0.9f);
    }

    return UI_Button_Activated(state);
}

/// Emit one HD mode-tab button. Active/hovered buttons use the atlas "_ON" art.
static bool emit_mode_tab_button(int x, int y, int w, int h,
                                 const char* fallback_label,
                                 bool is_active,
                                 bool use_atlas,
                                 const AtlasModeTabSprites* atlas)
{
    UIButtonStyle style = UI_Default_Button_Style();
    if (use_atlas) {
        // Atlas sprites own the chrome — suppress the default fill.
        style.normal_r = 0; style.normal_g = 0; style.normal_b = 0; style.normal_a = 0;
        style.hover_r  = 0; style.hover_g  = 0; style.hover_b  = 0; style.hover_a  = 0;
        style.press_r  = 0; style.press_g  = 0; style.press_b  = 0; style.press_a  = 0;
        style.border_r = 0; style.border_g = 0; style.border_b = 0; style.border_a = 0;
    } else {
        style.normal_r = 30; style.normal_g = 38; style.normal_b = 30; style.normal_a = 220;
        style.hover_r  = 44; style.hover_g  = 58; style.hover_b  = 44; style.hover_a  = 230;
        style.press_r  = 22; style.press_g  = 32; style.press_b  = 22; style.press_a  = 235;
        style.border_r = 0;  style.border_g = 110; style.border_b = 0;  style.border_a = 180;
    }
    style.text_r = is_active ? 220 : 180;
    style.text_g = 255;
    style.text_b = is_active ? 120 : 180;
    style.text_a = 255;
    style.font = UI_FONT_6PT;

    UI_Input_Push_String_ID(atlas && atlas->icon ? atlas->icon : fallback_label);
    UIButtonState state = UI_Button(x, y, w, h, nullptr, style);
    UI_Input_Pop_ID();
    bool highlight = is_active || state == UI_BTN_HOVERED || state == UI_BTN_PRESSED;

    if (use_atlas && atlas) {
        const char* bg = highlight ? atlas->bg_on : atlas->bg_off;
        if (bg) {
            emit_atlas_sprite(bg, x, y, w, h);
        }

        bool drew_icon = false;
        if (atlas->icon) {
            int pad_x = w / 4;
            int pad_y = h / 5;
            if (pad_x < 4) pad_x = 4;
            if (pad_y < 3) pad_y = 3;
            drew_icon = emit_atlas_sprite_centered(atlas->icon, x, y, w, h, pad_x, pad_y);
        }

        if (!drew_icon && fallback_label && fallback_label[0]) {
            g_ui_draw_list.Draw_Text(x + w / 2 - 6, y + h / 2 - 4, fallback_label, UI_FONT_6PT,
                                     style.text_r, style.text_g, style.text_b, style.text_a, 0.9f);
        }
    } else if (fallback_label && fallback_label[0]) {
        int text_w = UI_Text_Measure_Width(UI_FONT_6PT, fallback_label,
                                           static_cast<int>(strlen(fallback_label)));
        g_ui_draw_list.Draw_Text(x + (w - text_w) / 2, y + h / 2 - 4, fallback_label, UI_FONT_6PT,
                                 style.text_r, style.text_g, style.text_b, style.text_a, 0.9f);
    }

    return UI_Button_Activated(state);
}

// ---------------------------------------------------------------------------
// Credits display
// ---------------------------------------------------------------------------

static void emit_credits(int x, int y, int w, float sx)
{
    if (!PlayerPtr) return;

    // Use the tweened display value from CreditClass for smooth countdown
    // animation.  CreditClass::AI() (called from TabClass::AI()) advances
    // the tween each tick and CreditClass::Graphic_Logic() (called from
    // TabClass::Draw_It()) plays VOC_UP / VOC_DOWN sounds automatically.
    long display_credits = Map.Credits.Current;
    long tiberium = PlayerPtr->Tiberium;

    char buf[32];
    snprintf(buf, sizeof(buf), "$%ld", display_credits);
    int text_w = UI_Text_Measure_Width(UI_FONT_6PT, buf, static_cast<int>(strlen(buf)));
    if (tiberium > 0) {
        char tbuf[32];
        snprintf(tbuf, sizeof(tbuf), "T:%ld", tiberium);
        int tib_w = UI_Text_Measure_Width(UI_FONT_6PT, tbuf, static_cast<int>(strlen(tbuf)));
        int gap = scale_x_from_legacy(6, sx);
        int pair_w = text_w + gap + tib_w;
        int text_x = x + (w - pair_w) / 2;
        g_ui_draw_list.Draw_Text(text_x, y + 2, buf, UI_FONT_6PT,
                                 0, 220, 0, 240, 0.9f);
        g_ui_draw_list.Draw_Text(text_x + text_w + gap, y + 2, tbuf, UI_FONT_6PT,
                                 180, 200, 0, 200, 0.8f);
    } else {
        int text_x = x + (w - text_w) / 2;
        g_ui_draw_list.Draw_Text(text_x, y + 2, buf, UI_FONT_6PT,
                                 0, 220, 0, 240, 0.9f);
    }
}

// ---------------------------------------------------------------------------
// Sidebar frame rendering
// ---------------------------------------------------------------------------

static void emit_sidebar_frame(int x, int y, int w, int h, float sx, float sy,
                               bool use_atlas)
{
    if (use_atlas) {
        // HD atlas sidebar background — stretch to fill
        emit_atlas_sprite(ATLAS_SIDEBAR_BUILDBARBG, x, y, w, h);
        return;
    }

    load_frame_shapes();

    // Try to render legacy sidebar frame shapes as background
    bool drew_frame = false;
    if (s_sidebar_shape1) {
        const CameoCacheEntry* frame1 = sprite_decode(s_sidebar_shape1, 0, nullptr, 255);
        if (frame1 && frame1->rgba) {
            // Tile the frame shape vertically across the sidebar
            int tile_h = scale_y_from_legacy(frame1->height, sy);
            for (int ty = y; ty < y + h; ty += tile_h) {
                int draw_h = (ty + tile_h > y + h) ? (y + h - ty) : tile_h;
                g_ui_draw_list.Draw_Icon(x, ty, w, draw_h,
                                         reinterpret_cast<const uint8_t*>(frame1->rgba),
                                         frame1->width, frame1->height);
            }
            drew_frame = true;
        }
    }

    if (!drew_frame) {
        // Fallback: dark background with subtle gradient feel
        g_ui_draw_list.Fill_Rect(x, y, w, h, 24, 24, 24, 56);
    }

    // Left edge highlight
    g_ui_draw_list.Fill_Rect(x, y, 1, h, 84, 84, 84, 220);
}

// ---------------------------------------------------------------------------
// SidebarMetrics — single source of truth for layout sizes
// ---------------------------------------------------------------------------

SidebarMetrics UI_Sidebar_Compute_Metrics()
{
    SidebarMetrics m{};

    Render_Bridge_Get_Sidebar_Rect(m.side_x, m.side_y, m.side_w, m.side_h);
    if (m.side_w <= 0) return m;  // caller checks m.valid()

    const int base_w = SeenBuff.Get_Width();
    const int base_h = SeenBuff.Get_Height();
    int logical_w = 0, logical_h = 0;
    Render_Bridge_Get_Logical_Screen_Size(logical_w, logical_h);
    m.sx = (base_w > 0) ? static_cast<float>(logical_w) / static_cast<float>(base_w) : 1.0f;
    m.sy = (base_h > 0) ? static_cast<float>(logical_h) / static_cast<float>(base_h) : 1.0f;
    m.hd_mode = Render_Bridge_Get_HD_Graphics();

    // Legacy-pixel constants, scaled once.
    // HD mode will override top_bar_h below from sidebar-layout.json.
    m.top_bar_h         = scale_y_from_legacy(16, m.sy);
    m.top_btn_inset     = 2;
    m.mode_tab_h        = scale_y_from_legacy(18, m.sy);
    m.mode_tab_gap      = scale_y_from_legacy(3,  m.sy);
    m.category_h        = scale_y_from_legacy(16, m.sy);
    m.category_gap      = scale_y_from_legacy(2,  m.sy);
    m.power_bar_w       = scale_x_from_legacy(8,  m.sx);
    m.power_bar_x_inset = 2;
    m.radar_gap_below   = scale_y_from_legacy(13, m.sy);
    m.btn_row_h         = scale_y_from_legacy(16, m.sy);
    m.credits_text_pad  = scale_x_from_legacy(6,  m.sx);
    m.grid_cols         = HD_GRID_COLS;
    m.grid_icon_pad     = HD_GRID_ICON_PAD;
    m.grid_text_h       = scale_y_from_legacy(HD_GRID_TEXT_H, m.sy);
    m.grid_x_inset      = scale_x_from_legacy(6, m.sx);
    m.grid_x_pad        = scale_x_from_legacy(8, m.sx);

    // Anchors — derive once.
    int radar_frame_x = 0, radar_frame_y = 0, radar_frame_w = 0, radar_frame_h = 0;
    if (UI_Radar_Resolve_Frame_Rect(logical_w, logical_h,
                                    radar_frame_x, radar_frame_y,
                                    radar_frame_w, radar_frame_h)) {
        m.radar_bottom_y = radar_frame_y + radar_frame_h + m.radar_gap_below;
    } else {
        m.radar_bottom_y = scale_y_from_legacy(Map.RadY + Map.RadHeight, m.sy)
                         + m.radar_gap_below;
    }
    m.tab_row_y      = m.radar_bottom_y;
    m.cat_row_y      = m.hd_mode ? (m.tab_row_y + m.mode_tab_h + m.mode_tab_gap)
                                 : m.tab_row_y;
    m.power_bar_x    = m.side_x + m.power_bar_x_inset;
    m.power_bar_y    = m.hd_mode
                     ? (m.cat_row_y + m.category_h + m.category_gap)
                     : m.radar_bottom_y;
    m.bottom_btn_y   = m.hd_mode
                     ? (m.side_y + m.side_h - 2)
                     : (m.side_y + m.side_h - m.btn_row_h - 2);
    m.power_bar_h    = m.bottom_btn_y - m.power_bar_y - 2;
    m.prod_area_x    = m.side_x + m.power_bar_w + m.grid_x_inset;
    m.prod_area_y    = m.power_bar_y;
    m.prod_area_w    = m.side_w - m.power_bar_w - m.grid_x_pad;
    m.prod_area_h    = m.bottom_btn_y - m.prod_area_y - 4;

    // Per-component rects — mirror the inline math in UI_Sidebar_Emit so
    // debug overrides can patch any single target via the metrics hook.
    //
    // HD top bar has its own geometry (sidebar-layout.json) — it is wider
    // than the sidebar and extends to its left. Legacy keeps the sidebar-span
    // derivation.
    if (m.hd_mode) {
        auto tb = render_bridge::HDSidebarLayout::Instance()
                      .Resolve("top_bar", logical_w, logical_h);
        if (tb.valid()) {
            m.top_bar_x = tb.x;
            m.top_bar_y = tb.y;
            m.top_bar_w = tb.w;
            m.top_bar_h = tb.h;
        } else {
            m.top_bar_x = m.side_x;
            m.top_bar_y = m.side_y;
            m.top_bar_w = m.side_w;
            m.top_bar_h = scale_y_from_legacy(16, m.sy);
        }
    } else {
        m.top_bar_x = m.side_x;
        m.top_bar_y = m.side_y;
        m.top_bar_w = m.side_w;
    }

    const int top_btn = m.top_bar_h - 2;
    if (m.hd_mode) {
        auto menu_btn = render_bridge::HDSidebarLayout::Instance()
                            .Resolve("menu_button", logical_w, logical_h);
        if (menu_btn.valid()) {
            m.menu_btn_x = menu_btn.x;
            m.menu_btn_y = menu_btn.y;
            m.menu_btn_w = menu_btn.w;
            m.menu_btn_h = menu_btn.h;
        } else {
            m.menu_btn_w  = top_btn;
            m.menu_btn_h  = top_btn;
            m.menu_btn_x  = m.side_x + m.side_w - m.menu_btn_w - 2;
            m.menu_btn_y  = m.side_y + 1;
        }

        // HD: map button is the 3rd (last) slot in the mode-tabs row, replacing
        // the legacy "B" tab. Allow a one-off "map_button" rect in
        // sidebar-layout.json to override.
        auto map_btn = render_bridge::HDSidebarLayout::Instance()
                           .Resolve("map_button", logical_w, logical_h);
        if (map_btn.valid()) {
            m.map_btn_x = map_btn.x;
            m.map_btn_y = map_btn.y;
            m.map_btn_w = map_btn.w;
            m.map_btn_h = map_btn.h;
        } else {
            const int tabs_x = m.side_x + 2;
            const int tabs_w = m.side_w - 4;
            const int slot_w = tabs_w / 3;
            const int rem    = tabs_w % 3;
            // First two slots absorb the remainder (matches the rendering
            // distribution below); last slot is the bare base width.
            m.map_btn_x = tabs_x + 2 * slot_w + rem;
            m.map_btn_y = m.tab_row_y;
            m.map_btn_w = slot_w;
            m.map_btn_h = m.mode_tab_h;
        }
    } else {
        m.menu_btn_w  = top_btn;
        m.menu_btn_h  = top_btn;
        m.menu_btn_x  = m.side_x + m.side_w - m.menu_btn_w - 2;
        m.menu_btn_y  = m.side_y + 1;

        m.map_btn_w   = top_btn;
        m.map_btn_h   = top_btn;
        m.map_btn_x   = m.menu_btn_x - m.map_btn_w - 2;
        m.map_btn_y   = m.side_y + 1;
    }

    if (m.hd_mode) {
        auto credits = render_bridge::HDSidebarLayout::Instance()
                           .Resolve("credits", logical_w, logical_h);
        if (credits.valid()) {
            m.credits_x = credits.x;
            m.credits_y = credits.y;
            m.credits_w = credits.w;
            m.credits_h = credits.h;
        } else {
            m.credits_x = m.side_x + 4;
            m.credits_y = m.side_y + 2;
            m.credits_w = m.map_btn_x - m.credits_x - 4;
            m.credits_h = m.top_bar_h - 4;
        }
    } else {
        m.credits_x = m.side_x + 4;
        m.credits_y = m.side_y + 2;
        m.credits_w = m.map_btn_x - m.credits_x - 4;
        m.credits_h = m.top_bar_h - 4;
    }

    m.mode_tabs_x = m.side_x + 2;
    m.mode_tabs_w = m.side_w - 4;

    m.cat_row_x   = m.side_x + 2;
    m.cat_row_w   = m.side_w - 4;

    if (g_sidebar_metrics_hook) g_sidebar_metrics_hook(m);
    return m;
}

// ---------------------------------------------------------------------------
// Main entry point
// ---------------------------------------------------------------------------

void UI_Sidebar_Emit()
{
    extern bool InMainLoop;
    if (!InMainLoop) return;
    if (!Map.IsSidebarActive) return;

    SidebarMetrics m = UI_Sidebar_Compute_Metrics();
    if (!m.valid()) return;

    // Try to load the HD command bar atlas when in HD mode
    bool use_atlas = m.hd_mode && ensure_commandbar_atlas();

    static bool logged_sidebar_state = false;
    if (!logged_sidebar_state) {
        logged_sidebar_state = true;
        DBG("[HD-SIDEBAR] UI_Sidebar_Emit: hd_mode=%d use_atlas=%d atlas_ready=%d "
            "side_rect=(%d,%d,%d,%d) sx=%.2f sy=%.2f top_h=%d tab_h=%d cat_h=%d pow_w=%d",
            m.hd_mode, use_atlas, Commandbar_Atlas_Is_Ready(),
            m.side_x, m.side_y, m.side_w, m.side_h, m.sx, m.sy,
            m.top_bar_h, m.mode_tab_h, m.category_h, m.power_bar_w);
    }

    // Sidebar background frame
    if (UI_Sidebar_Debug_Is_On(COMP_SIDEBAR_FRAME)) {
        emit_sidebar_frame(m.side_x, m.side_y, m.side_w, m.side_h, m.sx, m.sy, use_atlas);
    }

    // --- Top button bar (HD only — legacy mode uses SIDE1.SHP chrome) ---
    if (m.hd_mode) {
        if (use_atlas && UI_Sidebar_Debug_Is_On(COMP_TOP_BAR)) {
            emit_atlas_sprite(ATLAS_SIDEBAR_TOPBUTTON,
                              m.top_bar_x, m.top_bar_y, m.top_bar_w, m.top_bar_h);
        }

        if (UI_Sidebar_Debug_Is_On(COMP_CREDITS)) {
            emit_credits(m.credits_x, m.credits_y, m.credits_w, m.sx);
        }

        if (UI_Sidebar_Debug_Is_On(COMP_MENU_BTN) &&
            emit_icon_button(m.menu_btn_x, m.menu_btn_y, m.menu_btn_w, m.menu_btn_h,
                             "M", use_atlas,
                             ATLAS_SIDEBAR_MENUBTN_OFF,
                             ATLAS_SIDEBAR_MENUBTN_HOVER,
                             ATLAS_SIDEBAR_MENUBTN_PRESS)) {
            // Defer to the outer conquer loop — Options.Process() runs its own
            // Main_Loop() and must not be invoked from inside UI emission.
            SpecialDialog = SDLG_OPTIONS;
        }
    }

    // Aliases for readability within the legacy block below.
    const int pow_w   = m.power_bar_w;
    const int pow_x   = m.power_bar_x;
    const int btn_h   = m.btn_row_h;
    const int btn_y   = m.bottom_btn_y;
    int       pow_y   = m.power_bar_y;

    bool repair_active = Map.IsRepairMode != 0;
    bool sell_active = Map.IsSellMode != 0;

    if (m.hd_mode) {
        int tab_y = m.tab_row_y;
        int tab_h = m.mode_tab_h;
        int tab_x = m.mode_tabs_x;
        int tab_w = m.mode_tabs_w;
        int tab_btn_base_w = tab_w / 3;
        int tab_btn_rem = tab_w % 3;
        int tab_btn_w0 = tab_btn_base_w + (tab_btn_rem > 0 ? 1 : 0);
        int tab_btn_w1 = tab_btn_base_w + (tab_btn_rem > 1 ? 1 : 0);
        int tab_btn_w2 = tab_btn_base_w;
        int tab_x0 = tab_x;
        int tab_x1 = tab_x0 + tab_btn_w0;
        int tab_x2 = tab_x1 + tab_btn_w1;

        static const AtlasModeTabSprites repair_tab = {
            ATLAS_SIDEBAR_TABBUTTON_ENABLED,
            ATLAS_SIDEBAR_TABBUTTON_HIGHLIGHTED,
            ATLAS_SIDEBAR_MODETAB_REPAIR_ICON
        };
        static const AtlasModeTabSprites sell_tab = {
            ATLAS_SIDEBAR_TABBUTTON_ENABLED,
            ATLAS_SIDEBAR_TABBUTTON_HIGHLIGHTED,
            ATLAS_SIDEBAR_MODETAB_SELL_ICON
        };
        if (UI_Sidebar_Debug_Is_On(COMP_MODE_TABS)) {
            if (emit_mode_tab_button(tab_x0, tab_y, tab_btn_w0, tab_h,
                                     "R", repair_active, use_atlas, &repair_tab)) {
                Map.Repair_Mode_Control(-1);
            }
            if (emit_mode_tab_button(tab_x1, tab_y, tab_btn_w1, tab_h,
                                     "$", sell_active, use_atlas, &sell_tab)) {
                Map.Sell_Mode_Control(-1);
            }
        }

        // Third slot of the mode-tabs row is the radar / player-names toggle
        // (replaces the legacy bottom MAP button). Gated by COMP_MAP_BTN so
        // the debug panel can still toggle it independently of R/$.
        if (UI_Sidebar_Debug_Is_On(COMP_MAP_BTN) &&
            emit_icon_button(tab_x2, tab_y, tab_btn_w2, tab_h,
                             "M", use_atlas,
                             ATLAS_SIDEBAR_BTN_MAP_OFF,
                             ATLAS_SIDEBAR_BTN_MAP_HOVER,
                             ATLAS_SIDEBAR_BTN_MAP_PRESS)) {
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
            } else if (GameToPlay != GAME_NORMAL) {
                Map.Player_Names(Map.Is_Player_Names() == 0);
            }
        }

        // Category selector row — filters the production grid.
        int cat_h = m.category_h;
        int cat_y = m.cat_row_y;
        int cat_x = m.cat_row_x;
        int cat_w = m.cat_row_w;
        int cat_btn_base_w = cat_w / 4;
        int cat_btn_rem = cat_w % 4;
        int cat_btn_w[4] = {
            cat_btn_base_w + (cat_btn_rem > 0 ? 1 : 0),
            cat_btn_base_w + (cat_btn_rem > 1 ? 1 : 0),
            cat_btn_base_w + (cat_btn_rem > 2 ? 1 : 0),
            cat_btn_base_w,
        };

        static const AtlasModeTabSprites cat_infantry = {
            ATLAS_SIDEBAR_TABBUTTON_ENABLED,
            ATLAS_SIDEBAR_TABBUTTON_HIGHLIGHTED,
            ATLAS_SIDEBAR_BUILDTABICON_INFANTRY
        };
        static const AtlasModeTabSprites cat_vehicle = {
            ATLAS_SIDEBAR_TABBUTTON_ENABLED,
            ATLAS_SIDEBAR_TABBUTTON_HIGHLIGHTED,
            ATLAS_SIDEBAR_BUILDTABICON_VEHICLE
        };
        static const AtlasModeTabSprites cat_structure = {
            ATLAS_SIDEBAR_TABBUTTON_ENABLED,
            ATLAS_SIDEBAR_TABBUTTON_HIGHLIGHTED,
            ATLAS_SIDEBAR_BUILDTABICON_STRUCTURE
        };
        static const AtlasModeTabSprites cat_support = {
            ATLAS_SIDEBAR_TABBUTTON_ENABLED,
            ATLAS_SIDEBAR_TABBUTTON_HIGHLIGHTED,
            ATLAS_SIDEBAR_BUILDTABICON_SUPPORT
        };

        struct CatButton {
            const char* fallback;
            SidebarCategory cat;
            const AtlasModeTabSprites* sprites;
        };
        const CatButton cat_buttons[4] = {
            { "I", CAT_INFANTRY,  &cat_infantry  },
            { "V", CAT_VEHICLE,   &cat_vehicle   },
            { "S", CAT_STRUCTURE, &cat_structure },
            { "?", CAT_SUPPORT,   &cat_support   },
        };

        if (UI_Sidebar_Debug_Is_On(COMP_CATEGORY_ROW)) {
            int cat_bx = cat_x;
            for (int i = 0; i < 4; i++) {
                bool is_active = (g_hd_grid_category == cat_buttons[i].cat);
                if (emit_mode_tab_button(cat_bx, cat_y, cat_btn_w[i], cat_h,
                                         cat_buttons[i].fallback,
                                         is_active, use_atlas,
                                         cat_buttons[i].sprites)) {
                    if (g_hd_grid_category != cat_buttons[i].cat) {
                        g_hd_grid_category = cat_buttons[i].cat;
                        g_hd_grid_top_index = 0;
                    }
                }
                cat_bx += cat_btn_w[i];
            }
        }
    }

    int pow_h = m.power_bar_h;
    if (pow_h > 10 && UI_Sidebar_Debug_Is_On(COMP_POWER_BAR)) {
        emit_power_bar(pow_x, pow_y, pow_w, pow_h, use_atlas);
    } else if (pow_h <= 10) {
        static bool logged_power_skip = false;
        if (!logged_power_skip) {
            logged_power_skip = true;
            DBG("[HD-SIDEBAR] skipping power bar: pow_y=%d btn_y=%d pow_h=%d side_h=%d hd_mode=%d",
                pow_y, btn_y, pow_h, m.side_h, m.hd_mode ? 1 : 0);
        }
    }

    // --- Production area ---
    if (m.hd_mode) {
        if (m.prod_area_w > 60 && m.prod_area_h > 40) {
            // The grid itself always runs; individual layers (backgrounds,
            // cameos, overlays, scroll arrows) are gated inside emit_hd_grid
            // by their respective COMP_* bits.
            emit_hd_grid(m.prod_area_x, m.prod_area_y, m.prod_area_w, m.prod_area_h,
                         m.sx, m.sy, use_atlas);
        }
    } else {
        // Legacy 2-column layout — use scaled strip positions
        for (int c = 0; c < 2; c++) {
            SidebarClass::StripClass strip = Map.Column[c];
            strip.X = scale_x_from_legacy(strip.X, m.sx);
            strip.Y = scale_y_from_legacy(strip.Y, m.sy);
            strip.ObjectWidth = scale_x_from_legacy(strip.ObjectWidth, m.sx);
            strip.ObjectHeight = scale_y_from_legacy(strip.ObjectHeight, m.sy);
            strip.StripWidth = scale_x_from_legacy(strip.StripWidth, m.sx);
            strip.LeftEdgeOffset = scale_x_from_legacy(strip.LeftEdgeOffset, m.sx);
            emit_column(strip, m.hd_mode, c);
        }
    }

    // --- Legacy-only Repair / Sell / Map bottom row ---
    // The HD path hosts repair/sell as mode tabs under the radar and the
    // radar/zoom toggle in the top strip, so no bottom chrome is needed.
    if (!m.hd_mode && UI_Sidebar_Debug_Is_On(COMP_LEGACY_BOTTOM)) {
        load_button_shapes();

        int btn_w = m.side_w / 3;

        UIButtonStyle bs = UI_Default_Button_Style();
        bs.normal_r = 54; bs.normal_g = 70; bs.normal_b = 54;
        bs.hover_r = 70; bs.hover_g = 94; bs.hover_b = 70;
        bs.press_r = 44; bs.press_g = 56; bs.press_b = 44;
        bs.text_r = 0; bs.text_g = 200; bs.text_b = 0;
        bs.font = UI_FONT_6PT;

        if (emit_sidebar_button(m.side_x + 2, btn_y, btn_w - 2, btn_h,
                                s_repair_shape, repair_active ? 1 : 0,
                                "RPR", repair_active, bs,
                                false, nullptr)) {
            Map.Repair_Mode_Control(-1);
        }

        if (emit_sidebar_button(m.side_x + btn_w + 1, btn_y, btn_w - 2, btn_h,
                                s_sell_shape, sell_active ? 1 : 0,
                                "SEL", sell_active, bs,
                                false, nullptr)) {
            Map.Sell_Mode_Control(-1);
        }

        if (emit_sidebar_button(m.side_x + btn_w * 2, btn_y, btn_w - 2, btn_h,
                                s_map_shape, 0,
                                "MAP", false, bs,
                                false, nullptr)) {
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
            } else if (GameToPlay != GAME_NORMAL) {
                Map.Player_Names(Map.Is_Player_Names() == 0);
            }
        }
    }
}
