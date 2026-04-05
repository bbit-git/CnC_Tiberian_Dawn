/**
 * ui_text_sdf.cpp — SDF atlas generation from TTF fonts via stb_truetype.
 */

#define STB_TRUETYPE_IMPLEMENTATION
#include "../../../libs/render/stb_truetype.h"

#include "ui_text_sdf.h"
#include "ui_text.h"
#include "dbg.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

#ifdef _WIN32
#include <io.h>
#define access _access
#define F_OK 0
#else
#include <unistd.h>
#endif

// ---------------------------------------------------------------------------
// Font scale
// ---------------------------------------------------------------------------

// Default scale maps 48px atlas to ~12px in game-buffer space (640x400).
// User can adjust via Render_Bridge_UI_Set_Font_Scale().
static float g_font_scale = 0.25f;

void UI_Text_Set_Font_Scale(float s)
{
    if (s < 0.5f) s = 0.5f;
    if (s > 4.0f) s = 4.0f;
    g_font_scale = s;
}

float UI_Text_Get_Font_Scale()
{
    return g_font_scale;
}

// ---------------------------------------------------------------------------
// Font discovery
// ---------------------------------------------------------------------------

static const char* k_sans_paths[] = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
#ifdef _WIN32
    "C:\\Windows\\Fonts\\segoeui.ttf",
    "C:\\Windows\\Fonts\\arial.ttf",
#endif
    nullptr
};

static const char* k_mono_paths[] = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
    "/usr/share/fonts/truetype/noto/NotoSansMono-Regular.ttf",
    "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
#ifdef _WIN32
    "C:\\Windows\\Fonts\\consola.ttf",
    "C:\\Windows\\Fonts\\cour.ttf",
#endif
    nullptr
};

const char* SDF_Font_Discover(const char* category)
{
    const char** paths = nullptr;
    if (strcmp(category, "sans") == 0) paths = k_sans_paths;
    else if (strcmp(category, "mono") == 0) paths = k_mono_paths;
    else return nullptr;

    for (int i = 0; paths[i]; i++) {
        if (access(paths[i], F_OK) == 0) {
            DBG("ui_text_sdf: discovered %s → %s", category, paths[i]);
            return paths[i];
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// SDF atlas builder
// ---------------------------------------------------------------------------

static unsigned char* load_file(const char* path, int* size_out)
{
    FILE* f = fopen(path, "rb");
    if (!f) return nullptr;
    fseek(f, 0, SEEK_END);
    int size = (int)ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char* buf = (unsigned char*)malloc(size);
    if (!buf) { fclose(f); return nullptr; }
    fread(buf, 1, size, f);
    fclose(f);
    *size_out = size;
    return buf;
}

// Declared in ui_text.cpp — direct access to the atlas array.
extern UIFontAtlas g_atlases[];

bool SDF_Build_Atlas(UIFontID font_id, const char* ttf_path, int font_size, int sdf_padding)
{
    if (font_id < 0 || font_id >= UI_FONT_COUNT) return false;
    if (!ttf_path) return false;

    UIFontAtlas& atlas = g_atlases[font_id];
    if (atlas.loaded) return true;

    int ttf_size = 0;
    unsigned char* ttf_data = load_file(ttf_path, &ttf_size);
    if (!ttf_data) {
        DBG("ui_text_sdf: failed to load %s", ttf_path);
        return false;
    }

    stbtt_fontinfo font;
    if (!stbtt_InitFont(&font, ttf_data, stbtt_GetFontOffsetForIndex(ttf_data, 0))) {
        DBG("ui_text_sdf: failed to parse %s", ttf_path);
        free(ttf_data);
        return false;
    }

    float scale = stbtt_ScaleForPixelHeight(&font, (float)font_size);

    // First pass: measure glyphs, compute atlas dimensions
    struct GlyphInfo {
        int w, h;       // SDF bitmap size (including padding)
        int advance;
        int lsb;
        int x0, y0, x1, y1; // bitmap box
    };
    GlyphInfo infos[256];
    memset(infos, 0, sizeof(infos));

    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &line_gap);
    int line_height = (int)ceilf((ascent - descent + line_gap) * scale);

    int total_width = 0;
    int max_h = 0;
    for (int ch = 32; ch < 256; ch++) {
        int adv, lsb;
        stbtt_GetCodepointHMetrics(&font, ch, &adv, &lsb);
        int bx0, by0, bx1, by1;
        stbtt_GetCodepointBitmapBox(&font, ch, scale, scale, &bx0, &by0, &bx1, &by1);

        int gw = (bx1 - bx0) + sdf_padding * 2;
        int gh = (by1 - by0) + sdf_padding * 2;
        if (gw <= 0) gw = 0;
        if (gh <= 0) gh = 0;

        infos[ch].w = gw;
        infos[ch].h = gh;
        infos[ch].advance = (int)roundf(adv * scale);
        infos[ch].lsb = lsb;
        infos[ch].x0 = bx0;
        infos[ch].y0 = by0;
        infos[ch].x1 = bx1;
        infos[ch].y1 = by1;
        total_width += gw + 1;
        if (gh > max_h) max_h = gh;
    }

    // Determine atlas size (power of two, shelf packing)
    int atlas_w = 256;
    while (atlas_w < 2048) {
        int rows_needed = 1;
        int cx = 0;
        for (int ch = 32; ch < 256; ch++) {
            if (infos[ch].w == 0) continue;
            if (cx + infos[ch].w + 1 > atlas_w) {
                rows_needed++;
                cx = 0;
            }
            cx += infos[ch].w + 1;
        }
        int atlas_h_needed = rows_needed * (max_h + 1);
        // Round up to power of two
        int ah = 256;
        while (ah < atlas_h_needed) ah *= 2;
        if (ah <= atlas_w) {
            // Good enough
            break;
        }
        atlas_w *= 2;
    }

    // Compute actual atlas height with chosen width
    int cx = 0, cy = 0;
    for (int ch = 32; ch < 256; ch++) {
        if (infos[ch].w == 0) continue;
        if (cx + infos[ch].w + 1 > atlas_w) {
            cy += max_h + 1;
            cx = 0;
        }
        cx += infos[ch].w + 1;
    }
    int atlas_h = cy + max_h + 1;
    // Round up to power of two
    {
        int ah = 256;
        while (ah < atlas_h) ah *= 2;
        atlas_h = ah;
    }

    if (atlas.pixels) {
        free(atlas.pixels);
        atlas.pixels = nullptr;
    }
    atlas.pixels = (uint8_t*)calloc(atlas_w * atlas_h, 1);
    if (!atlas.pixels) {
        free(ttf_data);
        return false;
    }

    atlas.atlas_w = atlas_w;
    atlas.atlas_h = atlas_h;
    atlas.line_height = line_height;
    atlas.is_sdf = true;
    atlas.sdf_pixel_range = (float)sdf_padding;
    atlas.sdf_font_size = font_size;

    // Second pass: render SDF glyphs and pack into atlas
    cx = 0;
    cy = 0;
    for (int ch = 32; ch < 256; ch++) {
        UIGlyph& g = atlas.glyphs[ch];
        g.width = infos[ch].w;
        g.height = infos[ch].h;
        g.top_blank = 0;
        g.advance = infos[ch].advance;

        if (infos[ch].w == 0 || infos[ch].h == 0) {
            g.u0 = g.v0 = g.u1 = g.v1 = 0.0f;
            continue;
        }

        if (cx + infos[ch].w + 1 > atlas_w) {
            cy += max_h + 1;
            cx = 0;
        }

        // Render SDF for this glyph
        int glyph_index = stbtt_FindGlyphIndex(&font, ch);
        if (glyph_index > 0) {
            int sdf_w, sdf_h, sdf_xoff, sdf_yoff;
            unsigned char* sdf_bitmap = stbtt_GetGlyphSDF(
                &font, scale, glyph_index, sdf_padding,
                128, 128.0f / sdf_padding,
                &sdf_w, &sdf_h, &sdf_xoff, &sdf_yoff);

            if (sdf_bitmap) {
                // Copy into atlas
                for (int row = 0; row < sdf_h && row < infos[ch].h; row++) {
                    for (int col = 0; col < sdf_w && col < infos[ch].w; col++) {
                        atlas.pixels[(cy + row) * atlas_w + (cx + col)] =
                            sdf_bitmap[row * sdf_w + col];
                    }
                }
                stbtt_FreeSDF(sdf_bitmap, nullptr);
            }
        }

        // Store glyph UVs and metrics
        g.u0 = (float)cx / atlas_w;
        g.v0 = (float)cy / atlas_h;
        g.u1 = (float)(cx + infos[ch].w) / atlas_w;
        g.v1 = (float)(cy + infos[ch].h) / atlas_h;

        // Store bitmap box offset for proper vertical positioning
        g.top_blank = infos[ch].y0 + (int)roundf(ascent * scale);

        cx += infos[ch].w + 1;
    }

    // Glyphs 0-31: leave as zero (no renderable glyphs)
    // Space (32) gets advance but no pixels — already handled above

    atlas.loaded = true;
    // Safe to free: all stbtt calls are complete. If re-rasterization at a
    // different size is needed later, the TTF must be reloaded from disk.
    free(ttf_data);

    DBG("ui_text_sdf: built SDF atlas %d from %s (%dx%d, size=%d, padding=%d)",
        font_id, ttf_path, atlas_w, atlas_h, font_size, sdf_padding);
    return true;
}
