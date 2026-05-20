#include "ui_forms.h"

#include "data/country_names.h"
#include "game/game.h"
#include "sim/simulation.h"
#include "ui/ui_invalidation.h"
#include "ui/ui_layout.h"
#include "ui/ui_worldgen_layout.h"

#include <stdio.h>
#include <stdlib.h>

static FormControls form;
static int suppress_form_change = 0;

static void get_window_text_utf8(HWND hwnd, char *buffer, int buffer_size) {
    WCHAR wide[128];
    int written;

    if (!buffer || buffer_size <= 0) return;
    buffer[0] = '\0';
    GetWindowTextW(hwnd, wide, (int)(sizeof(wide) / sizeof(wide[0])));
    written = WideCharToMultiByte(CP_UTF8, 0, wide, -1, buffer, buffer_size, NULL, NULL);
    if (written <= 0) buffer[0] = '\0';
    buffer[buffer_size - 1] = '\0';
}

static void set_window_text_utf8(HWND hwnd, const char *text) {
    WCHAR wide[128];

    MultiByteToWideChar(CP_UTF8, 0, text ? text : "", -1, wide, (int)(sizeof(wide) / sizeof(wide[0])));
    suppress_form_change++;
    SetWindowTextW(hwnd, wide);
    suppress_form_change--;
}

static int read_int_control_clamped(HWND hwnd, int fallback, int min_value, int max_value) {
    char buffer[32];
    char *end;
    long value;

    GetWindowTextA(hwnd, buffer, sizeof(buffer));
    value = strtol(buffer, &end, 10);
    if (end == buffer) return fallback;
    return clamp((int)value, min_value, max_value);
}

static void write_int_control(HWND hwnd, int value) {
    char buffer[32];

    snprintf(buffer, sizeof(buffer), "%d", value);
    suppress_form_change++;
    SetWindowTextA(hwnd, buffer);
    suppress_form_change--;
}

static HWND metric_edit_from_id(int control_id) {
    switch (control_id) {
        case ID_MILITARY_EDIT: return form.military_edit;
        case ID_LOGISTICS_EDIT: return form.logistics_edit;
        case ID_GOVERNANCE_EDIT: return form.governance_edit;
        case ID_COHESION_EDIT: return form.cohesion_edit;
        case ID_PRODUCTION_EDIT: return form.production_edit;
        case ID_COMMERCE_EDIT: return form.commerce_edit;
        case ID_INNOVATION_EDIT: return form.innovation_edit;
        default: return NULL;
    }
}

static int read_metric_control(HWND edit, int fallback) {
    char buffer[32];
    char *end;
    long value;
    int normalized;

    GetWindowTextA(edit, buffer, sizeof(buffer));
    value = strtol(buffer, &end, 10);
    normalized = end == buffer ? fallback : (int)value;
    normalized = clamp(normalized, 0, 10);
    write_int_control(edit, normalized);
    return normalized;
}

int ui_forms_handle_metric_change(int control_id) {
    HWND edit = metric_edit_from_id(control_id);

    if (suppress_form_change) return 1;
    if (!edit) return 0;
    return 1;
}

int ui_forms_normalize_metric_edit(int control_id) {
    HWND edit = metric_edit_from_id(control_id);

    if (!edit) return 0;
    read_metric_control(edit, 5);
    return 1;
}

static int selected_edit_civ_id(void) {
    int civ_id = selected_civ;

    if (civ_id < 0 || civ_id >= civ_count) {
        if (selected_x >= 0 && selected_y >= 0) civ_id = world[selected_y][selected_x].owner;
    }
    return civ_id >= 0 && civ_id < civ_count ? civ_id : -1;
}

static int alive_symbol_used(char symbol) {
    int i;
    for (i = 0; i < civ_count; i++) {
        if (civs[i].alive && civs[i].symbol == symbol) return 1;
    }
    return 0;
}

static char random_unused_symbol(void) {
    int start = rnd(26);
    int i;
    for (i = 0; i < 26; i++) {
        char symbol = (char)('A' + (start + i) % 26);
        if (!alive_symbol_used(symbol)) return symbol;
    }
    return (char)('A' + rnd(26));
}

static int random_metric_value(void) {
    int roll = rnd(100);
    if (roll < 75) return 3 + rnd(6);
    if (roll < 88) return rnd(3);
    return 9 + rnd(2);
}

static int random_range(int min_value, int max_value) {
    return min_value + rnd(max_value - min_value + 1);
}

static void sync_color_from_civ(int civ_id) {
    if (civ_id < 0 || civ_id >= civ_count) return;
    selected_civ_color = civs[civ_id].color;
    selected_civ_color_index = -1;
}

void ui_forms_write_civ(int civ_id) {
    char buffer[32];

    if (civ_id < 0 || civ_id >= civ_count) return;
    set_window_text_utf8(form.name_edit, civilization_display_name(civ_id));
    snprintf(buffer, sizeof(buffer), "%c", civs[civ_id].symbol);
    set_window_text_utf8(form.symbol_edit, buffer);
    write_int_control(form.military_edit, civs[civ_id].military);
    write_int_control(form.logistics_edit, civs[civ_id].logistics);
    write_int_control(form.governance_edit, civs[civ_id].governance);
    write_int_control(form.cohesion_edit, civs[civ_id].cohesion);
    write_int_control(form.production_edit, civs[civ_id].production);
    write_int_control(form.commerce_edit, civs[civ_id].commerce);
    write_int_control(form.innovation_edit, civs[civ_id].innovation);
    sync_color_from_civ(civ_id);
}

static void ui_randomize_civilization_form(HWND hwnd) {
    int name_id = civilization_pick_unused_name_id();
    char symbol_text[2] = {random_unused_symbol(), '\0'};
    set_window_text_utf8(form.name_edit, country_name_localized(name_id, ui_language));
    set_window_text_utf8(form.symbol_edit, symbol_text);
    write_int_control(form.military_edit, random_metric_value());
    write_int_control(form.logistics_edit, random_metric_value());
    write_int_control(form.governance_edit, random_metric_value());
    write_int_control(form.cohesion_edit, random_metric_value());
    write_int_control(form.production_edit, random_metric_value());
    write_int_control(form.commerce_edit, random_metric_value());
    write_int_control(form.innovation_edit, random_metric_value());
    selected_civ_color = game_preview_civilization_color_auto_avoid(-1, selected_civ_color);
    selected_civ_color_index = 0;
    ui_invalidate_side_panel(hwnd);
}

static void ui_randomize_physical_world_sliders(HWND hwnd) {
    ocean_slider = random_range(25, 75);
    continent_slider = random_range(10, 85);
    relief_slider = random_range(20, 85);
    moisture_slider = random_range(15, 85);
    drought_slider = random_range(15, 85);
    vegetation_slider = random_range(15, 85);
    ui_invalidate_side_panel(hwnd);
}

static void ui_randomize_advanced_world_sliders(HWND hwnd) {
    bias_forest_slider = random_range(20, 80);
    bias_desert_slider = random_range(20, 80);
    bias_mountain_slider = random_range(20, 80);
    bias_wetland_slider = random_range(20, 80);
    region_size_slider = random_range(15, 90);
    ui_invalidate_side_panel(hwnd);
}

static int worldgen_button_hit(RECT viewport, RECT rect, int mouse_x, int mouse_y) {
    return worldgen_rect_visible(viewport, rect) && point_in_rect(rect, mouse_x, mouse_y);
}

int ui_forms_handle_worldgen_random_click(HWND hwnd, RECT client, int mouse_x, int mouse_y) {
    WorldgenLayout layout;

    worldgen_layout_build(client, side_panel_w, worldgen_scroll_offset, &layout);
    if (worldgen_button_hit(layout.viewport, layout.physical_random_button, mouse_x, mouse_y)) {
        ui_randomize_physical_world_sliders(hwnd);
        return 1;
    }
    if (worldgen_button_hit(layout.viewport, layout.terrain_random_button, mouse_x, mouse_y)) {
        ui_randomize_advanced_world_sliders(hwnd);
        return 1;
    }
    if (worldgen_button_hit(layout.viewport, layout.civ_random_button, mouse_x, mouse_y)) {
        ui_randomize_civilization_form(hwnd);
        return 1;
    }
    return 0;
}

void ui_forms_translate_name_input(void) {
    char name[NAME_LEN];
    int name_id;

    if (!form.name_edit) return;
    get_window_text_utf8(form.name_edit, name, sizeof(name));
    name_id = country_name_find_by_text(name);
    if (name_id >= 0) set_window_text_utf8(form.name_edit, country_name_localized(name_id, ui_language));
}

void ui_forms_refresh_language(HWND hwnd) {
    ui_forms_translate_name_input();
    (void)hwnd;
}

void ui_forms_add_civ(HWND hwnd) {
    char name[NAME_LEN];
    char symbol_text[8];
    char symbol;
    int civ_id;

    get_window_text_utf8(form.name_edit, name, sizeof(name));
    GetWindowTextA(form.symbol_edit, symbol_text, sizeof(symbol_text));
    symbol = symbol_text[0] ? symbol_text[0] : (char)('A' + civ_count);
    civ_id = game_request_add_civilization_from_selection(
        name, symbol,
        read_metric_control(form.military_edit, 5),
        read_metric_control(form.logistics_edit, 5),
        read_metric_control(form.governance_edit, 5),
        read_metric_control(form.cohesion_edit, 5),
        read_metric_control(form.production_edit, 5),
        read_metric_control(form.commerce_edit, 5),
        read_metric_control(form.innovation_edit, 5));
    if (civ_id >= 0) {
        if (selected_civ_color_index >= 0) game_request_set_civilization_color_exact(civ_id, selected_civ_color);
        ui_forms_write_civ(civ_id);
    } else {
        MessageBoxA(hwnd,
                    "Could not add civilization. The world may already be full, or there is no valid empty land. Select an empty land tile or rebuild with more land.",
                    "Add Civilization", MB_OK | MB_ICONINFORMATION);
    }
    ui_invalidate_side_panel(hwnd);
}

void ui_forms_apply_selected(HWND hwnd) {
    char name[NAME_LEN];
    char symbol_text[8];
    int civ_id = selected_edit_civ_id();
    int fallback_military = civ_id >= 0 ? civs[civ_id].military : 5;
    int fallback_logistics = civ_id >= 0 ? civs[civ_id].logistics : 5;
    int fallback_governance = civ_id >= 0 ? civs[civ_id].governance : 5;
    int fallback_cohesion = civ_id >= 0 ? civs[civ_id].cohesion : 5;
    int fallback_production = civ_id >= 0 ? civs[civ_id].production : 5;
    int fallback_commerce = civ_id >= 0 ? civs[civ_id].commerce : 5;
    int fallback_innovation = civ_id >= 0 ? civs[civ_id].innovation : 5;

    get_window_text_utf8(form.name_edit, name, sizeof(name));
    GetWindowTextA(form.symbol_edit, symbol_text, sizeof(symbol_text));
    if (game_request_edit_selected_civilization(
            name, symbol_text[0],
            read_metric_control(form.military_edit, fallback_military),
            read_metric_control(form.logistics_edit, fallback_logistics),
            read_metric_control(form.governance_edit, fallback_governance),
            read_metric_control(form.cohesion_edit, fallback_cohesion),
            read_metric_control(form.production_edit, fallback_production),
            read_metric_control(form.commerce_edit, fallback_commerce),
            read_metric_control(form.innovation_edit, fallback_innovation))) {
        if (selected_civ_color_index >= 0) game_request_set_civilization_color_exact(selected_civ, selected_civ_color);
        ui_forms_write_civ(selected_civ);
        ui_invalidate_side_panel(hwnd);
    }
}

int ui_forms_handle_command(HWND hwnd, int control_id) {
    if (control_id == ID_ADD_BUTTON) ui_forms_add_civ(hwnd);
    else if (control_id == ID_APPLY_BUTTON) ui_forms_apply_selected(hwnd);
    else return 0;
    return 1;
}

static int form_edit_id(int id) {
    return id == ID_NAME_EDIT || id == ID_SYMBOL_EDIT || id == ID_INITIAL_CIVS_EDIT ||
           (id >= ID_MILITARY_EDIT && id <= ID_INNOVATION_EDIT);
}

HBRUSH ui_forms_control_color(WPARAM wparam, LPARAM lparam) {
    static HBRUSH edit_brush = NULL;
    int id = GetDlgCtrlID((HWND)lparam);
    HDC hdc = (HDC)wparam;

    if (!form_edit_id(id)) return NULL;
    if (!edit_brush) edit_brush = CreateSolidBrush(RGB(32, 39, 43));
    SetTextColor(hdc, RGB(235, 241, 244));
    SetBkColor(hdc, RGB(32, 39, 43));
    return edit_brush;
}

static void redraw_control(HWND control) {
    if (!control || !IsWindowVisible(control)) return;
    RedrawWindow(control, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
}

void ui_forms_redraw_visible_controls(void) {
    redraw_control(form.initial_civs_edit);
    redraw_control(form.name_edit);
    redraw_control(form.symbol_edit);
    redraw_control(form.military_edit);
    redraw_control(form.logistics_edit);
    redraw_control(form.governance_edit);
    redraw_control(form.cohesion_edit);
    redraw_control(form.production_edit);
    redraw_control(form.commerce_edit);
    redraw_control(form.innovation_edit);
    redraw_control(form.add_button);
    redraw_control(form.apply_button);
}

static void layout_control(HWND control, RECT viewport, RECT rect, int show_world) {
    int intersects = rect.right > viewport.left && rect.left < viewport.right &&
                     rect.bottom > viewport.top && rect.top < viewport.bottom;
    int should_show = show_world && intersects;
    RECT current;
    POINT top_left;
    int is_visible;
    int changed = 0;

    if (!control) return;
    GetWindowRect(control, &current);
    top_left.x = current.left;
    top_left.y = current.top;
    MapWindowPoints(NULL, GetParent(control), &top_left, 1);
    current.right = top_left.x + (current.right - current.left);
    current.bottom = top_left.y + (current.bottom - current.top);
    current.left = top_left.x;
    current.top = top_left.y;
    if (current.left != rect.left || current.top != rect.top ||
        current.right - current.left != rect.right - rect.left ||
        current.bottom - current.top != rect.bottom - rect.top) {
        MoveWindow(control, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, FALSE);
        changed = 1;
    }
    is_visible = IsWindowVisible(control);
    if (!!is_visible != !!should_show) {
        ShowWindow(control, should_show ? SW_SHOWNA : SW_HIDE);
        changed = 1;
    }
    if (changed && should_show) redraw_control(control);
}

void ui_forms_layout(HWND hwnd) {
    RECT client;
    WorldgenLayout layout;
    int show_world = panel_tab == PANEL_WORLD && !pause_menu_open && !side_panel_collapsed;

    GetClientRect(hwnd, &client);
    ui_side_panel_apply_state(client);
    worldgen_scroll_offset = worldgen_layout_clamp_scroll(client, side_panel_w, worldgen_scroll_offset);
    worldgen_layout_build(client, side_panel_w, worldgen_scroll_offset, &layout);
    layout_control(form.initial_civs_edit, layout.viewport, layout.initial_input, show_world);
    layout_control(form.name_edit, layout.viewport, layout.name_input, show_world);
    layout_control(form.symbol_edit, layout.viewport, layout.symbol_input, show_world);
    layout_control(form.military_edit, layout.viewport, layout.metric_input[WORLDGEN_METRIC_MILITARY], show_world);
    layout_control(form.logistics_edit, layout.viewport, layout.metric_input[WORLDGEN_METRIC_LOGISTICS], show_world);
    layout_control(form.governance_edit, layout.viewport, layout.metric_input[WORLDGEN_METRIC_GOVERNANCE], show_world);
    layout_control(form.cohesion_edit, layout.viewport, layout.metric_input[WORLDGEN_METRIC_COHESION], show_world);
    layout_control(form.production_edit, layout.viewport, layout.metric_input[WORLDGEN_METRIC_PRODUCTION], show_world);
    layout_control(form.commerce_edit, layout.viewport, layout.metric_input[WORLDGEN_METRIC_COMMERCE], show_world);
    layout_control(form.innovation_edit, layout.viewport, layout.metric_input[WORLDGEN_METRIC_INNOVATION], show_world);
    layout_control(form.add_button, layout.viewport, layout.add_button, show_world);
    layout_control(form.apply_button, layout.viewport, layout.apply_button, show_world);
}

static HWND create_edit(HWND parent, const char *text, int id, int number_only, int max_chars) {
    DWORD style = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | ES_AUTOHSCROLL;
    HWND edit;

    if (number_only) style |= ES_NUMBER;
    edit = CreateWindowExW(0, L"EDIT", L"", style,
                           0, 0, 80, 24, parent, (HMENU)(INT_PTR)id, GetModuleHandle(NULL), NULL);
    if (edit && max_chars > 0) SendMessageW(edit, EM_SETLIMITTEXT, (WPARAM)max_chars, 0);
    if (edit) set_window_text_utf8(edit, text);
    return edit;
}

void ui_forms_create(HWND hwnd) {
    form.name_edit = create_edit(hwnd, "New Realm", ID_NAME_EDIT, 0, NAME_LEN - 1);
    form.symbol_edit = create_edit(hwnd, "N", ID_SYMBOL_EDIT, 0, 1);
    form.military_edit = create_edit(hwnd, "5", ID_MILITARY_EDIT, 1, 2);
    form.logistics_edit = create_edit(hwnd, "5", ID_LOGISTICS_EDIT, 1, 2);
    form.governance_edit = create_edit(hwnd, "5", ID_GOVERNANCE_EDIT, 1, 2);
    form.cohesion_edit = create_edit(hwnd, "5", ID_COHESION_EDIT, 1, 2);
    form.production_edit = create_edit(hwnd, "5", ID_PRODUCTION_EDIT, 1, 2);
    form.commerce_edit = create_edit(hwnd, "5", ID_COMMERCE_EDIT, 1, 2);
    form.innovation_edit = create_edit(hwnd, "5", ID_INNOVATION_EDIT, 1, 2);
    form.initial_civs_edit = create_edit(hwnd, "0", ID_INITIAL_CIVS_EDIT, 1, 2);
    form.add_button = CreateWindowA("BUTTON", "Add Civilization", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | BS_PUSHBUTTON,
                                    0, 0, 140, 30, hwnd, (HMENU)ID_ADD_BUTTON, GetModuleHandle(NULL), NULL);
    form.apply_button = CreateWindowA("BUTTON", "Apply Selected", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | BS_PUSHBUTTON,
                                      0, 0, 150, 30, hwnd, (HMENU)ID_APPLY_BUTTON, GetModuleHandle(NULL), NULL);
    ui_forms_layout(hwnd);
}

void ui_forms_read_world_setup_controls(void) {
    if (form.initial_civs_edit) {
        initial_civ_count = read_int_control_clamped(form.initial_civs_edit, initial_civ_count, 0, MAX_CIVS);
    }
}

void ui_forms_write_world_setup_controls(void) {
    if (form.initial_civs_edit) write_int_control(form.initial_civs_edit, initial_civ_count);
}
