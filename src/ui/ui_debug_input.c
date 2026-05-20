#include "ui/ui_debug_input.h"

#include "core/game_types.h"
#include "game/game_loop.h"
#include "render/panel_debug.h"
#include "ui/ui_invalidation.h"
#include "ui/ui_selection.h"
#include "ui/ui_types.h"

static void invalidate_debug_panel(HWND hwnd, RECT client) {
    (void)client;
    ui_invalidate_side_panel(hwnd);
}

int ui_handle_debug_panel_click(HWND hwnd, RECT client, int mouse_x, int mouse_y) {
    int subtab = debug_panel_subtab_hit_test(client, mouse_x, mouse_y);
    int event_civ;
    int filter;
    int mode;

    if (subtab >= 0) {
        debug_subtab = subtab;
        ui_invalidate_side_panel(hwnd);
        return 1;
    }
    if (debug_subtab != DEBUG_SUBTAB_MAP_LOG) return 0;

    event_civ = debug_panel_event_country_hit_test(client, mouse_x, mouse_y);
    filter = debug_panel_event_filter_hit_test(client, mouse_x, mouse_y);
    mode = debug_panel_map_mode_hit_test(client, mouse_x, mouse_y);
    if (event_civ >= 0) {
        ui_highlight_civ(event_civ, UI_HIGHLIGHT_SOURCE_EVENT_LOG);
        ui_invalidate_game_redraw(hwnd, GAME_REDRAW_MAP_DYNAMIC | GAME_REDRAW_SIDE_PANEL);
        return 1;
    }
    if (debug_panel_event_clear_highlight_hit_test(client, mouse_x, mouse_y)) {
        ui_clear_map_highlight();
        ui_invalidate_game_redraw(hwnd, GAME_REDRAW_MAP_DYNAMIC | GAME_REDRAW_SIDE_PANEL);
        return 1;
    }
    if (debug_panel_event_top_hit_test(client, mouse_x, mouse_y)) {
        debug_panel_event_log_top();
        ui_invalidate_side_panel(hwnd);
        return 1;
    }
    if (debug_panel_event_scrollbar_hit_test(client, mouse_x, mouse_y)) {
        debug_panel_event_scrollbar_begin_drag(mouse_y);
        SetCapture(hwnd);
        ui_invalidate_side_panel(hwnd);
        return 1;
    }
    if (filter >= 0) {
        debug_event_filter = filter;
        debug_panel_event_log_top();
        ui_invalidate_side_panel(hwnd);
        return 1;
    }
    if (mode >= 0) {
        display_mode = MAP_DISPLAY_MODES[mode];
        ui_invalidate_map_viewport(hwnd);
        return 1;
    }
    return 0;
}

int ui_handle_debug_panel_wheel(HWND hwnd, RECT client, POINT point, int steps) {
    if (debug_subtab == DEBUG_SUBTAB_MAP_LOG &&
        debug_panel_event_log_hit_test(client, point.x, point.y)) {
        debug_panel_event_log_scroll(-steps * LOG_SCROLL_ITEMS_PER_WHEEL_NOTCH);
        invalidate_debug_panel(hwnd, client);
        return 1;
    }
    if (debug_subtab == DEBUG_SUBTAB_PERFORMANCE_SYSTEM) {
        debug_system_scroll_offset = clamp(debug_system_scroll_offset - steps * 72, 0, 2400);
        invalidate_debug_panel(hwnd, client);
        return 1;
    }
    return 1;
}
