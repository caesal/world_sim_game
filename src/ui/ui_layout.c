#include "ui_layout.h"
#include "render/snapshot_ui.h"

int ui_side_panel_reserved_width(void) {
    return side_panel_collapsed ? 0 : side_panel_w;
}

RECT get_map_viewport_rect(RECT client) {
    RECT viewport;
    int panel_w = ui_side_panel_reserved_width();

    viewport.left = client.left;
    viewport.top = TOP_BAR_H;
    viewport.right = client.right - panel_w;
    viewport.bottom = client.bottom - BOTTOM_BAR_H;
    if (viewport.right < viewport.left + 80) viewport.right = viewport.left + 80;
    if (viewport.bottom < viewport.top + 80) viewport.bottom = viewport.top + 80;
    return viewport;
}

RECT get_map_frame_rect(RECT client) {
    RECT frame = get_map_viewport_rect(client);
    InflateRect(&frame, -12, -12);
    if (frame.right < frame.left + 120) frame.right = frame.left + 120;
    if (frame.bottom < frame.top + 120) frame.bottom = frame.top + 120;
    return frame;
}

static RECT get_map_legend_frame_rect(RECT client) {
    RECT frame = get_map_frame_rect(client);
    if (side_panel_collapsed) {
        RECT unsafe = get_side_panel_handle_dirty_rect(client);
        if (unsafe.left < frame.right && unsafe.right > frame.left &&
            unsafe.top < frame.bottom && unsafe.bottom > frame.top) {
            frame.right = min(frame.right, unsafe.left - 8);
        }
    }
    if (frame.right < frame.left + 120) SetRectEmpty(&frame);
    return frame;
}

RECT get_map_content_rect(RECT client) {
    return get_map_viewport_rect(client);
}

RECT get_map_actual_speed_badge_rect(RECT client) {
    RECT viewport = get_map_viewport_rect(client);
    RECT badge = {viewport.left + 12, viewport.top + 10,
                  viewport.left + 72, viewport.top + 34};
    if (badge.right > viewport.right - 4) {
        badge.right = viewport.right - 4;
        badge.left = badge.right - 60;
    }
    if (badge.bottom > viewport.bottom - 4) {
        badge.bottom = viewport.bottom - 4;
        badge.top = badge.bottom - 24;
    }
    return badge;
}

MapLayout get_map_layout(RECT client) {
    MapLayout layout;
    RECT viewport = get_map_content_rect(client);
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
    int panel_left = client.right - ui_side_panel_reserved_width();
    int handle_w = 28;
    int handle_h = 28;
    int area_top = TOP_BAR_H;
    int area_bottom = client.bottom - BOTTOM_BAR_H;
    if (side_panel_collapsed) {
        rect.right = client.right - 8;
        rect.left = rect.right - handle_w;
    } else {
        rect.left = panel_left;
        rect.right = rect.left + handle_w;
    }
    rect.top = area_top + ((area_bottom - area_top) - handle_h) / 2;
    rect.bottom = rect.top + handle_h;
    if (rect.left < client.left + 4) {
        rect.left = client.left + 4;
        rect.right = rect.left + handle_w;
    }
    return rect;
}

RECT get_side_panel_handle_dirty_rect(RECT client) {
    RECT rect = get_side_panel_handle_rect(client);
    InflateRect(&rect, 10, 10);
    return rect;
}

RECT get_side_panel_body_rect(RECT client) {
    RECT rect;
    if (side_panel_collapsed) return (RECT){0, 0, 0, 0};
    rect.left = client.right - side_panel_w;
    rect.top = TOP_BAR_H;
    rect.right = client.right;
    rect.bottom = client.bottom;
    return rect;
}

RECT get_side_panel_draw_rect(RECT client) {
    if (side_panel_collapsed) return get_side_panel_handle_rect(client);
    return get_side_panel_body_rect(client);
}

int side_panel_handle_hit_test(RECT client, int x, int y) {
    RECT rect = get_side_panel_handle_rect(client);
    InflateRect(&rect, 8, 8);
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
    RECT viewport = get_map_content_rect(client);
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
        if (side_panel_w >= MIN_SIDE_PANEL_W) side_panel_expanded_w = side_panel_w;
        side_panel_w = clamp(side_panel_expanded_w, MIN_SIDE_PANEL_W, MAX_SIDE_PANEL_W);
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
        side_panel_collapsed = 1;
    }
    if (map_view_auto_centered) ui_map_view_reset();
    ui_map_view_clamp(client);
}

RECT get_bottom_control_row_rect(RECT client) {
    RECT rect = {client.left, client.bottom - 38, client.right, client.bottom - 8};
    return rect;
}

RECT get_play_button_rect(RECT client) {
    RECT row = get_bottom_control_row_rect(client);
    RECT rect = {18, row.top, 58, row.bottom};
    return rect;
}

RECT get_speed_button_rect(RECT client, int index) {
    RECT row = get_bottom_control_row_rect(client);
    RECT rect;
    rect.left = 68 + index * 64;
    rect.top = row.top;
    rect.right = rect.left + 58;
    rect.bottom = row.bottom;
    return rect;
}

RECT get_mode_button_rect(RECT client, int index) {
    RECT rect;
    int gap = 6;
    int compact_min_w = 38 * MAP_DISPLAY_MODE_COUNT +
                        gap * (MAP_DISPLAY_MODE_COUNT - 1);
    int left = client.right - ui_side_panel_reserved_width() + 14;
    int right = client.right - 14;
    int width = right - left;
    int button_w = (width - gap * (MAP_DISPLAY_MODE_COUNT - 1)) / MAP_DISPLAY_MODE_COUNT;
    if (side_panel_collapsed || width < compact_min_w) {
        RECT reset = get_reset_view_button_rect(client);
        right = reset.left - 10;
        left = max(client.left + 86, right - 430);
        width = right - left;
        button_w = (width - gap * (MAP_DISPLAY_MODE_COUNT - 1)) / MAP_DISPLAY_MODE_COUNT;
    }
    button_w = max(54, button_w);
    if (button_w * MAP_DISPLAY_MODE_COUNT + gap * (MAP_DISPLAY_MODE_COUNT - 1) > width) {
        button_w = max(38, (width - gap * (MAP_DISPLAY_MODE_COUNT - 1)) / MAP_DISPLAY_MODE_COUNT);
    }
    rect.left = left + index * (button_w + gap);
    rect.top = 16;
    rect.right = index == MAP_DISPLAY_MODE_COUNT - 1 ? right : rect.left + button_w;
    rect.bottom = 46;
    return rect;
}

RECT get_map_size_button_rect(RECT client, int index) {
    RECT rect;
    int panel_x = client.right - side_panel_w + FORM_X_PAD;
    int gap = 8;
    int button_w = (side_panel_w - FORM_X_PAD * 2 - gap * (MAP_SIZE_COUNT - 1)) / MAP_SIZE_COUNT;
    rect.left = panel_x + index * (button_w + gap);
    rect.top = TOP_BAR_H + 128;
    rect.right = index == MAP_SIZE_COUNT - 1 ? panel_x + side_panel_w - FORM_X_PAD : rect.left + button_w;
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
    int panel_w = ui_side_panel_reserved_width();
    rect.left = client.right - panel_w - 92;
    rect.top = 16;
    rect.right = client.right - panel_w - 18;
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

RECT get_world_announcement_rect(RECT client) {
    RECT viewport = get_map_viewport_rect(client);
    RECT badge = get_map_actual_speed_badge_rect(client);
    RECT rect;
    rect.left = max(viewport.left + 12, badge.right + 12);
    rect.top = viewport.top + 8;
    rect.right = viewport.right - 12;
    rect.bottom = rect.top + 82;
    if (rect.bottom > viewport.bottom - 8) rect.bottom = viewport.bottom - 8;
    if (rect.right <= rect.left || rect.bottom <= rect.top) SetRectEmpty(&rect);
    return rect;
}

RECT get_world_announcement_control_rect(RECT client, WorldAnnouncementControl control) {
    RECT band = get_world_announcement_rect(client);
    int slot = control == WORLD_ANNOUNCEMENT_CONTROL_DISMISS ? 0 :
               control == WORLD_ANNOUNCEMENT_CONTROL_LOCATE ? 1 :
               control == WORLD_ANNOUNCEMENT_CONTROL_NEXT ? 2 : 3;
    RECT rect;
    rect.right = band.right - 8 - slot * 32;
    rect.left = rect.right - 28;
    rect.top = band.top + 8;
    rect.bottom = rect.top + 28;
    return rect;
}

static RECT map_legend_collapsed_rect(RECT frame) {
    RECT box;
    box.right = frame.right - 8;
    box.left = box.right - 34;
    box.bottom = frame.bottom - 8;
    box.top = box.bottom - 34;
    if (box.left < frame.left + 8 || box.top < frame.top + 8) SetRectEmpty(&box);
    return box;
}

static int map_legend_full_height(int show_geography, int show_climate, int show_routes, int show_city_glyphs) {
    int geo_count = 11;
    int climate_count = CLIMATE_COUNT;
    int line_h = 20;
    int left_bottom = 0;
    int right_bottom = 0;
    int city_bottom = 0;
    if (show_routes && !show_geography && !show_climate) return 30 + line_h * 3 + 12;
    if (show_geography) left_bottom = 30 + line_h + line_h * 3 + geo_count * line_h;
    if (show_climate) {
        right_bottom = 30 + line_h + climate_count * line_h;
        if (show_routes) right_bottom += line_h * 4;
    }
    if (show_city_glyphs) city_bottom = 30 + line_h * 6;
    return max(max(left_bottom, right_bottom), city_bottom) + 12;
}

static int map_legend_alliance_height(void) {
    const RenderSnapshot *snapshot = snapshot_ui_current();
    int i, active_count = 0, rows;
    if (snapshot) for (i = 0; i < snapshot->alliance_count && i < ALLIANCE_MAX; i++) {
        if (snapshot->alliances[i].active) active_count++;
    }
    rows = active_count > 0 ? min(active_count, 8) + (active_count > 8 ? 1 : 0) : 1;
    return 30 + 20 * (1 + rows) + 12;
}

RECT get_map_legend_box_rect(RECT client) {
    RECT box;
    RECT frame = get_map_legend_frame_rect(client);
    int route_only = display_mode == DISPLAY_ROUTE_POTENTIAL;
    int alliance_mode = display_mode == DISPLAY_ALLIANCE;
    int show_geography = !route_only && !alliance_mode && display_mode != DISPLAY_CLIMATE;
    int show_climate = display_mode == DISPLAY_POLITICAL || display_mode == DISPLAY_REGIONS ||
                       display_mode == DISPLAY_ALL ||
                       display_mode == DISPLAY_CLIMATE;
    int show_routes = route_only;
    int show_city_glyphs = display_mode == DISPLAY_POLITICAL;
    int box_w = alliance_mode ? 320 :
                show_routes && !show_geography && !show_climate ? 230 :
                show_city_glyphs ? 540 :
                show_geography && show_climate ? 390 : 210;
    int full_h = alliance_mode ? map_legend_alliance_height() :
                 map_legend_full_height(show_geography, show_climate, show_routes, show_city_glyphs);

    if (IsRectEmpty(&frame)) {
        SetRectEmpty(&box);
        return box;
    }
    if (map_legend_collapsed) return map_legend_collapsed_rect(frame);
    if (!map_legend_collapsed && full_h + 180 > frame.bottom - frame.top) {
        return map_legend_collapsed_rect(frame);
    }
    if (!map_legend_collapsed && box_w + 24 > frame.right - frame.left) {
        return map_legend_collapsed_rect(frame);
    }
    box.right = frame.right - 8;
    box.left = box.right - box_w;
    box.bottom = frame.bottom - 8;
    box.top = box.bottom - full_h;
    if (box.left < frame.left + 8 || box.top < frame.top + 8) {
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
    if (map_legend_collapsed || box.right - box.left <= 40) return box;
    button.left = box.right - 34;
    button.top = box.top + 6;
    button.right = box.right - 8;
    button.bottom = box.top + 28;
    return button;
}

RECT get_map_legend_hit_rect(RECT client) {
    RECT box = get_map_legend_box_rect(client), hit = get_map_legend_toggle_rect(client);

    if (IsRectEmpty(&box)) {
        SetRectEmpty(&hit);
        return hit;
    }
    if (map_legend_collapsed || box.right - box.left <= 40) {
        hit = box;
        InflateRect(&hit, 12, 12);
        return hit;
    }
    InflateRect(&hit, 8, 8);
    return hit;
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
