/**
 * ui_draw_list.h — Explicit UI draw command list for bridge-native UI.
 *
 * Records UI rendering operations as structured data instead of
 * direct buffer writes. The GL renderer consumes these each frame.
 */

#ifndef CNC_UI_DRAW_LIST_H
#define CNC_UI_DRAW_LIST_H

#include <cstdint>
#undef min
#undef max
#include <vector>

/// UI draw command types.
enum UIDrawCmdType : uint8_t {
    UI_CMD_FILL_RECT,     // filled rectangle
    UI_CMD_DRAW_RECT,     // outlined rectangle
    UI_CMD_TEXT,          // text string
    UI_CMD_ICON,          // palette-indexed icon/sprite quad
    UI_CMD_CLIP_PUSH,     // push scissor clip rect
    UI_CMD_CLIP_POP,      // pop scissor clip rect
};

/// Filled or outlined rectangle.
struct UIRectCmd {
    int x, y, w, h;
    uint8_t r, g, b, a;
};

/// Text rendering command.
struct UITextCmd {
    int x, y;
    uint8_t r, g, b, a;
    int text_offset;      // offset into string pool
    int text_length;      // byte count
    int font_id;          // font identifier
};

/// Icon/sprite quad (palette-indexed).
struct UIIconCmd {
    int x, y, w, h;
    const uint8_t* pixels;  // pointer to pixel data (valid this frame)
    int src_w, src_h;       // source dimensions
};

/// Clip rect push.
struct UIClipCmd {
    int x, y, w, h;
};

/// A single UI draw command.
struct UIDrawCmd {
    UIDrawCmdType type;
    union {
        UIRectCmd  rect;
        UITextCmd  text;
        UIIconCmd  icon;
        UIClipCmd  clip;
    };
};

/// UI draw list — records commands for one frame.
class UIDrawList {
public:
    void Clear();
    int  Command_Count() const;
    const UIDrawCmd& Get(int index) const;

    void Fill_Rect(int x, int y, int w, int h,
                   uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
    void Draw_Rect(int x, int y, int w, int h,
                   uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
    void Draw_Text(int x, int y, const char* text, int font_id,
                   uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
    void Draw_Icon(int x, int y, int w, int h,
                   const uint8_t* pixels, int src_w, int src_h);
    void Clip_Push(int x, int y, int w, int h);
    void Clip_Pop();

    /// Access the string pool for text command lookups.
    const char* Get_String(int offset, int length) const;

private:
    std::vector<UIDrawCmd> commands_;
    std::vector<char>      strings_;
};

/// Global UI draw list instance.
extern UIDrawList g_ui_draw_list;

#endif // CNC_UI_DRAW_LIST_H
