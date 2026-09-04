#include "ui/ui_worldgen_legacy_forms.h"

#include "data/country_names.h"
#include "ui/ui_worldgen_config_adapter.h"

#include <stdio.h>
#include <stdlib.h>

static FormControls form;
static int suppress_form_change;

static HWND create_edit(HWND parent, const char *text, int id,
                        int number_only, int max_chars) {
    DWORD style = WS_CHILD | WS_CLIPSIBLINGS | ES_AUTOHSCROLL;
    HWND edit;
    if (number_only) style |= ES_NUMBER;
    edit = CreateWindowExW(0, L"EDIT", L"", style, 0, 0, 80, 24,
                           parent, (HMENU)(INT_PTR)id,
                           GetModuleHandle(NULL), NULL);
    if (edit && max_chars > 0) {
        SendMessageW(edit, EM_SETLIMITTEXT, (WPARAM)max_chars, 0);
    }
    if (edit) ui_worldgen_legacy_forms_set_text_utf8(edit, text);
    return edit;
}

static void redraw_control(HWND control) {
    if (!control || !IsWindowVisible(control)) return;
    RedrawWindow(control, NULL, NULL, RDW_INVALIDATE | RDW_NOERASE);
}

static void hide_control(HWND control) {
    if (control && IsWindowVisible(control)) ShowWindow(control, SW_HIDE);
}

static void layout_control(HWND control, RECT viewport, RECT rect, int show) {
    int contained = rect.left >= viewport.left && rect.right <= viewport.right &&
                    rect.top >= viewport.top && rect.bottom <= viewport.bottom;
    int should_show = show && contained;
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
        MoveWindow(control, rect.left, rect.top,
                   rect.right - rect.left, rect.bottom - rect.top, FALSE);
        changed = 1;
    }
    is_visible = IsWindowVisible(control);
    if (!!is_visible != !!should_show) {
        ShowWindow(control, should_show ? SW_SHOWNA : SW_HIDE);
        changed = 1;
    }
    if (changed && should_show) redraw_control(control);
}

void ui_worldgen_legacy_forms_get_text_utf8(HWND control, char *buffer,
                                            int buffer_size) {
    WCHAR wide[128];
    int written;
    if (!buffer || buffer_size <= 0) return;
    buffer[0] = '\0';
    if (!control) return;
    GetWindowTextW(control, wide, (int)(sizeof(wide) / sizeof(wide[0])));
    written = WideCharToMultiByte(CP_UTF8, 0, wide, -1, buffer,
                                  buffer_size, NULL, NULL);
    if (written <= 0) buffer[0] = '\0';
    buffer[buffer_size - 1] = '\0';
}

void ui_worldgen_legacy_forms_set_text_utf8(HWND control, const char *text) {
    WCHAR wide[128];
    if (!control) return;
    MultiByteToWideChar(CP_UTF8, 0, text ? text : "", -1, wide,
                        (int)(sizeof(wide) / sizeof(wide[0])));
    suppress_form_change++;
    SetWindowTextW(control, wide);
    suppress_form_change--;
}

int ui_worldgen_legacy_forms_read_int(HWND control, int fallback,
                                      int min_value, int max_value,
                                      int normalize) {
    char buffer[32];
    char *end;
    long parsed;
    int value;
    if (!control) return fallback;
    GetWindowTextA(control, buffer, sizeof(buffer));
    parsed = strtol(buffer, &end, 10);
    value = end == buffer ? fallback : (int)parsed;
    if (value < min_value) value = min_value;
    if (value > max_value) value = max_value;
    if (normalize) ui_worldgen_legacy_forms_write_int(control, value);
    return value;
}

void ui_worldgen_legacy_forms_write_int(HWND control, int value) {
    char buffer[32];
    if (!control) return;
    snprintf(buffer, sizeof(buffer), "%d", value);
    suppress_form_change++;
    SetWindowTextA(control, buffer);
    suppress_form_change--;
}

static HWND initial_civs_control_from_id(int control_id) {
    if (control_id == ID_INITIAL_CIVS_EDIT) return form.initial_civs_edit;
    if (control_id == ID_HYDROLOGY_INITIAL_CIVS_EDIT)
        return form.hydrology_initial_civs_edit;
    return NULL;
}

static int initial_civ_cap(void) {
    return ui_worldgen_initial_civ_cap_for_map_size(
        ui_worldgen_config_get_field(UI_WORLDGEN_FIELD_PENDING_MAP_SIZE));
}

static int read_strict_decimal(HWND control, long *out_value) {
    char buffer[32];
    char *end;
    char *scan;
    long value;
    if (!control || !out_value) return 0;
    GetWindowTextA(control, buffer, sizeof(buffer));
    if (!buffer[0]) return 0;
    scan = buffer;
    if (*scan == '+' || *scan == '-') scan++;
    if (!*scan) return 0;
    while (*scan) {
        if (*scan < '0' || *scan > '9') return 0;
        scan++;
    }
    value = strtol(buffer, &end, 10);
    if (end == buffer || *end != '\0') return 0;
    *out_value = value;
    return 1;
}

static int control_has_visible_style(HWND control) {
    return control && (GetWindowLongPtrW(control, GWL_STYLE) & WS_VISIBLE) != 0;
}

void ui_worldgen_legacy_forms_create(HWND parent) {
    form.name_edit = create_edit(parent, "New Realm", ID_NAME_EDIT, 0, NAME_LEN - 1);
    form.symbol_edit = create_edit(parent, "N", ID_SYMBOL_EDIT, 0, 1);
    form.military_edit = create_edit(parent, "5", ID_MILITARY_EDIT, 1, 2);
    form.logistics_edit = create_edit(parent, "5", ID_LOGISTICS_EDIT, 1, 2);
    form.governance_edit = create_edit(parent, "5", ID_GOVERNANCE_EDIT, 1, 2);
    form.cohesion_edit = create_edit(parent, "5", ID_COHESION_EDIT, 1, 2);
    form.production_edit = create_edit(parent, "5", ID_PRODUCTION_EDIT, 1, 2);
    form.commerce_edit = create_edit(parent, "5", ID_COMMERCE_EDIT, 1, 2);
    form.innovation_edit = create_edit(parent, "5", ID_INNOVATION_EDIT, 1, 2);
    form.initial_civs_edit = create_edit(parent, "0", ID_INITIAL_CIVS_EDIT, 1, 3);
    form.hydrology_initial_civs_edit = create_edit(
        parent, "0", ID_HYDROLOGY_INITIAL_CIVS_EDIT, 1, 3);
    form.region_custom_edit = create_edit(parent, "70", ID_REGION_CUSTOM_EDIT, 1, 3);
    form.add_button = CreateWindowA(
        "BUTTON", "Add Civilization",
        WS_CHILD | WS_CLIPSIBLINGS | BS_PUSHBUTTON,
        0, 0, 140, 30, parent, (HMENU)ID_ADD_BUTTON,
        GetModuleHandle(NULL), NULL);
    form.apply_button = CreateWindowA(
        "BUTTON", "Apply Selected",
        WS_CHILD | WS_CLIPSIBLINGS | BS_PUSHBUTTON,
        0, 0, 150, 30, parent, (HMENU)ID_APPLY_BUTTON,
        GetModuleHandle(NULL), NULL);
}

void ui_worldgen_legacy_forms_localize_name(void) {
    char name[NAME_LEN];
    int heritage;
    int name_id;
    if (!form.name_edit) return;
    ui_worldgen_legacy_forms_get_text_utf8(form.name_edit, name, sizeof(name));
    if (country_name_find_by_text_any(name, &heritage, &name_id)) {
        ui_worldgen_legacy_forms_set_text_utf8(
            form.name_edit,
            country_name_localized_for_heritage(heritage, name_id, ui_language));
    }
}

void ui_worldgen_legacy_forms_hide(void) {
    hide_control(form.initial_civs_edit);
    hide_control(form.hydrology_initial_civs_edit);
    hide_control(form.region_custom_edit);
    hide_control(form.name_edit);
    hide_control(form.symbol_edit);
    hide_control(form.military_edit);
    hide_control(form.logistics_edit);
    hide_control(form.governance_edit);
    hide_control(form.cohesion_edit);
    hide_control(form.production_edit);
    hide_control(form.commerce_edit);
    hide_control(form.innovation_edit);
    hide_control(form.add_button);
    hide_control(form.apply_button);
}

void ui_worldgen_legacy_forms_layout(const UiWorldgenLegacyFormsLayout *input) {
    const WorldgenLayout *layout;
    if (!input) {
        ui_worldgen_legacy_forms_hide();
        return;
    }
    layout = input->legacy_layout;
    if (layout) {
        layout_control(form.initial_civs_edit, layout->viewport,
                       layout->initial_input, input->show_legacy);
        layout_control(form.name_edit, layout->viewport, layout->name_input,
                       input->show_legacy);
        layout_control(form.symbol_edit, layout->viewport, layout->symbol_input,
                       input->show_legacy);
        layout_control(form.military_edit, layout->viewport,
                       layout->metric_input[WORLDGEN_METRIC_MILITARY], input->show_legacy);
        layout_control(form.logistics_edit, layout->viewport,
                       layout->metric_input[WORLDGEN_METRIC_LOGISTICS], input->show_legacy);
        layout_control(form.governance_edit, layout->viewport,
                       layout->metric_input[WORLDGEN_METRIC_GOVERNANCE], input->show_legacy);
        layout_control(form.cohesion_edit, layout->viewport,
                       layout->metric_input[WORLDGEN_METRIC_COHESION], input->show_legacy);
        layout_control(form.production_edit, layout->viewport,
                       layout->metric_input[WORLDGEN_METRIC_PRODUCTION], input->show_legacy);
        layout_control(form.commerce_edit, layout->viewport,
                       layout->metric_input[WORLDGEN_METRIC_COMMERCE], input->show_legacy);
        layout_control(form.innovation_edit, layout->viewport,
                       layout->metric_input[WORLDGEN_METRIC_INNOVATION], input->show_legacy);
        layout_control(form.add_button, layout->viewport, layout->add_button,
                       input->show_legacy);
        layout_control(form.apply_button, layout->viewport, layout->apply_button,
                       input->show_legacy);
    } else {
        ui_worldgen_legacy_forms_hide();
    }
    layout_control(form.region_custom_edit, input->region_custom_viewport,
                   input->region_custom_input, input->show_region_custom);
    layout_control(form.hydrology_initial_civs_edit,
                   input->hydrology_initial_viewport,
                   input->hydrology_initial_input,
                   input->show_hydrology_initial);
}

const FormControls *ui_worldgen_legacy_forms_controls(void) { return &form; }

int ui_worldgen_legacy_forms_is_edit_id(int id) {
    return id == ID_NAME_EDIT || id == ID_SYMBOL_EDIT ||
           id == ID_INITIAL_CIVS_EDIT ||
           id == ID_HYDROLOGY_INITIAL_CIVS_EDIT ||
           id == ID_REGION_CUSTOM_EDIT ||
           (id >= ID_MILITARY_EDIT && id <= ID_INNOVATION_EDIT);
}

int ui_worldgen_legacy_forms_handle_change(int id) {
    if (suppress_form_change) return -1;
    return id == ID_INITIAL_CIVS_EDIT ||
           id == ID_HYDROLOGY_INITIAL_CIVS_EDIT ||
           id == ID_REGION_CUSTOM_EDIT ||
           (id >= ID_MILITARY_EDIT && id <= ID_INNOVATION_EDIT);
}

int ui_worldgen_legacy_forms_custom_region_has_focus(void) {
    return form.region_custom_edit && GetFocus() == form.region_custom_edit;
}

int ui_worldgen_legacy_forms_focused_numeric_id(void) {
    HWND focused = GetFocus();
    if (focused == form.initial_civs_edit &&
        control_has_visible_style(focused)) return ID_INITIAL_CIVS_EDIT;
    if (focused == form.hydrology_initial_civs_edit &&
        control_has_visible_style(focused))
        return ID_HYDROLOGY_INITIAL_CIVS_EDIT;
    if (focused == form.region_custom_edit &&
        control_has_visible_style(focused)) return ID_REGION_CUSTOM_EDIT;
    return 0;
}

int ui_worldgen_legacy_forms_commit_custom_region(int fallback, int *out_value) {
    int value;
    if (!form.region_custom_edit) return 0;
    value = ui_worldgen_legacy_forms_read_int(form.region_custom_edit,
                                              fallback, 0, 100, 1);
    if (out_value) *out_value = value;
    return 1;
}

int ui_worldgen_legacy_forms_try_read_initial_civs(int control_id,
                                                    int *out_value) {
    long value;
    int capped;
    HWND control = initial_civs_control_from_id(control_id);
    if (!read_strict_decimal(control, &value)) return 0;
    capped = (int)value;
    if (capped < 0) capped = 0;
    if (capped > initial_civ_cap()) capped = initial_civ_cap();
    if (value != capped) ui_worldgen_legacy_forms_write_int(control, capped);
    if (out_value) *out_value = capped;
    return 1;
}

int ui_worldgen_legacy_forms_commit_initial_civs(int control_id, int fallback,
                                                  int *out_value) {
    long parsed;
    int value;
    HWND control = initial_civs_control_from_id(control_id);
    if (!control) return 0;
    value = read_strict_decimal(control, &parsed) ? (int)parsed : fallback;
    if (value < 0) value = 0;
    if (value > initial_civ_cap()) value = initial_civ_cap();
    ui_worldgen_legacy_forms_write_int(control, value);
    if (out_value) *out_value = value;
    return 1;
}

void ui_worldgen_legacy_forms_mirror_initial_civs(int source_control_id,
                                                   int value) {
    HWND peer = source_control_id == ID_INITIAL_CIVS_EDIT ?
                    form.hydrology_initial_civs_edit :
                    form.initial_civs_edit;
    ui_worldgen_legacy_forms_write_int(peer, value);
}

void ui_worldgen_legacy_forms_write_initial_civs(int value) {
    ui_worldgen_legacy_forms_write_int(form.initial_civs_edit, value);
    ui_worldgen_legacy_forms_write_int(form.hydrology_initial_civs_edit, value);
}

void ui_worldgen_legacy_forms_commit_numeric_edits(int initial_fallback,
                                                    int custom_fallback,
                                                    int *out_initial,
                                                    int *out_custom) {
    int focused_id = ui_worldgen_legacy_forms_focused_numeric_id();
    if (focused_id == ID_INITIAL_CIVS_EDIT ||
        focused_id == ID_HYDROLOGY_INITIAL_CIVS_EDIT) {
        ui_worldgen_legacy_forms_commit_initial_civs(
            focused_id, initial_fallback, out_initial);
    } else if (out_initial) {
        *out_initial = initial_fallback;
    }
    ui_worldgen_legacy_forms_commit_custom_region(custom_fallback, out_custom);
}

void ui_worldgen_legacy_forms_write_region_custom(int value) {
    ui_worldgen_legacy_forms_write_int(form.region_custom_edit, value);
}

HBRUSH ui_worldgen_legacy_forms_control_color(WPARAM wparam, LPARAM lparam) {
    static HBRUSH edit_brush;
    int id = GetDlgCtrlID((HWND)lparam);
    HDC hdc = (HDC)wparam;
    if (!ui_worldgen_legacy_forms_is_edit_id(id)) return NULL;
    if (!edit_brush) edit_brush = CreateSolidBrush(RGB(32, 39, 43));
    SetTextColor(hdc, RGB(235, 241, 244));
    SetBkColor(hdc, RGB(32, 39, 43));
    return edit_brush;
}

void ui_worldgen_legacy_forms_redraw_visible(void) {
    redraw_control(form.initial_civs_edit);
    redraw_control(form.hydrology_initial_civs_edit);
    redraw_control(form.region_custom_edit);
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
