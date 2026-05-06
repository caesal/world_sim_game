#include "ui.h"

#include "game/game.h"
#include "core/game_types.h"
#include "render/render.h"
#include "ui/ui_forms.h"
#include "ui/ui_layout.h"
#include "ui/ui_sliders.h"
#include "ui/ui_types.h"
#include "ui/ui_worldgen_layout.h"
#include <string.h>
#include <windowsx.h>

static int tracking_mouse_leave = 0;

static void invalidate_side_panel(HWND hwnd) {
    RECT client;
    RECT panel;

    GetClientRect(hwnd, &client);
    panel.left = client.right - side_panel_w;
    panel.top = TOP_BAR_H;
    panel.right = client.right;
    panel.bottom = client.bottom;
    InvalidateRect(hwnd, &panel, FALSE);
}

static int selected_tile_owner(void) { return selected_x < 0 || selected_y < 0 ? -1 : world[selected_y][selected_x].owner; }

static void select_tile_from_mouse(HWND hwnd, int mouse_x, int mouse_y) {
    RECT client;
    MapLayout layout;
    int x;
    int y;
    int owner;

    if (!world_generated) return;
    GetClientRect(hwnd, &client);
    layout = get_map_layout(client);
    if (mouse_x < layout.map_x || mouse_y < layout.map_y ||
        mouse_x >= layout.map_x + layout.draw_w || mouse_y >= layout.map_y + layout.draw_h) {
        return;
    }

    x = (mouse_x - layout.map_x) * MAP_W / layout.draw_w;
    y = (mouse_y - layout.map_y) * MAP_H / layout.draw_h;
    if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) return;

    selected_x = x;
    selected_y = y;
    owner = selected_tile_owner();
    if (owner >= 0 && owner < civ_count) {
        selected_civ = owner;
        ui_forms_write_civ(owner);
    }
    InvalidateRect(hwnd, NULL, FALSE);
}

static void set_speed(int index) { speed_index = clamp(index, 0, 2); }

static void handle_mouse_down(HWND hwnd, int mouse_x, int mouse_y) {
    RECT client;
    RECT legend_toggle;
    int i;
    int slider;

    GetClientRect(hwnd, &client);
    legend_toggle = get_map_legend_toggle_rect(client);
    if (!IsRectEmpty(&legend_toggle) && point_in_rect(legend_toggle, mouse_x, mouse_y)) {
        map_legend_collapsed = !map_legend_collapsed;
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }
    if (point_in_rect(get_language_button_rect(client), mouse_x, mouse_y)) {
        ui_language = ui_language == UI_LANG_EN ? UI_LANG_ZH : UI_LANG_EN;
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }
    if (point_in_rect(get_play_button_rect(client), mouse_x, mouse_y)) {
        game_toggle_auto_run();
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }
    for (i = 0; i < 3; i++) {
        if (point_in_rect(get_speed_button_rect(client, i), mouse_x, mouse_y)) {
            set_speed(i);
            InvalidateRect(hwnd, NULL, FALSE);
            return;
        }
    }
    for (i = 0; i < PANEL_TAB_COUNT; i++) {
        if (point_in_rect(get_panel_tab_rect(client, i), mouse_x, mouse_y)) {
            panel_tab = i;
            ui_forms_layout(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
            return;
        }
    }
    if (panel_tab == PANEL_DEBUG) {
        for (i = 0; i < MAP_DISPLAY_MODE_COUNT; i++) {
            if (point_in_rect(get_mode_button_rect(client, i), mouse_x, mouse_y)) {
                display_mode = MAP_DISPLAY_MODES[i];
                InvalidateRect(hwnd, NULL, FALSE);
                return;
            }
        }
    }
    if (panel_tab == PANEL_WORLD) {
        WorldgenLayout layout;
        worldgen_layout_build(client, side_panel_w, worldgen_scroll_offset, &layout);
        for (i = 0; i < MAP_SIZE_COUNT; i++) {
            if (point_in_rect(layout.map_size_buttons[i], mouse_x, mouse_y)) {
                pending_map_size = i;
                InvalidateRect(hwnd, NULL, FALSE);
                return;
            }
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
    select_tile_from_mouse(hwnd, mouse_x, mouse_y);
}

static void handle_mouse_move(HWND hwnd, int mouse_x, int mouse_y) {
    RECT client;
    TRACKMOUSEEVENT track;
    int old_hover_x = hover_x;
    int old_hover_y = hover_y;
    int was_panel;
    int is_panel;

    GetClientRect(hwnd, &client);
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
    if ((panel_tab == PANEL_SELECTION || panel_tab == PANEL_COUNTRY || panel_tab == PANEL_DIPLOMACY ||
         panel_tab == PANEL_POPULATION || panel_tab == PANEL_PLAGUE || panel_tab == PANEL_DEBUG) &&
        (was_panel || is_panel) && (old_hover_x != hover_x || old_hover_y != hover_y)) {
        invalidate_side_panel(hwnd);
    }
    if (dragging_map) {
        map_interaction_preview = 1;
        map_offset_x += mouse_x - last_mouse_x;
        map_offset_y += mouse_y - last_mouse_y;
        last_mouse_x = mouse_x;
        last_mouse_y = mouse_y;
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }
    if (dragging_panel) {
        side_panel_w = clamp(client.right - mouse_x, MIN_SIDE_PANEL_W, MAX_SIDE_PANEL_W);
        ui_forms_layout(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
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
        invalidate_side_panel(hwnd);
    }
}

static void handle_mouse_up(HWND hwnd) {
    int was_dragging_map = dragging_map;
    if (dragging_panel || dragging_slider >= 0 || dragging_map) {
        dragging_panel = 0;
        dragging_slider = -1;
        dragging_map = 0;
        if (was_dragging_map) map_interaction_preview = 0;
        ReleaseCapture();
        InvalidateRect(hwnd, NULL, FALSE);
    }
}

static void handle_right_mouse_down(HWND hwnd, int mouse_x, int mouse_y) {
    RECT client;

    GetClientRect(hwnd, &client);
    if (mouse_x >= client.right - side_panel_w || mouse_y < TOP_BAR_H || mouse_y > client.bottom - BOTTOM_BAR_H) return;
    dragging_map = 1;
    map_interaction_preview = 1;
    last_mouse_x = mouse_x;
    last_mouse_y = mouse_y;
    SetCapture(hwnd);
}

static void handle_mouse_wheel(HWND hwnd, int screen_x, int screen_y, int delta) {
    RECT client;
    POINT point;
    MapLayout before;
    int tile_x_scaled;
    int tile_y_scaled;
    int old_zoom = map_zoom_percent;
    int steps = delta / WHEEL_DELTA;

    point.x = screen_x;
    point.y = screen_y;
    ScreenToClient(hwnd, &point);
    GetClientRect(hwnd, &client);
    if (steps == 0) steps = delta > 0 ? 1 : -1;
    if (point.x >= client.right - side_panel_w) {
        if (panel_tab == PANEL_WORLD && point.y >= TOP_BAR_H && point.y <= client.bottom) {
            int old_scroll = worldgen_scroll_offset;
            worldgen_scroll_offset = worldgen_layout_clamp_scroll(
                client, side_panel_w, worldgen_scroll_offset - steps * 72);
            if (worldgen_scroll_offset != old_scroll) {
                ui_forms_layout(hwnd);
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        return;
    }
    if (point.y < TOP_BAR_H || point.y > client.bottom - BOTTOM_BAR_H) return;

    before = get_map_layout(client);
    tile_x_scaled = (point.x - before.map_x) * 100000 / before.draw_w;
    tile_y_scaled = (point.y - before.map_y) * 100000 / before.draw_h;

    map_zoom_percent = clamp(map_zoom_percent + steps * 5, 25, 700);
    if (map_zoom_percent == old_zoom) return;

    {
        MapLayout after = get_map_layout(client);
        map_offset_x += point.x - (after.map_x + tile_x_scaled * after.draw_w / 100000);
        map_offset_y += point.y - (after.map_y + tile_y_scaled * after.draw_h / 100000);
    }
    map_interaction_preview = 1;
    KillTimer(hwnd, MAP_PREVIEW_TIMER_ID);
    SetTimer(hwnd, MAP_PREVIEW_TIMER_ID, 140, NULL);
    InvalidateRect(hwnd, NULL, FALSE);
}

int handle_shortcut(HWND hwnd, WPARAM key) {
    if (key == VK_SPACE) {
        game_toggle_auto_run();
        InvalidateRect(hwnd, NULL, FALSE);
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
        game_request_new_world();
        InvalidateRect(hwnd, NULL, FALSE);
        return 1;
    }
    if (key == VK_ESCAPE) {
        DestroyWindow(hwnd);
        return 1;
    }
    return 0;
}

int is_game_shortcut(WPARAM key) {
    return key == VK_SPACE || key == VK_F1 || key == VK_F2 || key == VK_F5 || key == VK_ESCAPE;
}

int is_game_char_shortcut(WPARAM key) { return key == ' '; }

int handle_char_shortcut(HWND hwnd, WPARAM key) {
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
            side_panel_w = clamp(side_panel_w, MIN_SIDE_PANEL_W, MAX_SIDE_PANEL_W);
            ui_forms_layout(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        case WM_COMMAND:
            if (LOWORD(wparam) == ID_ADD_BUTTON) ui_forms_add_civ(hwnd);
            else if (LOWORD(wparam) == ID_APPLY_BUTTON) ui_forms_apply_selected(hwnd);
            return 0;
        case WM_TIMER:
            if (wparam == MAP_PREVIEW_TIMER_ID) {
                map_interaction_preview = 0;
                KillTimer(hwnd, MAP_PREVIEW_TIMER_ID);
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            if (wparam == TIMER_ID && game_tick_auto_run()) {
                InvalidateRect(hwnd, NULL, FALSE);
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
            handle_mouse_wheel(hwnd, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam), GET_WHEEL_DELTA_WPARAM(wparam));
            return 0;
        case WM_KEYDOWN:
            handle_shortcut(hwnd, wparam);
            return 0;
        case WM_PAINT:
            paint_window(hwnd);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_DESTROY:
            KillTimer(hwnd, TIMER_ID);
            KillTimer(hwnd, MAP_PREVIEW_TIMER_ID);
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProc(hwnd, msg, wparam, lparam);
    }
}
