#include "ui.h"
#include "game/game.h"
#include "game/game_loop.h"
#include "game/game_worldgen.h"
#include "core/game_types.h"
#include "render/panel_country.h"
#include "render/panel_country_events.h"
#include "render/panel_debug.h"
#include "render/panel_country_diplomacy_hits.h"
#include "render/render.h"
#include "ui/color_picker.h"
#include "ui/pause_menu.h"
#include "ui/ui_actions.h"
#include "ui/ui_debug_input.h"
#include "ui/ui_forms.h"
#include "ui/ui_invalidation.h"
#include "ui/ui_layout.h"
#include "ui/ui_map_input.h"
#include "ui/ui_selection.h"
#include "ui/ui_snapshot_read.h"
#include "ui/ui_sliders.h"
#include "ui/ui_types.h"
#include "ui/ui_wheel.h"
#include "ui/ui_worldgen_layout.h"
#include <string.h>
#include <windowsx.h>
static int tracking_mouse_leave = 0;
static int last_panel_hover_target = -1;
static void invalidate_side_panel(HWND hwnd) {
    ui_invalidate_side_panel(hwnd);
}

static int panel_hover_target(RECT client, int x, int y) {
    if (x < client.right - side_panel_w || y < TOP_BAR_H || y > client.bottom) return -1;
    if (panel_tab == PANEL_COUNTRY) return panel_tab * 100000 + country_panel_hit_test(client, x, y);
    return panel_tab * 100000 + (x / 24) * 31 + y / 24;
}
static void handle_mouse_down(HWND hwnd, int mouse_x, int mouse_y) {
    RECT client;
    RECT legend_toggle;
    int i;
    int slider;
    GetClientRect(hwnd, &client);
    if (color_picker_mouse_down(hwnd, client, mouse_x, mouse_y)) return;
    if (pause_menu_open) {
        int hit = pause_menu_hit_test(client, mouse_x, mouse_y);
        if (hit != PAUSE_MENU_HIT_NONE) ui_handle_pause_menu_action(hwnd, hit);
        ui_invalidate_full(hwnd);
        return;
    }
    legend_toggle = get_map_legend_toggle_rect(client);
    if (!IsRectEmpty(&legend_toggle) && point_in_rect(legend_toggle, mouse_x, mouse_y)) {
        map_legend_collapsed = !map_legend_collapsed;
        ui_invalidate_map_viewport(hwnd);
        return;
    }
    if (point_in_rect(get_language_button_rect(client), mouse_x, mouse_y)) {
        ui_language = ui_language == UI_LANG_EN ? UI_LANG_ZH : UI_LANG_EN;
        ui_forms_refresh_language(hwnd);
        ui_invalidate_full(hwnd);
        return;
    }
    if (point_in_rect(get_reset_view_button_rect(client), mouse_x, mouse_y)) { ui_map_view_reset(); ui_map_view_clamp(client); ui_invalidate_map_viewport(hwnd); return; }
    if (point_in_rect(get_play_button_rect(client), mouse_x, mouse_y)) {
        game_toggle_auto_run();
        ui_invalidate_game_redraw(hwnd, GAME_REDRAW_TOP_BAR | GAME_REDRAW_BOTTOM_BAR);
        return;
    }
    for (i = 0; i < SPEED_COUNT; i++) {
        if (point_in_rect(get_speed_button_rect(client, i), mouse_x, mouse_y)) {
            ui_set_speed(i);
            ui_invalidate_game_redraw(hwnd, GAME_REDRAW_TOP_BAR | GAME_REDRAW_BOTTOM_BAR);
            return;
        }
    }
    if (side_panel_handle_hit_test(client, mouse_x, mouse_y)) { ui_toggle_side_panel(client); ui_forms_layout(hwnd); ui_invalidate_full(hwnd); return; }
    if (side_panel_collapsed) { ui_select_tile_from_mouse(hwnd, mouse_x, mouse_y); return; }
    for (i = 0; i < PANEL_TAB_COUNT; i++) {
        if (point_in_rect(get_panel_tab_rect(client, i), mouse_x, mouse_y)) {
            panel_tab = i;
            ui_forms_layout(hwnd);
            ui_invalidate_side_panel(hwnd);
            return;
        }
    }
    if (panel_tab == PANEL_DEBUG) {
        if (ui_handle_debug_panel_click(hwnd, client, mouse_x, mouse_y)) return;
    }
    if (panel_tab == PANEL_COUNTRY && mouse_x >= client.right - side_panel_w) {
        int hit = country_panel_hit_test(client, mouse_x, mouse_y);
        if (selected_civ >= 0 && country_detail_subtab == COUNTRY_DETAIL_OVERVIEW) {
            int event_civ = country_recent_events_country_hit_test(mouse_x, mouse_y);
            if (event_civ >= 0) {
                ui_highlight_civ(event_civ, UI_HIGHLIGHT_SOURCE_EVENT_LOG);
                ui_invalidate_game_redraw(hwnd, GAME_REDRAW_MAP_DYNAMIC | GAME_REDRAW_SIDE_PANEL);
                return;
            }
        }
        if (selected_civ >= 0 && country_detail_subtab == COUNTRY_DETAIL_DIPLOMACY) {
            int relation_civ = country_diplomacy_civ_hit_test(mouse_x, mouse_y);
            if (ui_snapshot_civ_alive(relation_civ) && relation_civ != selected_civ) {
                ui_select_civ_preserve_view(relation_civ, UI_SELECT_SOURCE_DIPLOMACY_CARD);
                ui_invalidate_game_redraw(hwnd, GAME_REDRAW_MAP_DYNAMIC | GAME_REDRAW_SIDE_PANEL);
                return;
            }
        }
        if (hit == COUNTRY_PANEL_HIT_TOGGLE_FALLEN) {
            country_show_fallen = !country_show_fallen;
            country_list_scroll_offset = 0;
            if (!country_show_fallen && selected_civ >= 0 && !ui_snapshot_civ_alive(selected_civ)) {
                ui_clear_selected_civ(UI_SELECT_SOURCE_COUNTRY_LIST);
            }
            ui_invalidate_side_panel(hwnd);
            return;
        }
        if (hit == COUNTRY_PANEL_HIT_BACK_TO_LIST) {
            ui_clear_selected_civ(UI_SELECT_SOURCE_COUNTRY_LIST);
            country_list_scroll_offset = 0;
            country_detail_scroll_offset = 0;
            ui_invalidate_game_redraw(hwnd, GAME_REDRAW_MAP_DYNAMIC | GAME_REDRAW_SIDE_PANEL);
            return;
        }
        if (hit == COUNTRY_PANEL_HIT_CIVIL_UNREST) {
            game_pause_for_modal_or_action();
            if (selected_civ >= 0 && !game_request_trigger_civil_unrest(selected_civ)) MessageBeep(MB_ICONWARNING);
            country_detail_scroll_offsets[clamp(country_detail_subtab, 0, COUNTRY_DETAIL_TAB_COUNT - 1)] = 0;
            ui_invalidate_game_redraw(hwnd, GAME_REDRAW_TOP_BAR | GAME_REDRAW_BOTTOM_BAR |
                                      GAME_REDRAW_MAP_DYNAMIC | GAME_REDRAW_SIDE_PANEL);
            return;
        }
        if (hit == COUNTRY_PANEL_HIT_VASSAL_ACTION) {
            int vassal_id = country_panel_vassal_action_target(client, mouse_x, mouse_y);
            game_pause_for_modal_or_action();
            if (vassal_id < 0 || !game_request_release_vassal(vassal_id)) MessageBeep(MB_ICONWARNING);
            ui_invalidate_game_redraw(hwnd, GAME_REDRAW_TOP_BAR | GAME_REDRAW_BOTTOM_BAR |
                                      GAME_REDRAW_MAP_DYNAMIC | GAME_REDRAW_SIDE_PANEL); return; }
        if (hit == COUNTRY_PANEL_HIT_COLOR) {
            if (selected_civ >= 0) {
                game_pause_for_modal_or_action();
                color_picker_open_civ(selected_civ, ui_snapshot_civ_color(selected_civ));
            }
            ui_invalidate_full(hwnd); return; }
        if (hit == COUNTRY_PANEL_HIT_LOCATE) {
            if (selected_civ >= 0) ui_locate_civ(hwnd, selected_civ);
            ui_invalidate_map_viewport(hwnd);
            return;
        }
        if (hit <= COUNTRY_PANEL_HIT_SUBTAB_BASE &&
            hit > COUNTRY_PANEL_HIT_SUBTAB_BASE - COUNTRY_DETAIL_TAB_COUNT) {
            country_detail_subtab = COUNTRY_PANEL_HIT_SUBTAB_BASE - hit;
            country_detail_subtab = clamp(country_detail_subtab, 0, COUNTRY_DETAIL_TAB_COUNT - 1);
            ui_invalidate_side_panel(hwnd);
            return;
        }
        if (hit <= COUNTRY_PANEL_HIT_DIPLOMACY_VIEW_BASE &&
            hit >= COUNTRY_PANEL_HIT_DIPLOMACY_VIEW_BASE - DIPLOMACY_VIEW_OTHER) {
            country_diplomacy_view = COUNTRY_PANEL_HIT_DIPLOMACY_VIEW_BASE - hit;
            country_detail_scroll_offsets[COUNTRY_DETAIL_DIPLOMACY] = 0;
            ui_invalidate_side_panel(hwnd);
            return;
        }
        if (hit <= COUNTRY_PANEL_HIT_SORT_POPULATION && hit >= COUNTRY_PANEL_HIT_SORT_DISORDER) {
            int column = COUNTRY_PANEL_HIT_SORT_POPULATION - hit;
            if (country_sort_column == column) country_sort_descending = !country_sort_descending;
            else {
                country_sort_column = column;
                country_sort_descending = 1;
            }
            country_list_scroll_offset = 0;
            ui_invalidate_side_panel(hwnd);
            return;
        }
        if (hit >= 0 && ui_snapshot_civ_alive(hit)) {
            ui_select_civ(hit, UI_SELECT_SOURCE_COUNTRY_LIST);
            ui_invalidate_game_redraw(hwnd, GAME_REDRAW_MAP_DYNAMIC | GAME_REDRAW_SIDE_PANEL);
            return;
        }
    }
    if (panel_tab == PANEL_WORLD) {
        WorldgenLayout layout;
        worldgen_layout_build(client, side_panel_w, worldgen_scroll_offset, &layout);
        if (ui_forms_handle_worldgen_random_click(hwnd, client, mouse_x, mouse_y)) return;
        for (i = 0; i < MAP_SIZE_COUNT; i++) {
            if (point_in_rect(layout.map_size_buttons[i], mouse_x, mouse_y)) {
                pending_map_size = i;
                ui_invalidate_side_panel(hwnd);
                return;
            }
        }
        if (worldgen_rect_visible(layout.viewport, layout.civ_color_preview) &&
            point_in_rect(layout.civ_color_preview, mouse_x, mouse_y)) {
            game_pause_for_modal_or_action();
            color_picker_open_setup(selected_civ_color);
            ui_invalidate_full(hwnd);
            return;
        }
    }
    slider = setup_slider_hit_test(hwnd, mouse_x, mouse_y);
    if (slider >= 0) {
        dragging_slider = slider;
        update_setup_slider(hwnd, slider, mouse_x);
        SetCapture(hwnd);
        return;
    }
    if (divider_hit_test(hwnd, mouse_x, mouse_y)) {
        dragging_panel = 1;
        SetCapture(hwnd);
        return;
    }
    ui_select_tile_from_mouse(hwnd, mouse_x, mouse_y);
}
static void handle_mouse_move(HWND hwnd, int mouse_x, int mouse_y) {
    RECT client;
    TRACKMOUSEEVENT track;
    int old_hover_x = hover_x;
    int old_hover_y = hover_y;
    int was_panel;
    int is_panel;
    GetClientRect(hwnd, &client);
    if (color_picker_mouse_move(hwnd, client, mouse_x, mouse_y)) return;
    if (color_picker_active()) return;
    if (pause_menu_open) return;
    if (!tracking_mouse_leave) {
        memset(&track, 0, sizeof(track));
        track.cbSize = sizeof(track);
        track.dwFlags = TME_LEAVE;
        track.hwndTrack = hwnd;
        tracking_mouse_leave = TrackMouseEvent(&track) ? 1 : 0;
    }
    hover_x = mouse_x;
    hover_y = mouse_y;
    was_panel = old_hover_x >= client.right - side_panel_w && old_hover_y >= TOP_BAR_H && old_hover_y <= client.bottom;
    is_panel = mouse_x >= client.right - side_panel_w && mouse_y >= TOP_BAR_H && mouse_y <= client.bottom;
    if ((panel_tab == PANEL_COUNTRY || panel_tab == PANEL_POPULATION ||
         panel_tab == PANEL_PLAGUE || panel_tab == PANEL_DEBUG) &&
        (was_panel || is_panel) && panel_hover_target(client, mouse_x, mouse_y) != last_panel_hover_target) {
        last_panel_hover_target = panel_hover_target(client, mouse_x, mouse_y);
        invalidate_side_panel(hwnd);
    }
    if (debug_panel_event_scrollbar_drag(mouse_y)) { ui_invalidate_side_panel(hwnd); return; }
    if (dragging_map) {
        map_interaction_preview = 1;
        map_offset_x += mouse_x - last_mouse_x;
        map_offset_y += mouse_y - last_mouse_y;
        map_view_auto_centered = 0; ui_map_view_clamp(client);
        last_mouse_x = mouse_x;
        last_mouse_y = mouse_y;
        ui_invalidate_map_viewport(hwnd);
        return;
    }
    if (dragging_panel) {
        side_panel_w = clamp(client.right - mouse_x, MIN_SIDE_PANEL_W, MAX_SIDE_PANEL_W);
        side_panel_expanded_w = side_panel_w; ui_map_view_clamp(client);
        ui_forms_layout(hwnd);
        ui_invalidate_full(hwnd);
        return;
    }
    if (dragging_slider >= 0) {
        update_setup_slider(hwnd, dragging_slider, mouse_x);
        return;
    }
    if (divider_hit_test(hwnd, mouse_x, mouse_y)) {
        SetCursor(LoadCursor(NULL, IDC_SIZEWE));
    }
}
static void handle_mouse_leave(HWND hwnd) {
    tracking_mouse_leave = 0;
    if (hover_x >= 0 || hover_y >= 0) {
        hover_x = -1;
        hover_y = -1;
        last_panel_hover_target = -1;
        invalidate_side_panel(hwnd);
    }
}
static void handle_mouse_up(HWND hwnd) {
    int was_dragging_map = dragging_map;
    if (color_picker_mouse_up(hwnd)) return;
    if (debug_panel_event_scrollbar_is_dragging()) { debug_panel_event_scrollbar_end_drag(); ReleaseCapture(); ui_invalidate_side_panel(hwnd); return; }
    if (dragging_panel || dragging_slider >= 0 || dragging_map) {
        dragging_panel = 0;
        dragging_slider = -1;
        dragging_map = 0;
        if (was_dragging_map) map_interaction_preview = 0;
        ReleaseCapture();
        if (was_dragging_map) ui_invalidate_map_viewport(hwnd);
        else ui_invalidate_full(hwnd);
    }
}
static void handle_right_mouse_down(HWND hwnd, int mouse_x, int mouse_y) {
    RECT client;
    GetClientRect(hwnd, &client);
    if (color_picker_active()) return;
    if (pause_menu_open) return;
    if (mouse_x >= client.right - side_panel_w || mouse_y < TOP_BAR_H || mouse_y > client.bottom - BOTTOM_BAR_H) return;
    dragging_map = 1;
    map_interaction_preview = 1;
    last_mouse_x = mouse_x;
    last_mouse_y = mouse_y;
    SetCapture(hwnd);
}
int handle_shortcut(HWND hwnd, WPARAM key) {
    if (key == VK_ESCAPE) {
        game_pause_for_modal_or_action();
        if (color_picker_active()) {
            color_picker_close();
            ui_invalidate_full(hwnd);
            return 1;
        }
        pause_menu_open = !pause_menu_open;
        ui_forms_layout(hwnd);
        ui_invalidate_full(hwnd);
        return 1;
    }
    if (color_picker_active()) return 1;
    if (pause_menu_open) return 1;
    if (key == VK_SPACE) {
        game_toggle_auto_run();
        ui_invalidate_game_redraw(hwnd, GAME_REDRAW_TOP_BAR | GAME_REDRAW_BOTTOM_BAR);
        return 1;
    }
    if (key == VK_F1) {
        ui_forms_add_civ(hwnd);
        return 1;
    }
    if (key == VK_F2) {
        ui_forms_apply_selected(hwnd);
        return 1;
    }
    if (key == VK_F5 || key == 'R') {
        ui_forms_read_world_setup_controls();
        game_request_new_world_with_progress(hwnd);
        ui_invalidate_full(hwnd);
        return 1;
    }
    return 0;
}
int is_game_shortcut(WPARAM key) {
    return key == VK_SPACE || key == VK_F1 || key == VK_F2 || key == VK_F5 || key == VK_ESCAPE;
}
int is_game_char_shortcut(WPARAM key) { return key == ' '; }
int handle_char_shortcut(HWND hwnd, WPARAM key) {
    if (color_picker_active()) return 1;
    if (pause_menu_open) return 1;
    if (key == ' ') return handle_shortcut(hwnd, VK_SPACE);
    return 0;
}
LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_CREATE:
            ui_forms_create(hwnd);
            SetTimer(hwnd, TIMER_ID, FRAME_TIMER_MS, NULL);
            return 0;
        case WM_SIZE:
            { RECT client; GetClientRect(hwnd, &client); ui_side_panel_apply_state(client); }
            ui_forms_layout(hwnd);
            ui_invalidate_full(hwnd);
            return 0;
        case WM_COMMAND:
            if (pause_menu_open) return 0;
            if (HIWORD(wparam) == EN_CHANGE && ui_forms_handle_metric_change(LOWORD(wparam))) return 0;
            if (HIWORD(wparam) == EN_KILLFOCUS && ui_forms_normalize_metric_edit(LOWORD(wparam))) return 0;
            if (ui_forms_handle_command(hwnd, LOWORD(wparam))) return 0;
            return 0;
        case WM_CTLCOLOREDIT:
            { HBRUSH brush = ui_forms_control_color(wparam, lparam); if (brush) return (LRESULT)brush; }
            return DefWindowProc(hwnd, msg, wparam, lparam);
        case WM_TIMER:
            if (wparam == MAP_PREVIEW_TIMER_ID) {
                map_interaction_preview = 0;
                KillTimer(hwnd, MAP_PREVIEW_TIMER_ID);
                ui_wheel_invalidate_map_viewport(hwnd);
                return 0;
            }
            if (wparam == WHEEL_INPUT_TIMER_ID) {
                ui_wheel_process_pending(hwnd);
                return 0;
            }
            if (wparam == TIMER_ID && !pause_menu_open && !color_picker_active()) {
                int redraw = game_tick_auto_run();
                if (ui_highlight_pulse_active()) redraw |= GAME_REDRAW_MAP_DYNAMIC | GAME_REDRAW_SIDE_PANEL;
                if (redraw) ui_invalidate_game_redraw(hwnd, redraw);
            }
            return 0;
        case WM_LBUTTONDOWN:
            handle_mouse_down(hwnd, LOWORD(lparam), HIWORD(lparam));
            return 0;
        case WM_MOUSEMOVE:
            handle_mouse_move(hwnd, LOWORD(lparam), HIWORD(lparam));
            return 0;
        case WM_MOUSELEAVE:
            handle_mouse_leave(hwnd);
            return 0;
        case WM_LBUTTONUP:
            handle_mouse_up(hwnd);
            return 0;
        case WM_RBUTTONDOWN:
            handle_right_mouse_down(hwnd, LOWORD(lparam), HIWORD(lparam));
            return 0;
        case WM_RBUTTONUP:
            handle_mouse_up(hwnd);
            return 0;
        case WM_MOUSEWHEEL:
            ui_wheel_accumulate(hwnd, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam), GET_WHEEL_DELTA_WPARAM(wparam));
            return 0;
        case WM_KEYDOWN:
            handle_shortcut(hwnd, wparam);
            return 0;
        case WM_PAINT:
            paint_window(hwnd);
            ui_forms_redraw_visible_controls();
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_DESTROY:
            KillTimer(hwnd, TIMER_ID);
            KillTimer(hwnd, MAP_PREVIEW_TIMER_ID);
            KillTimer(hwnd, WHEEL_INPUT_TIMER_ID);
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProc(hwnd, msg, wparam, lparam);
    }
}
