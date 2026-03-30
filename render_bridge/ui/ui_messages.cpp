/**
 * ui_messages.cpp — Bridge-native replay of the legacy on-screen message list.
 *
 * This mirrors the currently visible MessageListClass labels into the UI draw
 * list so status messages no longer need to arrive through the full-screen
 * HidPage diff overlay.
 */

#include "ui_messages.h"
#include "ui_draw_list.h"
#include "ui_text.h"
#include "ui_controls.h"
#include "function.h"
#include "msglist.h"
#include "txtlabel.h"

namespace {

UIFontID map_font(TextPrintType style)
{
    switch ((int)style & 0x000F) {
    case TPF_6POINT:
    case TPF_6PT_GRAD:
    case TPF_MAP:
    case TPF_GREEN12:
    case TPF_GREEN12_GRAD:
        return UI_FONT_6PT;
    case TPF_LED:
        return UI_FONT_LED;
    case TPF_VCR:
        return UI_FONT_VCR;
    case TPF_8POINT:
    default:
        return UI_FONT_8PT;
    }
}

void map_color(int color, uint8_t& r, uint8_t& g, uint8_t& b)
{
    const uint8_t* pal = static_cast<const uint8_t*>((void*)Get_Palette());
    if (!pal) pal = GamePalette;
    if (!pal || color < 0 || color > 255) {
        r = 220; g = 220; b = 220;
        return;
    }

    r = pal[color * 3 + 0] << 2;
    g = pal[color * 3 + 1] << 2;
    b = pal[color * 3 + 2] << 2;
}

void emit_shadowed_text(const TextLabelClass& label)
{
    if (!label.Text || !label.Text[0]) return;

    UIFontID font = map_font(label.Style);
    uint8_t r = 255, g = 255, b = 255;
    map_color(label.Color, r, g, b);

    int len = static_cast<int>(strlen(label.Text));
    int x = label.X;
    int y = label.Y;
    int max_width = label.PixWidth;
    UIAlign align = UI_ALIGN_LEFT;
    if (label.Style & TPF_CENTER) align = UI_ALIGN_CENTER;
    else if (label.Style & TPF_RIGHT) align = UI_ALIGN_RIGHT;

    if (!(label.Style & TPF_NOSHADOW)) {
        UI_Label(x + 1, y + 1, label.Text, font, 0, 0, 0, 255, align, max_width);
    }

    UI_Label(x, y, label.Text, font, r, g, b, 255, align, max_width);
}

void visit_message(const TextLabelClass& label, void* /*context*/)
{
    emit_shadowed_text(label);
}

} // namespace

void UI_Messages_Emit()
{
    extern MessageListClass Messages;
    if (Messages.Num_Messages() <= 0) return;
    Messages.Visit_For_Render_Bridge(visit_message, nullptr);
}
