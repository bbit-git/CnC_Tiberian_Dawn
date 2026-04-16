/**
 * ui_main_menu.cpp — Bridge-native main menu emitter.
 *
 * Draws a semi-transparent dark green panel with stacked buttons
 * over the existing HTITLE.PCX background rendered via the palette shader.
 * No dialog frame or scrim — the title art shows through.
 *
 * Supports two pages:
 *   ROOT     — New Game, Load Game, Skirmish, Options, Exit
 *   CAMPAIGN — GDI / NOD side select, Back
 *
 * Fade animations: appear (0→1), dismiss (1→0), page crossfade (0.3→1).
 */

#include "ui_main_menu.h"
#include "ui_controls.h"
#include "render_bridge.h"
#include "gl_present_internal.h"
#include "function.h"
#include "meg_reader.h"
#include "dds_reader.h"
#include "stb_image.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

static uint8_t mod_alpha(uint8_t base, float fade)
{
    return static_cast<uint8_t>(base * fade);
}

// Helper: emit a button with optional focus highlight and fade.
static UIButtonState Focused_Button(int x, int y, int w, int h,
                                     const char* label, UIButtonStyle style,
                                     bool focused, float fade)
{
    if (focused) {
        style.border_r = 0;
        style.border_g = 160;
        style.border_b = 0;
        style.border_a = 255;
    }
    style.normal_a = mod_alpha(style.normal_a, fade);
    style.hover_a  = mod_alpha(style.hover_a,  fade);
    style.press_a  = mod_alpha(style.press_a,  fade);
    style.text_a   = mod_alpha(style.text_a,   fade);
    style.border_a = mod_alpha(style.border_a, fade);
    return UI_Button(x, y, w, h, label, style);
}

void UI_Main_Menu_Emit()
{
    if (!Render_Bridge_UI_Has_Active_Main_Menu()) {
        return;
    }

    int screen_w = 0, screen_h = 0;
    int page = 0, focused = -1;
    float fade = 0.0f;
    bool dismissing = false;
    UIMainMenuInternalLabels lbl;
    Render_Bridge_UI_Main_Menu_Get_Internal(screen_w, screen_h, page, focused,
                                             fade, dismissing, lbl);
    if (screen_w == 0) screen_w = 640;
    if (screen_h == 0) screen_h = 400;

    // --- Advance fade ---
    float fade_speed = 0.05f;  // ~20 frames to full (~0.7s at 30fps)
    float target = dismissing ? 0.0f : 1.0f;
    if (fade < target) {
        fade += fade_speed;
        if (fade > target) fade = target;
    } else if (fade > target) {
        fade -= fade_speed * 2.0f;  // dismiss is faster
        if (fade < target) fade = target;
    }
    Render_Bridge_UI_Main_Menu_Set_Fade(fade);

    // Complete dismiss: commit pending result once fade reaches zero.
    if (dismissing && fade <= 0.01f) {
        Render_Bridge_UI_Main_Menu_Commit_Dismiss();
        return;
    }

    // Block all input behind the menu (full screen)
    UI_Input_Register_Zone(0, 0, screen_w, screen_h);

    // Input blocked during transitions
    bool input_blocked = dismissing || fade < 0.5f;

    // C&C green button style (font set before layout so btn_h is dynamic)
    UIButtonStyle bs = UI_Default_Button_Style();
    bs.normal_r = 0;   bs.normal_g = 35;  bs.normal_b = 0;   bs.normal_a = 240;
    bs.hover_r  = 0;   bs.hover_g  = 65;  bs.hover_b  = 0;   bs.hover_a  = 255;
    bs.press_r  = 0;   bs.press_g  = 20;  bs.press_b  = 0;   bs.press_a  = 255;
    bs.text_r   = 200; bs.text_g   = 220; bs.text_b   = 200; bs.text_a   = 255;
    bs.border_r = 0;   bs.border_g = 100; bs.border_b = 0;   bs.border_a = 200;
    // bs.font stays UI_FONT_SDF_DEFAULT from UI_Default_Button_Style()

    int panel_w = 200;
    int padding = 8;
    int btn_h = UI_Text_Line_Height(bs.font) + 8;
    int gap = 4;

    // --- Keyboard input ---
    int btn_count = 0;
    bool activate = false;
    bool escape = false;

    if (page == UI_MM_PAGE_ROOT) {
        btn_count = lbl.has_expansion ? 6 : 5;
    } else {
        btn_count = 3; // GDI, NOD, Back
    }

    // Clamp focus into range
    if (focused < 0) focused = 0;
    if (focused >= btn_count) focused = btn_count - 1;

    // Consume keys even during transitions (to prevent queued input)
    unsigned short key;
    while ((key = UI_Input_Pop_Key()) != 0) {
        if (input_blocked) continue;  // drain but ignore
        Render_Bridge_UI_Main_Menu_Signal_Input();  // any key resets attract timer
        unsigned short plain = key & 0xFF;
        if (plain == (KN_UP & 0xFF) ||
            (page == UI_MM_PAGE_CAMPAIGN && plain == (KN_LEFT & 0xFF))) {
            focused--;
            if (focused < 0) focused = btn_count - 1;
        } else if (plain == (KN_DOWN & 0xFF) || plain == (KN_TAB & 0xFF) ||
                   (page == UI_MM_PAGE_CAMPAIGN && plain == (KN_RIGHT & 0xFF))) {
            focused++;
            if (focused >= btn_count) focused = 0;
        } else if (plain == (KN_RETURN & 0xFF)) {
            activate = true;
        } else if (plain == (KN_ESC & 0xFF)) {
            escape = true;
        }
    }

    // ESC on campaign page returns to root
    if (!input_blocked && escape && page == UI_MM_PAGE_CAMPAIGN) {
        Render_Bridge_UI_Main_Menu_Set_Page(UI_MM_PAGE_ROOT);
        return; // re-emit next frame with root page
    }

    // Write focus back to state
    Render_Bridge_UI_Main_Menu_Set_Focus(focused);

    // --- Layout ---
    if (page == UI_MM_PAGE_ROOT) {
        // ===== ROOT PAGE =====
        int panel_h = padding * 2 + btn_count * btn_h + (btn_count - 1) * gap;
        int panel_x = (screen_w - panel_w) / 2;
        int panel_y = screen_h * 25 / 100;

        g_ui_draw_list.Fill_Rect(panel_x, panel_y, panel_w, panel_h,
                                 0, 12, 0, mod_alpha(160, fade));
        g_ui_draw_list.Draw_Rect(panel_x, panel_y, panel_w, panel_h,
                                 0, 80, 0, mod_alpha(200, fade));

        int bx = panel_x + padding;
        int bw = panel_w - padding * 2;
        int by = panel_y + padding;

        struct { const char* label; int result_code; } buttons[7];
        int bi = 0;
        buttons[bi++] = { lbl.new_game,  -1 };  // navigate to campaign page
        buttons[bi++] = { lbl.load_game,  3 };
        buttons[bi++] = { lbl.skirmish,   4 };
        if (lbl.has_expansion)
            buttons[bi++] = { lbl.expansion, 7 };
        buttons[bi++] = { lbl.options,    5 };
        buttons[bi++] = { lbl.exit,       6 };

        for (int i = 0; i < btn_count; i++) {
            bool is_focused = (i == focused);
            UIButtonState state = Focused_Button(bx, by, bw, btn_h,
                                                  buttons[i].label, bs,
                                                  is_focused, fade);
            if (!input_blocked && state >= UI_BTN_HOVERED)
                Render_Bridge_UI_Main_Menu_Signal_Input();
            bool pressed = !input_blocked &&
                           (UI_Button_Activated(state) || (activate && is_focused));
            if (pressed) {
                if (buttons[i].result_code == -1) {
                    Render_Bridge_UI_Main_Menu_Set_Page(UI_MM_PAGE_CAMPAIGN);
                    return;
                }
                Render_Bridge_UI_Main_Menu_Begin_Dismiss(buttons[i].result_code);
                return;
            }
            by += btn_h + gap;
        }
    } else {
        // ===== CAMPAIGN PAGE =====
        int title_h = UI_Text_Line_Height(bs.font) + 4;
        int panel_h = padding + title_h + gap + btn_h + gap + btn_h + padding;
        int panel_x = (screen_w - panel_w) / 2;
        int panel_y = screen_h * 25 / 100;

        g_ui_draw_list.Fill_Rect(panel_x, panel_y, panel_w, panel_h,
                                 0, 12, 0, mod_alpha(160, fade));
        g_ui_draw_list.Draw_Rect(panel_x, panel_y, panel_w, panel_h,
                                 0, 80, 0, mod_alpha(200, fade));

        // Title label
        int title_y = panel_y + padding;
        UI_Label(panel_x, title_y, lbl.campaign_title, bs.font,
                 220, 220, 220, mod_alpha(255, fade), UI_ALIGN_CENTER, panel_w);

        // GDI / NOD buttons side by side
        int side_y = title_y + title_h + gap;
        int half_w = (panel_w - padding * 2 - gap) / 2;

        // GDI button (green)
        bool gdi_focused = (focused == 0);
        UIButtonState gdi_state = Focused_Button(panel_x + padding, side_y,
                                                  half_w, btn_h,
                                                  lbl.gdi, bs, gdi_focused, fade);

        // NOD button (red variant)
        UIButtonStyle nod_bs = bs;
        nod_bs.normal_r = 50;  nod_bs.normal_g = 0;  nod_bs.normal_b = 0;
        nod_bs.hover_r  = 80;  nod_bs.hover_g  = 0;  nod_bs.hover_b  = 0;
        nod_bs.press_r  = 30;  nod_bs.press_g  = 0;  nod_bs.press_b  = 0;
        nod_bs.border_r = 100; nod_bs.border_g = 0;  nod_bs.border_b = 0;
        nod_bs.text_r   = 220; nod_bs.text_g   = 200; nod_bs.text_b  = 200;

        bool nod_focused = (focused == 1);
        UIButtonState nod_state = Focused_Button(panel_x + padding + half_w + gap, side_y,
                                                  half_w, btn_h,
                                                  lbl.nod, nod_bs, nod_focused, fade);

        // Back button (centered, green)
        int back_y = side_y + btn_h + gap;
        int back_w = panel_w - padding * 2;
        bool back_focused = (focused == 2);
        UIButtonState back_state = Focused_Button(panel_x + padding, back_y,
                                                   back_w, btn_h,
                                                   lbl.back, bs, back_focused, fade);

        if (!input_blocked) {
            if (gdi_state >= UI_BTN_HOVERED || nod_state >= UI_BTN_HOVERED ||
                back_state >= UI_BTN_HOVERED)
                Render_Bridge_UI_Main_Menu_Signal_Input();
            bool gdi_pressed  = UI_Button_Activated(gdi_state) || (activate && gdi_focused);
            bool nod_pressed  = UI_Button_Activated(nod_state) || (activate && nod_focused);
            bool back_pressed = UI_Button_Activated(back_state) || (activate && back_focused);

            if (gdi_pressed) {
                Render_Bridge_UI_Main_Menu_Begin_Dismiss(1); // GDI
            } else if (nod_pressed) {
                Render_Bridge_UI_Main_Menu_Begin_Dismiss(2); // NOD
            } else if (back_pressed) {
                Render_Bridge_UI_Main_Menu_Set_Page(UI_MM_PAGE_ROOT);
            }
        }
    }

    // --- Version text (bottom-right, small grey) ---
    if (lbl.version && lbl.version[0]) {
        int vw = UI_Text_Measure_Width(UI_FONT_6PT, lbl.version,
                                        static_cast<int>(strlen(lbl.version)));
        UI_Label(screen_w - vw - 4, screen_h - 10,
                 lbl.version, UI_FONT_6PT,
                 100, 100, 100, mod_alpha(180, fade));
    }

    // --- Copyright text (centered below panel) ---
    if (page == UI_MM_PAGE_ROOT && lbl.copyright && lbl.copyright[0]) {
        int panel_h = padding * 2 + btn_count * btn_h + (btn_count - 1) * gap;
        int panel_y = screen_h * 25 / 100;
        int cy = panel_y + panel_h + 8;
        UI_Label((screen_w - panel_w) / 2, cy, lbl.copyright, UI_FONT_6PT,
                 80, 80, 80, mod_alpha(140, fade), UI_ALIGN_CENTER, panel_w);
    }
}

// ========== HD Title Background ==========

static GLuint g_title_texture = 0;
static int    g_title_w = 0, g_title_h = 0;
static bool   g_title_attempted = false;

// Read an entire file into a malloc'd buffer. Caller must free().
static void* load_file(const char* path, size_t* size_out)
{
    FILE* f = fopen(path, "rb");
    if (!f) return nullptr;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    if (len <= 0) { fclose(f); return nullptr; }
    fseek(f, 0, SEEK_SET);
    void* buf = malloc(static_cast<size_t>(len));
    if (!buf) { fclose(f); return nullptr; }
    size_t rd = fread(buf, 1, static_cast<size_t>(len), f);
    fclose(f);
    if (rd != static_cast<size_t>(len)) { free(buf); return nullptr; }
    *size_out = static_cast<size_t>(len);
    return buf;
}

bool UI_Main_Menu_Load_HD_Title()
{
    if (g_title_attempted) return g_title_texture != 0;
    g_title_attempted = true;

    fprintf(stderr, "[HD_TITLE] Searching for HD title background...\n");

    unsigned char* pixels = nullptr;
    int w = 0, h = 0, channels = 0;

    // 1. Try loose TGA file
    {
        size_t sz = 0;
        void* data = load_file("data/TITLE_HD.TGA", &sz);
        if (data) {
            fprintf(stderr, "[HD_TITLE] Found loose file data/TITLE_HD.TGA (%zu bytes)\n", sz);
            pixels = stbi_load_from_memory(static_cast<const uint8_t*>(data),
                                            static_cast<int>(sz),
                                            &w, &h, &channels, 4);
            free(data);
            if (pixels) {
                fprintf(stderr, "[HD_TITLE] Decoded loose TGA: %dx%d, %d channels\n", w, h, channels);
            } else {
                fprintf(stderr, "[HD_TITLE] stbi_load failed for loose TGA: %s\n", stbi_failure_reason());
            }
        } else {
            fprintf(stderr, "[HD_TITLE] No loose file data/TITLE_HD.TGA\n");
        }
    }

    // 2. Try loose DDS file
    if (!pixels) {
        size_t sz = 0;
        void* data = load_file("data/TITLE_HD.DDS", &sz);
        if (data) {
            fprintf(stderr, "[HD_TITLE] Found loose file data/TITLE_HD.DDS (%zu bytes)\n", sz);
            pixels = DDS_Decode_RGBA(data, sz, w, h);
            free(data);
            if (pixels) {
                fprintf(stderr, "[HD_TITLE] Decoded loose DDS: %dx%d\n", w, h);
            } else {
                fprintf(stderr, "[HD_TITLE] DDS decode failed for loose DDS\n");
            }
        }
    }

    // 3. Try DDS from remastered MEG archive
    if (!pixels) {
        // Build full paths to search
        const char* home = getenv("HOME");
        char meg_paths[4][512];
        int meg_count = 0;

        // Relative paths (if user copies MEG alongside the binary)
        snprintf(meg_paths[meg_count++], 512, "DATA/TEXTURES_SRGB.MEG");
        snprintf(meg_paths[meg_count++], 512, "data/TEXTURES_SRGB.MEG");
        // Linux Steam install
        if (home) {
            snprintf(meg_paths[meg_count++], 512,
                     "%s/.local/share/Steam/steamapps/common/CnCRemastered/Data/TEXTURES_SRGB.MEG", home);
        }

        // Entry names to try (backslash paths as stored in MEG)
        static const char* entry_names[] = {
            "DATA\\ART\\TEXTURES\\SRGB\\UI_MAINMENUBG_00.DDS",
            "DATA\\ART\\TEXTURES\\SRGB\\UI_MAINMENUBG_01.DDS",
            "DATA\\ART\\TEXTURES\\SRGB\\UI_C&C_LOGO.DDS",
            nullptr,
        };

        for (int mi = 0; mi < meg_count && !pixels; mi++) {
            MegReader meg;
            fprintf(stderr, "[HD_TITLE] Trying MEG: %s\n", meg_paths[mi]);
            if (!meg.Open(meg_paths[mi])) {
                fprintf(stderr, "[HD_TITLE]   failed to open\n");
                continue;
            }
            fprintf(stderr, "[HD_TITLE]   opened, %d entries\n", meg.Entry_Count());
            for (int ei = 0; entry_names[ei] && !pixels; ei++) {
                const MegEntry* entry = meg.Find(entry_names[ei]);
                if (!entry) {
                    fprintf(stderr, "[HD_TITLE]   '%s' not found\n", entry_names[ei]);
                    continue;
                }
                size_t sz = 0;
                void* data = meg.Read_Alloc(entry, &sz);
                if (!data) continue;
                fprintf(stderr, "[HD_TITLE]   found '%s' (%zu bytes)\n", entry_names[ei], sz);
                pixels = DDS_Decode_RGBA(data, sz, w, h);
                if (!pixels) {
                    // Fallback: try stb_image in case it's TGA
                    pixels = stbi_load_from_memory(static_cast<const uint8_t*>(data),
                                                    static_cast<int>(sz),
                                                    &w, &h, &channels, 4);
                }
                free(data);
                if (pixels) {
                    fprintf(stderr, "[HD_TITLE]   decoded: %dx%d\n", w, h);
                } else {
                    fprintf(stderr, "[HD_TITLE]   decode failed\n");
                }
            }
        }
    }

    if (!pixels) {
        fprintf(stderr, "[HD_TITLE] No HD background available, using PCX fallback\n");
        return false;
    }

    glGenTextures(1, &g_title_texture);
    glBindTexture(GL_TEXTURE_2D, g_title_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    free(pixels);
    g_title_w = w;
    g_title_h = h;
    return true;
}

bool UI_Main_Menu_Has_HD_Background()
{
    return g_title_texture != 0;
}

void UI_Main_Menu_Draw_HD_Background(int win_w, int win_h)
{
    if (!g_title_texture || !g_rgba_program) return;

    // Center-crop: fill the entire window, crop the wider dimension.
    // The remastered backgrounds are ultrawide (4000x1688 = 2.37:1).
    // For a 16:9 window (1.78:1), we crop the left/right sides.
    // For an ultrawide monitor, more of the image is visible.
    float img_aspect = static_cast<float>(g_title_w) / static_cast<float>(g_title_h);
    float win_aspect = static_cast<float>(win_w) / static_cast<float>(win_h);

    float u0 = 0.0f, v0 = 0.0f, u1 = 1.0f, v1 = 1.0f;

    if (img_aspect > win_aspect) {
        // Image is wider than window — crop sides
        float visible_frac = win_aspect / img_aspect;
        float margin = (1.0f - visible_frac) * 0.5f;
        u0 = margin;
        u1 = 1.0f - margin;
    } else {
        // Image is taller than window — crop top/bottom
        float visible_frac = img_aspect / win_aspect;
        float margin = (1.0f - visible_frac) * 0.5f;
        v0 = margin;
        v1 = 1.0f - margin;
    }

    // Full-screen quad in NDC
    glUseProgram(g_rgba_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_title_texture);
    glUniform1i(g_rgba_u_tex, 0);
    glUniform4f(g_rgba_u_src_rect, u0, v0, u1, v1);
    glUniform4f(g_rgba_u_dst_rect, -1.0f, 1.0f, 1.0f, -1.0f);

    static const float quad[] = { 0,0, 1,0, 0,1, 1,1 };
    GL_Present_Bind_Client_Quad(quad);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    GL_Present_Bind_Palette_Program();
}

void UI_Main_Menu_Shutdown()
{
    if (g_title_texture) {
        glDeleteTextures(1, &g_title_texture);
        g_title_texture = 0;
    }
    g_title_w = 0;
    g_title_h = 0;
    g_title_attempted = false;
}
