#include "ui_layout.h"

MapLayout get_map_layout(RECT client) {
    MapLayout layout;
    int available_w = (client.right - client.left) - side_panel_w - 36;
    int available_h = (client.bottom - client.top) - TOP_BAR_H - BOTTOM_BAR_H - 24;
    int fit_w = available_w;
    int fit_h = fit_w * MAP_H / MAP_W;

    if (fit_h > available_h) {
        fit_h = available_h;
        fit_w = fit_h * MAP_W / MAP_H;
    }
    layout.draw_w = clamp(fit_w * map_zoom_percent / 100, MAP_W / 2, MAP_W * 10);
    layout.draw_h = clamp(fit_h * map_zoom_percent / 100, MAP_H / 2, MAP_H * 10);
    layout.tile_size = clamp(layout.draw_w / MAP_W, 1, 42);
    layout.map_x = 18 + map_offset_x;
    layout.map_y = TOP_BAR_H + 10 + map_offset_y;
    return layout;
}

RECT get_play_button_rect(RECT client) {
    RECT rect = {18, client.bottom - 38, 58, client.bottom - 8};
    return rect;
}

RECT get_speed_button_rect(RECT client, int index) {
    RECT rect;
    rect.left = 68 + index * 46;
    rect.top = client.bottom - 38;
    rect.right = rect.left + 40;
    rect.bottom = client.bottom - 8;
    return rect;
}

RECT get_mode_button_rect(RECT client, int index) {
    RECT rect;
    int panel_x = client.right - side_panel_w + FORM_X_PAD;
    rect.left = panel_x;
    rect.top = TOP_BAR_H + 126 + index * 32;
    rect.right = panel_x + side_panel_w - FORM_X_PAD * 2;
    rect.bottom = rect.top + 28;
    return rect;
}

RECT get_map_size_button_rect(RECT client, int index) {
    RECT rect;
    int panel_x = client.right - side_panel_w + FORM_X_PAD;
    int gap = 8;
    int button_w = (side_panel_w - FORM_X_PAD * 2 - gap * 2) / 3;
    rect.left = panel_x + index * (button_w + gap);
    rect.top = TOP_BAR_H + 128;
    rect.right = rect.left + button_w;
    rect.bottom = rect.top + 28;
    return rect;
}

RECT get_panel_tab_rect(RECT client, int index) {
    RECT rect;
    int panel_x = client.right - side_panel_w + 12;
    int tab_w = (side_panel_w - 24) / PANEL_TAB_COUNT;
    rect.left = panel_x + index * tab_w;
    rect.top = TOP_BAR_H + 10;
    rect.right = index == PANEL_TAB_COUNT - 1 ? client.right - 12 : rect.left + tab_w - 4;
    rect.bottom = rect.top + 30;
    return rect;
}

RECT get_language_button_rect(RECT client) {
    RECT rect;
    rect.left = client.right - side_panel_w - 92;
    rect.top = 16;
    rect.right = client.right - side_panel_w - 18;
    rect.bottom = 46;
    if (rect.left < client.left + 250) {
        rect.left = client.left + 250;
        rect.right = rect.left + 74;
    }
    return rect;
}

RECT get_map_legend_box_rect(RECT client) {
    RECT box;
    int panel_left = client.right - side_panel_w;
    int geo_count = GEO_COUNT;
    int climate_count = CLIMATE_COUNT;
    int line_h = 20;
    int rows = display_mode == DISPLAY_CLIMATE ? climate_count : geo_count;
    int box_w = display_mode == DISPLAY_ALL ? 390 : 210;

    if (display_mode == DISPLAY_ALL && climate_count > rows) rows = climate_count;
    box.right = panel_left - 14;
    box.left = box.right - box_w;
    box.bottom = client.bottom - BOTTOM_BAR_H - 12;
    box.top = map_legend_collapsed ? box.bottom - 34 : box.bottom - (rows * line_h + 38);
    if (box.left < 20 || box.top < TOP_BAR_H + 12) {
        SetRectEmpty(&box);
    }
    return box;
}

RECT get_map_legend_toggle_rect(RECT client) {
    RECT box = get_map_legend_box_rect(client);
    RECT button;

    if (IsRectEmpty(&box)) {
        SetRectEmpty(&button);
        return button;
    }
    button.left = box.right - 34;
    button.top = box.top + 6;
    button.right = box.right - 8;
    button.bottom = box.top + 28;
    return button;
}

const char *speed_seconds_text(int index) {
    switch (index) {
        case 0: return "1s";
        case 1: return "0.25s";
        default: return "0.05s";
    }
}
