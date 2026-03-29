/**
 * draw_list.cpp — Draw command list implementation.
 */

#include "draw_list.h"
#include <cstring>

DrawList g_draw_list;

void DrawList::Clear()
{
    commands_.clear();
}

void DrawList::Record_Shape(const void* shapefile, int shapenum, int x, int y,
                            int window, int flags, const void* fadingdata,
                            const void* ghostdata, DrawLayer layer)
{
    DrawCommand cmd;
    cmd.type = CMD_SHAPE;
    cmd.layer = layer;
    cmd.shape = {shapefile, shapenum, x, y, window, flags, fadingdata, ghostdata};
    commands_.push_back(cmd);
}

void DrawList::Record_Stamp(const void* icondata, int icon, int x, int y,
                            const void* remap, int window)
{
    DrawCommand cmd;
    cmd.type = CMD_STAMP;
    cmd.layer = LAYER_TERRAIN;
    cmd.stamp = {icondata, icon, x, y, remap, window};
    commands_.push_back(cmd);
}

void DrawList::Record_Fill_Rect(int x1, int y1, int x2, int y2, uint8_t color,
                                DrawLayer layer)
{
    DrawCommand cmd;
    cmd.type = CMD_FILL_RECT;
    cmd.layer = layer;
    memset(&cmd.prim, 0, sizeof(cmd.prim));
    cmd.prim = {x1, y1, x2, y2, color};
    commands_.push_back(cmd);
}

void DrawList::Record_Draw_Rect(int x1, int y1, int x2, int y2, uint8_t color,
                                DrawLayer layer)
{
    DrawCommand cmd;
    cmd.type = CMD_DRAW_RECT;
    cmd.layer = layer;
    cmd.prim = {x1, y1, x2, y2, color};
    commands_.push_back(cmd);
}

void DrawList::Record_Draw_Line(int x1, int y1, int x2, int y2, uint8_t color,
                                DrawLayer layer)
{
    DrawCommand cmd;
    cmd.type = CMD_DRAW_LINE;
    cmd.layer = layer;
    cmd.prim = {x1, y1, x2, y2, color};
    commands_.push_back(cmd);
}

void DrawList::Record_Put_Pixel(int x, int y, uint8_t color, DrawLayer layer)
{
    DrawCommand cmd;
    cmd.type = CMD_PUT_PIXEL;
    cmd.layer = layer;
    cmd.prim = {x, y, 0, 0, color};
    commands_.push_back(cmd);
}
