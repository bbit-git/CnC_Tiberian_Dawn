/**
 * ui_text.h — Font atlas for bridge-native UI text rendering.
 *
 * Reads the game's FNT font data and builds a GL-ready alpha atlas.
 * Provides text measurement and per-glyph UV lookup for the GL renderer.
 */

#ifndef CNC_UI_TEXT_H
#define CNC_UI_TEXT_H

#include <cstdint>

/// Font identifier for UI draw commands.
enum UIFontID : int {
    UI_FONT_6PT  = 0,  // 6-point (6POINT.FNT)
    UI_FONT_8PT  = 1,  // 8-point default (8POINT.FNT / Font8Ptr)
    UI_FONT_LED  = 2,  // LED digits (LED.FNT)
    UI_FONT_VCR  = 3,  // VCR style (VCR.FNT)
    UI_FONT_COUNT_LEGACY = 4,
    UI_FONT_SDF_DEFAULT = 4,  // System sans-serif (SDF)
    UI_FONT_SDF_MONO    = 5,  // System monospace (SDF)
    UI_FONT_COUNT = 6,
};

/// Per-glyph metrics and atlas position.
struct UIGlyph {
    float u0, v0, u1, v1;  // UV in atlas texture
    int   width;            // pixel width
    int   height;           // pixel height (usually = font height)
    int   top_blank;        // blank rows above glyph data
    int   advance;          // total advance including spacing
};

/// Font atlas for one font (FNT bitmap or SDF).
struct UIFontAtlas {
    uint8_t* pixels;     // alpha-only atlas (owned, freed on shutdown)
    int      atlas_w;
    int      atlas_h;
    int      line_height; // max glyph height
    UIGlyph  glyphs[256];
    bool     loaded;
    bool     is_sdf;            // true for SDF TTF atlases
    float    sdf_pixel_range;   // SDF spread in atlas pixels (e.g. 6.0)
    int      sdf_font_size;     // rasterization size in pixels (e.g. 48)
};

/// Build atlas for a font from its FNT data pointer. Call after fonts are loaded.
bool UI_Text_Build_Atlas(UIFontID font_id, const void* fnt_data);

/// Get the atlas for a font. Returns null if not built.
const UIFontAtlas* UI_Text_Get_Atlas(UIFontID font_id);

/// Measure text width in pixels for a given font.
int UI_Text_Measure_Width(UIFontID font_id, const char* text, int length);

/// Get line height for a font.
int UI_Text_Line_Height(UIFontID font_id);

/// Shut down and free all atlas memory.
void UI_Text_Shutdown();

#endif // CNC_UI_TEXT_H
