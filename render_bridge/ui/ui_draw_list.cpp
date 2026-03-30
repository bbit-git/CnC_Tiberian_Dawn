/**
 * ui_draw_list.cpp — UI draw list recording implementation.
 */

#include "ui_draw_list.h"
#include <cstring>

UIDrawList g_ui_draw_list;

void UIDrawList::Clear()
{
    commands_.clear();
    strings_.clear();
}

int UIDrawList::Command_Count() const
{
    return static_cast<int>(commands_.size());
}

const UIDrawCmd& UIDrawList::Get(int index) const
{
    return commands_[index];
}

void UIDrawList::Fill_Rect(int x, int y, int w, int h,
                           uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    UIDrawCmd cmd;
    cmd.type = UI_CMD_FILL_RECT;
    cmd.rect = {x, y, w, h, r, g, b, a};
    commands_.push_back(cmd);
}

void UIDrawList::Draw_Rect(int x, int y, int w, int h,
                           uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    UIDrawCmd cmd;
    cmd.type = UI_CMD_DRAW_RECT;
    cmd.rect = {x, y, w, h, r, g, b, a};
    commands_.push_back(cmd);
}

void UIDrawList::Draw_Text(int x, int y, const char* text, int font_id,
                           uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!text || !text[0]) return;

    int len = static_cast<int>(strlen(text));
    int offset = static_cast<int>(strings_.size());
    strings_.insert(strings_.end(), text, text + len);

    UIDrawCmd cmd;
    cmd.type = UI_CMD_TEXT;
    cmd.text = {x, y, r, g, b, a, offset, len, font_id};
    commands_.push_back(cmd);
}

void UIDrawList::Draw_Icon(int x, int y, int w, int h,
                           const uint8_t* pixels, int src_w, int src_h)
{
    UIDrawCmd cmd;
    cmd.type = UI_CMD_ICON;
    cmd.icon = {x, y, w, h, pixels, src_w, src_h};
    commands_.push_back(cmd);
}

void UIDrawList::Clip_Push(int x, int y, int w, int h)
{
    UIDrawCmd cmd;
    cmd.type = UI_CMD_CLIP_PUSH;
    cmd.clip = {x, y, w, h};
    commands_.push_back(cmd);
}

void UIDrawList::Clip_Pop()
{
    UIDrawCmd cmd;
    cmd.type = UI_CMD_CLIP_POP;
    cmd.clip = {0, 0, 0, 0};
    commands_.push_back(cmd);
}

const char* UIDrawList::Get_String(int offset, int length) const
{
    if (offset < 0 || offset + length > static_cast<int>(strings_.size())) {
        return "";
    }
    return &strings_[offset];
}
