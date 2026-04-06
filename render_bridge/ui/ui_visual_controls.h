/**
 * ui_visual_controls.h — Bridge-native visual controls dialog.
 */

#ifndef CNC_UI_VISUAL_CONTROLS_H
#define CNC_UI_VISUAL_CONTROLS_H

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

void UI_Visual_Controls_Emit();

#endif // CNC_UI_VISUAL_CONTROLS_H
