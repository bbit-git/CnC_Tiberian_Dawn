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
#include "texture_atlas.h"
#include "gl/gl_house_color.h"
#include "gl/gl_sprite_batch.h"
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
};

static LegacySpriteProvider g_provider;
static HDSpriteProvider*    g_hd_provider = nullptr;
static std::unordered_map<const void*, uint32_t> g_shape_identity;
static TextureAtlas         g_atlas;
static GLSpriteBatch        g_batch;
static bool                 g_atlas_ready = false;
static int                  g_last_rendered_sprites = 0;
static int                  g_last_draw_calls = 0;
static int                  g_last_fallback_sprites = 0;
static int                  g_last_atlas_new_shapes = 0;

// Cache: keyed by shapefile pointer, frame, and style hash.
static std::unordered_map<uint64_t, AtlasFrameID> g_atlas_cache;

// GL textures for atlas pages
static GLuint* g_page_textures = nullptr;
static int     g_page_tex_count = 0;

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

static SpriteRenderStyle classify_style(const ShapeCmd& cmd)
{
    SpriteRenderStyle style = {};
    int effective_flags = cmd.flags;
    const uint8_t* remap = static_cast<const uint8_t*>(cmd.fadingdata);
    const uint8_t* ghost = static_cast<const uint8_t*>(cmd.ghostdata);

    // The experimental GL sprite path is an overlay drawn after the native
    // tactical texture. Only centered object sprites preserve ordering there.
    // Ground-attached cell art such as bib patches, scenery, and shroud-related
    // shapes must stay in the native replay path for correct layering.
    if ((effective_flags & SHAPE_CENTER) == 0) {
        style.supported = false;
        return style;
    }

    // Legacy mixed ghost+fading draws combine two remap tables in one blit.
    // That path covers unit bodies, their shadows, and tree lighting. The GL
    // approximation does not match it yet, so keep those draws on the CPU path.
    if ((effective_flags & SHAPE_GHOST) && (effective_flags & SHAPE_FADING)) {
        style.supported = false;
        return style;
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

    if (ghost == Map.UnitShadow) {
        // UnitShadow is reused by several legacy draw sites (unit shadows,
        // tiberium/overlay sprites, theater scenery, and other tactical
        // silhouettes). Keep all of them on the CPU path until the bridge can
        // distinguish those cases without breaking ordering against shroud.
        style.supported = false;
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
    return classify_style(cmd).supported;
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
    SpriteRenderStyle style = classify_style(cmd);
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
    }
    if (!have_frame) {
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

/// Initialize GL sprite rendering.
void GL_Sprites_Init()
{
    g_atlas.Init(2048);
    static HDSpriteProvider g_hd_provider_storage;
    const char* data_dir = std::getenv("CNC_REMASTERED_DATA");
    if (data_dir && data_dir[0] != '\0') {
        char textures_meg[1024];
        char config_meg[1024];
        std::snprintf(textures_meg, sizeof(textures_meg), "%s/TEXTURES_TD_SRGB.MEG", data_dir);
        std::snprintf(config_meg, sizeof(config_meg), "%s/CONFIG.MEG", data_dir);
        if (g_hd_provider_storage.Open(textures_meg) &&
            g_hd_provider_storage.Load_Tileset_From_Meg(config_meg, "DATA\\XML\\TILESETS\\TD_UNITS.XML")) {
            Render_Bridge_Register_HD_Sprite_Provider(&g_hd_provider_storage);
        }
    }
    if (!Options.HasHDGraphicsSetting) {
        Render_Bridge_Set_HD_Graphics(g_hd_provider != nullptr);
    }
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

    // Check if any new (uncached) shapes exist in this frame's draw list
    bool has_new = false;
    if (g_atlas_ready) {
        for (int i = 0; i < g_draw_list.Command_Count(); i++) {
            const DrawCommand& cmd = g_draw_list.Get(i);
            if (cmd.type != CMD_SHAPE) continue;
            SpriteRenderStyle style = classify_style(cmd.shape);
            if (!style.supported) continue;
            uint64_t key = make_cache_key(cmd.shape.shapefile, cmd.shape.shapenum, style);
            if (g_atlas_cache.find(key) == g_atlas_cache.end()) {
                has_new = true;
                break;
            }
        }
        if (!has_new) return static_cast<int>(g_atlas_cache.size()); // all cached
        // New shapes: must rebuild (atlas was finalized)
        invalidate_atlas();
    }

    int new_shapes = 0;
    for (int i = 0; i < g_draw_list.Command_Count(); i++) {
        const DrawCommand& cmd = g_draw_list.Get(i);
        if (cmd.type != CMD_SHAPE) continue;

        SpriteRenderStyle style = classify_style(cmd.shape);
        if (!style.supported) continue;
        uint64_t key = make_cache_key(cmd.shape.shapefile, cmd.shape.shapenum, style);
        if (g_atlas_cache.find(key) != g_atlas_cache.end()) continue;

        AtlasFrameID id = ensure_in_atlas(cmd.shape, vga_palette);
        if (id != static_cast<AtlasFrameID>(-1)) new_shapes++;
    }

    if (new_shapes > 0 || (g_atlas_cache.size() > 0 && !g_atlas_ready)) {
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

    return static_cast<int>(g_atlas_cache.size());
}

/// Get atlas stats for debug display.
int GL_Sprites_Atlas_Frame_Count() { return static_cast<int>(g_atlas_cache.size()); }
int GL_Sprites_Atlas_Page_Count()  { return g_atlas.Page_Count(); }
int GL_Sprites_Last_Atlas_New_Count() { return g_last_atlas_new_shapes; }
int GL_Sprites_Last_Sprite_Count() { return g_last_rendered_sprites; }
int GL_Sprites_Last_Draw_Calls()   { return g_last_draw_calls; }
int GL_Sprites_Last_Fallback_Count() { return g_last_fallback_sprites; }

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
        if (cmd.type != CMD_SHAPE) continue;

        SpriteRenderStyle style = classify_style(cmd.shape);
        if (!style.supported) {
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

        // Apply SHAPE_CENTER
        if (cmd.shape.flags & SHAPE_CENTER) {
            game_x -= canvas_w * 0.5f;
            game_y -= canvas_h * 0.5f;
        }

        game_x += static_cast<float>(region.origin_x);
        game_y += static_cast<float>(region.origin_y);

        // Transform game position → screen position
        float screen_x = tac_screen_x + (game_x - vp_x) * scale;
        float screen_y = tac_screen_y + (game_y - vp_y) * scale;
        float native_scale = region.native_scale > 0.0f ? region.native_scale : 1.0f;
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
