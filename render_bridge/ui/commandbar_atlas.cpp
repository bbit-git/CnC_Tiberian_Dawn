/**
 * commandbar_atlas.cpp — HD command bar atlas manager.
 *
 * Loads MT_COMMANDBAR_COMMON.MTD + TGA from TEXTURES_SRGB.MEG,
 * decodes the TGA via stb_image, uploads to a GL RGBA texture.
 * Sprite lookup is backed by the MTDReader.
 */

#include "commandbar_atlas.h"
#include "dbg.h"
#include "hd_assets.h"
#include "meg_reader.h"
#include "mtd_reader.h"

// stb_image implementation is in hd_sprite_provider.cpp (render_core).
// We only need the declarations here.
#define STBI_ONLY_TGA
#define STBI_NO_STDIO
#define STBI_NO_HDR
#include "stb_image.h"

#include <GLES2/gl2.h>

#include <cstdlib>
#include <cstring>
#include <cctype>
#include <vector>

/// MEG paths for the atlas files (backslash as stored in MEG).
static const char* MTD_MEG_PATH = "DATA\\ART\\TEXTURES\\SRGB\\MT_COMMANDBAR_COMMON.MTD";
static const char* TGA_MEG_PATH = "DATA\\ART\\TEXTURES\\SRGB\\MT_COMMANDBAR_COMMON.TGA";

namespace {

struct AtlasState {
    MTDReader                     mtd;
    std::vector<AtlasSpriteRect>  rects;      // Parallel to MTD entries
    GLuint                        gl_texture;
    int                           atlas_w;
    int                           atlas_h;
    bool                          ready;
};

AtlasState g_atlas = {};

} // namespace

bool Commandbar_Atlas_Init()
{
    if (g_atlas.ready) return true;

    MegReader* meg_ptr = HD_Assets_Get_Meg("TEXTURES_SRGB.MEG");
    if (!meg_ptr) {
        DBG("commandbar_atlas: TEXTURES_SRGB.MEG not in HD cache");
        return false;
    }
    MegReader& meg = *meg_ptr;

    // --- Load MTD ---
    const MegEntry* mtd_entry = meg.Find(MTD_MEG_PATH);
    if (!mtd_entry) {
        DBG("commandbar_atlas: MTD not found in MEG");
        return false;
    }

    size_t mtd_size = 0;
    void* mtd_data = meg.Read_Alloc(mtd_entry, &mtd_size);
    if (!mtd_data) {
        DBG("commandbar_atlas: failed to read MTD (%u bytes)", mtd_entry->size);
        return false;
    }

    if (!g_atlas.mtd.Open(mtd_data, mtd_size)) {
        DBG("commandbar_atlas: failed to parse MTD");
        free(mtd_data);
        return false;
    }
    free(mtd_data);
    DBG("commandbar_atlas: MTD parsed — %d sprites, %d bpp",
        g_atlas.mtd.Count(), g_atlas.mtd.BPP());

    // --- Load TGA ---
    const MegEntry* tga_entry = meg.Find(TGA_MEG_PATH);
    if (!tga_entry) {
        DBG("commandbar_atlas: TGA not found in MEG");
        g_atlas.mtd.Close();
        return false;
    }

    size_t tga_size = 0;
    void* tga_data = meg.Read_Alloc(tga_entry, &tga_size);
    if (!tga_data) {
        DBG("commandbar_atlas: failed to read TGA (%u bytes)", tga_entry->size);
        g_atlas.mtd.Close();
        return false;
    }
    DBG("commandbar_atlas: TGA loaded — %.1f MB", tga_size / (1024.0 * 1024.0));

    int w = 0, h = 0, channels = 0;
    uint8_t* pixels = stbi_load_from_memory(
        static_cast<const uint8_t*>(tga_data),
        static_cast<int>(tga_size),
        &w, &h, &channels, 4);  // Force 4 channels (RGBA)
    free(tga_data);

    if (!pixels) {
        DBG("commandbar_atlas: TGA decode failed: %s", stbi_failure_reason());
        g_atlas.mtd.Close();
        return false;
    }
    DBG("commandbar_atlas: TGA decoded — %dx%d, %d channels", w, h, channels);

    // Check GL_MAX_TEXTURE_SIZE
    GLint max_tex_size = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_tex_size);
    DBG("commandbar_atlas: GL_MAX_TEXTURE_SIZE = %d", max_tex_size);

    int required = w > h ? w : h;
    if (max_tex_size > 0 && required > max_tex_size) {
        DBG("commandbar_atlas: atlas %dx%d exceeds GL_MAX_TEXTURE_SIZE %d — cannot load",
            w, h, max_tex_size);
        stbi_image_free(pixels);
        g_atlas.mtd.Close();
        return false;
    }

    // --- Upload to GL texture ---
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    GLenum err = glGetError();
    stbi_image_free(pixels);

    if (err != GL_NO_ERROR) {
        DBG("commandbar_atlas: GL texture upload failed (error 0x%04X)", err);
        glDeleteTextures(1, &tex);
        g_atlas.mtd.Close();
        return false;
    }

    g_atlas.gl_texture = tex;
    g_atlas.atlas_w = w;
    g_atlas.atlas_h = h;

    // --- Build sprite rects with pre-computed UVs ---
    float inv_w = 1.0f / static_cast<float>(w);
    float inv_h = 1.0f / static_cast<float>(h);

    int count = g_atlas.mtd.Count();
    g_atlas.rects.resize(count);

    for (int i = 0; i < count; i++) {
        const MTDEntry* e = g_atlas.mtd.Get_Entry(i);
        if (!e) continue;

        AtlasSpriteRect& r = g_atlas.rects[i];
        r.atlas_x = e->atlas_x;
        r.atlas_y = e->atlas_y;
        r.width   = e->width;
        r.height  = e->height;
        r.flag    = e->flag;
        r.u0      = e->atlas_x * inv_w;
        r.v0      = e->atlas_y * inv_h;
        r.u1      = (e->atlas_x + e->width) * inv_w;
        r.v1      = (e->atlas_y + e->height) * inv_h;
    }

    g_atlas.ready = true;
    DBG("commandbar_atlas: ready — %d sprites, GL tex %u, %dx%d",
        count, tex, w, h);
    return true;
}

void Commandbar_Atlas_Shutdown()
{
    if (g_atlas.gl_texture) {
        glDeleteTextures(1, &g_atlas.gl_texture);
        g_atlas.gl_texture = 0;
    }
    g_atlas.mtd.Close();
    g_atlas.rects.clear();
    g_atlas.atlas_w = 0;
    g_atlas.atlas_h = 0;
    g_atlas.ready = false;
}

bool Commandbar_Atlas_Is_Ready()
{
    return g_atlas.ready;
}

const AtlasSpriteRect* Commandbar_Atlas_Find(const char* name)
{
    if (!g_atlas.ready || !name) return nullptr;

    int idx = g_atlas.mtd.Find_Index(name);
    if (idx < 0) return nullptr;
    return &g_atlas.rects[idx];
}

uint32_t Commandbar_Atlas_Get_Texture()
{
    return g_atlas.gl_texture;
}

void Commandbar_Atlas_Get_Size(int& width, int& height)
{
    width  = g_atlas.atlas_w;
    height = g_atlas.atlas_h;
}
