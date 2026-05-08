#include "ui_forms.h"

#include "game/game.h"
#include "sim/simulation.h"
#include "ui/ui_worldgen_layout.h"

#include <stdio.h>
#include <stdlib.h>

static FormControls form;

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
    SetWindowTextA(hwnd, buffer);
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
    char buffer[32];
    char *end;
    long value;

    if (!edit) return 0;
    GetWindowTextA(edit, buffer, sizeof(buffer));
    if (buffer[0] == '\0') return 1;
    value = strtol(buffer, &end, 10);
    if (end != buffer && value > 10) write_int_control(edit, 10);
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

static int palette_index_for_color(Color32 color) {
    int i;

    for (i = 0; i < CIV_COLOR_PALETTE_COUNT; i++) {
        if (UI_CIV_COLOR_PALETTE[i] == color) return i;
    }
    return -1;
}

static void sync_color_from_civ(int civ_id) {
    if (civ_id < 0 || civ_id >= civ_count) return;
    selected_civ_color = civs[civ_id].color;
    selected_civ_color_index = palette_index_for_color(civs[civ_id].color);
}

void ui_forms_write_civ(int civ_id) {
    char buffer[32];

    if (civ_id < 0 || civ_id >= civ_count) return;
    SetWindowTextA(form.name_edit, civilization_display_name(civ_id));
    snprintf(buffer, sizeof(buffer), "%c", civs[civ_id].symbol);
    SetWindowTextA(form.symbol_edit, buffer);
    write_int_control(form.military_edit, civs[civ_id].military);
    write_int_control(form.logistics_edit, civs[civ_id].logistics);
    write_int_control(form.governance_edit, civs[civ_id].governance);
    write_int_control(form.cohesion_edit, civs[civ_id].cohesion);
    write_int_control(form.production_edit, civs[civ_id].production);
    write_int_control(form.commerce_edit, civs[civ_id].commerce);
    write_int_control(form.innovation_edit, civs[civ_id].innovation);
    sync_color_from_civ(civ_id);
}

void ui_forms_add_civ(HWND hwnd) {
    char name[NAME_LEN];
    char symbol_text[8];
    char symbol;
    int civ_id;

    GetWindowTextA(form.name_edit, name, sizeof(name));
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
        if (selected_civ_color_index >= 0) {
            game_request_set_civilization_color(civ_id, selected_civ_color);
        }
        ui_forms_write_civ(civ_id);
    } else {
        MessageBoxA(hwnd,
                    "Could not add civilization. The world may already be full, or there is no valid empty land. Select an empty land tile or rebuild with more land.",
                    "Add Civilization", MB_OK | MB_ICONINFORMATION);
    }
    InvalidateRect(hwnd, NULL, FALSE);
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

    GetWindowTextA(form.name_edit, name, sizeof(name));
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
        if (selected_civ_color_index >= 0) {
            game_request_set_civilization_color(selected_civ, selected_civ_color);
        }
        ui_forms_write_civ(selected_civ);
        InvalidateRect(hwnd, NULL, FALSE);
    }
}

static void layout_control(HWND control, RECT viewport, RECT rect, int show_world) {
    int fully_visible = rect.left >= viewport.left && rect.right <= viewport.right &&
                        rect.top >= viewport.top && rect.bottom <= viewport.bottom;

    MoveWindow(control, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, TRUE);
    ShowWindow(control, show_world && fully_visible ? SW_SHOW : SW_HIDE);
}

void ui_forms_layout(HWND hwnd) {
    RECT client;
    WorldgenLayout layout;
    int show_world = panel_tab == PANEL_WORLD && !pause_menu_open;

    GetClientRect(hwnd, &client);
    side_panel_w = clamp(side_panel_w, MIN_SIDE_PANEL_W, MAX_SIDE_PANEL_W);
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
    DWORD style = WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL;
    HWND edit;

    if (number_only) style |= ES_NUMBER;
    edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", text, style,
                           0, 0, 80, 24, parent, (HMENU)(INT_PTR)id, GetModuleHandle(NULL), NULL);
    if (edit && max_chars > 0) SendMessageA(edit, EM_SETLIMITTEXT, (WPARAM)max_chars, 0);
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
    form.add_button = CreateWindowA("BUTTON", "Add Civilization", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                    0, 0, 140, 30, hwnd, (HMENU)ID_ADD_BUTTON, GetModuleHandle(NULL), NULL);
    form.apply_button = CreateWindowA("BUTTON", "Apply Selected", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
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
