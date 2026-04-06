/**
 * draw_list.h — Deferred draw command list for tactical rendering.
 *
 * During Draw_It(), rendering calls record to the draw list instead of
 * (or in addition to) writing pixels immediately. After Draw_It(),
 * the list is replayed with zoom/viewport transforms applied.
 *
 * Command categories:
 *   WORLD — terrain tiles, unit/building sprites, animations, shadows
 *           These are zoomed (position + scale) during replay.
 *   OVERLAY — health bars, selection boxes, pips, debug lines
 *           These zoom with the world (position follows unit) which is
 *           standard RTS behavior (SC2, AoE2 DE work the same way).
 *
 * Out-of-game UI (sidebar, tab bar, buttons, messages) is NOT in the
 * draw list — it's drawn after replay at 1:1.
 */

#ifndef CNC_DRAW_LIST_H
#define CNC_DRAW_LIST_H

#include <cstdint>
#include <vector>

/// Draw command types.
enum DrawCmdType : uint8_t {
    CMD_SHAPE,       // CC_Draw_Shape: sprite/icon
    CMD_STAMP,       // Draw_Stamp: terrain tile
    CMD_FILL_RECT,   // Fill_Rect: health bars, shadow rects
    CMD_DRAW_RECT,   // Draw_Rect: selection boxes, rubber band
    CMD_DRAW_LINE,   // Draw_Line: selection corners, aim lines
    CMD_PUT_PIXEL,   // Put_Pixel: debug markers
};

/// Rendering layer for ordering and transform decisions.
enum DrawLayer : uint8_t {
    LAYER_TERRAIN,   // Ground tiles (Draw_Stamp)
    LAYER_SPRITE,    // Units, buildings, animations (CC_Draw_Shape)
    LAYER_SHADOW,    // Fog of war, shroud
    LAYER_OVERLAY,   // Health bars, selection, pips, debug
};

/// A recorded CC_Draw_Shape call.
struct ShapeCmd {
    const void* shapefile;
    uint32_t    entity_hash;
    int         shapenum;
    int         x, y;
    int         window;
    int         flags;
    const void* fadingdata;
    const void* ghostdata;
};

/// A recorded Draw_Stamp call.
struct StampCmd {
    const void* icondata;
    int         icon;
    int         x, y;
    const void* remap;
    int         window;
};

/// A recorded primitive (rect, line, pixel).
struct PrimCmd {
    int          x1, y1, x2, y2;
    uint8_t      color;
};

/// Single draw command with type tag.
struct DrawCommand {
    DrawCmdType type;
    DrawLayer   layer;
    union {
        ShapeCmd  shape;
        StampCmd  stamp;
        PrimCmd   prim;
    };
};

/// The draw list: records commands during Draw_It(), replayed with transforms.
class DrawList {
public:
    void Clear();

    void Record_Shape(const void* shapefile, int shapenum, int x, int y,
                      int window, int flags, const void* fadingdata,
                      const void* ghostdata, DrawLayer layer = LAYER_SPRITE,
                      uint32_t entity_hash = 0);

    void Record_Stamp(const void* icondata, int icon, int x, int y,
                      const void* remap, int window);

    void Record_Fill_Rect(int x1, int y1, int x2, int y2, uint8_t color,
                          DrawLayer layer = LAYER_OVERLAY);

    void Record_Draw_Rect(int x1, int y1, int x2, int y2, uint8_t color,
                          DrawLayer layer = LAYER_OVERLAY);

    void Record_Draw_Line(int x1, int y1, int x2, int y2, uint8_t color,
                          DrawLayer layer = LAYER_OVERLAY);

    void Record_Put_Pixel(int x, int y, uint8_t color,
                          DrawLayer layer = LAYER_OVERLAY);

    int Command_Count() const { return static_cast<int>(commands_.size()); }
    const DrawCommand& Get(int index) const { return commands_[index]; }
    const std::vector<DrawCommand>& Commands() const { return commands_; }

    bool IsRecording() const { return recording_; }
    void SetRecording(bool r) { recording_ = r; }

private:
    std::vector<DrawCommand> commands_;
    bool recording_ = false;
};

/// Global draw list instance.
extern DrawList g_draw_list;

#endif // CNC_DRAW_LIST_H
