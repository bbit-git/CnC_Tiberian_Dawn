/**
 * ui_dialog.h — Bridge-native modal dialog and menu system.
 *
 * Provides centered modal windows with title bar, buttons, and
 * content areas. Handles modal input capture (blocks tactical input
 * while a dialog is open).
 */

#ifndef CNC_UI_DIALOG_H
#define CNC_UI_DIALOG_H

#include "ui_controls.h"
#include <cstdint>

/// Dialog result codes.
enum UIDialogResult : int {
    UI_DIALOG_NONE   = 0,   // dialog still open
    UI_DIALOG_OK     = 1,   // confirmed
    UI_DIALOG_CANCEL = 2,   // cancelled / closed
};

/// Dialog style.
struct UIDialogStyle {
    uint8_t bg_r, bg_g, bg_b, bg_a;
    uint8_t title_bg_r, title_bg_g, title_bg_b, title_bg_a;
    uint8_t title_text_r, title_text_g, title_text_b, title_text_a;
    uint8_t border_r, border_g, border_b, border_a;
    uint8_t scrim_r, scrim_g, scrim_b, scrim_a;  // background dimming
    UIFontID title_font;
    int title_height;
    int button_w;
    int button_h;
    int padding;
};

UIDialogStyle UI_Default_Dialog_Style();

/// Begin a modal dialog. Draws scrim, window frame, title bar.
/// Returns the content area origin (x, y) and size (w, h) for the caller
/// to fill with controls.
/// screen_w, screen_h: game buffer dimensions for centering.
void UI_Dialog_Begin(int screen_w, int screen_h,
                     int dialog_w, int dialog_h,
                     const char* title, const UIDialogStyle& style,
                     int& content_x, int& content_y,
                     int& content_w, int& content_h);

/// End the dialog and draw OK / Cancel buttons.
/// Returns UI_DIALOG_NONE while open, UI_DIALOG_OK or UI_DIALOG_CANCEL on action.
/// ok_label: text for confirm button (null = no OK button).
/// cancel_label: text for cancel button (null = no Cancel button).
UIDialogResult UI_Dialog_End(int dialog_x, int dialog_y,
                             int dialog_w, int dialog_h,
                             const char* ok_label,
                             const char* cancel_label,
                             const UIDialogStyle& style);

/// Convenience: simple confirm dialog with message, OK, and Cancel.
/// Returns result code.
UIDialogResult UI_Confirm_Dialog(int screen_w, int screen_h,
                                  const char* title, const char* message,
                                  const char* ok_label,
                                  const char* cancel_label);

/// Convenience: message box with OK only.
UIDialogResult UI_Message_Box(int screen_w, int screen_h,
                               const char* title, const char* message);

#endif // CNC_UI_DIALOG_H
