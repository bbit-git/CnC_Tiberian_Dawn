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
#include "texture_atlas.h"
#include "function.h"
#include "dbg.h"

// td_platform.h defines min/max macros that conflict with GL headers
#undef min
#undef max
#include <GLES2/gl2.h>
#include <unordered_map>
#include <cstring>

static LegacySpriteProvider g_provider;
static TextureAtlas         g_atlas;
static bool                 g_atlas_ready = false;

// Cache: (shapefile_ptr << 16 | frame) → atlas frame ID
static std::unordered_map<uint64_t, AtlasFrameID> g_atlas_cache;

// Palette state for atlas invalidation
static const uint8_t* g_cached_pal = nullptr;
static uint32_t       g_cached_pal_hash = 0;

// GL textures for atlas pages
static GLuint* g_page_textures = nullptr;
static int     g_page_tex_count = 0;

static uint64_t make_cache_key(const void* shapefile, int frame)
{
    return (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(shapefile)) << 16) |
           (frame & 0xFFFF);
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

    // Reset atlas (re-init for new frames)
    g_atlas.~TextureAtlas();
    new (&g_atlas) TextureAtlas();
    g_atlas.Init(2048);
    g_atlas_ready = false;
}

/// Ensure a shape frame is in the atlas. Returns atlas frame ID.
static AtlasFrameID ensure_in_atlas(const void* shapefile, int shapenum,
                                      const uint8_t* palette)
{
    uint64_t key = make_cache_key(shapefile, shapenum);
    auto it = g_atlas_cache.find(key);
    if (it != g_atlas_cache.end()) return it->second;

    // Decode sprite
    SpriteFrame frame;
    if (!g_provider.Get_Frame(shapefile, shapenum, frame))
        return static_cast<AtlasFrameID>(-1);
    if (!frame.pixels || frame.width <= 0 || frame.height <= 0)
        return static_cast<AtlasFrameID>(-1);

    // Convert 8-bit indexed → RGBA using palette
    int pixel_count = frame.width * frame.height;
    uint32_t* rgba = static_cast<uint32_t*>(malloc(pixel_count * 4));
    if (!rgba) return static_cast<AtlasFrameID>(-1);

    const uint8_t* src = static_cast<const uint8_t*>(frame.pixels);
    for (int i = 0; i < pixel_count; i++) {
        uint8_t idx = src[i];
        if (idx == 0) {
            rgba[i] = 0; // transparent
        } else {
            uint8_t r = palette[idx * 3 + 0] << 2;
            uint8_t g = palette[idx * 3 + 1] << 2;
            uint8_t b = palette[idx * 3 + 2] << 2;
            rgba[i] = (0xFFu << 24) | (b << 16) | (g << 8) | r;
        }
    }

    AtlasFrameID id = g_atlas.Add_Frame(rgba, frame.width, frame.height);
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
}

/// Build atlas from current draw list shapes.
/// Only rebuilds when palette changes or new shapes appear.
int GL_Sprites_Build_Atlas(const uint8_t* vga_palette)
{
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
            uint64_t key = make_cache_key(cmd.shape.shapefile, cmd.shape.shapenum);
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

        uint64_t key = make_cache_key(cmd.shape.shapefile, cmd.shape.shapenum);
        if (g_atlas_cache.find(key) != g_atlas_cache.end()) continue;

        AtlasFrameID id = ensure_in_atlas(cmd.shape.shapefile,
                                           cmd.shape.shapenum, vga_palette);
        if (id != static_cast<AtlasFrameID>(-1)) new_shapes++;
    }

    if (new_shapes > 0 || (g_atlas_cache.size() > 0 && !g_atlas_ready)) {
        g_atlas.Finalize();
        g_atlas_ready = true;

        // Upload atlas pages to GL
        for (int i = 0; i < g_page_tex_count; i++)
            if (g_page_textures[i]) glDeleteTextures(1, &g_page_textures[i]);
        free(g_page_textures);

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
        }

        DBG("gl_sprites: atlas built — %d frames (+%d new), %d pages",
            static_cast<int>(g_atlas_cache.size()), new_shapes, pages);
    }

    return static_cast<int>(g_atlas_cache.size());
}

/// Get atlas stats for debug display.
int GL_Sprites_Atlas_Frame_Count() { return static_cast<int>(g_atlas_cache.size()); }
int GL_Sprites_Atlas_Page_Count()  { return g_atlas.Page_Count(); }

// ---- RGBA sprite shader for atlas quad rendering ----

static GLuint g_sprite_prog = 0;
static GLint  g_sp_u_texture = -1;
static bool   g_sprite_shader_ready = false;

static const char* sprite_vert = R"(
    attribute vec2 a_pos;
    attribute vec2 a_uv;
    varying vec2 v_uv;
    void main() {
        gl_Position = vec4(a_pos, 0.0, 1.0);
        v_uv = a_uv;
    }
)";

static const char* sprite_frag = R"(
    precision mediump float;
    varying vec2 v_uv;
    uniform sampler2D u_texture;
    void main() {
        vec4 c = texture2D(u_texture, v_uv);
        if (c.a < 0.01) discard;
        gl_FragColor = c;
    }
)";

static bool init_sprite_shader()
{
    if (g_sprite_shader_ready) return true;

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &sprite_vert, nullptr);
    glCompileShader(vs);
    GLint ok = 0;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
    if (!ok) { glDeleteShader(vs); return false; }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &sprite_frag, nullptr);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
    if (!ok) { glDeleteShader(vs); glDeleteShader(fs); return false; }

    g_sprite_prog = glCreateProgram();
    glAttachShader(g_sprite_prog, vs);
    glAttachShader(g_sprite_prog, fs);
    glBindAttribLocation(g_sprite_prog, 0, "a_pos");
    glBindAttribLocation(g_sprite_prog, 1, "a_uv");
    glLinkProgram(g_sprite_prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    glGetProgramiv(g_sprite_prog, GL_LINK_STATUS, &ok);
    if (!ok) return false;

    g_sp_u_texture = glGetUniformLocation(g_sprite_prog, "u_texture");
    g_sprite_shader_ready = true;
    DBG("gl_sprites: shader compiled");
    return true;
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
    if (!g_atlas_ready || g_page_tex_count == 0) return 0;
    if (!init_sprite_shader()) return 0;

    glUseProgram(g_sprite_prog);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glActiveTexture(GL_TEXTURE0);
    glUniform1i(g_sp_u_texture, 0);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    int rendered = 0;
    GLuint current_page_tex = 0;

    for (int i = 0; i < g_draw_list.Command_Count(); i++) {
        const DrawCommand& cmd = g_draw_list.Get(i);
        if (cmd.type != CMD_SHAPE) continue;

        uint64_t key = make_cache_key(cmd.shape.shapefile, cmd.shape.shapenum);
        auto it = g_atlas_cache.find(key);
        if (it == g_atlas_cache.end()) continue;

        AtlasRegion region;
        if (!g_atlas.Get_Region(it->second, region)) continue;

        // Bind atlas page texture (batch by page)
        if (!g_page_textures || region.atlas_id >= g_page_tex_count) continue;
        GLuint page_tex = g_page_textures[region.atlas_id];
        if (page_tex != current_page_tex) {
            glBindTexture(GL_TEXTURE_2D, page_tex);
            current_page_tex = page_tex;
        }

        // Sprite position: game-buffer relative to tactical window
        float game_x = static_cast<float>(cmd.shape.x);
        float game_y = static_cast<float>(cmd.shape.y);

        // Apply SHAPE_CENTER
        if (cmd.shape.flags & SHAPE_CENTER) {
            game_x -= region.w * 0.5f;
            game_y -= region.h * 0.5f;
        }

        // Transform game position → screen position
        float screen_x = tac_screen_x + (game_x - vp_x) * scale;
        float screen_y = tac_screen_y + (game_y - vp_y) * scale;
        float screen_w = region.w * scale;
        float screen_h = region.h * scale;

        // Clip: skip if entirely outside tactical screen area
        if (screen_x + screen_w < tac_screen_x || screen_x > tac_screen_x + tac_screen_w) continue;
        if (screen_y + screen_h < tac_screen_y || screen_y > tac_screen_y + tac_screen_h) continue;

        // Convert screen pixels → NDC
        float nx0 = screen_x / win_w * 2.0f - 1.0f;
        float ny0 = 1.0f - screen_y / win_h * 2.0f;
        float nx1 = (screen_x + screen_w) / win_w * 2.0f - 1.0f;
        float ny1 = 1.0f - (screen_y + screen_h) / win_h * 2.0f;

        // Quad vertices: position + UV
        float verts[] = {
            nx0, ny0, region.u0, region.v0,
            nx1, ny0, region.u1, region.v0,
            nx0, ny1, region.u0, region.v1,
            nx1, ny1, region.u1, region.v1,
        };

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, &verts[0]);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, &verts[2]);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        rendered++;
    }

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisable(GL_BLEND);

    return rendered;
}
