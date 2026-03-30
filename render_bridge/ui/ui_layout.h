/**
 * ui_layout.h — Simple layout helpers for bridge-native UI.
 *
 * Provides a layout cursor that tracks position within a region.
 * Supports row and column arrangement with padding and alignment.
 * Not a constraint solver — just a position accumulator.
 */

#ifndef CNC_UI_LAYOUT_H
#define CNC_UI_LAYOUT_H

/// Layout direction.
enum UIDirection : int {
    UI_DIR_VERTICAL   = 0,  // children stack top-to-bottom
    UI_DIR_HORIZONTAL = 1,  // children stack left-to-right
};

/// Layout cursor — tracks position within a rectangular region.
struct UILayout {
    int origin_x, origin_y;   // top-left of the layout region
    int width, height;        // total region size
    int cursor_x, cursor_y;  // current placement position
    int padding;              // gap between items
    UIDirection direction;

    /// Start a layout region.
    void Begin(int x, int y, int w, int h, UIDirection dir, int pad = 2);

    /// Get the next item rect of the given size. Advances the cursor.
    void Next(int item_w, int item_h, int& out_x, int& out_y);

    /// Get remaining width/height from the cursor to the region edge.
    int Remaining_Width() const;
    int Remaining_Height() const;

    /// Reset cursor to origin.
    void Reset();
};

#endif // CNC_UI_LAYOUT_H
