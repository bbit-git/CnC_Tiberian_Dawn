/**
 * ui_input.cpp — UI hit-testing and input capture implementation.
 */

#include "ui_input.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

struct HitZone {
    int x, y, w, h;
    UIWidgetID widget_id;
};

struct MouseButtonLatch {
    bool down = false;
    bool pressed = false;
    bool released = false;
    int press_x = 0;
    int press_y = 0;
    int release_x = 0;
    int release_y = 0;
};

static constexpr int UI_KEY_BUF_SIZE = 64;
static constexpr int UI_ID_STACK_SIZE = 32;
static constexpr UIWidgetID UI_STABLE_WIDGET_MASK = 0x7fffffffu;
static constexpr UIWidgetID UI_EPHEMERAL_WIDGET_BIT = 0x80000000u;
unsigned short g_key_buf[UI_KEY_BUF_SIZE];
int g_key_head = 0;
int g_key_tail = 0;

HitZone  g_zones[UI_MAX_HIT_ZONES];
int      g_zone_count    = 0;
UIHitZoneID g_hovered    = UI_HIT_NONE;
UIWidgetID g_pressed_widget = UI_WIDGET_NONE;
UIWidgetID g_pressed_origin_widget = UI_WIDGET_NONE;
UIWidgetID g_clicked_widget = UI_WIDGET_NONE;
UIWidgetID g_right_pressed_origin_widget = UI_WIDGET_NONE;
UIWidgetID g_right_clicked_widget = UI_WIDGET_NONE;
bool     g_has_capture   = false;
float    g_scroll_delta  = 0.0f;
MouseButtonLatch g_left_button;
MouseButtonLatch g_right_button;
UIWidgetID g_id_stack[UI_ID_STACK_SIZE] = {};
int g_id_stack_size = 1;

UIWidgetID hash_bytes(UIWidgetID seed, const void* data, std::size_t size)
{
    const auto* bytes = static_cast<const unsigned char*>(data);
    UIWidgetID hash = seed;
    for (std::size_t i = 0; i < size; i++) {
        hash ^= static_cast<UIWidgetID>(bytes[i]);
        hash *= 16777619u;
    }

    hash &= UI_STABLE_WIDGET_MASK;
    if (hash == UI_WIDGET_NONE) {
        hash = 1;
    }

    return hash;
}

UIWidgetID make_ephemeral_widget_id(int zone_index)
{
    return UI_EPHEMERAL_WIDGET_BIT | static_cast<UIWidgetID>(zone_index + 1);
}

UIWidgetID zone_widget(UIHitZoneID zone)
{
    if (zone == UI_HIT_NONE || zone < 0 || zone >= g_zone_count) {
        return UI_WIDGET_NONE;
    }

    return g_zones[zone].widget_id;
}

UIHitZoneID find_zone_by_widget(UIWidgetID widget_id)
{
    if (widget_id == UI_WIDGET_NONE) {
        return UI_HIT_NONE;
    }

    for (int i = g_zone_count - 1; i >= 0; --i) {
        if (g_zones[i].widget_id == widget_id) {
            return i;
        }
    }

    return UI_HIT_NONE;
}

MouseButtonLatch& get_button_latch(bool right_button)
{
    return right_button ? g_right_button : g_left_button;
}

} // namespace

UIHitZoneID UI_Input_Register_Zone(int x, int y, int w, int h, UIWidgetID widget_id)
{
    if (g_zone_count >= UI_MAX_HIT_ZONES) return UI_HIT_NONE;
    int id = g_zone_count++;
    if (widget_id == UI_WIDGET_NONE) {
        widget_id = make_ephemeral_widget_id(id);
    }
    g_zones[id] = {x, y, w, h, widget_id};
    return id;
}

void UI_Input_Push_ID(int id)
{
    if (g_id_stack_size >= UI_ID_STACK_SIZE) {
        return;
    }

    g_id_stack[g_id_stack_size] = hash_bytes(g_id_stack[g_id_stack_size - 1], &id, sizeof(id));
    ++g_id_stack_size;
}

void UI_Input_Push_String_ID(const char* id)
{
    if (g_id_stack_size >= UI_ID_STACK_SIZE) {
        return;
    }

    const char* text = id ? id : "";
    g_id_stack[g_id_stack_size] = hash_bytes(g_id_stack[g_id_stack_size - 1],
                                             text, std::strlen(text));
    ++g_id_stack_size;
}

void UI_Input_Push_Pointer_ID(const void* ptr)
{
    if (g_id_stack_size >= UI_ID_STACK_SIZE) {
        return;
    }

    std::uintptr_t value = reinterpret_cast<std::uintptr_t>(ptr);
    g_id_stack[g_id_stack_size] = hash_bytes(g_id_stack[g_id_stack_size - 1],
                                             &value, sizeof(value));
    ++g_id_stack_size;
}

void UI_Input_Pop_ID()
{
    if (g_id_stack_size > 1) {
        --g_id_stack_size;
    }
}

UIWidgetID UI_Input_Make_Widget_ID(std::uint32_t local_salt)
{
    if (g_id_stack_size <= 1) {
        return UI_WIDGET_NONE;
    }

    UIWidgetID widget_id = g_id_stack[g_id_stack_size - 1];
    if (local_salt != 0) {
        widget_id = hash_bytes(widget_id, &local_salt, sizeof(local_salt));
    }

    return widget_id;
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
UIHitZoneID UI_Input_Get_Pressed()  { return find_zone_by_widget(g_pressed_widget); }
bool        UI_Input_Has_Capture()  { return g_has_capture; }
bool        UI_Input_Was_Clicked(UIHitZoneID zone) { return zone_widget(zone) != UI_WIDGET_NONE && zone_widget(zone) == g_clicked_widget; }
bool        UI_Input_Was_Right_Clicked(UIHitZoneID zone) { return zone_widget(zone) != UI_WIDGET_NONE && zone_widget(zone) == g_right_clicked_widget; }

void UI_Input_Set_Scroll_Delta(float delta) { g_scroll_delta += delta; }
float UI_Input_Consume_Scroll_Delta() { float d = g_scroll_delta; g_scroll_delta = 0.0f; return d; }

void UI_Input_On_Mouse_Button_Down(int screen_x, int screen_y, bool right_button)
{
    MouseButtonLatch& button = get_button_latch(right_button);
    if (!button.down) {
        button.pressed = true;
        button.press_x = screen_x;
        button.press_y = screen_y;
    }
    button.down = true;
}

void UI_Input_On_Mouse_Button_Up(int screen_x, int screen_y, bool right_button)
{
    MouseButtonLatch& button = get_button_latch(right_button);
    if (button.down) {
        button.released = true;
        button.release_x = screen_x;
        button.release_y = screen_y;
    }
    button.down = false;
}

void UI_Input_Update(int screen_x, int screen_y)
{
    g_hovered = UI_Input_Hit_Test(screen_x, screen_y);
    g_clicked_widget = UI_WIDGET_NONE;
    g_right_clicked_widget = UI_WIDGET_NONE;

    if (g_left_button.pressed) {
        UIHitZoneID press_zone = UI_Input_Hit_Test(g_left_button.press_x, g_left_button.press_y);
        g_pressed_origin_widget = zone_widget(press_zone);
        g_pressed_widget = g_pressed_origin_widget;
        if (g_pressed_origin_widget != UI_WIDGET_NONE) {
            g_has_capture = true;
        }
    }

    if (g_right_button.pressed) {
        UIHitZoneID press_zone = UI_Input_Hit_Test(g_right_button.press_x, g_right_button.press_y);
        g_right_pressed_origin_widget = zone_widget(press_zone);
        if (g_right_pressed_origin_widget != UI_WIDGET_NONE) {
            g_has_capture = true;
        }
    }

    if (g_left_button.released) {
        UIHitZoneID release_zone = UI_Input_Hit_Test(g_left_button.release_x, g_left_button.release_y);
        UIWidgetID release_widget = zone_widget(release_zone);
        if (g_pressed_origin_widget != UI_WIDGET_NONE &&
            release_widget == g_pressed_origin_widget) {
            g_clicked_widget = release_widget;
        }

        g_pressed_widget = UI_WIDGET_NONE;
        g_pressed_origin_widget = UI_WIDGET_NONE;
    }

    if (g_right_button.released) {
        UIHitZoneID release_zone = UI_Input_Hit_Test(g_right_button.release_x, g_right_button.release_y);
        UIWidgetID release_widget = zone_widget(release_zone);
        if (g_right_pressed_origin_widget != UI_WIDGET_NONE &&
            release_widget == g_right_pressed_origin_widget) {
            g_right_clicked_widget = release_widget;
        }

        g_right_pressed_origin_widget = UI_WIDGET_NONE;
    }

    g_has_capture =
        (g_pressed_origin_widget != UI_WIDGET_NONE && g_left_button.down) ||
        (g_right_pressed_origin_widget != UI_WIDGET_NONE && g_right_button.down);

    g_left_button.pressed = false;
    g_left_button.released = false;
    g_right_button.pressed = false;
    g_right_button.released = false;
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
    g_id_stack[0] = 2166136261u;
    g_id_stack_size = 1;
    // Key buffer is NOT cleared here — keys arrive from SDL events asynchronously
    // and are consumed by UI_TextInput via UI_Input_Pop_Key().
    // Keep pressed/capture state across frame — only cleared on mouse release
}

void UI_Input_End_Frame()
{
    // Nothing needed yet — capture persists until release
}
