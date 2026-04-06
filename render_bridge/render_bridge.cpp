/**
 * render_bridge.cpp ��� Compositor setup and layer management for TD.
 *
 * Reads game layout (TacPixelX/Y, SideX, SideBarWidth) and configures
 * compositor layers to match. The legacy Draw_It() chain and Blit_Display()
 * are unchanged — they still render into HidPage and copy to SeenBuff.
 *
 * The bridge reads SeenBuff (after Blit_Display), splits it into compositor
 * layers by screen region, and presents via the compositor path.
 */

#include "render_bridge.h"
#include "ui/ui_main_menu.h"
#include "function.h"
#include <cstdio>

static RenderCompositor g_compositor;
static bool             g_bridge_active = false;
static int              g_output_w = 0;
static int              g_output_h = 0;
static bool             g_ui_fonts_initialized = false;
static bool             g_dialog_active = false;
static UIDialogResult    g_dialog_result = UI_DIALOG_NONE;
static int              g_dialog_screen_w = 0;
static int              g_dialog_screen_h = 0;
static char             g_dialog_title[128] = {0};
static char             g_dialog_message[512] = {0};
static char             g_dialog_ok_label[32] = {0};
static char             g_dialog_cancel_label[32] = {0};
static char             g_dialog_extra_label[32] = {0};

// --- Game Options state (Phase 2) ---
static struct {
    bool     active;
    bool     show_restate;
    bool     is_multiplayer;
    int      result;
    // Localized button labels (set by game thread via Text_String)
    char     lbl_load[32];
    char     lbl_save[32];
    char     lbl_delete[32];
    char     lbl_resign[32];
    char     lbl_game[32];
    char     lbl_quit[32];
    char     lbl_resume[32];
    char     lbl_restate[32];
    char     lbl_title[32];
} g_game_options_state = {};

// --- Load dialog state (Phase 3) ---
static struct {
    bool        active;
    int         mode;           // 0=load 1=save 2=delete
    char        items[8][40];
    int         item_count;
    int         selected;
    float       scroll_pos;
    char        edit_text[40];
    int         result;         // 0=none 1=ok 2=cancel
    int         screen_w, screen_h;
    char        lbl_title[32];
    char        lbl_action[32];
    char        lbl_cancel[32];
} g_load_dialog_state = {};

// --- Game Controls state (Phase 4) ---
static struct {
    bool  active;
    float game_speed;
    float scroll_rate;
    int   result;
    int   screen_w, screen_h;
    char  lbl_title[32];
    char  lbl_speed[32];
    char  lbl_slower[32];
    char  lbl_faster[32];
    char  lbl_scrollrate[32];
    char  lbl_visual[32];
    char  lbl_sound[32];
    char  lbl_ok[32];
} g_game_controls_state = {};

// --- Visual Controls state (Phase 4) ---
static struct {
    bool  active;
    float brightness, color, contrast, tint;
    bool  hd_graphics;
    int   result;
    int   screen_w, screen_h;
    char  lbl_title[32];
    char  lbl_brightness[32];
    char  lbl_color[32];
    char  lbl_contrast[32];
    char  lbl_tint[32];
    char  lbl_hd_graphics[32];
    char  lbl_reset[32];
    char  lbl_ok[32];
} g_visual_controls_state = {};

static bool g_hd_graphics = true;

// --- Sound Controls state (Phase 4) ---
static struct {
    bool  active;
    float music_vol;
    float sfx_vol;
    int   track_count;
    char  track_names[32][64];
    int   selected_track;
    float track_scroll;
    bool  shuffle, repeat;
    bool  is_playing;
    int   result;
    int   screen_w, screen_h;
    char  lbl_title[32];
    char  lbl_music_vol[32];
    char  lbl_sound_vol[32];
    char  lbl_shuffle[32];
    char  lbl_repeat[32];
    char  lbl_on[16];
    char  lbl_off[16];
    char  lbl_play[16];
    char  lbl_stop[16];
    char  lbl_ok[32];
} g_sound_controls_state = {};

// --- Special Dialog state (Phase 5) ---
static struct {
    bool  active;
    bool  options[10];
    char  labels[10][48];
    int   option_count;
    int   result;
    int   screen_w, screen_h;
    char  lbl_title[32];
    char  lbl_ok[32];
    char  lbl_cancel[32];
} g_special_dialog_state = {};

// --- Expansion Dialog state (Phase 6) ---
static struct {
    bool  active;
    int   mode;          // 0=expansion, 1=bonus
    char  title[64];
    char  items[64][40];
    int   item_count;
    int   selected;
    float scroll_pos;
    int   result;
    int   screen_w, screen_h;
    char  lbl_ok[32];
    char  lbl_cancel[32];
} g_expansion_dialog_state = {};

// --- Com Scenario state (Phase 7) ---
static UIComScenarioState g_com_scenario_state = {};

// --- Main Menu state (Phase 8) ---
static UIMainMenuState g_main_menu_state = {};

// Palette LUT for 8-bit → RGBA conversion
static uint32_t         g_pal_lut[256];
static const uint8_t*   g_pal_lut_src = nullptr;
static uint32_t         g_pal_lut_hash = 0;

static void update_palette_lut(const uint8_t* pal)
{
    uint32_t hash = pal[0] | (pal[3] << 8) | (pal[384] << 16) | (pal[765] << 24);
    if (pal == g_pal_lut_src && hash == g_pal_lut_hash) return;

    for (int i = 0; i < 256; i++) {
        uint8_t r = pal[i * 3 + 0] << 2;
        uint8_t g = pal[i * 3 + 1] << 2;
        uint8_t b = pal[i * 3 + 2] << 2;
        g_pal_lut[i] = (0xFFu << 24) | (b << 16) | (g << 8) | r;
    }
    g_pal_lut_src = pal;
    g_pal_lut_hash = hash;
}

/// Copy a rectangular region from an 8-bit indexed buffer into an RGBA layer (1:1).
static void copy_indexed_region(
    const uint8_t* src, int src_pitch,
    int src_x, int src_y,
    uint8_t* dst_rgba, int dst_w, int dst_h)
{
    for (int row = 0; row < dst_h; row++) {
        const uint8_t* sp = src + (src_y + row) * src_pitch + src_x;
        uint32_t* dp = reinterpret_cast<uint32_t*>(dst_rgba + row * dst_w * 4);
        for (int col = 0; col < dst_w; col++) {
            dp[col] = g_pal_lut[sp[col]];
        }
    }
}

/// Copy a viewport sub-region with nearest-neighbor zoom into an RGBA layer.
/// viewport_x/y are source-space coordinates of the visible region's top-left.
/// zoom < 1.0 = zoomed out (source shrinks, black borders), zoom > 1.0 = zoomed in.
static void copy_indexed_zoomed(
    const uint8_t* src, int src_pitch,
    int src_origin_x, int src_origin_y,   // tactical area origin in SeenBuff
    int src_w, int src_h,                  // tactical area dimensions
    float viewport_x, float viewport_y,   // top-left of visible region in source space
    float zoom,
    uint8_t* dst_rgba, int dst_w, int dst_h)
{
    static constexpr uint32_t BLACK = 0xFF000000u;

    for (int row = 0; row < dst_h; row++) {
        uint32_t* dp = reinterpret_cast<uint32_t*>(dst_rgba + row * dst_w * 4);
        int sy = static_cast<int>(viewport_y + static_cast<float>(row) / zoom);

        if (sy < 0 || sy >= src_h) {
            for (int col = 0; col < dst_w; col++) dp[col] = BLACK;
            continue;
        }

        const uint8_t* sp = src + (src_origin_y + sy) * src_pitch + src_origin_x;

        for (int col = 0; col < dst_w; col++) {
            int sx = static_cast<int>(viewport_x + static_cast<float>(col) / zoom);
            if (sx < 0 || sx >= src_w) {
                dp[col] = BLACK;
            } else {
                dp[col] = g_pal_lut[sp[sx]];
            }
        }
    }
}

bool Render_Bridge_Init(int output_width, int output_height)
{
    g_output_w = output_width;
    g_output_h = output_height;

    g_compositor.Init(output_width, output_height);
    Render_Bridge_Update_Layout();

    g_bridge_active = true;
    return true;
}

void Render_Bridge_Shutdown()
{
    g_bridge_active = false;
}

void Render_Bridge_Update_Layout()
{
    Render_Bridge_Refresh_Layout();

    int tac_x, tac_y, tac_w, tac_h;
    Render_Bridge_Get_Tactical_Rect(tac_x, tac_y, tac_w, tac_h);

    // Clamp to output bounds
    if (tac_w > g_output_w - tac_x) tac_w = g_output_w - tac_x;
    if (tac_h > g_output_h - tac_y) tac_h = g_output_h - tac_y;

    int side_x, side_y, side_w, side_h;
    Render_Bridge_Get_Sidebar_Rect(side_x, side_y, side_w, side_h);

    int hdr_x, hdr_y, hdr_w, hdr_h;
    Render_Bridge_Get_Header_Rect(hdr_x, hdr_y, hdr_w, hdr_h);

    // World terrain layer — tactical area
    if (tac_w > 0 && tac_h > 0) {
        RenderLayerDesc world = {};
        world.id       = RenderLayerID::WORLD_TERRAIN;
        world.width    = tac_w;
        world.height   = tac_h;
        world.zoom     = 1.0f;
        world.offset_x = tac_x;
        world.offset_y = tac_y;
        world.dirty    = true;
        g_compositor.Configure_Layer(world);
    }

    // UI sidebar layer
    if (side_w > 0) {
        RenderLayerDesc sidebar = {};
        sidebar.id       = RenderLayerID::UI_SIDEBAR;
        sidebar.width    = side_w;
        sidebar.height   = side_h;
        sidebar.zoom     = 1.0f;
        sidebar.offset_x = side_x;
        sidebar.offset_y = side_y;
        sidebar.dirty    = true;
        g_compositor.Configure_Layer(sidebar);
    }

    // Tab bar — top strip above tactical area
    if (hdr_h > 0) {
        RenderLayerDesc tab = {};
        tab.id       = RenderLayerID::UI_TAB;
        tab.width    = hdr_w;
        tab.height   = hdr_h;
        tab.zoom     = 1.0f;
        tab.offset_x = hdr_x;
        tab.offset_y = hdr_y;
        tab.dirty    = true;
        g_compositor.Configure_Layer(tab);
    }
}

/// Passthrough: convert full SeenBuff to RGBA without compositor (menus, dialogs).
static void blit_passthrough()
{
    const uint8_t* seen = static_cast<const uint8_t*>(SeenBuff.Get_Buffer());
    if (!seen) return;
    int w = SeenBuff.Get_Width();
    int h = SeenBuff.Get_Height();
    int pitch = SeenBuff.Get_Full_Pitch();

    const uint8_t* pal = static_cast<const uint8_t*>((void*)Get_Palette());
    if (!pal) pal = GamePalette;
    if (!pal) return;
    update_palette_lut(pal);

    // Write directly to compositor output (full screen, 1:1)
    uint8_t* out = static_cast<uint8_t*>(
        const_cast<void*>(g_compositor.Get_Output()));
    int ow = g_compositor.Get_Output_Width();
    int oh = g_compositor.Get_Output_Height();
    if (!out || ow <= 0 || oh <= 0) return;

    int rows = (h < oh) ? h : oh;
    int cols = (w < ow) ? w : ow;
    for (int row = 0; row < rows; row++) {
        const uint8_t* sp = seen + row * pitch;
        uint32_t* dp = reinterpret_cast<uint32_t*>(out + row * ow * 4);
        for (int col = 0; col < cols; col++) {
            dp[col] = g_pal_lut[sp[col]];
        }
    }
}

void Render_Bridge_Blit_Display()
{
    if (!g_bridge_active) return;

    // Zoom is now applied in-place to HidPage by Render_Bridge_Zoom_Tactical()
    // (called from GScreenClass::Render between Draw_It and Buttons).
    // By the time we get here, SeenBuff already has the zoomed world + 1:1 UI.
    // Just passthrough.
    blit_passthrough();
}

void Render_Bridge_Set_Zoom(float zoom)
{
    // Zoom is applied during the SeenBuff → layer copy (copy_indexed_zoomed),
    // not by the compositor. The compositor always blits layers 1:1.
    (void)zoom;
}

float Render_Bridge_Get_Zoom()
{
    return g_compositor.Get_World_Zoom();
}

RenderCompositor* Render_Bridge_Get_Compositor()
{
    return &g_compositor;
}

// --- Bridge-native UI ---

#include "ui/ui_draw_list.h"
#include "ui/ui_text.h"
#include "ui/ui_text_sdf.h"
#include "ui/ui_input.h"

void Render_Bridge_UI_Begin_Frame()
{
    // UI font atlases are built lazily once the legacy font pointers are live.
    if (!g_ui_fonts_initialized) {
        Render_Bridge_UI_Init_Fonts();
        g_ui_fonts_initialized = true;
    }

    g_ui_draw_list.Clear();
    UI_Input_Begin_Frame();
}

void Render_Bridge_UI_End_Frame()
{
    // UI hit zones are registered during emission, so pointer state must be
    // refreshed after the draw list is built for the current frame.
    extern int g_mouse_x, g_mouse_y;
    UI_Input_Update(g_mouse_x, g_mouse_y, Key_Down(KN_LMOUSE) != 0);
    UI_Input_End_Frame();
}

void Render_Bridge_UI_Init_Fonts()
{
    // SDF system fonts (optional — fall back gracefully if no TTF found)
    const char* sans = SDF_Font_Discover("sans");
    if (sans) SDF_Build_Atlas(UI_FONT_SDF_DEFAULT, sans, 48, 6);

    const char* mono = SDF_Font_Discover("mono");
    if (mono) SDF_Build_Atlas(UI_FONT_SDF_MONO, mono, 48, 6);

    // Legacy FNT fonts (always loaded)
    if (Font6Ptr)   UI_Text_Build_Atlas(UI_FONT_6PT, Font6Ptr);
    if (FontPtr)    UI_Text_Build_Atlas(UI_FONT_8PT, FontPtr);
    if (FontLEDPtr) UI_Text_Build_Atlas(UI_FONT_LED, FontLEDPtr);
    if (VCRFontPtr) UI_Text_Build_Atlas(UI_FONT_VCR, VCRFontPtr);
}

void Render_Bridge_UI_Set_Font_Scale(float s)
{
    UI_Text_Set_Font_Scale(s);
}

float Render_Bridge_UI_Get_Font_Scale()
{
    return UI_Text_Get_Font_Scale();
}

bool Render_Bridge_UI_Has_Capture()
{
    return UI_Input_Has_Capture();
}

bool Render_Bridge_UI_Use_Native_Messages()
{
    return true;
}

bool Render_Bridge_UI_Use_Native_Help()
{
    return true;
}

bool Render_Bridge_UI_Has_Active_Dialog()
{
    return g_dialog_active;
}

void Render_Bridge_UI_Set_Dialog(const char* title,
                                 const char* message,
                                 const char* ok_label,
                                 const char* cancel_label,
                                 int screen_w, int screen_h,
                                 const char* extra_label)
{
    g_dialog_active = true;
    g_dialog_result = UI_DIALOG_NONE;
    g_dialog_screen_w = screen_w;
    g_dialog_screen_h = screen_h;
    std::snprintf(g_dialog_title, sizeof(g_dialog_title), "%s", title ? title : "");
    std::snprintf(g_dialog_message, sizeof(g_dialog_message), "%s", message ? message : "");
    std::snprintf(g_dialog_ok_label, sizeof(g_dialog_ok_label), "%s", ok_label ? ok_label : "");
    std::snprintf(g_dialog_cancel_label, sizeof(g_dialog_cancel_label), "%s", cancel_label ? cancel_label : "");
    std::snprintf(g_dialog_extra_label, sizeof(g_dialog_extra_label), "%s", extra_label ? extra_label : "");
}

void Render_Bridge_UI_Clear_Dialog(UIDialogResult result)
{
    g_dialog_active = false;
    g_dialog_result = result;
    g_dialog_title[0] = '\0';
    g_dialog_message[0] = '\0';
    g_dialog_ok_label[0] = '\0';
    g_dialog_cancel_label[0] = '\0';
    g_dialog_extra_label[0] = '\0';
}

UIDialogResult Render_Bridge_UI_Get_Dialog_Result()
{
    return g_dialog_result;
}

void Render_Bridge_UI_Get_Dialog(int& screen_w, int& screen_h,
                                 const char*& title,
                                 const char*& message,
                                 const char*& ok_label,
                                 const char*& cancel_label,
                                 const char*& extra_label)
{
    screen_w = g_dialog_screen_w;
    screen_h = g_dialog_screen_h;
    title = g_dialog_title;
    message = g_dialog_message;
    ok_label = g_dialog_ok_label[0] ? g_dialog_ok_label : nullptr;
    cancel_label = g_dialog_cancel_label[0] ? g_dialog_cancel_label : nullptr;
    extra_label = g_dialog_extra_label[0] ? g_dialog_extra_label : nullptr;
}

// ========== Game Options (Phase 2) ==========

static void copy_label(char* dst, int size, const char* src)
{
    std::snprintf(dst, size, "%s", src ? src : "");
}

void Render_Bridge_UI_Set_Game_Options(bool show_restate, bool is_multiplayer,
                                       const UIGameOptionsLabels& labels)
{
    g_game_options_state.active = true;
    g_game_options_state.show_restate = show_restate;
    g_game_options_state.is_multiplayer = is_multiplayer;
    g_game_options_state.result = UI_GOPTION_NONE;
    copy_label(g_game_options_state.lbl_title, 32, labels.title);
    copy_label(g_game_options_state.lbl_load, 32, labels.load);
    copy_label(g_game_options_state.lbl_save, 32, labels.save);
    copy_label(g_game_options_state.lbl_delete, 32, labels.del);
    copy_label(g_game_options_state.lbl_resign, 32, labels.del);
    copy_label(g_game_options_state.lbl_game, 32, labels.game);
    copy_label(g_game_options_state.lbl_quit, 32, labels.quit);
    copy_label(g_game_options_state.lbl_resume, 32, labels.resume);
    copy_label(g_game_options_state.lbl_restate, 32, labels.restate);
}

UIGameOptionsResult Render_Bridge_UI_Get_Game_Options_Result()
{
    return static_cast<UIGameOptionsResult>(g_game_options_state.result);
}

void Render_Bridge_UI_Clear_Game_Options()
{
    g_game_options_state.active = false;
    g_game_options_state.result = UI_GOPTION_NONE;
}

bool Render_Bridge_UI_Has_Active_Game_Options()
{
    return g_game_options_state.active;
}

bool Render_Bridge_UI_Game_Options_Get_Flags(bool& restate, bool& mp)
{
    restate = g_game_options_state.show_restate;
    mp = g_game_options_state.is_multiplayer;
    return g_game_options_state.active;
}

struct UIGameOptionsInternalLabels {
    const char* title;
    const char* load;
    const char* save;
    const char* del;
    const char* resign;
    const char* game;
    const char* quit;
    const char* resume;
    const char* restate;
};

void Render_Bridge_UI_Game_Options_Get_Labels(UIGameOptionsInternalLabels& out)
{
    out.title   = g_game_options_state.lbl_title;
    out.load    = g_game_options_state.lbl_load;
    out.save    = g_game_options_state.lbl_save;
    out.del     = g_game_options_state.lbl_delete;
    out.resign  = g_game_options_state.lbl_resign;
    out.game    = g_game_options_state.lbl_game;
    out.quit    = g_game_options_state.lbl_quit;
    out.resume  = g_game_options_state.lbl_resume;
    out.restate = g_game_options_state.lbl_restate;
}

void Render_Bridge_UI_Set_Game_Options_Result(int r)
{
    g_game_options_state.result = r;
    g_game_options_state.active = false;
}

// ========== Load Dialog (Phase 3) ==========

void Render_Bridge_UI_Set_Load_Dialog(UILoadMode mode, const char* items[], int count,
                                       int selected, const char* edit_text,
                                       int screen_w, int screen_h,
                                       const char* title, const char* action_label,
                                       const char* cancel_label)
{
    g_load_dialog_state.active = true;
    g_load_dialog_state.mode = static_cast<int>(mode);
    g_load_dialog_state.item_count = count > 8 ? 8 : count;
    for (int i = 0; i < g_load_dialog_state.item_count; i++) {
        std::snprintf(g_load_dialog_state.items[i], 40, "%s", items[i] ? items[i] : "");
    }
    g_load_dialog_state.selected = selected;
    g_load_dialog_state.scroll_pos = 0.0f;
    std::snprintf(g_load_dialog_state.edit_text, 40, "%s", edit_text ? edit_text : "");
    g_load_dialog_state.result = 0;
    g_load_dialog_state.screen_w = screen_w;
    g_load_dialog_state.screen_h = screen_h;
    copy_label(g_load_dialog_state.lbl_title, 32, title);
    copy_label(g_load_dialog_state.lbl_action, 32, action_label);
    copy_label(g_load_dialog_state.lbl_cancel, 32, cancel_label);
}

void Render_Bridge_UI_Update_Load_Dialog_Items(const char* items[], int count)
{
    g_load_dialog_state.item_count = count > 8 ? 8 : count;
    for (int i = 0; i < g_load_dialog_state.item_count; i++) {
        std::snprintf(g_load_dialog_state.items[i], 40, "%s", items[i] ? items[i] : "");
    }
}

void Render_Bridge_UI_Get_Load_Dialog_State(int& selected, float& scroll_pos,
                                             char* edit_text, int edit_size)
{
    selected = g_load_dialog_state.selected;
    scroll_pos = g_load_dialog_state.scroll_pos;
    if (edit_text && edit_size > 0) {
        std::snprintf(edit_text, edit_size, "%s", g_load_dialog_state.edit_text);
    }
}

int Render_Bridge_UI_Get_Load_Dialog_Result()
{
    return g_load_dialog_state.result;
}

void Render_Bridge_UI_Clear_Load_Dialog()
{
    g_load_dialog_state.active = false;
    g_load_dialog_state.result = 0;
}

bool Render_Bridge_UI_Has_Active_Load_Dialog()
{
    return g_load_dialog_state.active;
}

static const char* g_load_item_ptrs[8];

bool Render_Bridge_UI_Load_Dialog_Get_State(
    int& mode, const char**& items, int& count, int& selected,
    float& scroll_pos, const char*& edit_text, int& screen_w, int& screen_h,
    const char*& title, const char*& action_label, const char*& cancel_label)
{
    mode = g_load_dialog_state.mode;
    count = g_load_dialog_state.item_count;
    for (int i = 0; i < count; i++) {
        g_load_item_ptrs[i] = g_load_dialog_state.items[i];
    }
    items = g_load_item_ptrs;
    selected = g_load_dialog_state.selected;
    scroll_pos = g_load_dialog_state.scroll_pos;
    edit_text = g_load_dialog_state.edit_text;
    screen_w = g_load_dialog_state.screen_w;
    screen_h = g_load_dialog_state.screen_h;
    title = g_load_dialog_state.lbl_title;
    action_label = g_load_dialog_state.lbl_action;
    cancel_label = g_load_dialog_state.lbl_cancel;
    return g_load_dialog_state.active;
}

void Render_Bridge_UI_Load_Dialog_Set_Result(int result, int selected, float scroll_pos)
{
    g_load_dialog_state.selected = selected;
    g_load_dialog_state.scroll_pos = scroll_pos;
    if (result != 0) {
        g_load_dialog_state.result = result;
        g_load_dialog_state.active = false;
    }
}

// ========== Game Controls (Phase 4) ==========

void Render_Bridge_UI_Set_Game_Controls(float game_speed, float scroll_rate,
                                         int screen_w, int screen_h,
                                         const UIGameControlsLabels& labels)
{
    g_game_controls_state.active = true;
    g_game_controls_state.game_speed = game_speed;
    g_game_controls_state.scroll_rate = scroll_rate;
    g_game_controls_state.result = UI_GCTRL_NONE;
    g_game_controls_state.screen_w = screen_w;
    g_game_controls_state.screen_h = screen_h;
    copy_label(g_game_controls_state.lbl_title, 32, labels.title);
    copy_label(g_game_controls_state.lbl_speed, 32, labels.speed);
    copy_label(g_game_controls_state.lbl_slower, 32, labels.slower);
    copy_label(g_game_controls_state.lbl_faster, 32, labels.faster);
    copy_label(g_game_controls_state.lbl_scrollrate, 32, labels.scrollrate);
    copy_label(g_game_controls_state.lbl_visual, 32, labels.visual);
    copy_label(g_game_controls_state.lbl_sound, 32, labels.sound);
    copy_label(g_game_controls_state.lbl_ok, 32, labels.ok);
}

void Render_Bridge_UI_Get_Game_Controls_State(float& game_speed, float& scroll_rate)
{
    game_speed = g_game_controls_state.game_speed;
    scroll_rate = g_game_controls_state.scroll_rate;
}

int Render_Bridge_UI_Get_Game_Controls_Result()
{
    return g_game_controls_state.result;
}

void Render_Bridge_UI_Clear_Game_Controls()
{
    g_game_controls_state.active = false;
    g_game_controls_state.result = UI_GCTRL_NONE;
}

bool Render_Bridge_UI_Has_Active_Game_Controls()
{
    return g_game_controls_state.active;
}

struct UIGameControlsInternalLabels {
    const char* title;
    const char* speed;
    const char* slower;
    const char* faster;
    const char* scrollrate;
    const char* visual;
    const char* sound;
    const char* ok;
};

bool Render_Bridge_UI_Game_Controls_Get_Internal(
    float& speed, float& scroll, int& screen_w, int& screen_h,
    UIGameControlsInternalLabels& labels)
{
    speed = g_game_controls_state.game_speed;
    scroll = g_game_controls_state.scroll_rate;
    screen_w = g_game_controls_state.screen_w;
    screen_h = g_game_controls_state.screen_h;
    labels.title = g_game_controls_state.lbl_title;
    labels.speed = g_game_controls_state.lbl_speed;
    labels.slower = g_game_controls_state.lbl_slower;
    labels.faster = g_game_controls_state.lbl_faster;
    labels.scrollrate = g_game_controls_state.lbl_scrollrate;
    labels.visual = g_game_controls_state.lbl_visual;
    labels.sound = g_game_controls_state.lbl_sound;
    labels.ok = g_game_controls_state.lbl_ok;
    return g_game_controls_state.active;
}

void Render_Bridge_UI_Game_Controls_Set_Result(int r, float speed, float scroll)
{
    g_game_controls_state.game_speed = speed;
    g_game_controls_state.scroll_rate = scroll;
    if (r != UI_GCTRL_NONE) {
        g_game_controls_state.result = r;
        g_game_controls_state.active = false;
    }
}

// ========== Visual Controls (Phase 4) ==========

void Render_Bridge_UI_Set_Visual_Controls(float brightness, float color,
                                           float contrast, float tint,
                                           bool hd_graphics,
                                           int screen_w, int screen_h,
                                           const UIVisualControlsLabels& labels)
{
    g_visual_controls_state.active = true;
    g_visual_controls_state.brightness = brightness;
    g_visual_controls_state.color = color;
    g_visual_controls_state.contrast = contrast;
    g_visual_controls_state.tint = tint;
    g_visual_controls_state.hd_graphics = hd_graphics;
    g_visual_controls_state.result = 0;
    g_visual_controls_state.screen_w = screen_w;
    g_visual_controls_state.screen_h = screen_h;
    copy_label(g_visual_controls_state.lbl_title, 32, labels.title);
    copy_label(g_visual_controls_state.lbl_brightness, 32, labels.brightness);
    copy_label(g_visual_controls_state.lbl_color, 32, labels.color);
    copy_label(g_visual_controls_state.lbl_contrast, 32, labels.contrast);
    copy_label(g_visual_controls_state.lbl_tint, 32, labels.tint);
    copy_label(g_visual_controls_state.lbl_hd_graphics, 32, labels.hd_graphics);
    copy_label(g_visual_controls_state.lbl_reset, 32, labels.reset);
    copy_label(g_visual_controls_state.lbl_ok, 32, labels.ok);
}

void Render_Bridge_UI_Get_Visual_Controls_State(float& brightness, float& color,
                                                 float& contrast, float& tint,
                                                 bool& hd_graphics)
{
    brightness = g_visual_controls_state.brightness;
    color = g_visual_controls_state.color;
    contrast = g_visual_controls_state.contrast;
    tint = g_visual_controls_state.tint;
    hd_graphics = g_visual_controls_state.hd_graphics;
}

int Render_Bridge_UI_Get_Visual_Controls_Result()
{
    return g_visual_controls_state.result;
}

void Render_Bridge_UI_Clear_Visual_Controls()
{
    g_visual_controls_state.active = false;
    g_visual_controls_state.result = 0;
}

bool Render_Bridge_UI_Has_Active_Visual_Controls()
{
    return g_visual_controls_state.active;
}

struct UIVisualControlsInternalLabels {
    const char* title;
    const char* brightness;
    const char* color;
    const char* contrast;
    const char* tint;
    const char* hd_graphics;
    const char* reset;
    const char* ok;
};

bool Render_Bridge_UI_Visual_Controls_Get_Internal(
    float& brightness, float& color, float& contrast, float& tint,
    bool& hd_graphics,
    int& screen_w, int& screen_h, UIVisualControlsInternalLabels& labels)
{
    brightness = g_visual_controls_state.brightness;
    color = g_visual_controls_state.color;
    contrast = g_visual_controls_state.contrast;
    tint = g_visual_controls_state.tint;
    hd_graphics = g_visual_controls_state.hd_graphics;
    screen_w = g_visual_controls_state.screen_w;
    screen_h = g_visual_controls_state.screen_h;
    labels.title = g_visual_controls_state.lbl_title;
    labels.brightness = g_visual_controls_state.lbl_brightness;
    labels.color = g_visual_controls_state.lbl_color;
    labels.contrast = g_visual_controls_state.lbl_contrast;
    labels.tint = g_visual_controls_state.lbl_tint;
    labels.hd_graphics = g_visual_controls_state.lbl_hd_graphics;
    labels.reset = g_visual_controls_state.lbl_reset;
    labels.ok = g_visual_controls_state.lbl_ok;
    return g_visual_controls_state.active;
}

void Render_Bridge_UI_Visual_Controls_Set_Result(
    int r, float brightness, float color, float contrast, float tint,
    bool hd_graphics)
{
    g_visual_controls_state.brightness = brightness;
    g_visual_controls_state.color = color;
    g_visual_controls_state.contrast = contrast;
    g_visual_controls_state.tint = tint;
    g_visual_controls_state.hd_graphics = hd_graphics;
    Render_Bridge_Set_HD_Graphics(hd_graphics);
    if (r != 0) {
        g_visual_controls_state.result = r;
        g_visual_controls_state.active = false;
    }
}

void Render_Bridge_Set_HD_Graphics(bool enabled)
{
    if (g_hd_graphics == enabled) {
        return;
    }
    g_hd_graphics = enabled;
    Render_Bridge_Invalidate_HD_Sprite_Atlas();
}

bool Render_Bridge_Get_HD_Graphics()
{
    return g_hd_graphics;
}

// ========== Sound Controls (Phase 4) ==========

void Render_Bridge_UI_Set_Sound_Controls(float music_vol, float sfx_vol,
                                          const char* track_names[], int track_count,
                                          int selected_track,
                                          bool shuffle, bool repeat, bool is_playing,
                                          int screen_w, int screen_h,
                                          const UISoundControlsLabels& labels)
{
    g_sound_controls_state.active = true;
    g_sound_controls_state.music_vol = music_vol;
    g_sound_controls_state.sfx_vol = sfx_vol;
    g_sound_controls_state.track_count = track_count > 32 ? 32 : track_count;
    for (int i = 0; i < g_sound_controls_state.track_count; i++) {
        std::snprintf(g_sound_controls_state.track_names[i], 64, "%s",
                      track_names[i] ? track_names[i] : "");
    }
    g_sound_controls_state.selected_track = selected_track;
    g_sound_controls_state.track_scroll = 0.0f;
    g_sound_controls_state.shuffle = shuffle;
    g_sound_controls_state.repeat = repeat;
    g_sound_controls_state.is_playing = is_playing;
    g_sound_controls_state.result = UI_SND_NONE;
    g_sound_controls_state.screen_w = screen_w;
    g_sound_controls_state.screen_h = screen_h;
    copy_label(g_sound_controls_state.lbl_title, 32, labels.title);
    copy_label(g_sound_controls_state.lbl_music_vol, 32, labels.music_vol);
    copy_label(g_sound_controls_state.lbl_sound_vol, 32, labels.sound_vol);
    copy_label(g_sound_controls_state.lbl_shuffle, 32, labels.shuffle);
    copy_label(g_sound_controls_state.lbl_repeat, 32, labels.repeat);
    copy_label(g_sound_controls_state.lbl_on, 16, labels.on);
    copy_label(g_sound_controls_state.lbl_off, 16, labels.off);
    copy_label(g_sound_controls_state.lbl_play, 16, labels.play);
    copy_label(g_sound_controls_state.lbl_stop, 16, labels.stop);
    copy_label(g_sound_controls_state.lbl_ok, 32, labels.ok);
}

void Render_Bridge_UI_Get_Sound_Controls_State(float& music_vol, float& sfx_vol,
                                                int& selected_track, float& track_scroll,
                                                bool& shuffle, bool& repeat)
{
    music_vol = g_sound_controls_state.music_vol;
    sfx_vol = g_sound_controls_state.sfx_vol;
    selected_track = g_sound_controls_state.selected_track;
    track_scroll = g_sound_controls_state.track_scroll;
    shuffle = g_sound_controls_state.shuffle;
    repeat = g_sound_controls_state.repeat;
}

int Render_Bridge_UI_Get_Sound_Controls_Result()
{
    return g_sound_controls_state.result;
}

void Render_Bridge_UI_Clear_Sound_Controls()
{
    g_sound_controls_state.active = false;
    g_sound_controls_state.result = UI_SND_NONE;
}

bool Render_Bridge_UI_Has_Active_Sound_Controls()
{
    return g_sound_controls_state.active;
}

static const char* g_sound_track_ptrs[32];

struct UISoundControlsInternalLabels {
    const char* title;
    const char* music_vol;
    const char* sound_vol;
    const char* shuffle;
    const char* repeat;
    const char* on;
    const char* off;
    const char* play;
    const char* stop;
    const char* ok;
};

bool Render_Bridge_UI_Sound_Controls_Get_Internal(
    float& music_vol, float& sfx_vol, const char**& tracks, int& track_count,
    int& selected, float& scroll, bool& shuffle, bool& repeat, bool& playing,
    int& screen_w, int& screen_h, UISoundControlsInternalLabels& labels)
{
    music_vol = g_sound_controls_state.music_vol;
    sfx_vol = g_sound_controls_state.sfx_vol;
    track_count = g_sound_controls_state.track_count;
    for (int i = 0; i < track_count; i++) {
        g_sound_track_ptrs[i] = g_sound_controls_state.track_names[i];
    }
    tracks = g_sound_track_ptrs;
    selected = g_sound_controls_state.selected_track;
    scroll = g_sound_controls_state.track_scroll;
    shuffle = g_sound_controls_state.shuffle;
    repeat = g_sound_controls_state.repeat;
    playing = g_sound_controls_state.is_playing;
    screen_w = g_sound_controls_state.screen_w;
    screen_h = g_sound_controls_state.screen_h;
    labels.title = g_sound_controls_state.lbl_title;
    labels.music_vol = g_sound_controls_state.lbl_music_vol;
    labels.sound_vol = g_sound_controls_state.lbl_sound_vol;
    labels.shuffle = g_sound_controls_state.lbl_shuffle;
    labels.repeat = g_sound_controls_state.lbl_repeat;
    labels.on = g_sound_controls_state.lbl_on;
    labels.off = g_sound_controls_state.lbl_off;
    labels.play = g_sound_controls_state.lbl_play;
    labels.stop = g_sound_controls_state.lbl_stop;
    labels.ok = g_sound_controls_state.lbl_ok;
    return g_sound_controls_state.active;
}

void Render_Bridge_UI_Sound_Controls_Update(
    int result, float music_vol, float sfx_vol, int selected, float scroll,
    bool shuffle, bool repeat)
{
    g_sound_controls_state.music_vol = music_vol;
    g_sound_controls_state.sfx_vol = sfx_vol;
    g_sound_controls_state.selected_track = selected;
    g_sound_controls_state.track_scroll = scroll;
    g_sound_controls_state.shuffle = shuffle;
    g_sound_controls_state.repeat = repeat;
    if (result != UI_SND_NONE) {
        g_sound_controls_state.result = result;
        if (result == UI_SND_OK) {
            g_sound_controls_state.active = false;
        }
    }
}

// ========== Special Dialog (Phase 5) ==========

void Render_Bridge_UI_Set_Special_Dialog(const bool options[10],
                                          const char* labels[10],
                                          int screen_w, int screen_h,
                                          const char* title,
                                          const char* ok_label,
                                          const char* cancel_label)
{
    g_special_dialog_state.active = true;
    g_special_dialog_state.option_count = 10;
    for (int i = 0; i < 10; i++) {
        g_special_dialog_state.options[i] = options[i];
        std::snprintf(g_special_dialog_state.labels[i], 48, "%s",
                      labels[i] ? labels[i] : "");
    }
    g_special_dialog_state.result = 0;
    g_special_dialog_state.screen_w = screen_w;
    g_special_dialog_state.screen_h = screen_h;
    copy_label(g_special_dialog_state.lbl_title, 32, title);
    copy_label(g_special_dialog_state.lbl_ok, 32, ok_label);
    copy_label(g_special_dialog_state.lbl_cancel, 32, cancel_label);
}

void Render_Bridge_UI_Get_Special_Dialog_State(bool options[10])
{
    for (int i = 0; i < 10; i++) {
        options[i] = g_special_dialog_state.options[i];
    }
}

int Render_Bridge_UI_Get_Special_Dialog_Result()
{
    return g_special_dialog_state.result;
}

void Render_Bridge_UI_Clear_Special_Dialog()
{
    g_special_dialog_state.active = false;
    g_special_dialog_state.result = 0;
}

bool Render_Bridge_UI_Has_Active_Special_Dialog()
{
    return g_special_dialog_state.active;
}

bool Render_Bridge_UI_Special_Dialog_Get_Internal(
    bool options[10], const char* labels[10], int& count,
    int& screen_w, int& screen_h,
    const char*& title, const char*& ok_label, const char*& cancel_label)
{
    count = g_special_dialog_state.option_count;
    for (int i = 0; i < count; i++) {
        options[i] = g_special_dialog_state.options[i];
        labels[i] = g_special_dialog_state.labels[i];
    }
    screen_w = g_special_dialog_state.screen_w;
    screen_h = g_special_dialog_state.screen_h;
    title = g_special_dialog_state.lbl_title;
    ok_label = g_special_dialog_state.lbl_ok;
    cancel_label = g_special_dialog_state.lbl_cancel;
    return g_special_dialog_state.active;
}

void Render_Bridge_UI_Special_Dialog_Set_Result(int r, const bool options[10])
{
    for (int i = 0; i < 10; i++) {
        g_special_dialog_state.options[i] = options[i];
    }
    if (r != 0) {
        g_special_dialog_state.result = r;
        g_special_dialog_state.active = false;
    }
}

// ========== Expansion Dialog (Phase 6) ==========

void Render_Bridge_UI_Set_Expansion_Dialog(int mode, const char* title,
                                            const char* items[], int count,
                                            int screen_w, int screen_h,
                                            const char* ok_label,
                                            const char* cancel_label)
{
    g_expansion_dialog_state.active = true;
    g_expansion_dialog_state.mode = mode;
    std::snprintf(g_expansion_dialog_state.title, 64, "%s", title ? title : "");
    g_expansion_dialog_state.item_count = count > 64 ? 64 : count;
    for (int i = 0; i < g_expansion_dialog_state.item_count; i++) {
        std::snprintf(g_expansion_dialog_state.items[i], 40, "%s",
                      items[i] ? items[i] : "");
    }
    g_expansion_dialog_state.selected = 0;
    g_expansion_dialog_state.scroll_pos = 0.0f;
    g_expansion_dialog_state.result = 0;
    g_expansion_dialog_state.screen_w = screen_w;
    g_expansion_dialog_state.screen_h = screen_h;
    copy_label(g_expansion_dialog_state.lbl_ok, 32, ok_label);
    copy_label(g_expansion_dialog_state.lbl_cancel, 32, cancel_label);
}

void Render_Bridge_UI_Get_Expansion_Dialog_State(int& selected, float& scroll_pos)
{
    selected = g_expansion_dialog_state.selected;
    scroll_pos = g_expansion_dialog_state.scroll_pos;
}

int Render_Bridge_UI_Get_Expansion_Dialog_Result()
{
    return g_expansion_dialog_state.result;
}

void Render_Bridge_UI_Clear_Expansion_Dialog()
{
    g_expansion_dialog_state.active = false;
    g_expansion_dialog_state.result = 0;
}

bool Render_Bridge_UI_Has_Active_Expansion_Dialog()
{
    return g_expansion_dialog_state.active;
}

static const char* g_expansion_item_ptrs[64];

bool Render_Bridge_UI_Expansion_Dialog_Get_Internal(
    int& mode, const char*& title, const char**& items, int& count,
    int& selected, float& scroll, int& screen_w, int& screen_h,
    const char*& ok_label, const char*& cancel_label)
{
    mode = g_expansion_dialog_state.mode;
    title = g_expansion_dialog_state.title;
    count = g_expansion_dialog_state.item_count;
    for (int i = 0; i < count; i++) {
        g_expansion_item_ptrs[i] = g_expansion_dialog_state.items[i];
    }
    items = g_expansion_item_ptrs;
    selected = g_expansion_dialog_state.selected;
    scroll = g_expansion_dialog_state.scroll_pos;
    screen_w = g_expansion_dialog_state.screen_w;
    screen_h = g_expansion_dialog_state.screen_h;
    ok_label = g_expansion_dialog_state.lbl_ok;
    cancel_label = g_expansion_dialog_state.lbl_cancel;
    return g_expansion_dialog_state.active;
}

void Render_Bridge_UI_Expansion_Dialog_Set_Result(int r, int selected, float scroll)
{
    g_expansion_dialog_state.selected = selected;
    g_expansion_dialog_state.scroll_pos = scroll;
    if (r != 0) {
        g_expansion_dialog_state.result = r;
        g_expansion_dialog_state.active = false;
    }
}

// ========== Com Scenario Dialog (Phase 7) ==========

void Render_Bridge_UI_Set_Com_Scenario(const UIComScenarioState& state)
{
    g_com_scenario_state = state;
    g_com_scenario_state.active = true;
    g_com_scenario_state.result = 0;
}

void Render_Bridge_UI_Get_Com_Scenario_State(UIComScenarioState& state)
{
    state = g_com_scenario_state;
}

int Render_Bridge_UI_Get_Com_Scenario_Result()
{
    return g_com_scenario_state.result;
}

void Render_Bridge_UI_Clear_Com_Scenario()
{
    g_com_scenario_state.active = false;
    g_com_scenario_state.result = 0;
}

bool Render_Bridge_UI_Has_Any_Active_Dialog()
{
    return Render_Bridge_UI_Has_Active_Dialog() ||
           Render_Bridge_UI_Has_Active_Game_Options() ||
           Render_Bridge_UI_Has_Active_Load_Dialog() ||
           Render_Bridge_UI_Has_Active_Game_Controls() ||
           Render_Bridge_UI_Has_Active_Visual_Controls() ||
           Render_Bridge_UI_Has_Active_Sound_Controls() ||
           Render_Bridge_UI_Has_Active_Special_Dialog() ||
           Render_Bridge_UI_Has_Active_Expansion_Dialog() ||
           Render_Bridge_UI_Has_Active_Com_Scenario() ||
           Render_Bridge_UI_Has_Active_Main_Menu();
}

bool Render_Bridge_UI_Has_Active_Com_Scenario()
{
    return g_com_scenario_state.active;
}

bool Render_Bridge_UI_Com_Scenario_Get_Internal(UIComScenarioState& state)
{
    state = g_com_scenario_state;
    return g_com_scenario_state.active;
}

void Render_Bridge_UI_Com_Scenario_Update(const UIComScenarioState& state)
{
    g_com_scenario_state = state;
}

// ========== Main Menu (Phase 8) ==========

void Render_Bridge_UI_Set_Main_Menu(const UIMainMenuState& state)
{
    g_main_menu_state = state;
    g_main_menu_state.active = true;
    g_main_menu_state.result = 0;
    g_main_menu_state.fade_alpha = 0.0f;
    g_main_menu_state.dismissing = false;
    g_main_menu_state.pending_result = 0;
    g_main_menu_state.had_input = false;
}

int Render_Bridge_UI_Get_Main_Menu_Result()
{
    return g_main_menu_state.result;
}

void Render_Bridge_UI_Clear_Main_Menu()
{
    g_main_menu_state.active = false;
    g_main_menu_state.result = 0;
}

bool Render_Bridge_UI_Has_Active_Main_Menu()
{
    return g_main_menu_state.active;
}

bool Render_Bridge_UI_Main_Menu_Get_Internal(int& screen_w, int& screen_h,
                                              int& page, int& focused_button,
                                              float& fade_alpha, bool& dismissing,
                                              UIMainMenuInternalLabels& lbl)
{
    screen_w = g_main_menu_state.screen_w;
    screen_h = g_main_menu_state.screen_h;
    page = g_main_menu_state.page;
    focused_button = g_main_menu_state.focused_button;
    fade_alpha = g_main_menu_state.fade_alpha;
    dismissing = g_main_menu_state.dismissing;
    lbl.new_game       = g_main_menu_state.lbl_new_game;
    lbl.load_game      = g_main_menu_state.lbl_load_game;
    lbl.skirmish       = g_main_menu_state.lbl_skirmish;
    lbl.options        = g_main_menu_state.lbl_options;
    lbl.exit           = g_main_menu_state.lbl_exit;
    lbl.gdi            = g_main_menu_state.lbl_gdi;
    lbl.nod            = g_main_menu_state.lbl_nod;
    lbl.back           = g_main_menu_state.lbl_back;
    lbl.campaign_title = g_main_menu_state.lbl_campaign_title;
    lbl.expansion      = g_main_menu_state.lbl_expansion;
    lbl.has_expansion  = g_main_menu_state.has_expansion;
    lbl.version        = g_main_menu_state.lbl_version;
    lbl.copyright      = g_main_menu_state.lbl_copyright;
    return g_main_menu_state.active;
}

void Render_Bridge_UI_Set_Main_Menu_Result(int r)
{
    g_main_menu_state.result = r;
}

void Render_Bridge_UI_Main_Menu_Begin_Dismiss(int result_code)
{
    g_main_menu_state.dismissing = true;
    g_main_menu_state.pending_result = result_code;
}

void Render_Bridge_UI_Main_Menu_Commit_Dismiss()
{
    g_main_menu_state.result = g_main_menu_state.pending_result;
}

void Render_Bridge_UI_Main_Menu_Set_Page(int page)
{
    g_main_menu_state.page = page;
    g_main_menu_state.focused_button = 0;
    g_main_menu_state.fade_alpha = 0.3f;  // crossfade from partial
}

void Render_Bridge_UI_Main_Menu_Set_Focus(int focused)
{
    g_main_menu_state.focused_button = focused;
}

void Render_Bridge_UI_Main_Menu_Set_Fade(float alpha)
{
    g_main_menu_state.fade_alpha = alpha;
}

void Render_Bridge_UI_Main_Menu_Signal_Input()
{
    g_main_menu_state.had_input = true;
}

bool Render_Bridge_UI_Main_Menu_Had_Input()
{
    bool v = g_main_menu_state.had_input;
    g_main_menu_state.had_input = false;
    return v;
}
