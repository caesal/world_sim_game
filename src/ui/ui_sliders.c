#include "ui_sliders.h"

#include "ui/ui_types.h"

int divider_hit_test(HWND hwnd, int mouse_x, int mouse_y) {
    RECT client;
    int divider_x;

    GetClientRect(hwnd, &client);
    divider_x = client.right - side_panel_w;
    return mouse_y >= TOP_BAR_H && mouse_x >= divider_x - 6 && mouse_x <= divider_x + 6;
}

RECT setup_slider_rect(HWND hwnd, int index) {
    RECT client;
    RECT rect;
    int x;
    int section_gap = index >= WORLD_SLIDER_BIAS_FOREST ? 34 : 0;

    GetClientRect(hwnd, &client);
    x = client.right - side_panel_w + FORM_X_PAD;
    rect.left = x + 158;
    rect.right = client.right - FORM_X_PAD - 8;
    rect.top = TOP_BAR_H + 292 + index * 34 + section_gap;
    rect.bottom = rect.top + 10;
    return rect;
}

int setup_slider_hit_test(HWND hwnd, int mouse_x, int mouse_y) {
    int i;

    if (panel_tab != PANEL_MAP) return -1;
    for (i = 0; i < WORLD_SLIDER_COUNT; i++) {
        RECT track = setup_slider_rect(hwnd, i);
        RECT hit = {track.left - 8, track.top - 14, track.right + 8, track.bottom + 14};
        if (point_in_rect(hit, mouse_x, mouse_y)) return i;
    }
    return -1;
}

void update_setup_slider(HWND hwnd, int index, int mouse_x) {
    RECT track = setup_slider_rect(hwnd, index);
    int value = clamp((mouse_x - track.left) * 100 / (track.right - track.left), 0, 100);

    if (index == WORLD_SLIDER_OCEAN) ocean_slider = value;
    else if (index == WORLD_SLIDER_CONTINENT) continent_slider = value;
    else if (index == WORLD_SLIDER_RELIEF) relief_slider = value;
    else if (index == WORLD_SLIDER_MOISTURE) moisture_slider = value;
    else if (index == WORLD_SLIDER_DROUGHT) drought_slider = value;
    else if (index == WORLD_SLIDER_VEGETATION) vegetation_slider = value;
    else if (index == WORLD_SLIDER_BIAS_FOREST) bias_forest_slider = value;
    else if (index == WORLD_SLIDER_BIAS_DESERT) bias_desert_slider = value;
    else if (index == WORLD_SLIDER_BIAS_MOUNTAIN) bias_mountain_slider = value;
    else if (index == WORLD_SLIDER_BIAS_WETLAND) bias_wetland_slider = value;
    InvalidateRect(hwnd, NULL, FALSE);
}
