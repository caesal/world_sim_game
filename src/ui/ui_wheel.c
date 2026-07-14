#include "ui/ui_wheel.h"

#include "core/constants.h"
#include "core/game_types.h"
#include "render/panel_country.h"
#include "render/panel_alliance.h"
#include "render/panel_country_events.h"
#include "ui/color_picker.h"
#include "ui/pause_menu.h"
#include "ui/ui_debug_input.h"
#include "ui/ui_forms.h"
#include "ui/ui_invalidation.h"
#include "ui/ui_alliance_panel_input.h"
#include "ui/ui_layout.h"
#include "ui/ui_plague_panel.h"
#include "ui/ui_plague_panel_layout.h"
#include "ui/ui_types.h"
#include "ui/ui_worldgen_layout.h"

#include <windowsx.h>

static int pending_delta = 0;
static POINT pending_screen_point = {0, 0};
static int pending_active = 0;

static int wheel_steps_from_delta(int delta) {
    int steps = delta / WHEEL_DELTA;
    if (steps == 0) steps = delta > 0 ? 1 : -1;
    return steps;
}

static int ui_wheel_handle_side_panel(HWND hwnd, RECT client, POINT point, int steps) {
    if (side_panel_collapsed && side_panel_handle_hit_test(client, point.x, point.y)) return 1;
    if (side_panel_collapsed || point.x < client.right - side_panel_w) return 0;
    if (panel_tab == PANEL_COUNTRY && point.y >= TOP_BAR_H && point.y <= client.bottom) {
        if (display_mode == DISPLAY_ALLIANCE && ui_alliance_panel_owns_input()) {
            if (alliance_panel_scroll(client, -steps * 72)) ui_invalidate_side_panel(hwnd);
            return 1;
        }
        if (selected_civ >= 0 && country_detail_subtab == COUNTRY_DETAIL_OVERVIEW &&
            country_recent_events_scroll_hit_test(point.x, point.y)) {
            if (country_recent_events_scroll(-steps * LOG_SCROLL_ITEMS_PER_WHEEL_NOTCH)) {
                ui_invalidate_side_panel(hwnd);
            }
            return 1;
        }
        if (country_panel_scroll(client, -steps * 72)) ui_invalidate_side_panel(hwnd);
        return 1;
    }
    if (panel_tab == PANEL_WORLD && point.y >= TOP_BAR_H && point.y <= client.bottom) {
        int old_scroll = worldgen_scroll_offset;
        worldgen_scroll_offset = worldgen_layout_clamp_scroll(
            client, side_panel_w, worldgen_scroll_offset - steps * 72);
        if (worldgen_scroll_offset != old_scroll) {
            ui_forms_layout(hwnd);
            ui_invalidate_side_panel(hwnd);
        }
        return 1;
    }
    if (panel_tab == PANEL_PLAGUE && point.y >= TOP_BAR_H && point.y <= client.bottom) {
        UiPlaguePanelLayout layout;
        ui_plague_panel_layout_build(client, side_panel_w, &layout);
        if (point_in_rect(layout.content_viewport, point.x, point.y) &&
            ui_plague_panel_scroll(client, side_panel_w, -steps * 72)) {
            ui_invalidate_side_panel(hwnd);
        }
        return 1;
    }
    if (panel_tab == PANEL_DEBUG && point.y >= TOP_BAR_H && point.y <= client.bottom) {
        ui_handle_debug_panel_wheel(hwnd, client, point, steps);
        return 1;
    }
    return 1;
}

void ui_wheel_invalidate_map_viewport(HWND hwnd) {
    ui_invalidate_map_viewport(hwnd);
}

void ui_wheel_accumulate(HWND hwnd, int screen_x, int screen_y, int delta) {
    RECT client;
    POINT point = {screen_x, screen_y};

    if (color_picker_active() || pause_menu_open || delta == 0) return;
    ScreenToClient(hwnd, &point);
    GetClientRect(hwnd, &client);
    if (ui_wheel_handle_side_panel(hwnd, client, point, wheel_steps_from_delta(delta))) return;

    pending_delta += delta;
    pending_screen_point.x = screen_x;
    pending_screen_point.y = screen_y;
    pending_active = 1;
    SetTimer(hwnd, WHEEL_INPUT_TIMER_ID, FRAME_TIMER_MS, NULL);
}

void ui_wheel_process_pending(HWND hwnd) {
    RECT client;
    POINT point;
    MapLayout before;
    int tile_x_scaled;
    int tile_y_scaled;
    int old_zoom;
    int delta;
    int steps;

    if (!pending_active) return;
    pending_active = 0;
    delta = pending_delta;
    pending_delta = 0;
    KillTimer(hwnd, WHEEL_INPUT_TIMER_ID);
    if (color_picker_active() || pause_menu_open || delta == 0) return;

    steps = wheel_steps_from_delta(delta);
    point = pending_screen_point;
    ScreenToClient(hwnd, &point);
    GetClientRect(hwnd, &client);

    if (ui_wheel_handle_side_panel(hwnd, client, point, steps)) return;

    if (point.y < TOP_BAR_H || point.y > client.bottom - BOTTOM_BAR_H) return;
    before = get_map_layout(client);
    old_zoom = map_zoom_percent;
    tile_x_scaled = (point.x - before.map_x) * 100000 / max(1, before.draw_w);
    tile_y_scaled = (point.y - before.map_y) * 100000 / max(1, before.draw_h);
    map_zoom_percent = clamp(map_zoom_percent + steps * 5, 25, 700);
    if (map_zoom_percent == old_zoom) return;
    {
        MapLayout after = get_map_layout(client);
        map_offset_x += point.x - (after.map_x + tile_x_scaled * after.draw_w / 100000);
        map_offset_y += point.y - (after.map_y + tile_y_scaled * after.draw_h / 100000);
        map_view_auto_centered = 0;
        ui_map_view_clamp(client);
    }
    map_interaction_preview = 1;
    KillTimer(hwnd, MAP_PREVIEW_TIMER_ID);
    SetTimer(hwnd, MAP_PREVIEW_TIMER_ID, 220, NULL);
    ui_wheel_invalidate_map_viewport(hwnd);
}
