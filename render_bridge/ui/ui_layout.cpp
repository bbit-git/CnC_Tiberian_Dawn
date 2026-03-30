/**
 * ui_layout.cpp — Layout cursor implementation.
 */

#include "ui_layout.h"

void UILayout::Begin(int x, int y, int w, int h, UIDirection dir, int pad)
{
    origin_x  = x;
    origin_y  = y;
    width     = w;
    height    = h;
    cursor_x  = x;
    cursor_y  = y;
    padding   = pad;
    direction = dir;
}

void UILayout::Next(int item_w, int item_h, int& out_x, int& out_y)
{
    out_x = cursor_x;
    out_y = cursor_y;

    if (direction == UI_DIR_VERTICAL) {
        cursor_y += item_h + padding;
    } else {
        cursor_x += item_w + padding;
    }
}

int UILayout::Remaining_Width() const
{
    int edge = origin_x + width;
    return (edge > cursor_x) ? edge - cursor_x : 0;
}

int UILayout::Remaining_Height() const
{
    int edge = origin_y + height;
    return (edge > cursor_y) ? edge - cursor_y : 0;
}

void UILayout::Reset()
{
    cursor_x = origin_x;
    cursor_y = origin_y;
}
