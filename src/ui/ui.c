#include "ui.h"

#include "game/game.h"
#include "core/game_types.h"
#include "render/render.h"
#include "sim/simulation.h"
#include "ui/ui_layout.h"
#include "ui/ui_sliders.h"
#include "ui/ui_types.h"
#include "world/terrain_query.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windowsx.h>

static int tracking_mouse_leave = 0;
static FormControls form;

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

static int read_int_control_clamped(HWND hwnd, int fallback, int min, int max) {
    char buffer[32];
    char *end;
    long value;

    GetWindowTextA(hwnd, buffer, sizeof(buffer));
    value = strtol(buffer, &end, 10);
    if (end == buffer) return fallback;
    return clamp((int)value, min, max);
}

static void write_civ_to_form(int civ_id) {
    char buffer[32];

    if (civ_id < 0 || civ_id >= civ_count) return;
    SetWindowTextA(form.name_edit, civs[civ_id].name);
    snprintf(buffer, sizeof(buffer), "%c", civs[civ_id].symbol);
    SetWindowTextA(form.symbol_edit, buffer);
    snprintf(buffer, sizeof(buffer), "%d", civs[civ_id].aggression);
    SetWindowTextA(form.aggression_edit, buffer);
    snprintf(buffer, sizeof(buffer), "%d", civs[civ_id].expansion);
    SetWindowTextA(form.expansion_edit, buffer);
    snprintf(buffer, sizeof(buffer), "%d", civs[civ_id].defense);
    SetWindowTextA(form.defense_edit, buffer);
    snprintf(buffer, sizeof(buffer), "%d", civs[civ_id].culture);
    SetWindowTextA(form.culture_edit, buffer);
}

static void add_civ_from_form(HWND hwnd) {
    char name[NAME_LEN];
    char symbol_text[8];
    char symbol;
    int x = -1;
    int y = -1;
    int before_count = civ_count;
    int added;

    GetWindowTextA(form.name_edit, name, sizeof(name));
    GetWindowTextA(form.symbol_edit, symbol_text, sizeof(symbol_text));
    if (name[0] == '\0') snprintf(name, sizeof(name), "New Realm");
    symbol = symbol_text[0] ? symbol_text[0] : (char)('A' + civ_count);

    if (selected_x >= 0 && selected_y >= 0 && is_land(world[selected_y][selected_x].geography) && world[selected_y][selected_x].owner == -1) {
        x = selected_x;
        y = selected_y;
    }

    added = add_civilization_at(name, symbol,
                                read_int_control_clamped(form.aggression_edit, 5, 0, 10),
                                read_int_control_clamped(form.expansion_edit, 5, 0, 10),
                                read_int_control_clamped(form.defense_edit, 5, 0, 10),
                                read_int_control_clamped(form.culture_edit, 5, 0, 10),
                                x, y);
    if (added) {
        selected_civ = before_count;
        write_civ_to_form(selected_civ);
    } else {
        MessageBoxA(hwnd,
                    "Could not add civilization. The world may already be full, or there is no valid empty land. Select an empty land tile or rebuild with more land.",
                    "Add Civilization",
                    MB_OK | MB_ICONINFORMATION);
    }
    InvalidateRect(hwnd, NULL, FALSE);
}

static int selected_tile_owner(void) {
    if (selected_x < 0 || selected_y < 0) return -1;
    return world[selected_y][selected_x].owner;
}

static void apply_form_to_selected_civ(HWND hwnd) {
    char name[NAME_LEN];
    char symbol_text[8];
    int civ_id = selected_civ;

    if (civ_id < 0 || civ_id >= civ_count) civ_id = selected_tile_owner();
    if (civ_id < 0 || civ_id >= civ_count) return;

    GetWindowTextA(form.name_edit, name, sizeof(name));
    GetWindowTextA(form.symbol_edit, symbol_text, sizeof(symbol_text));
    simulation_apply_civilization_edit(civ_id, name, symbol_text[0],
                                       read_int_control_clamped(form.aggression_edit, civs[civ_id].aggression, 0, 10),
                                       read_int_control_clamped(form.expansion_edit, civs[civ_id].expansion, 0, 10),
                                       read_int_control_clamped(form.defense_edit, civs[civ_id].defense, 0, 10),
                                       read_int_control_clamped(form.culture_edit, civs[civ_id].culture, 0, 10));
    selected_civ = civ_id;
    InvalidateRect(hwnd, NULL, FALSE);
}

static void set_form_visibility(void) {
    int show_civ = panel_tab == PANEL_CIV ? SW_SHOW : SW_HIDE;
    int show_map = panel_tab == PANEL_MAP ? SW_SHOW : SW_HIDE;

    ShowWindow(form.name_edit, show_civ);
    ShowWindow(form.symbol_edit, show_civ);
    ShowWindow(form.aggression_edit, show_civ);
    ShowWindow(form.expansion_edit, show_civ);
    ShowWindow(form.defense_edit, show_civ);
    ShowWindow(form.culture_edit, show_civ);
    ShowWindow(form.add_button, show_civ);
    ShowWindow(form.apply_button, show_civ);
    ShowWindow(form.initial_civs_edit, show_map);
}

static void layout_form_controls(HWND hwnd) {
    RECT client;
    int panel_x;
    int y;

    GetClientRect(hwnd, &client);
    side_panel_w = clamp(side_panel_w, MIN_SIDE_PANEL_W, MAX_SIDE_PANEL_W);
    panel_x = client.right - side_panel_w + FORM_X_PAD;
    y = client.bottom - 214;

    MoveWindow(form.initial_civs_edit, panel_x + 150, TOP_BAR_H + 254, 48, 24, TRUE);
    MoveWindow(form.name_edit, panel_x, y, 160, 24, TRUE);
    MoveWindow(form.symbol_edit, panel_x + 176, y, 50, 24, TRUE);
    y += 60;
    MoveWindow(form.aggression_edit, panel_x, y, 58, 24, TRUE);
    MoveWindow(form.expansion_edit, panel_x + 82, y, 58, 24, TRUE);
    MoveWindow(form.defense_edit, panel_x + 164, y, 58, 24, TRUE);
    MoveWindow(form.culture_edit, panel_x + 246, y, 58, 24, TRUE);
    y += 42;
    MoveWindow(form.add_button, panel_x, y, 140, 30, TRUE);
    MoveWindow(form.apply_button, panel_x + 156, y, 150, 30, TRUE);
    set_form_visibility();
}

static HWND create_edit(HWND parent, const char *text) {
    return CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", text,
                           WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                           0, 0, 80, 24, parent, NULL, GetModuleHandle(NULL), NULL);
}

static void create_form_controls(HWND hwnd) {
    form.name_edit = create_edit(hwnd, "New Realm");
    form.symbol_edit = create_edit(hwnd, "N");
    form.aggression_edit = create_edit(hwnd, "5");
    form.expansion_edit = create_edit(hwnd, "5");
    form.defense_edit = create_edit(hwnd, "5");
    form.culture_edit = create_edit(hwnd, "5");
    form.initial_civs_edit = create_edit(hwnd, "4");
    form.add_button = CreateWindowA("BUTTON", "Add Civilization", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                    0, 0, 140, 30, hwnd, (HMENU)ID_ADD_BUTTON, GetModuleHandle(NULL), NULL);
    form.apply_button = CreateWindowA("BUTTON", "Apply Selected", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                      0, 0, 150, 30, hwnd, (HMENU)ID_APPLY_BUTTON, GetModuleHandle(NULL), NULL);
    layout_form_controls(hwnd);
}

static void select_tile_from_mouse(HWND hwnd, int mouse_x, int mouse_y) {
    RECT client;
    MapLayout layout;
    int x;
    int y;
    int owner;

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
        write_civ_to_form(owner);
    }
    InvalidateRect(hwnd, NULL, FALSE);
}

static void set_speed(HWND hwnd, int index) {
    speed_index = clamp(index, 0, 2);
    KillTimer(hwnd, TIMER_ID);
    SetTimer(hwnd, TIMER_ID, SPEED_MS[speed_index], NULL);
}

static void read_world_setup_controls(void) {
    if (form.initial_civs_edit) {
        initial_civ_count = read_int_control_clamped(form.initial_civs_edit, initial_civ_count, 0, MAX_CIVS);
    }
}

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
            set_speed(hwnd, i);
            InvalidateRect(hwnd, NULL, FALSE);
            return;
        }
    }
    for (i = 0; i < PANEL_TAB_COUNT; i++) {
        if (point_in_rect(get_panel_tab_rect(client, i), mouse_x, mouse_y)) {
            panel_tab = i;
            layout_form_controls(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
            return;
        }
    }
    if (panel_tab == PANEL_MAP) {
        for (i = 0; i < 4; i++) {
            if (point_in_rect(get_mode_button_rect(client, i), mouse_x, mouse_y)) {
                display_mode = MAP_DISPLAY_MODES[i];
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
    if ((panel_tab == PANEL_INFO || panel_tab == PANEL_DIPLOMACY) &&
        (was_panel || is_panel) && (old_hover_x != hover_x || old_hover_y != hover_y)) {
        invalidate_side_panel(hwnd);
    }
    if (dragging_map) {
        map_offset_x += mouse_x - last_mouse_x;
        map_offset_y += mouse_y - last_mouse_y;
        last_mouse_x = mouse_x;
        last_mouse_y = mouse_y;
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }
    if (dragging_panel) {
        side_panel_w = clamp(client.right - mouse_x, MIN_SIDE_PANEL_W, MAX_SIDE_PANEL_W);
        layout_form_controls(hwnd);
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
    if (dragging_panel || dragging_slider >= 0 || dragging_map) {
        dragging_panel = 0;
        dragging_slider = -1;
        dragging_map = 0;
        ReleaseCapture();
        InvalidateRect(hwnd, NULL, FALSE);
    }
}

static void handle_right_mouse_down(HWND hwnd, int mouse_x, int mouse_y) {
    RECT client;

    GetClientRect(hwnd, &client);
    if (mouse_x >= client.right - side_panel_w || mouse_y < TOP_BAR_H || mouse_y > client.bottom - BOTTOM_BAR_H) return;
    dragging_map = 1;
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
    if (point.x >= client.right - side_panel_w || point.y < TOP_BAR_H || point.y > client.bottom - BOTTOM_BAR_H) return;
    if (steps == 0) steps = delta > 0 ? 1 : -1;

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
    InvalidateRect(hwnd, NULL, FALSE);
}

int handle_shortcut(HWND hwnd, WPARAM key) {
    if (key == VK_SPACE) {
        game_toggle_auto_run();
        InvalidateRect(hwnd, NULL, FALSE);
        return 1;
    }
    if (key == VK_F1) {
        add_civ_from_form(hwnd);
        return 1;
    }
    if (key == VK_F2) {
        apply_form_to_selected_civ(hwnd);
        return 1;
    }
    if (key == VK_F5 || key == 'R') {
        read_world_setup_controls();
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

int is_game_char_shortcut(WPARAM key) {
    return key == ' ';
}

int handle_char_shortcut(HWND hwnd, WPARAM key) {
    if (key == ' ') return handle_shortcut(hwnd, VK_SPACE);
    return 0;
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_CREATE:
            create_form_controls(hwnd);
            SetTimer(hwnd, TIMER_ID, SPEED_MS[speed_index], NULL);
            return 0;
        case WM_SIZE:
            side_panel_w = clamp(side_panel_w, MIN_SIDE_PANEL_W, MAX_SIDE_PANEL_W);
            layout_form_controls(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        case WM_COMMAND:
            if (LOWORD(wparam) == ID_ADD_BUTTON) add_civ_from_form(hwnd);
            else if (LOWORD(wparam) == ID_APPLY_BUTTON) apply_form_to_selected_civ(hwnd);
            return 0;
        case WM_TIMER:
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
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProc(hwnd, msg, wparam, lparam);
    }
}
