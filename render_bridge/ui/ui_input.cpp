/**
 * ui_input.cpp — UI hit-testing and input capture implementation.
 */

#include "ui_input.h"

namespace {

struct HitZone {
    int x, y, w, h;
};

static constexpr int UI_KEY_BUF_SIZE = 64;
unsigned short g_key_buf[UI_KEY_BUF_SIZE];
int g_key_head = 0;
int g_key_tail = 0;

HitZone  g_zones[UI_MAX_HIT_ZONES];
int      g_zone_count    = 0;
UIHitZoneID g_hovered    = UI_HIT_NONE;
UIHitZoneID g_pressed    = UI_HIT_NONE;
UIHitZoneID g_clicked    = UI_HIT_NONE;  // zone that received left mouse-down THIS frame
UIHitZoneID g_right_clicked = UI_HIT_NONE; // zone that received right mouse-down THIS frame
bool     g_has_capture   = false;
bool     g_was_left_down = false;
bool     g_was_right_down = false;
float    g_scroll_delta  = 0.0f;

} // namespace

UIHitZoneID UI_Input_Register_Zone(int x, int y, int w, int h)
{
    if (g_zone_count >= UI_MAX_HIT_ZONES) return UI_HIT_NONE;
    int id = g_zone_count++;
    g_zones[id] = {x, y, w, h};
    return id;
}

UIHitZoneID UI_Input_Hit_Test(int sx, int sy)
{
    // Last registered = topmost (reverse iteration)
    for (int i = g_zone_count - 1; i >= 0; i--) {
        const HitZone& z = g_zones[i];
        if (sx >= z.x && sy >= z.y && sx < z.x + z.w && sy < z.y + z.h) {
            return i;
        }
    }
    return UI_HIT_NONE;
}

UIHitZoneID UI_Input_Get_Hovered()  { return g_hovered; }
UIHitZoneID UI_Input_Get_Pressed()  { return g_pressed; }
bool        UI_Input_Has_Capture()  { return g_has_capture; }
bool        UI_Input_Was_Clicked(UIHitZoneID zone) { return zone != UI_HIT_NONE && zone == g_clicked; }
bool        UI_Input_Was_Right_Clicked(UIHitZoneID zone) { return zone != UI_HIT_NONE && zone == g_right_clicked; }

void UI_Input_Set_Scroll_Delta(float delta) { g_scroll_delta += delta; }
float UI_Input_Consume_Scroll_Delta() { float d = g_scroll_delta; g_scroll_delta = 0.0f; return d; }

void UI_Input_Update(int screen_x, int screen_y, bool left_down, bool right_down)
{
    g_hovered = UI_Input_Hit_Test(screen_x, screen_y);
    g_clicked = UI_HIT_NONE;  // only valid for one frame
    g_right_clicked = UI_HIT_NONE;

    if (left_down && !g_was_left_down) {
        // Mouse just pressed
        if (g_hovered != UI_HIT_NONE) {
            g_pressed = g_hovered;
            g_clicked = g_hovered;  // single-frame click edge
            g_has_capture = true;
        }
    } else if (!left_down && g_was_left_down) {
        // Mouse just released
        g_pressed = UI_HIT_NONE;
        g_has_capture = false;
    }

    if (right_down && !g_was_right_down) {
        if (g_hovered != UI_HIT_NONE) {
            g_right_clicked = g_hovered;
            g_has_capture = true;
        }
    } else if (!right_down && g_was_right_down) {
        if (!left_down) {
            g_has_capture = false;
        }
    }

    g_was_left_down = left_down;
    g_was_right_down = right_down;
}

void UI_Input_Push_Key(unsigned short key)
{
    int next = (g_key_tail + 1) % UI_KEY_BUF_SIZE;
    if (next != g_key_head) {  // drop if full
        g_key_buf[g_key_tail] = key;
        g_key_tail = next;
    }
}

unsigned short UI_Input_Pop_Key()
{
    if (g_key_head == g_key_tail) return 0;
    unsigned short k = g_key_buf[g_key_head];
    g_key_head = (g_key_head + 1) % UI_KEY_BUF_SIZE;
    return k;
}

void UI_Input_Begin_Frame()
{
    g_zone_count = 0;
    // Key buffer is NOT cleared here — keys arrive from SDL events asynchronously
    // and are consumed by UI_TextInput via UI_Input_Pop_Key().
    // Keep pressed/capture state across frame — only cleared on mouse release
}

void UI_Input_End_Frame()
{
    // Nothing needed yet — capture persists until release
}
