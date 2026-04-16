#ifndef CNC_UI_SOUND_CONTROLS_LAYOUT_H
#define CNC_UI_SOUND_CONTROLS_LAYOUT_H

struct UISoundControlsLayout {
    int list_h = 0;
    int action_y = 0;
    int ok_y = 0;
    int bottom_y = 0;
};

inline UISoundControlsLayout UI_Sound_Controls_Calc_Layout(int content_y,
                                                           int content_h,
                                                           int current_y,
                                                           int btn_h,
                                                           int gap)
{
    UISoundControlsLayout out;

    int consumed_h = current_y - content_y;
    if (consumed_h < 0) consumed_h = 0;

    int remaining_h = content_h - consumed_h;
    if (remaining_h < 0) remaining_h = 0;

    const int reserved_h = btn_h * 2 + gap * 2;
    int list_h = remaining_h - reserved_h;
    if (list_h < 0) list_h = 0;

    out.list_h = list_h;
    out.action_y = current_y + out.list_h + gap;
    out.ok_y = out.action_y + btn_h + gap;
    out.bottom_y = out.ok_y + btn_h;
    return out;
}

#endif // CNC_UI_SOUND_CONTROLS_LAYOUT_H
