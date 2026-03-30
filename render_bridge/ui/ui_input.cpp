/**
 * ui_input.cpp — UI hit-testing and input capture implementation.
 */

#include "ui_input.h"

namespace {

struct HitZone {
    int x, y, w, h;
};

HitZone  g_zones[UI_MAX_HIT_ZONES];
int      g_zone_count    = 0;
UIHitZoneID g_hovered    = UI_HIT_NONE;
UIHitZoneID g_pressed    = UI_HIT_NONE;
bool     g_has_capture   = false;
bool     g_was_left_down = false;

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

void UI_Input_Update(int screen_x, int screen_y, bool left_down)
{
    g_hovered = UI_Input_Hit_Test(screen_x, screen_y);

    if (left_down && !g_was_left_down) {
        // Mouse just pressed
        if (g_hovered != UI_HIT_NONE) {
            g_pressed = g_hovered;
            g_has_capture = true;
        }
    } else if (!left_down && g_was_left_down) {
        // Mouse just released
        g_pressed = UI_HIT_NONE;
        g_has_capture = false;
    }

    g_was_left_down = left_down;
}

void UI_Input_Begin_Frame()
{
    g_zone_count = 0;
    // Keep pressed/capture state across frame — only cleared on mouse release
}

void UI_Input_End_Frame()
{
    // Nothing needed yet — capture persists until release
}
