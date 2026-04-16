/**
 * ui_radar_math.h — Pure radar geometry helpers shared by runtime and tests.
 */

#ifndef CNC_UI_RADAR_MATH_H
#define CNC_UI_RADAR_MATH_H

#include "function.h"

struct UIRadarViewportRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

struct UIRadarClickTarget {
    int clicked_cell_x = 0;
    int clicked_cell_y = 0;
    int request_x = 0;
    int request_y = 0;
};

inline bool UI_Radar_Calc_Viewport_Rect(int radar_x, int radar_y, int radar_w, int radar_h,
                                        int map_w, int map_h,
                                        int visible_origin_x, int visible_origin_y,
                                        int visible_w, int visible_h,
                                        UIRadarViewportRect& out)
{
    if (radar_w <= 0 || radar_h <= 0 || map_w <= 0 || map_h <= 0 ||
        visible_w <= 0 || visible_h <= 0) {
        return false;
    }

    float cell_vp_x = static_cast<float>(visible_origin_x) / static_cast<float>(CELL_LEPTON_W);
    float cell_vp_y = static_cast<float>(visible_origin_y) / static_cast<float>(CELL_LEPTON_H);
    float cell_vis_w = static_cast<float>(visible_w) / static_cast<float>(CELL_LEPTON_W);
    float cell_vis_h = static_cast<float>(visible_h) / static_cast<float>(CELL_LEPTON_H);
    float px_per_cell_x = static_cast<float>(radar_w) / static_cast<float>(map_w);
    float px_per_cell_y = static_cast<float>(radar_h) / static_cast<float>(map_h);

    int vr_x = radar_x + static_cast<int>(cell_vp_x * px_per_cell_x);
    int vr_y = radar_y + static_cast<int>(cell_vp_y * px_per_cell_y);
    int vr_w = static_cast<int>(cell_vis_w * px_per_cell_x);
    int vr_h = static_cast<int>(cell_vis_h * px_per_cell_y);

    if (vr_x < radar_x) {
        vr_w -= radar_x - vr_x;
        vr_x = radar_x;
    }
    if (vr_y < radar_y) {
        vr_h -= radar_y - vr_y;
        vr_y = radar_y;
    }
    if (vr_x + vr_w > radar_x + radar_w) vr_w = radar_x + radar_w - vr_x;
    if (vr_y + vr_h > radar_y + radar_h) vr_h = radar_y + radar_h - vr_y;
    if (vr_w <= 0 || vr_h <= 0) return false;

    out.x = vr_x;
    out.y = vr_y;
    out.w = vr_w;
    out.h = vr_h;
    return true;
}

inline bool UI_Radar_Calc_Click_Target(int map_cell_x, int map_cell_y,
                                       int map_w, int map_h,
                                       int visible_w, int visible_h,
                                       int clamp_max_x, int clamp_max_y,
                                       float click_cell_x, float click_cell_y,
                                       UIRadarClickTarget& out)
{
    if (map_w <= 0 || map_h <= 0 || visible_w <= 0 || visible_h <= 0) return false;

    int rel_click_x = static_cast<int>(click_cell_x);
    int rel_click_y = static_cast<int>(click_cell_y);
    if (rel_click_x < 0) rel_click_x = 0;
    if (rel_click_y < 0) rel_click_y = 0;
    if (rel_click_x >= map_w) rel_click_x = map_w - 1;
    if (rel_click_y >= map_h) rel_click_y = map_h - 1;

    int request_x = Cell_To_Lepton(rel_click_x) - visible_w / 2;
    int request_y = Cell_To_Lepton(rel_click_y) - visible_h / 2;
    if (clamp_max_x < 0) clamp_max_x = 0;
    if (clamp_max_y < 0) clamp_max_y = 0;
    if (request_x < 0) request_x = 0;
    if (request_y < 0) request_y = 0;
    if (request_x > clamp_max_x) request_x = clamp_max_x;
    if (request_y > clamp_max_y) request_y = clamp_max_y;

    out.clicked_cell_x = map_cell_x + rel_click_x;
    out.clicked_cell_y = map_cell_y + rel_click_y;
    out.request_x = request_x;
    out.request_y = request_y;
    return true;
}

#endif // CNC_UI_RADAR_MATH_H
