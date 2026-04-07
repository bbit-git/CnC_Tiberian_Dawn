/**
 * gl_sprites.cpp — GPU sprite rendering from the draw list.
 *
 * Connects the draw list to the render library's ISpriteProvider
 * and TextureAtlas. Instead of replaying CC_Draw_Shape (CPU decode +
 * Buffer_Frame_To_Page), sprites are:
 *   1. Decoded via LegacySpriteProvider::Get_Frame() → 8-bit pixels
 *   2. Converted to RGBA using current palette
 *   3. Packed into TextureAtlas pages
 *   4. Rendered as textured quads via GL (one draw call per atlas page)
 *
 * Atlas is built incrementally — shapes are packed on first encounter
 * and cached for subsequent frames. Palette changes invalidate the atlas.
 *
 * This is the final step toward full GL tactical rendering.
 * Currently a prototype — functional but not optimized.
 */

#include "render_bridge.h"
#include "draw_list.h"
#include "legacy_sprite_provider.h"
#include "hd_sprite_provider.h"
#include "hd_terrain_provider.h"
#include "texture_atlas.h"
#include "gl/gl_house_color.h"
#include "gl/gl_sprite_batch.h"
#include "terrain_replay_math.h"
#include "function.h"
#include "dbg.h"
#include <cstdio>
#include <cstdlib>

// td_platform.h defines min/max macros that conflict with GL headers
#undef min
#undef max
#include <GLES2/gl2.h>
#include <unordered_map>
#include <cstring>

enum class SpriteRenderKind : uint8_t {
    Normal,
    Shadow,
    Ghost,
    TranslucentGhost,
    SpecialGhost,
    WhiteGhost,
    MouseGhost,
};

struct SpriteRenderStyle {
    const uint8_t* remap_table = nullptr;
    SpriteRenderKind kind = SpriteRenderKind::Normal;
    uint8_t alpha = 255;
    float house_hue = -1.0f;
    bool supported = true;
    bool hd_only = false;  // shape only passed classify because HD is available;
                           // must NOT fall back to legacy GL if HD frame missing
};

static LegacySpriteProvider g_provider;
static HDSpriteProvider*    g_hd_provider = nullptr;
static HDTerrainProvider*   g_terrain_provider = nullptr;
static std::unordered_map<const void*, uint32_t> g_shape_identity;
static TextureAtlas         g_atlas;
static GLSpriteBatch        g_batch;
static bool                 g_atlas_ready = false;
static int                  g_last_rendered_sprites = 0;
static int                  g_last_draw_calls = 0;
static int                  g_last_fallback_sprites = 0;
static int                  g_last_atlas_new_shapes = 0;
static int                  g_last_terrain_miss_count = 0;

// Sprite cache: keyed by (shapefile ptr, frame, style hash).
static std::unordered_map<uint64_t, AtlasFrameID> g_atlas_cache;

// Terrain cache: keyed by (terrain_hash << 32 | icon).
static std::unordered_map<uint64_t, AtlasFrameID> g_terrain_atlas_cache;

// GL textures for atlas pages
static GLuint* g_page_textures = nullptr;
static int     g_page_tex_count = 0;
static char    g_config_meg_path[1024] = {};

extern unsigned char const RemapBlue[256];
extern unsigned char const RemapOrange[256];
extern unsigned char const RemapYellow[256];
extern unsigned char const RemapRed[256];
extern unsigned char const RemapBlueGreen[256];
extern unsigned char const RemapGreen[256];
extern unsigned char const RemapNone[256];

static float classify_house_hue(const uint8_t* remap)
{
    if (!remap || remap == RemapNone) {
        return -1.0f;
    }
    if (remap == RemapYellow) {
        return HouseColors::GDI_GOLD.hue;
    }
    if (remap == RemapRed) {
        return HouseColors::NOD_RED.hue;
    }
    if (remap == RemapOrange) {
        return HouseColors::MP_ORANGE.hue;
    }
    if (remap == RemapBlueGreen) {
        return HouseColors::MP_TEAL.hue;
    }
    if (remap == RemapBlue) {
        return HouseColors::ALLIES_BLUE.hue;
    }
    if (remap == RemapGreen) {
        return 0.33f;
    }
    return -1.0f;
}

static uint64_t hash_style(const SpriteRenderStyle& style)
{
    uint64_t key = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(style.remap_table));
    uint32_t hue_bits = 0;
    static_assert(sizeof(hue_bits) == sizeof(style.house_hue), "float size mismatch");
    memcpy(&hue_bits, &style.house_hue, sizeof(hue_bits));
    key ^= static_cast<uint64_t>(hue_bits) << 8;
    return key;
}

static uint64_t make_cache_key(const void* shapefile, int frame, const SpriteRenderStyle& style)
{
    uint64_t key = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(shapefile));
    key ^= static_cast<uint64_t>(frame & 0xFFFF) << 32;
    key ^= hash_style(style);
    return key;
}

static const void* get_shadow_shapes()
{
    static const void* shadow_shapes = MixFileClass::Retrieve("SHADOW.SHP");
    return shadow_shapes;
}

static SpriteRenderStyle classify_style(const DrawCommand& draw_cmd);

static DrawCommand make_shape_draw_command(const ShapeCmd& cmd)
{
    DrawCommand draw_cmd = {};
    draw_cmd.type = CMD_SHAPE;
    draw_cmd.layer = (cmd.shapefile == get_shadow_shapes()) ? LAYER_SHADOW : LAYER_SPRITE;
    draw_cmd.shape = cmd;
    return draw_cmd;
}

static bool rects_overlap(float ax0, float ay0, float ax1, float ay1,
                          float bx0, float by0, float bx1, float by1)
{
    return ax0 < bx1 && ax1 > bx0 && ay0 < by1 && ay1 > by0;
}

static bool has_hd_frame_for_shape(const ShapeCmd& cmd)
{
    if (!g_hd_provider || !Render_Bridge_Get_HD_Graphics()) {
        return false;
    }

    SpriteFrame frame = {};
    const void* shape_id = cmd.entity_hash
                         ? reinterpret_cast<const void*>(static_cast<uintptr_t>(cmd.entity_hash))
                         : cmd.shapefile;
    return g_hd_provider->Get_Frame(shape_id, cmd.shapenum, frame);
}

static bool get_shape_frame(const ShapeCmd& cmd, SpriteFrame& frame_out)
{
    if (g_hd_provider && Render_Bridge_Get_HD_Graphics()) {
        const void* shape_id = cmd.entity_hash
                             ? reinterpret_cast<const void*>(static_cast<uintptr_t>(cmd.entity_hash))
                             : cmd.shapefile;
        if (g_hd_provider->Get_Frame(shape_id, cmd.shapenum, frame_out)) {
            return true;
        }
    }
    return g_provider.Get_Frame(cmd.shapefile, cmd.shapenum, frame_out);
}

static bool get_shape_rect(const ShapeCmd& cmd, float& x0, float& y0, float& x1, float& y1)
{
    SpriteFrame frame = {};
    if (!get_shape_frame(cmd, frame) || frame.width <= 0 || frame.height <= 0) {
        return false;
    }

    float game_x = static_cast<float>(cmd.x);
    float game_y = static_cast<float>(cmd.y);
    float native_scale = frame.native_scale > 0.0f ? frame.native_scale : 1.0f;
    float canvas_w = static_cast<float>(frame.canvas_width > 0 ? frame.canvas_width : frame.width);
    float canvas_h = static_cast<float>(frame.canvas_height > 0 ? frame.canvas_height : frame.height);

    if (cmd.flags & SHAPE_CENTER) {
        game_x -= (canvas_w / native_scale) * 0.5f;
        game_y -= (canvas_h / native_scale) * 0.5f;
    }

    game_x += static_cast<float>(frame.origin_x) / native_scale;
    game_y += static_cast<float>(frame.origin_y) / native_scale;

    x0 = game_x;
    y0 = game_y;
    x1 = game_x + static_cast<float>(frame.width) / native_scale;
    y1 = game_y + static_cast<float>(frame.height) / native_scale;
    return true;
}

static bool shape_overlaps_shroud(const ShapeCmd& cmd)
{
    float ax0 = 0.0f, ay0 = 0.0f, ax1 = 0.0f, ay1 = 0.0f;
    if (!get_shape_rect(cmd, ax0, ay0, ax1, ay1)) {
        return false;
    }

    for (int i = 0; i < g_draw_list.Command_Count(); i++) {
        const DrawCommand& other = g_draw_list.Get(i);
        if (other.layer != LAYER_SHADOW) {
            continue;
        }

        if (other.type == CMD_FILL_RECT) {
            const float bx0 = static_cast<float>(other.prim.x1);
            const float by0 = static_cast<float>(other.prim.y1);
            const float bx1 = static_cast<float>(other.prim.x2 + 1);
            const float by1 = static_cast<float>(other.prim.y2 + 1);
            if (rects_overlap(ax0, ay0, ax1, ay1, bx0, by0, bx1, by1)) {
                return true;
            }
            continue;
        }

        if (other.type == CMD_SHAPE) {
            float bx0 = 0.0f, by0 = 0.0f, bx1 = 0.0f, by1 = 0.0f;
            if (get_shape_rect(other.shape, bx0, by0, bx1, by1) &&
                rects_overlap(ax0, ay0, ax1, ay1, bx0, by0, bx1, by1)) {
                return true;
            }
        }
    }

    return false;
}

static SpriteRenderStyle classify_style(const DrawCommand& draw_cmd)
{
    SpriteRenderStyle style = {};
    const ShapeCmd& cmd = draw_cmd.shape;
    int effective_flags = cmd.flags;
    const uint8_t* remap = static_cast<const uint8_t*>(cmd.fadingdata);
    const uint8_t* ghost = static_cast<const uint8_t*>(cmd.ghostdata);

    // When an HD provider is active and this shape has a known entity identity,
    // skip the legacy flag restrictions — HD frames are full RGBA and don't need
    // ghost/fading remap tables.
    bool may_have_hd = false;
    if (g_hd_provider && Render_Bridge_Get_HD_Graphics() && cmd.entity_hash) {
        may_have_hd = true;
    }

    // The experimental GL sprite path is an overlay drawn after the native
    // tactical texture. Non-centered legacy sprites are generally unsafe there,
    // but theater-backed HD assets such as trees and bib smudges already carry
    // explicit crop/canvas metadata, so they can be replayed deterministically
    // as long as we do not fall back to legacy GL for them.
    if ((effective_flags & SHAPE_CENTER) == 0) {
        if (!may_have_hd) {
            style.supported = false;
            return style;
        }
        style.hd_only = true;
    }

    // Legacy mixed ghost+fading draws combine two remap tables in one blit.
    // That path covers unit bodies, their shadows, and tree lighting. The GL
    // approximation does not match it yet — but HD frames bypass this since
    // they're already full RGBA.
    if ((effective_flags & SHAPE_GHOST) && (effective_flags & SHAPE_FADING)) {
        if (!may_have_hd) {
            style.supported = false;
            return style;
        }
        // Allowed only because HD might have this frame. If the HD provider
        // doesn't, ensure_in_atlas must send it to CPU path, not legacy GL.
        style.hd_only = true;
    }

    if ((effective_flags & (SHAPE_FADING | SHAPE_PREDATOR)) == (SHAPE_FADING | SHAPE_PREDATOR)) {
        effective_flags &= ~(SHAPE_FADING | SHAPE_PREDATOR);
        effective_flags |= SHAPE_GHOST;
        ghost = Map.SpecialGhost;
        remap = nullptr;
    }

    if (effective_flags & SHAPE_PREDATOR) {
        style.supported = false;
        return style;
    }

    if ((effective_flags & SHAPE_FADING) && remap) {
        style.house_hue = classify_house_hue(remap);
        if (style.house_hue < 0.0f) {
            style.remap_table = remap;
        }
    }

    if (!(effective_flags & SHAPE_GHOST) || !ghost) {
        return style;
    }

    if (draw_cmd.layer == LAYER_SHADOW && cmd.shapefile == get_shadow_shapes()) {
        // Shroud-edge shadow tiles must render in the same late overlay pass as
        // HD units/trees, otherwise the native buffer bakes them underneath.
        style.kind = SpriteRenderKind::Shadow;
        style.alpha = 120;
        return style;
    }

    if (ghost == Map.UnitShadow) {
        if (!may_have_hd) {
            // UnitShadow is reused by several legacy draw sites (unit shadows,
            // tiberium/overlay sprites, theater scenery, and other tactical
            // silhouettes). Keep all of them on the CPU path until the bridge can
            // distinguish those cases without breaking ordering against shroud.
            style.supported = false;
            return style;
        }
        if (!cmd.fadingdata) {
            // Ghost-only UnitShadow draws are still used for real translucent
            // silhouettes (for example rotor shadows and some overlays). Keep
            // those on the CPU path until the bridge has a faithful HD match.
            style.supported = false;
            return style;
        }

        // Many normal body draws (units, buildings, trees) combine a fade/remap
        // table with UnitShadow in the legacy blitter. Treating that as a GL
        // shadow effect makes the HD sprite itself translucent. Preserve the
        // remap/fade classification above, ignore the legacy ghost table here,
        // and require a real HD frame for this path.
        style.hd_only = true;
        return style;
    }
    if (ghost == Map.SpecialGhost) {
        style.kind = SpriteRenderKind::SpecialGhost;
        style.alpha = 100;
        return style;
    }
    if (ghost == Map.TranslucentTable) {
        style.kind = SpriteRenderKind::TranslucentGhost;
        style.alpha = 110;
        return style;
    }
    if (ghost == Map.WhiteTranslucentTable) {
        style.kind = SpriteRenderKind::WhiteGhost;
        style.alpha = 80;
        return style;
    }
    if (ghost == Map.MouseTranslucentTable) {
        style.kind = SpriteRenderKind::MouseGhost;
        style.alpha = 110;
        return style;
    }

    // Unknown ghost tables are kept on the CPU path until they are classified
    // explicitly. Rendering an approximate but incorrect effect in GL is worse
    // than deferring to the legacy draw path.
    style.supported = false;
    return style;
}

bool GL_Sprites_Should_Skip_CPU(const ShapeCmd& cmd)
{
    SpriteRenderStyle style = classify_style(make_shape_draw_command(cmd));
    if (!style.supported) return false;
    if (cmd.shapefile == get_shadow_shapes()) {
        return true;
    }
    if (shape_overlaps_shroud(cmd)) {
        return false;
    }
    if (style.hd_only) {
        return has_hd_frame_for_shape(cmd);
    }
    // The atlas is built later in the same frame during GL_Present_Frame().
    // If the CPU replay waits for a pre-existing cache entry, newly encountered
    // animation frames briefly fall back to legacy pixels in the native buffer,
    // then get covered by GL on the next frame, which shows up as blinking.
    // Treat the GL sprite pass as the owner for any style it supports.
    return true;
}

// Snapshot of the palette used to build the atlas (768 bytes)
static uint8_t g_cached_pal_data[768] = {};
static bool    g_cached_pal_valid = false;

/// Check if palette content actually changed (full comparison).
static bool palette_changed(const uint8_t* pal)
{
    if (!g_cached_pal_valid || memcmp(g_cached_pal_data, pal, 768) != 0) {
        memcpy(g_cached_pal_data, pal, 768);
        g_cached_pal_valid = true;
        return true;
    }
    return false;
}

/// Invalidate atlas — called on palette change or scene transition.
static void invalidate_atlas()
{
    g_atlas_cache.clear();
    g_terrain_atlas_cache.clear();
    // Delete GL textures
    for (int i = 0; i < g_page_tex_count; i++) {
        if (g_page_textures[i]) glDeleteTextures(1, &g_page_textures[i]);
    }
    free(g_page_textures);
    g_page_textures = nullptr;
    g_page_tex_count = 0;
    g_batch.Clear_Page_Textures();

    // Reset atlas (re-init for new frames)
    g_atlas.~TextureAtlas();
    new (&g_atlas) TextureAtlas();
    g_atlas.Init(2048);
    g_atlas_ready = false;
}

void Render_Bridge_Invalidate_HD_Sprite_Atlas()
{
    invalidate_atlas();
}

/// Ensure a shape frame is in the atlas. Returns atlas frame ID.
static AtlasFrameID ensure_in_atlas(const ShapeCmd& cmd, const uint8_t* palette)
{
    SpriteRenderStyle style = classify_style(make_shape_draw_command(cmd));
    if (!style.supported) {
        return static_cast<AtlasFrameID>(-1);
    }

    uint64_t key = make_cache_key(cmd.shapefile, cmd.shapenum, style);
    auto it = g_atlas_cache.find(key);
    if (it != g_atlas_cache.end()) return it->second;

    // Decode sprite. Prefer HD RGBA frames when a provider is registered and
    // the shape identifier is an entity hash recognized by that provider.
    SpriteFrame frame = {};
    bool have_frame = false;
    if (g_hd_provider && Render_Bridge_Get_HD_Graphics()) {
        const void* shape_id = cmd.entity_hash
                             ? reinterpret_cast<const void*>(static_cast<uintptr_t>(cmd.entity_hash))
                             : cmd.shapefile;
        have_frame = g_hd_provider->Get_Frame(shape_id, cmd.shapenum, frame);
        // (frame obtained from HD provider)
    }
    if (!have_frame) {
        // Shape was only allowed through classify_style because HD might have it.
        // HD doesn't → send to CPU path, not legacy GL (which can't handle the
        // ghost/fading flags correctly).
        if (style.hd_only)
            return static_cast<AtlasFrameID>(-1);
        have_frame = g_provider.Get_Frame(cmd.shapefile, cmd.shapenum, frame);
    }
    if (!have_frame)
        return static_cast<AtlasFrameID>(-1);
    if (!frame.pixels || frame.width <= 0 || frame.height <= 0)
        return static_cast<AtlasFrameID>(-1);

    // Convert indexed pixels into RGBA atlas content. HD frames are already
    // RGBA; legacy frames are palette-converted here.
    int pixel_count = frame.width * frame.height;
    uint32_t* rgba = static_cast<uint32_t*>(malloc(pixel_count * 4));
    if (!rgba) return static_cast<AtlasFrameID>(-1);

    if (frame.pixel_format == SpritePixelFormat::RGBA_32BIT) {
        memcpy(rgba, frame.pixels, pixel_count * 4);
    } else {
        const uint8_t* src = static_cast<const uint8_t*>(frame.pixels);
        for (int y = 0; y < frame.height; y++) {
            const uint8_t* row = src + y * frame.pitch;
            for (int x = 0; x < frame.width; x++) {
                int i = y * frame.width + x;
                uint8_t idx = row[x];
                if (idx == 0) {
                    rgba[i] = 0;
                    continue;
                }

                if (style.remap_table) {
                    idx = style.remap_table[idx];
                }

                uint8_t r = palette[idx * 3 + 0] << 2;
                uint8_t g = palette[idx * 3 + 1] << 2;
                uint8_t b = palette[idx * 3 + 2] << 2;
                uint8_t a = 255;

                rgba[i] = (static_cast<uint32_t>(a) << 24) |
                          (static_cast<uint32_t>(b) << 16) |
                          (static_cast<uint32_t>(g) << 8) |
                          r;
            }
        }
    }

    AtlasFrameID id = g_atlas.Add_Frame(rgba, frame.width, frame.height,
                                        frame.origin_x, frame.origin_y,
                                        frame.canvas_width, frame.canvas_height,
                                        frame.native_scale);
    free(rgba);

    if (id != static_cast<AtlasFrameID>(-1)) {
        g_atlas_cache[key] = id;
    }
    return id;
}

/// Discover the directory containing remastered MEG files.
/// Tries CNC_REMASTERED_DATA env var first, then common Steam install paths.
static const char* find_remastered_data_dir()
{
    static char resolved[1024];

    // 1. Explicit env var
    const char* env = std::getenv("CNC_REMASTERED_DATA");
    if (env && env[0] != '\0') {
        std::snprintf(resolved, sizeof(resolved), "%s", env);
        return resolved;
    }

    // 2. Relative to working directory
    {
        char probe[1024];
        std::snprintf(probe, sizeof(probe), "data/TEXTURES_TD_SRGB.MEG");
        FILE* f = fopen(probe, "rb");
        if (f) { fclose(f); std::snprintf(resolved, sizeof(resolved), "data"); return resolved; }
        std::snprintf(probe, sizeof(probe), "Data/TEXTURES_TD_SRGB.MEG");
        f = fopen(probe, "rb");
        if (f) { fclose(f); std::snprintf(resolved, sizeof(resolved), "Data"); return resolved; }
    }

    // 3. Linux Steam install
    const char* home = std::getenv("HOME");
    if (home && home[0] != '\0') {
        std::snprintf(resolved, sizeof(resolved),
                      "%s/.local/share/Steam/steamapps/common/CnCRemastered/Data", home);
        char probe[1024];
        std::snprintf(probe, sizeof(probe), "%s/TEXTURES_TD_SRGB.MEG", resolved);
        FILE* f = fopen(probe, "rb");
        if (f) { fclose(f); return resolved; }
    }

    return nullptr;
}

static const char* initial_hd_sprite_theater()
{
    if (LastTheater > THEATER_NONE && LastTheater < THEATER_COUNT) {
        return Theaters[LastTheater].Name;
    }
    return "TEMPERATE";
}

/// Initialize GL sprite rendering.
void GL_Sprites_Init()
{
    g_atlas.Init(2048);
    static HDSpriteProvider   g_hd_provider_storage;
    static HDTerrainProvider  g_terrain_provider_storage;
    const char* data_dir = find_remastered_data_dir();
    if (!data_dir) {
        fprintf(stderr, "[HD] Remastered data not found — HD textures disabled\n");
    } else {
        fprintf(stderr, "[HD] Remastered data dir: %s\n", data_dir);
        char textures_meg[1024];
        char config_meg[1024];
        std::snprintf(textures_meg, sizeof(textures_meg), "%s/TEXTURES_TD_SRGB.MEG", data_dir);
        std::snprintf(config_meg, sizeof(config_meg), "%s/CONFIG.MEG", data_dir);
        std::snprintf(g_config_meg_path, sizeof(g_config_meg_path), "%s", config_meg);

        // --- Sprite provider (units + buildings; terrain XML is theater-specific) ---
        if (!g_hd_provider_storage.Open(textures_meg)) {
            fprintf(stderr, "[HD] Failed to open %s\n", textures_meg);
        } else {
            fprintf(stderr, "[HD] Opened %s\n", textures_meg);
            char terrain_xml[256];
            const char* theater_name = initial_hd_sprite_theater();
            std::snprintf(terrain_xml, sizeof(terrain_xml),
                          "DATA\\XML\\TILESETS\\TD_TERRAIN_%s.XML", theater_name);

            bool units   = g_hd_provider_storage.Load_Tileset_From_Meg(config_meg, "DATA\\XML\\TILESETS\\TD_UNITS.XML");
            bool structs = g_hd_provider_storage.Load_Tileset_From_Meg(config_meg, "DATA\\XML\\TILESETS\\TD_STRUCTURES.XML");
            bool terrain = g_hd_provider_storage.Load_Tileset_From_Meg(config_meg, terrain_xml);
            fprintf(stderr, "[HD] Tilesets: units=%s structures=%s terrain(%s)=%s\n",
                    units ? "ok" : "FAIL",
                    structs ? "ok" : "FAIL",
                    theater_name,
                    terrain ? "ok" : "FAIL");
            if (units || structs || terrain) {
                Render_Bridge_Register_HD_Sprite_Provider(&g_hd_provider_storage);
                fprintf(stderr, "[HD] Sprite provider registered — HD textures available\n");
            } else {
                fprintf(stderr, "[HD] No tilesets loaded — HD textures disabled\n");
            }
        }

        // --- Terrain tile provider (ground terrain stamps) ---
        if (g_terrain_provider_storage.Open(textures_meg)) {
            g_terrain_provider = &g_terrain_provider_storage;
            fprintf(stderr, "[HD] Terrain provider ready\n");
        } else {
            fprintf(stderr, "[HD] Failed to open terrain provider from %s\n", textures_meg);
        }
    }
    if (!Options.HasHDGraphicsSetting) {
        Render_Bridge_Set_HD_Graphics(g_hd_provider != nullptr);
    }
}

/// Update the terrain provider's active theater and invalidate the terrain atlas.
void GL_Sprites_Set_Theater(const char* theater_name)
{
    if (!g_terrain_provider || !theater_name || theater_name[0] == '\0') return;
    g_terrain_provider->Set_Theater(theater_name);

    if (g_hd_provider && g_config_meg_path[0] != '\0') {
        char terrain_xml[256];
        std::snprintf(terrain_xml, sizeof(terrain_xml),
                      "DATA\\XML\\TILESETS\\TD_TERRAIN_%s.XML", theater_name);
        bool terrain_ok = g_hd_provider->Load_Tileset_From_Meg(g_config_meg_path, terrain_xml);
        fprintf(stderr, "[HD] Terrain sprite tileset %s: %s\n",
                terrain_xml, terrain_ok ? "ok" : "FAIL");
    }

    // Invalidate so terrain tiles for the new theater are re-fetched next frame.
    g_terrain_atlas_cache.clear();
    // Full atlas invalidation so the texture upload reflects new terrain tiles.
    invalidate_atlas();
    fprintf(stderr, "[HD] Theater set to %s — terrain atlas invalidated\n", theater_name);
}

/// Build atlas from current draw list shapes.
/// Only rebuilds when palette changes or new shapes appear.
int GL_Sprites_Build_Atlas(const uint8_t* vga_palette)
{
    g_last_atlas_new_shapes = 0;
    bool pal_changed = palette_changed(vga_palette);
    if (pal_changed) {
        invalidate_atlas();
    }

    // Check if any new (uncached) shapes or terrain tiles exist in this frame's draw list
    bool has_new = false;
    if (g_atlas_ready) {
        for (int i = 0; i < g_draw_list.Command_Count() && !has_new; i++) {
            const DrawCommand& cmd = g_draw_list.Get(i);
            if (cmd.type == CMD_STAMP) {
                if (cmd.stamp.terrain_hash && g_terrain_provider && Render_Bridge_Get_HD_Graphics()) {
                    uint64_t tkey = ((uint64_t)cmd.stamp.terrain_hash << 32) | (uint32_t)cmd.stamp.icon;
                    if (g_terrain_atlas_cache.find(tkey) == g_terrain_atlas_cache.end())
                        has_new = true;
                }
            } else if (cmd.type == CMD_SHAPE) {
                SpriteRenderStyle style = classify_style(cmd);
                if (!style.supported) continue;
                uint64_t key = make_cache_key(cmd.shape.shapefile, cmd.shape.shapenum, style);
                if (g_atlas_cache.find(key) == g_atlas_cache.end())
                    has_new = true;
            }
        }
        if (!has_new) return static_cast<int>(g_atlas_cache.size() + g_terrain_atlas_cache.size());
        // New shapes: must rebuild (atlas was finalized)
        invalidate_atlas();
    }

    int new_shapes = 0;

    // Pack HD terrain tiles (CMD_STAMP with terrain_hash) into the atlas.
    if (g_terrain_provider && Render_Bridge_Get_HD_Graphics()) {
        for (int i = 0; i < g_draw_list.Command_Count(); i++) {
            const DrawCommand& cmd = g_draw_list.Get(i);
            if (cmd.type != CMD_STAMP) continue;
            if (!cmd.stamp.terrain_hash) continue;

            uint64_t tkey = ((uint64_t)cmd.stamp.terrain_hash << 32) | (uint32_t)cmd.stamp.icon;
            if (g_terrain_atlas_cache.find(tkey) != g_terrain_atlas_cache.end()) continue;

            SpriteFrame frame = {};
            if (!g_terrain_provider->Get_Tile_By_Hash(cmd.stamp.terrain_hash, cmd.stamp.icon, frame))
                continue;
            if (!frame.pixels || frame.width <= 0 || frame.height <= 0) continue;

            int pixel_count = frame.width * frame.height;
            uint32_t* rgba = static_cast<uint32_t*>(malloc(pixel_count * 4));
            if (!rgba) continue;
            memcpy(rgba, frame.pixels, pixel_count * 4);

            AtlasFrameID id = g_atlas.Add_Frame(rgba, frame.width, frame.height,
                                                 frame.origin_x, frame.origin_y,
                                                 frame.canvas_width  ? frame.canvas_width  : frame.width,
                                                 frame.canvas_height ? frame.canvas_height : frame.height,
                                                 frame.native_scale  ? frame.native_scale  : 1.0f);
            free(rgba);
            if (id != static_cast<AtlasFrameID>(-1)) {
                g_terrain_atlas_cache[tkey] = id;
                new_shapes++;
            }
        }
    }

    // Pack sprite frames (CMD_SHAPE).
    for (int i = 0; i < g_draw_list.Command_Count(); i++) {
        const DrawCommand& cmd = g_draw_list.Get(i);
        if (cmd.type != CMD_SHAPE) continue;

        SpriteRenderStyle style = classify_style(cmd);
        if (!style.supported) continue;
        uint64_t key = make_cache_key(cmd.shape.shapefile, cmd.shape.shapenum, style);
        if (g_atlas_cache.find(key) != g_atlas_cache.end()) continue;

        AtlasFrameID id = ensure_in_atlas(cmd.shape, vga_palette);
        if (id != static_cast<AtlasFrameID>(-1)) new_shapes++;
    }

    if (new_shapes > 0 || (g_atlas_cache.size() + g_terrain_atlas_cache.size() > 0 && !g_atlas_ready)) {
        g_atlas.Finalize();
        g_atlas_ready = true;
        g_last_atlas_new_shapes = new_shapes;

        // Upload atlas pages to GL
        for (int i = 0; i < g_page_tex_count; i++)
            if (g_page_textures[i]) glDeleteTextures(1, &g_page_textures[i]);
        free(g_page_textures);
        g_batch.Clear_Page_Textures();

        int pages = g_atlas.Page_Count();
        g_page_textures = static_cast<GLuint*>(calloc(pages, sizeof(GLuint)));
        g_page_tex_count = pages;

        for (int p = 0; p < pages; p++) {
            glGenTextures(1, &g_page_textures[p]);
            glBindTexture(GL_TEXTURE_2D, g_page_textures[p]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                         g_atlas.Page_Size(), g_atlas.Page_Size(), 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, g_atlas.Page_Pixels(p));
            g_batch.Set_Page_Texture(static_cast<uint16_t>(p), g_page_textures[p]);
        }
    }

    return static_cast<int>(g_atlas_cache.size() + g_terrain_atlas_cache.size());
}

/// Return true when the GL atlas contains an HD frame for the given terrain stamp.
/// Used by zoom_tactical.cpp to skip the CPU stamp draw when GL will handle it.
bool GL_Sprites_Has_Terrain_Frame(uint32_t terrain_hash, int icon)
{
    if (!g_terrain_provider || !Render_Bridge_Get_HD_Graphics()) return false;
    uint64_t key = ((uint64_t)terrain_hash << 32) | (uint32_t)icon;
    return g_terrain_atlas_cache.find(key) != g_terrain_atlas_cache.end();
}

void Render_Bridge_Set_Theater(const char* theater_name)
{
    GL_Sprites_Set_Theater(theater_name);
}

/// Get atlas stats for debug display.
int GL_Sprites_Atlas_Frame_Count() { return static_cast<int>(g_atlas_cache.size()); }
int GL_Sprites_Atlas_Page_Count()  { return g_atlas.Page_Count(); }
int GL_Sprites_Last_Atlas_New_Count() { return g_last_atlas_new_shapes; }
int GL_Sprites_Last_Sprite_Count() { return g_last_rendered_sprites; }
int GL_Sprites_Last_Draw_Calls()   { return g_last_draw_calls; }
int GL_Sprites_Last_Fallback_Count() { return g_last_fallback_sprites; }
int GL_Sprites_Last_Terrain_Miss_Count() { return g_last_terrain_miss_count; }
int GL_Sprites_Terrain_Atlas_Count() { return static_cast<int>(g_terrain_atlas_cache.size()); }

/// Find the terrain stamp under a given native-buffer position.
/// Returns the terrain_hash (0 if none), and fills icon_out.
uint32_t GL_Sprites_Hit_Test_Terrain(float native_x, float native_y, int& icon_out,
                                      bool& has_hd_out)
{
    icon_out = 0;
    has_hd_out = false;
    // Walk draw list in reverse so the topmost (last drawn) stamp wins.
    for (int i = g_draw_list.Command_Count() - 1; i >= 0; i--) {
        const DrawCommand& cmd = g_draw_list.Get(i);
        if (cmd.type != CMD_STAMP) continue;
        float x0 = static_cast<float>(cmd.stamp.x);
        float y0 = static_cast<float>(cmd.stamp.y);
        // Legacy terrain tiles are ICON_PIXEL_W x ICON_PIXEL_W (24x24).
        float x1 = x0 + 24.0f;
        float y1 = y0 + 24.0f;
        if (native_x >= x0 && native_x < x1 && native_y >= y0 && native_y < y1) {
            icon_out = cmd.stamp.icon;
            if (cmd.stamp.terrain_hash) {
                uint64_t tkey = ((uint64_t)cmd.stamp.terrain_hash << 32) | (uint32_t)cmd.stamp.icon;
                has_hd_out = g_terrain_atlas_cache.find(tkey) != g_terrain_atlas_cache.end();
            }
            return cmd.stamp.terrain_hash;
        }
    }
    return 0;
}

void Render_Bridge_Register_HD_Sprite_Provider(void* provider)
{
    g_hd_provider = static_cast<HDSpriteProvider*>(provider);
}

void Render_Bridge_Register_Shape_Identity(const void* shapefile, uint32_t entity_hash)
{
    if (!shapefile || entity_hash == 0) return;
    g_shape_identity[shapefile] = entity_hash;
}

uint32_t Render_Bridge_Get_Shape_Identity(const void* shapefile)
{
    if (!shapefile) return 0;
    auto it = g_shape_identity.find(shapefile);
    return it == g_shape_identity.end() ? 0u : it->second;
}

/// Render HD terrain tiles (CMD_STAMP) as a separate GL layer.
/// Called BEFORE the native tactical texture so terrain appears underneath
/// shroud, objects, and sprites. The native buffer has terrain-key-index holes
/// that become transparent in the tactical shader, letting this layer show through.
int GL_Sprites_Render_Terrain(int win_w, int win_h,
                               int tac_screen_x, int tac_screen_y,
                               int tac_screen_w, int tac_screen_h,
                               float scale, float vp_x, float vp_y)
{
    if (!g_atlas_ready || g_page_tex_count == 0) return 0;
    g_batch.Begin();
    g_last_terrain_miss_count = 0;

    for (int i = 0; i < g_draw_list.Command_Count(); i++) {
        const DrawCommand& cmd = g_draw_list.Get(i);
        if (cmd.type != CMD_STAMP) continue;
        if (!cmd.stamp.terrain_hash) continue;

        uint64_t tkey = ((uint64_t)cmd.stamp.terrain_hash << 32) | (uint32_t)cmd.stamp.icon;
        auto it = g_terrain_atlas_cache.find(tkey);
        if (it == g_terrain_atlas_cache.end()) {
            g_last_terrain_miss_count++;
            continue;
        }

        AtlasRegion region;
        if (!g_atlas.Get_Region(it->second, region)) continue;

        TerrainReplayQuad quad = Render_Bridge_Compute_HD_Terrain_Quad(
            cmd.stamp, region,
            tac_screen_x, tac_screen_y, tac_screen_w, tac_screen_h,
            scale, vp_x, vp_y);

        if (!Render_Bridge_Debug_No_GL_Cull() && !quad.visible) continue;

        SpriteBatchEntry entry = {};
        entry.region = region;
        entry.dst_x = quad.screen_x;
        entry.dst_y = quad.screen_y;
        entry.scale_x = scale / (region.native_scale > 0.0f ? region.native_scale : 1.0f);
        entry.scale_y = scale / (region.native_scale > 0.0f ? region.native_scale : 1.0f);
        entry.house_hue = -1.0f;
        entry.flags = 0;
        entry.fade = 0;
        g_batch.Add(entry);
    }

    g_batch.Flush();
    return g_batch.Sprite_Count();
}

/// Render all CMD_SHAPE from draw list as GL textured quads.
/// win_w/h: native window pixels. tac_*: tactical area in screen pixels.
/// scale: zoom (screen pixels per game pixel). vp_x/y: viewport offset.
int GL_Sprites_Render(int win_w, int win_h,
                       int tac_screen_x, int tac_screen_y,
                       int tac_screen_w, int tac_screen_h,
                       int tac_game_x, int tac_game_y,
                       int tac_game_w, int tac_game_h,
                       float scale, float vp_x, float vp_y)
{
    (void)tac_game_x;
    (void)tac_game_y;
    (void)tac_game_w;
    (void)tac_game_h;
    if (!g_atlas_ready || g_page_tex_count == 0) {
        g_last_rendered_sprites = 0;
        g_last_draw_calls = 0;
        g_last_fallback_sprites = 0;
        return 0;
    }
    g_batch.Begin();
    g_last_fallback_sprites = 0;

    for (int i = 0; i < g_draw_list.Command_Count(); i++) {
        const DrawCommand& cmd = g_draw_list.Get(i);

        // Terrain stamps are now rendered in GL_Sprites_Render_Terrain (pre-pass).
        if (cmd.type == CMD_STAMP) continue;
        if (cmd.type != CMD_SHAPE) continue;
        if (cmd.layer == LAYER_SHADOW) continue;

        SpriteRenderStyle style = classify_style(cmd);
        if (!style.supported) {
            g_last_fallback_sprites++;
            continue;
        }
        if (shape_overlaps_shroud(cmd.shape)) {
            g_last_fallback_sprites++;
            continue;
        }

        uint64_t key = make_cache_key(cmd.shape.shapefile, cmd.shape.shapenum, style);
        auto it = g_atlas_cache.find(key);
        if (it == g_atlas_cache.end()) {
            g_last_fallback_sprites++;
            continue;
        }

        AtlasRegion region;
        if (!g_atlas.Get_Region(it->second, region)) continue;

        // Sprite position: game-buffer relative to tactical window
        float game_x = static_cast<float>(cmd.shape.x);
        float game_y = static_cast<float>(cmd.shape.y);
        float sprite_w = static_cast<float>(region.w);
        float sprite_h = static_cast<float>(region.h);
        float canvas_w = static_cast<float>(region.canvas_w ? region.canvas_w : region.w);
        float canvas_h = static_cast<float>(region.canvas_h ? region.canvas_h : region.h);

        // native_scale: HD sprites are Nx native resolution (e.g. 4x).
        // Canvas/origin values are in HD pixel space and must be divided
        // by native_scale to convert back to legacy game-buffer coords.
        float native_scale = region.native_scale > 0.0f ? region.native_scale : 1.0f;

        // Apply SHAPE_CENTER (legacy engine centers using legacy dimensions)
        if (cmd.shape.flags & SHAPE_CENTER) {
            game_x -= (canvas_w / native_scale) * 0.5f;
            game_y -= (canvas_h / native_scale) * 0.5f;
        }

        game_x += static_cast<float>(region.origin_x) / native_scale;
        game_y += static_cast<float>(region.origin_y) / native_scale;

        // Transform game position → screen position
        float screen_x = tac_screen_x + (game_x - vp_x) * scale;
        float screen_y = tac_screen_y + (game_y - vp_y) * scale;
        float screen_w = sprite_w * scale / native_scale;
        float screen_h = sprite_h * scale / native_scale;

        // Clip: skip if entirely outside tactical screen area
        if (!Render_Bridge_Debug_No_GL_Cull()) {
            if (screen_x + screen_w < tac_screen_x || screen_x > tac_screen_x + tac_screen_w) continue;
            if (screen_y + screen_h < tac_screen_y || screen_y > tac_screen_y + tac_screen_h) continue;
        }
        SpriteBatchEntry entry = {};
        entry.region = region;
        entry.dst_x = screen_x;
        entry.dst_y = screen_y;
        entry.scale_x = scale / native_scale;
        entry.scale_y = scale / native_scale;
        entry.house_hue = style.house_hue;
        entry.flags = 0;
        if (cmd.shape.flags & SHAPE_HORZ_REV) entry.flags |= 0x01;
        if (cmd.shape.flags & SHAPE_VERT_REV) entry.flags |= 0x02;
        if (style.kind == SpriteRenderKind::Ghost) entry.flags |= 0x04;
        if (style.kind == SpriteRenderKind::Shadow) entry.flags |= 0x08;
        if (style.kind == SpriteRenderKind::TranslucentGhost) entry.flags |= 0x80;
        if (style.kind == SpriteRenderKind::SpecialGhost) entry.flags |= 0x20;
        if (style.kind == SpriteRenderKind::WhiteGhost) entry.flags |= 0x10;
        if (style.kind == SpriteRenderKind::MouseGhost) entry.flags |= 0x40;
        entry.fade = (style.kind == SpriteRenderKind::Normal) ? 0 : style.alpha;
        g_batch.Add(entry);
    }

    g_batch.Flush();
    g_last_rendered_sprites = g_batch.Sprite_Count();
    g_last_draw_calls = g_batch.Draw_Call_Count();
    return g_last_rendered_sprites;
}

/// Render SHADOW.SHP shadow tiles as the authoritative shroud mask pass.
int GL_Sprites_Render_Shadow(int win_w, int win_h,
                             int tac_screen_x, int tac_screen_y,
                             int tac_screen_w, int tac_screen_h,
                             int tac_game_x, int tac_game_y,
                             int tac_game_w, int tac_game_h,
                             float scale, float vp_x, float vp_y)
{
    (void)win_w;
    (void)win_h;
    (void)tac_game_x;
    (void)tac_game_y;
    (void)tac_game_w;
    (void)tac_game_h;
    if (!g_atlas_ready || g_page_tex_count == 0) {
        return 0;
    }
    g_batch.Begin();

    const void* shadow_shapes = get_shadow_shapes();
    for (int i = 0; i < g_draw_list.Command_Count(); i++) {
        const DrawCommand& cmd = g_draw_list.Get(i);
        if (cmd.type != CMD_SHAPE || cmd.shape.shapefile != shadow_shapes) {
            continue;
        }

        AtlasFrameID id = ensure_in_atlas(cmd.shape, g_cached_pal_data);
        if (id == static_cast<AtlasFrameID>(-1)) {
            continue;
        }
        AtlasRegion region;
        if (!g_atlas.Get_Region(id, region)) {
            continue;
        }

        float game_x = static_cast<float>(cmd.shape.x);
        float game_y = static_cast<float>(cmd.shape.y);
        float sprite_w = static_cast<float>(region.w);
        float sprite_h = static_cast<float>(region.h);
        float canvas_w = static_cast<float>(region.canvas_w ? region.canvas_w : region.w);
        float canvas_h = static_cast<float>(region.canvas_h ? region.canvas_h : region.h);
        float native_scale = region.native_scale > 0.0f ? region.native_scale : 1.0f;

        if (cmd.shape.flags & SHAPE_CENTER) {
            game_x -= (canvas_w / native_scale) * 0.5f;
            game_y -= (canvas_h / native_scale) * 0.5f;
        }

        game_x += static_cast<float>(region.origin_x) / native_scale;
        game_y += static_cast<float>(region.origin_y) / native_scale;

        float screen_x = tac_screen_x + (game_x - vp_x) * scale;
        float screen_y = tac_screen_y + (game_y - vp_y) * scale;
        float screen_w = sprite_w * scale / native_scale;
        float screen_h = sprite_h * scale / native_scale;

        if (!Render_Bridge_Debug_No_GL_Cull()) {
            if (screen_x + screen_w < tac_screen_x || screen_x > tac_screen_x + tac_screen_w) continue;
            if (screen_y + screen_h < tac_screen_y || screen_y > tac_screen_y + tac_screen_h) continue;
        }

        SpriteBatchEntry entry = {};
        entry.region = region;
        entry.dst_x = screen_x;
        entry.dst_y = screen_y;
        entry.scale_x = scale / native_scale;
        entry.scale_y = scale / native_scale;
        entry.house_hue = -1.0f;
        entry.flags = 0x08;
        if (cmd.shape.flags & SHAPE_HORZ_REV) entry.flags |= 0x01;
        if (cmd.shape.flags & SHAPE_VERT_REV) entry.flags |= 0x02;
        entry.fade = 120;
        g_batch.Add(entry);
    }

    g_batch.Flush();
    return g_batch.Sprite_Count();
}
