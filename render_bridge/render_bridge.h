/**
 * render_bridge.h — Bridge between legacy engine rendering and libs/render/.
 *
 * Provides compositor-based layer separation for the existing Draw_It() chain.
 * The legacy code continues to render into HidPage as before; the bridge
 * splits HidPage into compositor layers (world, sidebar, radar) and presents
 * via the new rendering pipeline.
 *
 * Build with USE_RENDER_BRIDGE defined to activate.
 * Without it, the legacy Blit_Display → TD_SDL_Present path is used.
 */

#ifndef CNC_RENDER_BRIDGE_H
#define CNC_RENDER_BRIDGE_H

#include "render_compositor.h"
#include "render_layer.h"
#include "ui/ui_dialog.h"
#include "ui/ui_main_menu_options.h"

/// Palette index used as transparency key for HD terrain holes in the native
/// tactical buffer. The tactical shader discards this index so that HD terrain
/// tiles drawn underneath show through.
static constexpr int TERRAIN_KEY_INDEX = 254;

/// Initialize the render bridge. Call once after game viewport is configured.
bool Render_Bridge_Init(int output_width, int output_height);

/// Set screen pixel dimensions for zoom range and native tactical calculation.
void Render_Bridge_Set_Screen_Size(int screen_w, int screen_h);

/// Get stored screen pixel dimensions.
void Render_Bridge_Get_Screen_Size(int& w, int& h);

/// Shut down and free bridge resources.
void Render_Bridge_Shutdown();

/// Update layer geometry from current game state (sidebar position, tactical area).
/// Call when viewport dimensions change (e.g. sidebar toggle, resolution change).
void Render_Bridge_Update_Layout();

/// Copy HidPage regions into compositor layers, composite, and present.
/// Replaces GScreenClass::Blit_Display() in the render bridge build.
void Render_Bridge_Blit_Display();

/// Present the composited frame to the window.
/// Replaces TD_SDL_Present() in the render bridge build.
void Render_Bridge_Present();

/// Set world zoom factor (1.0 = normal). Applies to tactical area only.
void Render_Bridge_Set_Zoom(float zoom);

/// Get current world zoom factor.
float Render_Bridge_Get_Zoom();

/// Get the compositor (for direct layer access if needed).
RenderCompositor* Render_Bridge_Get_Compositor();

/// Apply accumulated mouse scroll zoom delta (centered on cursor). Call once per frame.
void Render_Bridge_Apply_Scroll_Zoom();

/// Refresh g_mouse_x/y from raw tactical screen space for bridge input handling.
/// Call after Apply_Scroll_Zoom, before the game reads mouse position.
void Render_Bridge_Transform_Mouse();

/// Map a tactical screen-space point into the bridge-visible source space.
bool Render_Bridge_Map_Tactical_Point(int screen_x, int screen_y, int& mapped_x, int& mapped_y);

/// Record the requested tactical world origin before legacy native-size clamping.
void Render_Bridge_Record_Tactical_Request(int x, int y);

/// Record tactical world origin only if no explicit bridge request is pending.
void Render_Bridge_Record_Tactical_Request_Fallback(int x, int y);

/// Get the requested visible world origin in map-relative pixels.
void Render_Bridge_Get_Requested_Tactical_Position(int& x, int& y);

/// Zoom to a specific level centered on a screen-space point (for pinch zoom).
void Render_Bridge_Zoom_At(float new_zoom, int center_x, int center_y);

/// Current zoom level (1.0 = normal, >1.0 = zoomed in).
float Render_Bridge_Get_Zoom_Level();

/// Get the startup/default zoom level derived from the current setup.
float Render_Bridge_Get_Default_Zoom();

/// Viewport offset in source tactical pixels (top-left of visible region).
float Render_Bridge_Get_Viewport_X();
float Render_Bridge_Get_Viewport_Y();

/// Visible region size in native tactical pixels after zoom/aspect fitting.
void Render_Bridge_Get_Visible_Size(float& w, float& h);

/// Get the native tactical replay size in lepton space for bridge-aware clamps.
void Render_Bridge_Get_Visible_Size_Leptons(int& w, int& h);

/// Dump current render-bridge HUD stats to the debug log.
void Render_Bridge_Debug_Dump();

/// Toggle the render-bridge debug HUD visibility.
void Render_Bridge_Debug_HUD_Toggle();

/// Return whether the render-bridge debug HUD is visible.
bool Render_Bridge_Debug_HUD_Enabled();

/// Toggle the render-bridge debug overlay bars and boxes visibility.
void Render_Bridge_Debug_Bars_Toggle();

/// Return whether the render-bridge debug overlay bars and boxes are visible.
bool Render_Bridge_Debug_Bars_Enabled();

/// Register an optional HD sprite provider for the tactical sprite batch.
/// Pass `nullptr` to clear it and fall back to legacy indexed sprites only.
void Render_Bridge_Register_HD_Sprite_Provider(void* provider);

/// Get the registered HD sprite provider, or nullptr if none.
void* Render_Bridge_Get_HD_Sprite_Provider();

/// Set the terrain type hash (FNV-1a of template IniName) for the next Draw_Stamp.
/// Must be called immediately before the Draw_Stamp call from CellClass::Draw_It.
/// The hook consumes and resets this value; stamps without a prior call get hash=0.
void Render_Bridge_Set_Stamp_Terrain_Hash(uint32_t hash);

/// Notify the bridge that the game theater has changed.
/// Re-initializes the HD terrain tile provider and invalidates the terrain atlas.
void Render_Bridge_Set_Theater(const char* theater_name);

struct UIMainMenuOptionsState {
    bool   active;
    int    result;           // UIMainMenuOptionsResult
    bool   hd_graphics;
    int    screen_w, screen_h;
    char   lbl_title[48];
    char   lbl_audio[32];
    char   lbl_video[32];
    char   lbl_back[32];
    char   lbl_hd[32];
    char   lbl_legacy[32];
};

/// Register a stable entity hash for a shapefile pointer.
void Render_Bridge_Register_Shape_Identity(const void* shapefile, uint32_t entity_hash);

/// Look up a previously registered entity hash for a shapefile pointer.
uint32_t Render_Bridge_Get_Shape_Identity(const void* shapefile);

/// Toggle the SeenBuff chrome sidebar on/off at runtime.
void Render_Bridge_Chrome_Toggle();

/// Toggle the render-source visualization overlay.
void Render_Bridge_Debug_Sources_Toggle();

/// Return whether the render-source visualization overlay is visible.
bool Render_Bridge_Debug_Sources_Enabled();

/// Returns true when scroll position clamping is bypassed (debug).
bool Render_Bridge_Debug_No_Scroll_Clamp();

/// Returns true when viewport clamping inside the native buffer is bypassed (debug).
bool Render_Bridge_Debug_No_VP_Clamp();

/// Returns true when GL sprite screen-space culling is bypassed (debug).
bool Render_Bridge_Debug_No_GL_Cull();

/// Returns true when cell-in-view culling always returns visible (debug).
bool Render_Bridge_Debug_No_Cell_Cull();

/// Get the last proportional viewport target in native tactical pixels.
void Render_Bridge_Get_Viewport_Target(float& x, float& y);

/// Get the last TacticalCoord delta in pixels.
void Render_Bridge_Get_Scroll_Delta(int& x, int& y);

// --- Bridge-owned screen layout (Phase 1: Geometry Authority) ---
// The bridge caches and owns the logical screen model. Callers must use these
// accessors instead of reading Map.TacPixelX, Map.SideX, WindowList, etc.

/// Refresh the cached screen layout from current game state.
/// Called automatically by Apply_Scroll_Zoom and Set_Screen_Size.
void Render_Bridge_Refresh_Layout();

/// Logical screen size in game-buffer pixels (SeenBuff dimensions).
void Render_Bridge_Get_Logical_Screen_Size(int& w, int& h);

/// Header/tab rect in game-buffer pixels (strip above the tactical area).
void Render_Bridge_Get_Header_Rect(int& x, int& y, int& w, int& h);

/// Sidebar rect in game-buffer pixels.
void Render_Bridge_Get_Sidebar_Rect(int& x, int& y, int& w, int& h);

/// Tactical screen rect in game-buffer pixels.
/// This is the authoritative tactical area — callers must not recompute from Map.*.
void Render_Bridge_Get_Tactical_Rect(int& x, int& y, int& w, int& h);

/// Render tactical rect — sidebar-independent.
/// Always spans the full content width below the header regardless of sidebar state.
/// Used for GL world presentation so toggling the sidebar does not change render
/// scale, visible size, or zoom behaviour.
void Render_Bridge_Get_Render_Tactical_Rect(int& x, int& y, int& w, int& h);

/// Tactical record rect in legacy buffer pixels used while capturing draw-list commands.
void Render_Bridge_Get_Record_Tactical_Rect(int& x, int& y, int& w, int& h);

/// Record a solid-black shroud fill rect with native-buffer-relative coordinates and
/// LAYER_SHADOW so the dedicated GL shroud pass can composite it after HD sprites.
/// Returns true when recording in bridge mode; caller falls back to
/// LogicPage->Fill_Rect only when false (CPU path).
bool Render_Bridge_Record_Shroud_Fill_Rect(int x, int y, int w, int h);

/// Mouse input rect in game-buffer pixels (area where clicks become tactical actions).
void Render_Bridge_Get_Mouse_Input_Rect(int& x, int& y, int& w, int& h);

/// Native world replay texture size in pixels.
void Render_Bridge_Get_Native_World_Rect(int& w, int& h);

/// Visible world rect in leptons (origin + size of what the bridge shows).
/// Origin is map-relative (0,0 = top-left of playable map).
void Render_Bridge_Get_Visible_World_Rect(int& origin_x, int& origin_y,
                                           int& width, int& height);

/// Maximum scroll position in map-relative leptons. Scrolling beyond this
/// would move the visible window past the map edge.
void Render_Bridge_Get_Clamp_Ranges(int& max_x, int& max_y);

/// Effective clamp window size in leptons. When the sidebar is active this
/// shrinks by the sidebar's world-space coverage, giving more scroll room
/// so the player can reach the map edge behind the sidebar.
void Render_Bridge_Get_Effective_Clamp_Size(int& w, int& h);

/// Map a world coordinate to tactical-relative pixel offset.
/// Returns true if the coordinate falls within the visible tactical area
/// (with EDGE_ZONE margin for sprites that overlap the boundary).
bool Render_Bridge_World_To_Tactical(int world_lepton_x, int world_lepton_y,
                                      int& pixel_x, int& pixel_y);

/// Map a tactical-relative pixel offset to a world coordinate (leptons).
/// Returns true if the pixel is inside the tactical area.
bool Render_Bridge_Tactical_To_World(int pixel_x, int pixel_y,
                                      int& world_lepton_x, int& world_lepton_y);

/// Check whether a cell is visible in the bridge viewport.
bool Render_Bridge_Is_Cell_In_View(int cell_x, int cell_y);

// --- Bridge-native UI (Phase 1: Infrastructure) ---

/// Begin a UI frame — clears the UI draw list and resets input zones.
void Render_Bridge_UI_Begin_Frame();

/// End a UI frame — finalizes input capture state.
void Render_Bridge_UI_End_Frame();

/// Initialize UI font atlases from loaded game font data.
void Render_Bridge_UI_Init_Fonts();

/// Set/get the UI font scale for SDF fonts (0.5–4.0, default 1.0).
void  Render_Bridge_UI_Set_Font_Scale(float s);
float Render_Bridge_UI_Get_Font_Scale();

/// Returns true if the UI has captured pointer input this frame.
bool Render_Bridge_UI_Has_Capture();

/// Returns true when bridge-native status messages replace legacy Messages.Draw().
bool Render_Bridge_UI_Use_Native_Messages();

/// Returns true when bridge-native help text replaces HelpClass::Draw_It().
bool Render_Bridge_UI_Use_Native_Help();

/// Returns true when a bridge-native modal dialog is pending or active.
bool Render_Bridge_UI_Has_Active_Dialog();

/// Update the bridge-owned pending modal dialog state.
void Render_Bridge_UI_Set_Dialog(const char* title,
                                 const char* message,
                                 const char* ok_label,
                                 const char* cancel_label,
                                 int screen_w, int screen_h,
                                 const char* extra_label = nullptr);

/// Clear the pending modal dialog state and store the last result.
void Render_Bridge_UI_Clear_Dialog(UIDialogResult result);

/// Read the last dialog result written by the bridge.
UIDialogResult Render_Bridge_UI_Get_Dialog_Result();

/// Read the current pending dialog fields.
void Render_Bridge_UI_Get_Dialog(int& screen_w, int& screen_h,
                                 const char*& title,
                                 const char*& message,
                                 const char*& ok_label,
                                 const char*& cancel_label,
                                 const char*& extra_label);

/// Drive one bridge UI frame without the game render chain.
/// Suitable for pre-game dialogs where PlayerPtr is not yet valid.
void Render_Bridge_Idle_Frame();

// --- Game Options dialog (Phase 2) ---

enum UIGameOptionsResult : int {
    UI_GOPTION_NONE    = 0,
    UI_GOPTION_LOAD    = 1,
    UI_GOPTION_SAVE    = 2,
    UI_GOPTION_DELETE  = 3,
    UI_GOPTION_GAME    = 4,
    UI_GOPTION_QUIT    = 5,
    UI_GOPTION_RESUME  = 6,
    UI_GOPTION_RESTATE = 7,
};

struct UIGameOptionsLabels {
    const char* title;
    const char* load;
    const char* save;
    const char* del;     // delete or resign
    const char* game;
    const char* quit;
    const char* resume;
    const char* restate;
};
void Render_Bridge_UI_Set_Game_Options(bool show_restate, bool is_multiplayer,
                                       const UIGameOptionsLabels& labels);
UIGameOptionsResult Render_Bridge_UI_Get_Game_Options_Result();
void Render_Bridge_UI_Clear_Game_Options();
bool Render_Bridge_UI_Has_Active_Game_Options();

// --- Load dialog (Phase 3) ---

enum UILoadMode : int { UI_LOAD_LOAD = 0, UI_LOAD_SAVE = 1, UI_LOAD_DELETE = 2 };

void Render_Bridge_UI_Set_Load_Dialog(UILoadMode mode, const char* items[], int count,
                                       int selected, const char* edit_text,
                                       int screen_w, int screen_h,
                                       const char* title = nullptr,
                                       const char* action_label = nullptr,
                                       const char* cancel_label = nullptr);
void Render_Bridge_UI_Get_Load_Dialog_State(int& selected, float& scroll_pos,
                                             char* edit_text, int edit_size);
int  Render_Bridge_UI_Get_Load_Dialog_Result();
void Render_Bridge_UI_Clear_Load_Dialog();
bool Render_Bridge_UI_Has_Active_Load_Dialog();
void Render_Bridge_UI_Update_Load_Dialog_Items(const char* items[], int count);

// --- Game Controls dialog (Phase 4) ---

struct UIGameControlsLabels {
    const char* title;
    const char* speed;
    const char* slower;
    const char* faster;
    const char* scrollrate;
    const char* visual;
    const char* sound;
    const char* ok;
};
void Render_Bridge_UI_Set_Game_Controls(float game_speed, float scroll_rate,
                                         int screen_w, int screen_h,
                                         const UIGameControlsLabels& labels);
void Render_Bridge_UI_Get_Game_Controls_State(float& game_speed, float& scroll_rate);
int  Render_Bridge_UI_Get_Game_Controls_Result();
void Render_Bridge_UI_Clear_Game_Controls();
bool Render_Bridge_UI_Has_Active_Game_Controls();

enum UIGameControlsResult : int {
    UI_GCTRL_NONE    = 0,
    UI_GCTRL_OK      = 1,
    UI_GCTRL_VISUAL  = 2,
    UI_GCTRL_SOUND   = 3,
};

// --- Visual Controls dialog (Phase 4) ---

struct UIVisualControlsLabels {
    const char* title;
    const char* brightness;
    const char* color;
    const char* contrast;
    const char* tint;
    const char* hd_graphics;
    const char* reset;
    const char* ok;
};
void Render_Bridge_UI_Set_Visual_Controls(float brightness, float color,
                                           float contrast, float tint,
                                           bool hd_graphics,
                                           int screen_w, int screen_h,
                                           const UIVisualControlsLabels& labels);
void Render_Bridge_UI_Get_Visual_Controls_State(float& brightness, float& color,
                                                 float& contrast, float& tint,
                                                 bool& hd_graphics);
int  Render_Bridge_UI_Get_Visual_Controls_Result();
void Render_Bridge_UI_Clear_Visual_Controls();
bool Render_Bridge_UI_Has_Active_Visual_Controls();

/// Enable or disable HD graphics rendering at runtime.
void Render_Bridge_Set_HD_Graphics(bool enabled);

/// Return whether HD graphics rendering is currently enabled.
bool Render_Bridge_Get_HD_Graphics();

/// Invalidate cached HD/classic sprite atlas content.
void Render_Bridge_Invalidate_HD_Sprite_Atlas();

// --- Sound Controls dialog (Phase 4) ---

struct UISoundControlsLabels {
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
void Render_Bridge_UI_Set_Sound_Controls(float music_vol, float sfx_vol,
                                          const char* track_names[], int track_count,
                                          int selected_track,
                                          bool shuffle, bool repeat, bool is_playing,
                                          int screen_w, int screen_h,
                                          const UISoundControlsLabels& labels);
void Render_Bridge_UI_Get_Sound_Controls_State(float& music_vol, float& sfx_vol,
                                                int& selected_track, float& track_scroll,
                                                bool& shuffle, bool& repeat);
int  Render_Bridge_UI_Get_Sound_Controls_Result();
void Render_Bridge_UI_Clear_Sound_Controls();
bool Render_Bridge_UI_Has_Active_Sound_Controls();

enum UISoundControlsResult : int {
    UI_SND_NONE    = 0,
    UI_SND_OK      = 1,
    UI_SND_PLAY    = 2,
    UI_SND_STOP    = 3,
};

// --- Special Dialog (Phase 5) ---

void Render_Bridge_UI_Set_Special_Dialog(const bool options[10],
                                          const char* labels[10],
                                          int screen_w, int screen_h,
                                          const char* title = nullptr,
                                          const char* ok_label = nullptr,
                                          const char* cancel_label = nullptr);
void Render_Bridge_UI_Get_Special_Dialog_State(bool options[10]);
int  Render_Bridge_UI_Get_Special_Dialog_Result();
void Render_Bridge_UI_Clear_Special_Dialog();
bool Render_Bridge_UI_Has_Active_Special_Dialog();

// --- Expansion/Bonus Dialog (Phase 6) ---

void Render_Bridge_UI_Set_Expansion_Dialog(int mode, const char* title,
                                            const char* items[], int count,
                                            int screen_w, int screen_h,
                                            const char* ok_label = nullptr,
                                            const char* cancel_label = nullptr);
void Render_Bridge_UI_Get_Expansion_Dialog_State(int& selected, float& scroll_pos);
int  Render_Bridge_UI_Get_Expansion_Dialog_Result();
void Render_Bridge_UI_Clear_Expansion_Dialog();
bool Render_Bridge_UI_Has_Active_Expansion_Dialog();

// --- Com Scenario Dialog (Phase 7) ---

struct UIComScenarioState {
    bool     active;
    char     player_name[32];
    int      faction;         // 0=GDI, 1=NOD
    int      credits;
    char     scenarios[64][40];
    int      scenario_count;
    int      selected_scenario;
    float    scenario_scroll;
    float    build_level;     // 0.0–1.0
    float    ai_players;      // 0.0–1.0
    float    ai_skill;        // 0.0–1.0
    float    unit_count;      // 0.0–1.0 (mapped to MPlayerCountMin..Max)
    bool     options[4];
    char     option_labels[4][32];
    int      result;          // 0=none 1=ok 2=cancel
    int      screen_w, screen_h;
    // Localized labels
    char     lbl_title[32];
    char     lbl_name[32];
    char     lbl_side[32];
    char     lbl_gdi[16];
    char     lbl_nod[16];
    char     lbl_credits[32];
    char     lbl_scenarios[32];
    char     lbl_build_level[32];
    char     lbl_ai_players[32];
    char     lbl_ai_skill[32];
    char     lbl_unit_count[32];
    char     lbl_ok[32];
    char     lbl_cancel[32];
};

void Render_Bridge_UI_Set_Com_Scenario(const UIComScenarioState& state);
void Render_Bridge_UI_Get_Com_Scenario_State(UIComScenarioState& state);
int  Render_Bridge_UI_Get_Com_Scenario_Result();
void Render_Bridge_UI_Clear_Com_Scenario();
bool Render_Bridge_UI_Has_Active_Com_Scenario();

// --- Main Menu Dialog (Phase 8) ---

enum UIMainMenuPage : int {
    UI_MM_PAGE_ROOT     = 0,
    UI_MM_PAGE_CAMPAIGN = 1,
};

struct UIMainMenuState {
    bool   active;
    int    result;            // 0=pending, 1=GDI, 2=NOD, 3=load, 4=skirmish, 5=options, 6=exit, 7=expansion
    int    page;              // UIMainMenuPage
    int    focused_button;    // keyboard focus index (-1 = none)
    float  fade_alpha;        // current opacity 0.0–1.0
    bool   dismissing;        // true during fade-out before committing result
    int    pending_result;    // result to commit after fade-out completes
    bool   had_input;         // set by emitter when input activity detected; cleared by Had_Input()
    int    screen_w, screen_h;
    char   lbl_new_game[48];
    char   lbl_load_game[48];
    char   lbl_skirmish[48];
    char   lbl_options[48];
    char   lbl_exit[48];
    char   lbl_gdi[32];
    char   lbl_nod[32];
    char   lbl_back[32];
    char   lbl_campaign_title[48];
    char   lbl_expansion[48];     // "Covert Operations" (NEWMENU only)
    bool   has_expansion;          // true if expansion button should show
    char   lbl_version[32];       // version string for bottom-right
    char   lbl_copyright[64];     // copyright for below panel
};

void Render_Bridge_UI_Set_Main_Menu(const UIMainMenuState& state);
int  Render_Bridge_UI_Get_Main_Menu_Result();
void Render_Bridge_UI_Clear_Main_Menu();
bool Render_Bridge_UI_Has_Active_Main_Menu();

// Returns true if any bridge-native dialog is currently open.
bool Render_Bridge_UI_Has_Any_Active_Dialog();

// --- Main Menu Options Dialog (standalone, from main menu "Options" button) ---

void Render_Bridge_UI_Set_Main_Menu_Options(const UIMainMenuOptionsState& state);
int  Render_Bridge_UI_Get_Main_Menu_Options_Result();
void Render_Bridge_UI_Clear_Main_Menu_Options();
bool Render_Bridge_UI_Has_Active_Main_Menu_Options();

#endif // CNC_RENDER_BRIDGE_H
