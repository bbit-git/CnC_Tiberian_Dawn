/**
 * ui_header.cpp — Bridge-native header/tab bar rendering.
 *
 * Reads game state to emit the header bar: background, tab labels,
 * and credits indicator. The legacy Draw_It still runs for buffer
 * compatibility; this emits an overlay on top via the UI draw list.
 */

#include "ui_header.h"
#include "ui_draw_list.h"
#include "ui_controls.h"
#include "ui_text.h"
#include "commandbar_atlas.h"
#include "commandbar_sprites.h"
#include "render_bridge.h"
#include "function.h"

void UI_Header_Emit()
{
    extern bool InMainLoop;
    if (!InMainLoop) return;

    int hdr_x = 0, hdr_y = 0, hdr_w = 0, hdr_h = 0;
    Render_Bridge_Get_Header_Rect(hdr_x, hdr_y, hdr_w, hdr_h);
    int tab_h = hdr_h;
    if (tab_h <= 0) return;

    bool use_atlas = Render_Bridge_Get_HD_Graphics() && Commandbar_Atlas_Is_Ready();

    if (use_atlas) {
        // HD header: 3-part stretch (left cap + middle tile + right cap)
        const AtlasSpriteRect* left  = Commandbar_Atlas_Find(ATLAS_SIDEBAR_BUTTONHEADER_LEFT);
        const AtlasSpriteRect* mid   = Commandbar_Atlas_Find(ATLAS_SIDEBAR_BUTTONHEADER_MID);
        const AtlasSpriteRect* right = Commandbar_Atlas_Find(ATLAS_SIDEBAR_BUTTONHEADER_RIGHT);
        if (left && mid && right) {
            int cap_w = tab_h * left->width / left->height;
            if (cap_w < 4) cap_w = 4;
            g_ui_draw_list.Draw_Atlas_Sprite(hdr_x, hdr_y, cap_w, tab_h,
                                             left->u0, left->v0, left->u1, left->v1);
            g_ui_draw_list.Draw_Atlas_Sprite(hdr_x + cap_w, hdr_y,
                                             hdr_w - 2 * cap_w, tab_h,
                                             mid->u0, mid->v0, mid->u1, mid->v1);
            g_ui_draw_list.Draw_Atlas_Sprite(hdr_x + hdr_w - cap_w, hdr_y, cap_w, tab_h,
                                             right->u0, right->v0, right->u1, right->v1);
        }
    } else {
        // Legacy: thin accent line at bottom
        g_ui_draw_list.Fill_Rect(hdr_x, hdr_y + tab_h - 1, hdr_w, 1,
                                 56, 56, 56, 180);
    }

    // Credits display — right side
    int eva_w = 80;
    if (hdr_w > 800) eva_w = 160;
    int credits_x = hdr_x + hdr_w - eva_w;

    char credits_buf[32];
    long credits_val = PlayerPtr ? PlayerPtr->Available_Money() : 0;
    snprintf(credits_buf, sizeof(credits_buf), "%ld", credits_val);

    int text_w = UI_Text_Measure_Width(UI_FONT_6PT, credits_buf,
                                        static_cast<int>(strlen(credits_buf)));
    int text_h = UI_Text_Line_Height(UI_FONT_6PT);
    int tx = credits_x + (eva_w - text_w) / 2;
    int ty = hdr_y + (tab_h - text_h) / 2;

    // Credits label with light shadow to match the legacy top bar better.
    g_ui_draw_list.Draw_Text(tx + 1, ty + 1, credits_buf, UI_FONT_6PT,
                             0, 0, 0, 255);
    g_ui_draw_list.Draw_Text(tx, ty, credits_buf, UI_FONT_6PT,
                             0, 220, 0, 255);
}
