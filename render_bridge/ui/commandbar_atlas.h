/**
 * commandbar_atlas.h — HD command bar atlas manager.
 *
 * Loads MT_COMMANDBAR_COMMON.MTD + TGA from TEXTURES_SRGB.MEG,
 * uploads the atlas as a GL texture, and provides sprite lookup
 * for the UI draw list.
 *
 * The atlas is 6871x6716 px, 32bpp, containing 1554 named sprites
 * (UI chrome, build cameos, buttons). See docs/hd-sidebar-integration-plan.md.
 */

#ifndef CNC_COMMANDBAR_ATLAS_H
#define CNC_COMMANDBAR_ATLAS_H

#include <cstdint>

/// UV rect for a sprite in the atlas texture.
struct AtlasSpriteRect {
    uint32_t  atlas_x, atlas_y;   // Pixel position in atlas
    uint32_t  width, height;       // Sprite size in pixels
    float     u0, v0, u1, v1;     // Normalized UV coordinates
    uint8_t   flag;                // 0 = build icon, 1 = UI element
};

/// Initialize the command bar atlas from a MEG archive path.
/// Loads MTD metadata + TGA pixel data, uploads GL texture.
/// @param meg_path  Path to TEXTURES_SRGB.MEG
/// @return true on success, false if files missing or GL_MAX_TEXTURE_SIZE too small
bool Commandbar_Atlas_Init(const char* meg_path);

/// Release atlas resources (GL texture + metadata).
void Commandbar_Atlas_Shutdown();

/// Returns true if the atlas is loaded and ready.
bool Commandbar_Atlas_Is_Ready();

/// Find a sprite by name (case-insensitive).
/// @param name  Sprite name (e.g. "UI_SIDEBAR_BUILDFRAME.TGA")
/// @return Pointer to sprite rect, or nullptr if not found. Valid until Shutdown.
const AtlasSpriteRect* Commandbar_Atlas_Find(const char* name);

/// Get the GL texture ID for the atlas.
uint32_t Commandbar_Atlas_Get_Texture();

/// Get atlas pixel dimensions.
void Commandbar_Atlas_Get_Size(int& width, int& height);

#endif // CNC_COMMANDBAR_ATLAS_H
