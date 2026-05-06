#include "ui_forms.h"

#include "game/game.h"
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

void ui_forms_write_civ(int civ_id) {
    char buffer[32];

    if (civ_id < 0 || civ_id >= civ_count) return;
    SetWindowTextA(form.name_edit, civs[civ_id].name);
    snprintf(buffer, sizeof(buffer), "%c", civs[civ_id].symbol);
    SetWindowTextA(form.symbol_edit, buffer);
    write_int_control(form.military_edit, civs[civ_id].military);
    write_int_control(form.logistics_edit, civs[civ_id].logistics);
    write_int_control(form.governance_edit, civs[civ_id].governance);
    write_int_control(form.cohesion_edit, civs[civ_id].cohesion);
    write_int_control(form.production_edit, civs[civ_id].production);
    write_int_control(form.commerce_edit, civs[civ_id].commerce);
    write_int_control(form.innovation_edit, civs[civ_id].innovation);
}

void ui_forms_add_civ(HWND hwnd) {
    char name[NAME_LEN];
    char symbol_text[8];
    char symbol;
    int civ_id;

    GetWindowTextA(form.name_edit, name, sizeof(name));
    GetWindowTextA(form.symbol_edit, symbol_text, sizeof(symbol_text));
    if (name[0] == '\0') snprintf(name, sizeof(name), "New Realm");
    symbol = symbol_text[0] ? symbol_text[0] : (char)('A' + civ_count);
    civ_id = game_request_add_civilization_from_selection(
        name, symbol,
        read_int_control_clamped(form.military_edit, 5, 0, 10),
        read_int_control_clamped(form.logistics_edit, 5, 0, 10),
        read_int_control_clamped(form.governance_edit, 5, 0, 10),
        read_int_control_clamped(form.cohesion_edit, 5, 0, 10),
        read_int_control_clamped(form.production_edit, 5, 0, 10),
        read_int_control_clamped(form.commerce_edit, 5, 0, 10),
        read_int_control_clamped(form.innovation_edit, 5, 0, 10));
    if (civ_id >= 0) {
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

    GetWindowTextA(form.name_edit, name, sizeof(name));
    GetWindowTextA(form.symbol_edit, symbol_text, sizeof(symbol_text));
    if (game_request_edit_selected_civilization(
            name, symbol_text[0],
            read_int_control_clamped(form.military_edit, 5, 0, 10),
            read_int_control_clamped(form.logistics_edit, 5, 0, 10),
            read_int_control_clamped(form.governance_edit, 5, 0, 10),
            read_int_control_clamped(form.cohesion_edit, 5, 0, 10),
            read_int_control_clamped(form.production_edit, 5, 0, 10),
            read_int_control_clamped(form.commerce_edit, 5, 0, 10),
            read_int_control_clamped(form.innovation_edit, 5, 0, 10))) {
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
    int show_world = panel_tab == PANEL_WORLD;

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

static HWND create_edit(HWND parent, const char *text) {
    return CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", text,
                           WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                           0, 0, 80, 24, parent, NULL, GetModuleHandle(NULL), NULL);
}

void ui_forms_create(HWND hwnd) {
    form.name_edit = create_edit(hwnd, "New Realm");
    form.symbol_edit = create_edit(hwnd, "N");
    form.military_edit = create_edit(hwnd, "5");
    form.logistics_edit = create_edit(hwnd, "5");
    form.governance_edit = create_edit(hwnd, "5");
    form.cohesion_edit = create_edit(hwnd, "5");
    form.production_edit = create_edit(hwnd, "5");
    form.commerce_edit = create_edit(hwnd, "5");
    form.innovation_edit = create_edit(hwnd, "5");
    form.initial_civs_edit = create_edit(hwnd, "0");
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
