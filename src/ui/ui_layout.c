#include "ui_layout.h"

static int panel_effective_width(void) {
    return side_panel_collapsed ? SIDE_PANEL_COLLAPSED_W : side_panel_w;
}

RECT get_map_viewport_rect(RECT client) {
    RECT viewport;
    int panel_w = panel_effective_width();

    viewport.left = client.left;
    viewport.top = TOP_BAR_H;
    viewport.right = client.right - panel_w;
    viewport.bottom = client.bottom - BOTTOM_BAR_H;
    if (viewport.right < viewport.left + 80) viewport.right = viewport.left + 80;
    if (viewport.bottom < viewport.top + 80) viewport.bottom = viewport.top + 80;
    return viewport;
}

MapLayout get_map_layout(RECT client) {
    MapLayout layout;
    RECT viewport = get_map_viewport_rect(client);
    int available_w = viewport.right - viewport.left;
    int available_h = viewport.bottom - viewport.top;
    int fit_w = available_w;
    int fit_h = fit_w * MAP_H / MAP_W;

    if (fit_h > available_h) {
        fit_h = available_h;
        fit_w = fit_h * MAP_W / MAP_H;
    }
    layout.draw_w = clamp(fit_w * map_zoom_percent / 100, MAP_W / 2, MAP_W * 10);
    layout.draw_h = clamp(fit_h * map_zoom_percent / 100, MAP_H / 2, MAP_H * 10);
    layout.tile_size = clamp(layout.draw_w / MAP_W, 1, 42);
    layout.map_x = viewport.left + (available_w - layout.draw_w) / 2 + map_offset_x;
    layout.map_y = viewport.top + (available_h - layout.draw_h) / 2 + map_offset_y;
    return layout;
}

RECT get_side_panel_handle_rect(RECT client) {
    RECT rect;
    int panel_left = client.right - panel_effective_width();
    if (!side_panel_collapsed) {
        int handle_w = SIDE_PANEL_COLLAPSED_W - 8;
        int handle_h = 68;
        int area_top = TOP_BAR_H;
        int area_bottom = client.bottom - BOTTOM_BAR_H;
        rect.left = panel_left - handle_w / 2;
        rect.right = rect.left + handle_w;
        rect.top = area_top + ((area_bottom - area_top) - handle_h) / 2;
        rect.bottom = rect.top + handle_h;
        return rect;
    }
    rect.left = panel_left;
    rect.top = TOP_BAR_H;
    rect.right = panel_left + SIDE_PANEL_COLLAPSED_W;
    rect.bottom = client.bottom - BOTTOM_BAR_H;
    if (rect.right > client.right) rect.right = client.right;
    return rect;
}

int side_panel_handle_hit_test(RECT client, int x, int y) {
    RECT rect = get_side_panel_handle_rect(client);
    return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
}

void ui_map_view_reset(void) {
    map_zoom_percent = 100;
    map_offset_x = 0;
    map_offset_y = 0;
    map_view_auto_centered = 1;
    map_interaction_preview = 0;
}

void ui_map_view_clamp(RECT client) {
    RECT viewport = get_map_viewport_rect(client);
    MapLayout layout;
    int viewport_w = viewport.right - viewport.left;
    int viewport_h = viewport.bottom - viewport.top;
    int keep = 48;
    int base_x, base_y, min_x, max_x, min_y, max_y;

    if (map_view_auto_centered) {
        map_offset_x = 0;
        map_offset_y = 0;
        return;
    }
    layout = get_map_layout(client);
    base_x = layout.map_x - map_offset_x;
    base_y = layout.map_y - map_offset_y;
    if (keep > viewport_w / 2) keep = viewport_w / 2;
    if (keep > viewport_h / 2) keep = viewport_h / 2;
    min_x = viewport.left + keep - (base_x + layout.draw_w);
    max_x = viewport.right - keep - base_x;
    min_y = viewport.top + keep - (base_y + layout.draw_h);
    max_y = viewport.bottom - keep - base_y;
    if (min_x > max_x) min_x = max_x = 0;
    if (min_y > max_y) min_y = max_y = 0;
    map_offset_x = clamp(map_offset_x, min_x, max_x);
    map_offset_y = clamp(map_offset_y, min_y, max_y);
}

void ui_side_panel_apply_state(RECT client) {
    if (side_panel_collapsed) {
        side_panel_w = SIDE_PANEL_COLLAPSED_W;
    } else {
        side_panel_w = clamp(side_panel_w, MIN_SIDE_PANEL_W, MAX_SIDE_PANEL_W);
        side_panel_expanded_w = side_panel_w;
    }
    ui_map_view_clamp(client);
}

void ui_toggle_side_panel(RECT client) {
    if (side_panel_collapsed) {
        side_panel_collapsed = 0;
        side_panel_w = clamp(side_panel_expanded_w, MIN_SIDE_PANEL_W, MAX_SIDE_PANEL_W);
    } else {
        side_panel_expanded_w = clamp(side_panel_w, MIN_SIDE_PANEL_W, MAX_SIDE_PANEL_W);
        side_panel_w = SIDE_PANEL_COLLAPSED_W;
        side_panel_collapsed = 1;
    }
    if (map_view_auto_centered) ui_map_view_reset();
    ui_map_view_clamp(client);
}

RECT get_play_button_rect(RECT client) {
    RECT rect = {18, client.bottom - 38, 58, client.bottom - 8};
    return rect;
}

RECT get_speed_button_rect(RECT client, int index) {
    RECT rect;
    rect.left = 68 + index * 50;
    rect.top = client.bottom - 38;
    rect.right = rect.left + 46;
    rect.bottom = client.bottom - 8;
    return rect;
}

RECT get_mode_button_rect(RECT client, int index) {
    RECT rect;
    int panel_x = client.right - side_panel_w + FORM_X_PAD;
    int gap = 5;
    int width = side_panel_w - FORM_X_PAD * 2;
    int button_w = (width - gap * (MAP_DISPLAY_MODE_COUNT - 1)) / MAP_DISPLAY_MODE_COUNT;
    rect.left = panel_x + index * (button_w + gap);
    rect.top = TOP_BAR_H + 126;
    rect.right = index == MAP_DISPLAY_MODE_COUNT - 1 ? panel_x + width : rect.left + button_w;
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

RECT get_reset_view_button_rect(RECT client) {
    RECT language = get_language_button_rect(client);
    RECT rect;
    rect.right = language.left - 8;
    rect.left = rect.right - 74;
    rect.top = language.top;
    rect.bottom = language.bottom;
    if (rect.left < client.left + 160) {
        rect.left = client.left + 160;
        rect.right = rect.left + 74;
    }
    return rect;
}

RECT get_map_legend_box_rect(RECT client) {
    RECT box;
    int panel_left = client.right - side_panel_w;
    int geo_count = 11;
    int climate_count = CLIMATE_COUNT;
    int line_h = 20;
    int water_rows = 3;
    int terrain_rows = geo_count + 1;
    int climate_rows = climate_count + 1;
    int rows = display_mode == DISPLAY_CLIMATE ? climate_rows : water_rows + terrain_rows;
    int box_w = display_mode == DISPLAY_ALL ? 390 : 210;

    if (display_mode == DISPLAY_ALL && climate_rows > rows) rows = climate_rows;
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
        case 0: return "10s";
        case 1: return "5s";
        case 2: return "1s";
        case 3: return "0.25s";
        default: return "0.1s";
    }
}

const char *speed_button_icon(int index) {
    switch (index) {
        case 0: return "▶";
        case 1: return "▶▶";
        case 2: return "▶▶▶";
        case 3: return "▶▶▶▶";
        default: return "▶▶▶▶▶";
    }
}
