#include "ui/ui_map_legend_layout.h"

#include "render/snapshot_ui.h"
#include "ui/ui_layout.h"

static RECT map_legend_frame_rect(RECT client) {
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

static RECT collapsed_rect(RECT frame) {
    RECT box;
    box.right = frame.right - 8;
    box.left = box.right - 34;
    box.bottom = frame.bottom - 8;
    box.top = box.bottom - 34;
    if (box.left < frame.left + 8 || box.top < frame.top + 8)
        SetRectEmpty(&box);
    return box;
}

static int full_height(int show_geography, int show_climate,
                       int show_routes, int show_city_glyphs) {
    int geo_count = 11;
    int climate_count = CLIMATE_COUNT;
    int line_h = 20;
    int left_bottom = 0;
    int right_bottom = 0;
    int city_bottom = 0;
    if (show_routes && !show_geography && !show_climate)
        return 30 + line_h * 3 + 12;
    if (show_geography)
        left_bottom = 30 + line_h + line_h * 4 + geo_count * line_h;
    if (show_climate) {
        right_bottom = 30 + line_h + climate_count * line_h;
        if (show_routes) right_bottom += line_h * 4;
    }
    if (show_city_glyphs) city_bottom = 30 + line_h * 6;
    return max(max(left_bottom, right_bottom), city_bottom) + 12;
}

static int alliance_height(void) {
    const RenderSnapshot *snapshot = snapshot_ui_current();
    int i, active_count = 0, rows;
    if (snapshot) {
        for (i = 0; i < snapshot->alliance_count && i < ALLIANCE_MAX; i++) {
            if (snapshot->alliances[i].active) active_count++;
        }
    }
    rows = active_count > 0 ? min(active_count, 8) +
           (active_count > 8 ? 1 : 0) : 1;
    return 30 + 20 * (1 + rows) + 12;
}

RECT get_map_legend_box_rect(RECT client) {
    RECT box;
    RECT frame = map_legend_frame_rect(client);
    int route_only = display_mode == DISPLAY_ROUTE_POTENTIAL;
    int alliance_mode = display_mode == DISPLAY_ALLIANCE;
    int show_geography = !route_only && !alliance_mode &&
                         display_mode != DISPLAY_CLIMATE;
    int show_climate = display_mode == DISPLAY_POLITICAL ||
                       display_mode == DISPLAY_REGIONS ||
                       display_mode == DISPLAY_ALL ||
                       display_mode == DISPLAY_CLIMATE;
    int show_routes = route_only;
    int show_city_glyphs = display_mode == DISPLAY_POLITICAL;
    int box_w = alliance_mode ? 320 :
                show_routes && !show_geography && !show_climate ? 230 :
                show_city_glyphs ? 540 :
                show_geography && show_climate ? 390 : 210;
    int height = alliance_mode ? alliance_height() :
                 full_height(show_geography, show_climate, show_routes,
                             show_city_glyphs);
    if (IsRectEmpty(&frame)) {
        SetRectEmpty(&box);
        return box;
    }
    if (map_legend_collapsed) return collapsed_rect(frame);
    if (height + 180 > frame.bottom - frame.top ||
        box_w + 24 > frame.right - frame.left)
        return collapsed_rect(frame);
    box.right = frame.right - 8;
    box.left = box.right - box_w;
    box.bottom = frame.bottom - 8;
    box.top = box.bottom - height;
    if (box.left < frame.left + 8 || box.top < frame.top + 8)
        SetRectEmpty(&box);
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
    RECT box = get_map_legend_box_rect(client);
    RECT hit = get_map_legend_toggle_rect(client);
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
