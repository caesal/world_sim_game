#include "ui_forms.h"

#include "data/country_names.h"
#include "game/game.h"
#include "game/game_loop.h"
#include "sim/simulation.h"
#include "ui/ui_invalidation.h"
#include "ui/ui_layout.h"
#include "ui/ui_worldgen_config_adapter.h"
#include "ui/ui_worldgen_control_state.h"
#include "ui/ui_worldgen_legacy_forms.h"
#include "ui/ui_worldgen_random.h"
#include "ui/ui_worldgen_view.h"

#include <stdio.h>

static const FormControls *controls(void) {
    return ui_worldgen_legacy_forms_controls();
}

static HWND metric_edit_from_id(int control_id) {
    const FormControls *form = controls();
    switch (control_id) {
        case ID_MILITARY_EDIT: return form->military_edit;
        case ID_LOGISTICS_EDIT: return form->logistics_edit;
        case ID_GOVERNANCE_EDIT: return form->governance_edit;
        case ID_COHESION_EDIT: return form->cohesion_edit;
        case ID_PRODUCTION_EDIT: return form->production_edit;
        case ID_COMMERCE_EDIT: return form->commerce_edit;
        case ID_INNOVATION_EDIT: return form->innovation_edit;
        default: return NULL;
    }
}

static int read_metric_control(HWND edit, int fallback) {
    return ui_worldgen_legacy_forms_read_int(edit, fallback, 0, 10, 1);
}

static void commit_initial_civs(int control_id) {
    int value = ui_worldgen_config_get_field(UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT);
    if (ui_worldgen_legacy_forms_commit_initial_civs(
            control_id, value, &value)) {
        ui_worldgen_control_state_set_field(UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT,
                                            value);
        ui_worldgen_legacy_forms_write_initial_civs(value);
    }
}

void ui_forms_commit_region_custom(void) {
    const UiWorldgenControlState *state = ui_worldgen_control_state_get();
    int value = ui_worldgen_config_get_field(UI_WORLDGEN_FIELD_REGION_SIZE);
    if (state->region_category != UI_WORLDGEN_REGION_CUSTOM) {
        ui_worldgen_legacy_forms_write_region_custom(value);
        return;
    }
    if (ui_worldgen_legacy_forms_commit_custom_region(value, &value)) {
        ui_worldgen_control_state_set_region_custom_value(value);
        ui_worldgen_legacy_forms_write_region_custom(
            ui_worldgen_config_get_field(UI_WORLDGEN_FIELD_REGION_SIZE));
    }
}

void ui_forms_commit_worldgen_numeric_edits(void) {
    const UiWorldgenControlState *state = ui_worldgen_control_state_get();
    int initial = ui_worldgen_config_get_field(UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT);
    int custom = ui_worldgen_config_get_field(UI_WORLDGEN_FIELD_REGION_SIZE);
    ui_worldgen_legacy_forms_commit_numeric_edits(initial, custom,
                                                   &initial, &custom);
    ui_worldgen_control_state_set_field(UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT,
                                        initial);
    ui_worldgen_legacy_forms_write_initial_civs(initial);
    if (state->region_category == UI_WORLDGEN_REGION_CUSTOM) {
        ui_worldgen_control_state_set_region_custom_value(custom);
    }
    ui_worldgen_legacy_forms_write_region_custom(
        ui_worldgen_config_get_field(UI_WORLDGEN_FIELD_REGION_SIZE));
}

int ui_forms_region_custom_has_focus(void) {
    return ui_worldgen_legacy_forms_custom_region_has_focus();
}

int ui_forms_handle_metric_change(int control_id) {
    int handled = ui_worldgen_legacy_forms_handle_change(control_id);
    int value;
    if (!handled) return 0;
    if (handled < 0) return 1;
    if (control_id == ID_INITIAL_CIVS_EDIT ||
        control_id == ID_HYDROLOGY_INITIAL_CIVS_EDIT) {
        if (ui_worldgen_legacy_forms_try_read_initial_civs(
                control_id, &value)) {
            ui_worldgen_control_state_set_field(
                UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT, value);
            ui_worldgen_legacy_forms_mirror_initial_civs(control_id, value);
        }
    }
    return 1;
}

int ui_forms_normalize_metric_edit(int control_id) {
    HWND edit = metric_edit_from_id(control_id);
    if (control_id == ID_INITIAL_CIVS_EDIT ||
        control_id == ID_HYDROLOGY_INITIAL_CIVS_EDIT) {
        commit_initial_civs(control_id);
        return 1;
    }
    if (control_id == ID_REGION_CUSTOM_EDIT) {
        ui_forms_commit_region_custom();
        return 1;
    }
    if (!edit) return 0;
    read_metric_control(edit, 5);
    return 1;
}

int ui_forms_commit_focused_worldgen_numeric(void) {
    int control_id = ui_worldgen_legacy_forms_focused_numeric_id();
    return control_id && ui_forms_normalize_metric_edit(control_id);
}

static int selected_edit_civ_id(void) {
    int civ_id = selected_civ;
    if (civ_id < 0 || civ_id >= civ_count) {
        if (selected_x >= 0 && selected_y >= 0) {
            civ_id = world[selected_y][selected_x].owner;
        }
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

static Color32 random_preview_civ_color(void) {
    Color32 old_color = selected_civ_color;
    Color32 color = old_color;
    int i;
    for (i = 0; i < 8; i++) {
        Color32 preferred = COLOR32_RGB(
            ui_worldgen_random_range(42, 238),
            ui_worldgen_random_range(42, 238),
            ui_worldgen_random_range(42, 238));
        color = game_preview_civilization_color_auto_avoid(-1, preferred);
        if (color != old_color) return color;
    }
    color = game_preview_civilization_color_auto_avoid(-1, 0);
    if (color != old_color) return color;
    return COLOR32_RGB((int)((old_color & 0xff) + 73) & 0xff,
                       (int)(((old_color >> 8) & 0xff) + 127) & 0xff,
                       (int)(((old_color >> 16) & 0xff) + 191) & 0xff);
}

static void sync_color_from_civ(int civ_id) {
    if (civ_id < 0 || civ_id >= civ_count) return;
    selected_civ_color = civs[civ_id].color;
    selected_civ_color_index = -1;
}

void ui_forms_write_civ(int civ_id) {
    const FormControls *form = controls();
    char buffer[32];
    if (civ_id < 0 || civ_id >= civ_count) return;
    ui_worldgen_legacy_forms_set_text_utf8(
        form->name_edit, civilization_display_name(civ_id));
    snprintf(buffer, sizeof(buffer), "%c", civs[civ_id].symbol);
    ui_worldgen_legacy_forms_set_text_utf8(form->symbol_edit, buffer);
    ui_worldgen_legacy_forms_write_int(form->military_edit, civs[civ_id].military);
    ui_worldgen_legacy_forms_write_int(form->logistics_edit, civs[civ_id].logistics);
    ui_worldgen_legacy_forms_write_int(form->governance_edit, civs[civ_id].governance);
    ui_worldgen_legacy_forms_write_int(form->cohesion_edit, civs[civ_id].cohesion);
    ui_worldgen_legacy_forms_write_int(form->production_edit, civs[civ_id].production);
    ui_worldgen_legacy_forms_write_int(form->commerce_edit, civs[civ_id].commerce);
    ui_worldgen_legacy_forms_write_int(form->innovation_edit, civs[civ_id].innovation);
    sync_color_from_civ(civ_id);
}

static void ui_randomize_civilization_form(HWND hwnd) {
    const FormControls *form = controls();
    int heritage = civilization_heritage_or_default(rnd(CIV_HERITAGE_COUNT));
    int name_id = civilization_pick_unused_name_id_for_heritage(heritage);
    char symbol_text[2] = {random_unused_symbol(), '\0'};
    ui_worldgen_legacy_forms_set_text_utf8(
        form->name_edit,
        country_name_localized_for_heritage(heritage, name_id, ui_language));
    ui_worldgen_legacy_forms_set_text_utf8(form->symbol_edit, symbol_text);
    ui_worldgen_legacy_forms_write_int(form->military_edit, random_metric_value());
    ui_worldgen_legacy_forms_write_int(form->logistics_edit, random_metric_value());
    ui_worldgen_legacy_forms_write_int(form->governance_edit, random_metric_value());
    ui_worldgen_legacy_forms_write_int(form->cohesion_edit, random_metric_value());
    ui_worldgen_legacy_forms_write_int(form->production_edit, random_metric_value());
    ui_worldgen_legacy_forms_write_int(form->commerce_edit, random_metric_value());
    ui_worldgen_legacy_forms_write_int(form->innovation_edit, random_metric_value());
    selected_civ_color = random_preview_civ_color();
    selected_civ_color_index = -1;
    ui_invalidate_side_panel(hwnd);
}

static void ui_randomize_physical_world_sliders(HWND hwnd) {
    ui_worldgen_randomize_physical();
    ui_worldgen_control_state_resync(UI_WORLDGEN_RESYNC_KEEP_APPLIED);
    ui_forms_write_world_setup_controls();
    ui_invalidate_side_panel(hwnd);
}

static void ui_randomize_advanced_world_sliders(HWND hwnd) {
    ui_worldgen_randomize_advanced();
    ui_worldgen_control_state_resync(UI_WORLDGEN_RESYNC_KEEP_APPLIED);
    ui_forms_write_world_setup_controls();
    ui_invalidate_side_panel(hwnd);
}

static int worldgen_button_hit(RECT viewport, RECT rect,
                               int mouse_x, int mouse_y) {
    return ui_worldgen_panel_hit_test(viewport, rect, mouse_x, mouse_y);
}

int ui_forms_handle_worldgen_random_click_in_layout(
    HWND hwnd, const WorldgenLayout *layout, int mouse_x, int mouse_y) {
    if (!layout) return 0;
    if (worldgen_button_hit(layout->viewport, layout->physical_random_button,
                            mouse_x, mouse_y)) {
        ui_randomize_physical_world_sliders(hwnd);
        return 1;
    }
    if (worldgen_button_hit(layout->viewport, layout->terrain_random_button,
                            mouse_x, mouse_y)) {
        ui_randomize_advanced_world_sliders(hwnd);
        return 1;
    }
    if (worldgen_button_hit(layout->viewport, layout->civ_random_button,
                            mouse_x, mouse_y)) {
        ui_randomize_civilization_form(hwnd);
        return 1;
    }
    return 0;
}

int ui_forms_handle_worldgen_random_click(HWND hwnd, RECT client,
                                          int mouse_x, int mouse_y) {
    UiWorldgenPanelLayout panel;
    WorldgenLayout layout;
    ui_worldgen_view_build(client, side_panel_w, &panel);
    if (panel.tab != UI_WORLDGEN_TAB_LEGACY) return 0;
    ui_worldgen_view_build_legacy_layout(&panel.legacy, &layout);
    return ui_forms_handle_worldgen_random_click_in_layout(
        hwnd, &layout, mouse_x, mouse_y);
}

void ui_forms_translate_name_input(void) {
    ui_worldgen_legacy_forms_localize_name();
}

void ui_forms_refresh_language(HWND hwnd) {
    ui_forms_translate_name_input();
    (void)hwnd;
}

void ui_forms_add_civ(HWND hwnd) {
    const FormControls *form = controls();
    char name[NAME_LEN];
    char symbol_text[8];
    char symbol;
    int civ_id;
    ui_worldgen_legacy_forms_get_text_utf8(form->name_edit, name, sizeof(name));
    GetWindowTextA(form->symbol_edit, symbol_text, sizeof(symbol_text));
    symbol = symbol_text[0] ? symbol_text[0] : (char)('A' + civ_count);
    civ_id = game_request_add_civilization_from_selection_with_color(
        name, symbol,
        read_metric_control(form->military_edit, 5),
        read_metric_control(form->logistics_edit, 5),
        read_metric_control(form->governance_edit, 5),
        read_metric_control(form->cohesion_edit, 5),
        read_metric_control(form->production_edit, 5),
        read_metric_control(form->commerce_edit, 5),
        read_metric_control(form->innovation_edit, 5),
        selected_civ_color);
    if (civ_id >= 0) {
        ui_forms_write_civ(civ_id);
        ui_invalidate_game_redraw(hwnd, GAME_REDRAW_MAP_STATIC |
                                  GAME_REDRAW_MAP_DYNAMIC |
                                  GAME_REDRAW_SIDE_PANEL);
    } else {
        MessageBoxA(hwnd,
                    "Could not add civilization. The world may already be full, or there is no valid empty land. Select an empty land tile or rebuild with more land.",
                    "Add Civilization", MB_OK | MB_ICONINFORMATION);
        ui_invalidate_side_panel(hwnd);
    }
}

void ui_forms_apply_selected(HWND hwnd) {
    const FormControls *form = controls();
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
    ui_worldgen_legacy_forms_get_text_utf8(form->name_edit, name, sizeof(name));
    GetWindowTextA(form->symbol_edit, symbol_text, sizeof(symbol_text));
    if (game_request_edit_selected_civilization(
            name, symbol_text[0],
            read_metric_control(form->military_edit, fallback_military),
            read_metric_control(form->logistics_edit, fallback_logistics),
            read_metric_control(form->governance_edit, fallback_governance),
            read_metric_control(form->cohesion_edit, fallback_cohesion),
            read_metric_control(form->production_edit, fallback_production),
            read_metric_control(form->commerce_edit, fallback_commerce),
            read_metric_control(form->innovation_edit, fallback_innovation))) {
        game_request_set_civilization_color_exact(selected_civ,
                                                   selected_civ_color);
        ui_forms_write_civ(selected_civ);
        ui_invalidate_game_redraw(hwnd, GAME_REDRAW_MAP_STATIC |
                                  GAME_REDRAW_MAP_DYNAMIC |
                                  GAME_REDRAW_SIDE_PANEL);
    }
}

int ui_forms_handle_command(HWND hwnd, int control_id) {
    if (control_id == ID_ADD_BUTTON) ui_forms_add_civ(hwnd);
    else if (control_id == ID_APPLY_BUTTON) ui_forms_apply_selected(hwnd);
    else return 0;
    return 1;
}

HBRUSH ui_forms_control_color(WPARAM wparam, LPARAM lparam) {
    return ui_worldgen_legacy_forms_control_color(wparam, lparam);
}

void ui_forms_redraw_visible_controls(void) {
    ui_worldgen_legacy_forms_redraw_visible();
}

static void build_native_form_layout(RECT client,
                                     UiWorldgenLegacyFormsLayout *native,
                                     WorldgenLayout *legacy) {
    const UiWorldgenControlState *state = ui_worldgen_control_state_get();
    UiWorldgenPanelLayout panel;
    ui_worldgen_view_build(client, side_panel_w, &panel);
    ui_worldgen_view_build_legacy_layout(&panel.legacy, legacy);
    native->legacy_layout = legacy;
    native->region_custom_viewport = panel.content_viewport;
    native->region_custom_input = panel.hydrology.region_custom_input;
    native->hydrology_initial_viewport = panel.content_viewport;
    native->hydrology_initial_input = panel.hydrology.initial_civs_input;
    native->show_legacy = state->tab == UI_WORLDGEN_TAB_LEGACY;
    native->show_region_custom =
        state->tab == UI_WORLDGEN_TAB_HYDROLOGY_REGIONS &&
        state->region_category == UI_WORLDGEN_REGION_CUSTOM;
    native->show_hydrology_initial =
        state->tab == UI_WORLDGEN_TAB_HYDROLOGY_REGIONS;
}

void ui_forms_layout(HWND hwnd) {
    RECT client;
    WorldgenLayout legacy;
    UiWorldgenLegacyFormsLayout native;
    int show_world = panel_tab == PANEL_WORLD && !pause_menu_open &&
                     !side_panel_collapsed;
    GetClientRect(hwnd, &client);
    ui_side_panel_apply_state(client);
    if (!show_world) {
        ui_worldgen_legacy_forms_hide();
        return;
    }
    build_native_form_layout(client, &native, &legacy);
    ui_worldgen_legacy_forms_layout(&native);
}

void ui_forms_create(HWND hwnd) {
    ui_worldgen_legacy_forms_create(hwnd);
    ui_forms_write_world_setup_controls();
    ui_forms_layout(hwnd);
}

void ui_forms_read_world_setup_controls(void) {
    ui_forms_commit_worldgen_numeric_edits();
}

void ui_forms_write_world_setup_controls(void) {
    UiWorldgenEffectiveConfig config;
    ui_worldgen_config_read(&config);
    ui_worldgen_legacy_forms_write_initial_civs(config.initial_civ_count);
    ui_worldgen_legacy_forms_write_region_custom(config.region_size_slider);
}
