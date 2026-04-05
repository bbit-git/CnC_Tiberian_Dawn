/**
 * ui_text_sdf.h — SDF atlas builder for TTF fonts.
 *
 * Loads system TTF fonts and builds SDF (Signed Distance Field) atlases
 * that slot into the existing UIFontAtlas / UIFontID system. SDF atlases
 * render resolution-independent anti-aliased text via smoothstep in the
 * fragment shader.
 */

#ifndef CNC_UI_TEXT_SDF_H
#define CNC_UI_TEXT_SDF_H

#include "ui_text.h"

/// Build an SDF atlas from a TTF file path into the given font slot.
/// font_size: rasterization height in pixels (e.g. 48).
/// sdf_padding: SDF spread in pixels (e.g. 6).
bool SDF_Build_Atlas(UIFontID font_id, const char* ttf_path, int font_size, int sdf_padding);

/// Discover a system TTF font by category ("sans" or "mono").
/// Returns a static path string, or nullptr if none found.
const char* SDF_Font_Discover(const char* category);

/// Font scale accessors (SDF fonts only).
void  UI_Text_Set_Font_Scale(float s);
float UI_Text_Get_Font_Scale();

#endif // CNC_UI_TEXT_SDF_H
