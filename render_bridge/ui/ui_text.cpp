/**
 * ui_text.cpp — Font atlas construction from game FNT data.
 *
 * FNT format: header shorts at offsets 0..12, then blocks:
 *   FONTINFOBLOCK(4)    -> info (maxheight at +4, maxwidth at +5)
 *   FONTOFFSETBLOCK(6)  -> unsigned short per char -> offset into data block
 *   FONTWIDTHBLOCK(8)   -> unsigned char per char -> pixel width
 *   FONTDATABLOCK(10)   -> nibble-packed pixel data (2 pixels/byte, row-major)
 *   FONTHEIGHTBLOCK(12) -> 2 bytes per char: [0]=top blank, [1]=char height
 *
 * Nibble packing: low nibble = left pixel, high nibble = right pixel.
 */

#include "ui_text.h"
#include "dbg.h"
#include <cstdlib>
#include <cstring>

static constexpr int FONTINFOBLOCK   = 4;
static constexpr int FONTOFFSETBLOCK = 6;
static constexpr int FONTWIDTHBLOCK  = 8;
static constexpr int FONTHEIGHTBLOCK = 12;
static constexpr int FONTINFOMAXHEIGHT = 4;
static constexpr int FONTINFOMAXWIDTH  = 5;

static UIFontAtlas g_atlases[UI_FONT_COUNT];

bool UI_Text_Build_Atlas(UIFontID font_id, const void* fnt_data)
{
    if (font_id < 0 || font_id >= UI_FONT_COUNT) return false;
    if (!fnt_data) return false;

    UIFontAtlas& atlas = g_atlases[font_id];
    if (atlas.loaded) return true;

    const uint8_t* base = static_cast<const uint8_t*>(fnt_data);
    const uint16_t* hdr = reinterpret_cast<const uint16_t*>(base);

    const uint8_t* info_block   = base + hdr[FONTINFOBLOCK / 2];
    const uint16_t* offset_block = reinterpret_cast<const uint16_t*>(base + hdr[FONTOFFSETBLOCK / 2]);
    const uint8_t* width_block  = base + hdr[FONTWIDTHBLOCK / 2];
    uint16_t height_off = hdr[FONTHEIGHTBLOCK / 2];
    const uint8_t* height_block = height_off ? (base + height_off) : nullptr;

    int header_max_height = info_block[FONTINFOMAXHEIGHT];
    int header_max_width  = info_block[FONTINFOMAXWIDTH];
    if (header_max_height <= 0 || header_max_height > 64) return false;

    int max_cell_width = header_max_width;
    int max_cell_height = header_max_height;
    for (int ch = 0; ch < 256; ch++) {
        int char_width = width_block[ch];
        int top_blank = 0;
        int char_height = header_max_height;
        if (height_block) {
            top_blank = height_block[ch * 2];
            char_height = height_block[ch * 2 + 1];
        }
        if (char_width > max_cell_width) max_cell_width = char_width;
        if (top_blank + char_height > max_cell_height) {
            max_cell_height = top_blank + char_height;
        }
    }
    if (max_cell_width < 0) max_cell_width = 0;
    if (max_cell_height <= 0) max_cell_height = header_max_height;

    atlas.line_height = max_cell_height;

    // Compute atlas size: 16x16 grid of glyphs, sized from actual glyph extents.
    int cell_w = max_cell_width + 1;
    int cell_h = max_cell_height + 1;
    atlas.atlas_w = cell_w * 16;
    atlas.atlas_h = cell_h * 16;

    if (atlas.pixels) {
        free(atlas.pixels);
        atlas.pixels = nullptr;
    }
    atlas.pixels = static_cast<uint8_t*>(calloc(atlas.atlas_w * atlas.atlas_h, 1));
    if (!atlas.pixels) return false;

    for (int ch = 0; ch < 256; ch++) {
        int char_width = width_block[ch];
        int top_blank = 0;
        int char_height = header_max_height;
        if (height_block) {
            top_blank = height_block[ch * 2];
            char_height = height_block[ch * 2 + 1];
        }
        if (char_height <= 0) char_height = 0;

        int grid_x = (ch % 16) * cell_w;
        int grid_y = (ch / 16) * cell_h;

        UIGlyph& g = atlas.glyphs[ch];
        g.width = char_width;
        g.height = max_cell_height;
        g.top_blank = top_blank;
        g.advance = char_width;
        g.u0 = static_cast<float>(grid_x) / atlas.atlas_w;
        g.v0 = static_cast<float>(grid_y) / atlas.atlas_h;
        g.u1 = static_cast<float>(grid_x + char_width) / atlas.atlas_w;
        g.v1 = static_cast<float>(grid_y + max_cell_height) / atlas.atlas_h;

        if (char_width <= 0 || char_height <= 0) continue;

        const uint8_t* char_data = base + offset_block[ch];

        // Decode nibble-packed glyph into atlas alpha channel
        for (int row = 0; row < char_height; row++) {
            int dst_y = grid_y + top_blank + row;
            if (dst_y < 0 || dst_y >= atlas.atlas_h) continue;
            int bytes_per_row = (char_width + 1) / 2;
            for (int col = 0; col < char_width; col++) {
                int byte_idx = row * bytes_per_row + col / 2;
                uint8_t byte_val = char_data[byte_idx];
                uint8_t nibble = (col & 1) ? (byte_val >> 4) : (byte_val & 0x0F);

                // Non-zero nibble = foreground (alpha 255)
                if (nibble != 0) {
                    int dst_x = grid_x + col;
                    if (dst_x < 0 || dst_x >= atlas.atlas_w) continue;
                    atlas.pixels[dst_y * atlas.atlas_w + dst_x] = 255;
                }
            }
        }
    }

    atlas.loaded = true;
    DBG("ui_text: built atlas %d (%dx%d, line_height=%d)",
        font_id, atlas.atlas_w, atlas.atlas_h, max_cell_height);
    return true;
}

const UIFontAtlas* UI_Text_Get_Atlas(UIFontID font_id)
{
    if (font_id < 0 || font_id >= UI_FONT_COUNT) return nullptr;
    if (!g_atlases[font_id].loaded) return nullptr;
    return &g_atlases[font_id];
}

int UI_Text_Measure_Width(UIFontID font_id, const char* text, int length)
{
    const UIFontAtlas* atlas = UI_Text_Get_Atlas(font_id);
    if (!atlas || !text) return 0;

    int width = 0;
    for (int i = 0; i < length; i++) {
        uint8_t ch = static_cast<uint8_t>(text[i]);
        width += atlas->glyphs[ch].advance;
    }
    return width;
}

int UI_Text_Line_Height(UIFontID font_id)
{
    const UIFontAtlas* atlas = UI_Text_Get_Atlas(font_id);
    return atlas ? atlas->line_height : 0;
}

void UI_Text_Shutdown()
{
    for (int i = 0; i < UI_FONT_COUNT; i++) {
        if (g_atlases[i].pixels) {
            free(g_atlases[i].pixels);
            g_atlases[i].pixels = nullptr;
        }
        g_atlases[i].loaded = false;
    }
}
