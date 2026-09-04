#include "game/game_presentation_worldgen_controls_probe_internal.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "game/game_presentation_static_physical_artifacts.h"
#include "render/worldgen_ui_assets.h"
#include "ui/ui.h"
#include "ui/ui_forms.h"
#include "ui/ui_worldgen_command.h"
#include "ui/ui_worldgen_config_adapter.h"
#include "ui/ui_worldgen_control_state.h"
#include "ui/ui_worldgen_input.h"
#include "ui/ui_worldgen_legacy_forms.h"
#include "ui/ui_worldgen_view.h"

#include <string.h>

static unsigned int generate_hook_calls;
static int generate_hook_initial;
static int generate_hook_region;

static int successful_generate_hook(HWND hwnd) {
    (void)hwnd;
    generate_hook_calls++;
    generate_hook_initial = ui_worldgen_config_get_field(
        UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT);
    generate_hook_region = ui_worldgen_config_get_field(
        UI_WORLDGEN_FIELD_REGION_SIZE);
    return 1;
}

static int failed_generate_hook(HWND hwnd) {
    (void)hwnd;
    generate_hook_calls++;
    generate_hook_initial = ui_worldgen_config_get_field(
        UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT);
    generate_hook_region = ui_worldgen_config_get_field(
        UI_WORLDGEN_FIELD_REGION_SIZE);
    return 0;
}

static HWND make_probe_window(void) {
    return CreateWindowExA(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, "STATIC",
                           "worldgen-controls-form-probe", WS_POPUP,
                           0, 0, 1280, 800, NULL, NULL,
                           GetModuleHandle(NULL), NULL);
}

static int text_equals(HWND control, const char *expected) {
    char buffer[128];
    if (!control || !expected) return 0;
    GetWindowTextA(control, buffer, sizeof(buffer));
    return strcmp(buffer, expected) == 0;
}

static int manual_form_values_equal(
    const FormControls *form, const HWND *metrics,
    const char *const *metric_values) {
    int ok = text_equals(form->name_edit, "Probe Realm") &&
             text_equals(form->symbol_edit, "Q");
    int index;
    for (index = 0; index < WORLDGEN_CORE_METRIC_COUNT; index++) {
        ok &= text_equals(metrics[index], metric_values[index]);
    }
    return ok;
}

static int case_native_forms_preserved(WorldgenControlsProbeReport *report,
                                       HWND hwnd) {
    static const char *metric_values[WORLDGEN_CORE_METRIC_COUNT] = {
        "1", "2", "3", "4", "5", "6", "7"
    };
    const FormControls *form = ui_worldgen_legacy_forms_controls();
    HWND metrics[WORLDGEN_CORE_METRIC_COUNT];
    UiWorldgenPanelLayout panel;
    WorldgenLayout legacy;
    Color32 saved_color = selected_civ_color;
    Color32 probe_color = COLOR32_RGB(17, 91, 203);
    int saved_color_index = selected_civ_color_index;
    int saved_selected = selected_civ;
    int values_ok = 1;
    int selection_ok;
    int order_ok;
    int tab;
    int i;
    metrics[WORLDGEN_METRIC_MILITARY] = form->military_edit;
    metrics[WORLDGEN_METRIC_LOGISTICS] = form->logistics_edit;
    metrics[WORLDGEN_METRIC_GOVERNANCE] = form->governance_edit;
    metrics[WORLDGEN_METRIC_COHESION] = form->cohesion_edit;
    metrics[WORLDGEN_METRIC_PRODUCTION] = form->production_edit;
    metrics[WORLDGEN_METRIC_COMMERCE] = form->commerce_edit;
    metrics[WORLDGEN_METRIC_INNOVATION] = form->innovation_edit;
    SetWindowTextA(form->name_edit, "Probe Realm");
    SetWindowTextA(form->symbol_edit, "Q");
    for (i = 0; i < WORLDGEN_CORE_METRIC_COUNT; i++) {
        SetWindowTextA(metrics[i], metric_values[i]);
    }
    selected_civ_color = probe_color;
    selected_civ_color_index = 4;
    selected_civ = 7;
    for (tab = 0; tab < UI_WORLDGEN_TAB_COUNT; tab++) {
        ui_worldgen_control_state_set_tab((UiWorldgenControlTab)tab);
        ui_worldgen_control_state_set_scroll(
            (UiWorldgenControlTab)tab, (tab + 1) * 137);
        ui_forms_layout(hwnd);
        values_ok &= manual_form_values_equal(form, metrics, metric_values);
    }
    ui_worldgen_command_dice(hwnd);
    ui_worldgen_command_reset(hwnd);
    ui_worldgen_control_state_set_field(UI_WORLDGEN_FIELD_OCEAN, 73);
    ui_worldgen_control_state_set_field(UI_WORLDGEN_FIELD_REGION_SIZE, 49);
    for (tab = UI_WORLDGEN_TAB_COUNT - 1; tab >= 0; tab--) {
        ui_worldgen_control_state_set_tab((UiWorldgenControlTab)tab);
        ui_worldgen_control_state_set_scroll(
            (UiWorldgenControlTab)tab, 100000 - tab * 73);
        ui_forms_layout(hwnd);
        values_ok &= manual_form_values_equal(form, metrics, metric_values);
    }
    values_ok &= manual_form_values_equal(form, metrics, metric_values);
    selection_ok = selected_civ_color == probe_color &&
                   selected_civ_color_index == 4 && selected_civ == 7;
    ui_worldgen_view_build((RECT){0, 0, 1280, 800}, 460, &panel);
    ui_worldgen_view_build_legacy_layout(&panel.legacy, &legacy);
    order_ok = legacy.civ_section.top > legacy.generate_row.bottom &&
               legacy.name_input.top > legacy.civ_section.top &&
               legacy.add_button.top >
                   legacy.metric_input[WORLDGEN_METRIC_INNOVATION].bottom &&
               legacy.add_button.bottom <= panel.legacy.content_bounds.bottom &&
               legacy.apply_button.bottom <= panel.legacy.content_bounds.bottom;
    selected_civ_color = saved_color;
    selected_civ_color_index = saved_color_index;
    selected_civ = saved_selected;
    worldgen_controls_probe_record(
        report, "worldgen_controls_native_form_tab_preservation",
        values_ok && selection_ok && order_ok,
        "tabs=%d scroll_cycles=%d dice_reset_parameter_edits=1 fields=%d values_preserved=%d selection_preserved=%d manual_form_bottom=%d",
        UI_WORLDGEN_TAB_COUNT, UI_WORLDGEN_TAB_COUNT * 2,
        WORLDGEN_CORE_METRIC_COUNT + 2, values_ok, selection_ok, order_ok);
    return values_ok && selection_ok && order_ok;
}

static POINT rect_center(RECT rect) {
    POINT point = {(rect.left + rect.right) / 2,
                   (rect.top + rect.bottom) / 2};
    return point;
}

static int command_counts_once(const UiWorldgenCommandDiagnostics *value) {
    return value->generate_commands == 1 && value->generator_calls == 1 &&
           value->successful_generations == 1;
}

static int failed_command_counts_once(
    const UiWorldgenCommandDiagnostics *value) {
    return value->generate_commands == 1 && value->generator_calls == 1 &&
           value->successful_generations == 0;
}

static int applied_clean_equals(
    const UiWorldgenEffectiveConfig *expected) {
    const UiWorldgenControlState *state = ui_worldgen_control_state_get();
    return state->has_applied_config && !state->dirty &&
           !state->generation_failed &&
           ui_worldgen_config_equal(&state->applied_config, expected);
}

static int case_footer_f5_shared_command(
    WorldgenControlsProbeReport *report, HWND hwnd) {
    RECT client = {0, 0, 1280, 800};
    const FormControls *form = ui_worldgen_legacy_forms_controls();
    UiWorldgenPanelLayout layout;
    UiWorldgenCommandDiagnostics footer_diagnostics;
    UiWorldgenCommandDiagnostics f5_diagnostics;
    UiWorldgenCommandDiagnostics failure_diagnostics;
    UiWorldgenEffectiveConfig expected;
    UiWorldgenEffectiveConfig applied_before_failure;
    const UiWorldgenControlState *state;
    POINT footer;
    int saved_pause = pause_menu_open;
    int footer_ok;
    int f5_ok;
    int failure_ok;
    pause_menu_open = 0;
    ui_worldgen_control_state_init_fresh();
    ui_worldgen_control_state_set_tab(UI_WORLDGEN_TAB_LEGACY);
    ui_worldgen_control_state_set_scroll(UI_WORLDGEN_TAB_LEGACY, 200);
    ui_forms_layout(hwnd);
    ui_worldgen_command_set_generate_hook_for_tests(
        successful_generate_hook);
    ui_worldgen_command_reset_diagnostics();
    generate_hook_calls = 0;
    ui_worldgen_control_state_mark_generation_failed();
    SetWindowTextA(form->initial_civs_edit, "42");
    SetFocus(form->initial_civs_edit);
    ui_worldgen_view_build(client, 460, &layout);
    footer = rect_center(layout.generate_button);
    footer_ok = GetFocus() == form->initial_civs_edit &&
                ui_worldgen_input_mouse_down(
                    hwnd, client, 460, footer.x, footer.y) &&
                ui_worldgen_input_mouse_up(
                    hwnd, client, 460, footer.x, footer.y);
    ui_worldgen_command_get_diagnostics(&footer_diagnostics);
    ui_worldgen_config_read(&expected);
    footer_ok &= generate_hook_calls == 1 &&
                 command_counts_once(&footer_diagnostics) &&
                 generate_hook_initial == 42 &&
                 applied_clean_equals(&expected);
    ui_worldgen_command_reset_diagnostics();
    generate_hook_calls = 0;
    ui_worldgen_control_state_set_region_custom_value(49);
    SetWindowTextA(form->region_custom_edit, "77");
    SetWindowTextA(form->initial_civs_edit, "44");
    SetFocus(form->initial_civs_edit);
    f5_ok = GetFocus() == form->initial_civs_edit &&
            handle_shortcut(hwnd, VK_F5) != 0;
    ui_worldgen_command_get_diagnostics(&f5_diagnostics);
    ui_worldgen_config_read(&expected);
    f5_ok &= generate_hook_calls == 1 &&
             command_counts_once(&f5_diagnostics) &&
             generate_hook_initial == 44 && generate_hook_region == 77 &&
             applied_clean_equals(&expected);

    state = ui_worldgen_control_state_get();
    applied_before_failure = state->applied_config;
    ui_worldgen_control_state_set_field(UI_WORLDGEN_FIELD_OCEAN, 61);
    ui_worldgen_command_set_generate_hook_for_tests(failed_generate_hook);
    ui_worldgen_command_reset_diagnostics();
    generate_hook_calls = 0;
    failure_ok = handle_shortcut(hwnd, VK_F5) != 0;
    ui_worldgen_command_get_diagnostics(&failure_diagnostics);
    state = ui_worldgen_control_state_get();
    failure_ok &= generate_hook_calls == 1 &&
                  failed_command_counts_once(&failure_diagnostics) &&
                  state->has_applied_config && state->dirty &&
                  state->generation_failed &&
                  ui_worldgen_config_equal(&state->applied_config,
                                           &applied_before_failure);
    ui_worldgen_command_set_generate_hook_for_tests(NULL);
    pause_menu_open = saved_pause;
    worldgen_controls_probe_record(
        report, "worldgen_controls_footer_f5_shared_command",
        footer_ok && f5_ok && failure_ok,
        "footer_once=%d footer_commands=%u footer_calls=%u footer_applied_clean=%d f5_once=%d f5_commands=%u f5_calls=%u f5_applied_clean=%d committed_initial=%d committed_region=%d failure_once=%d failure_calls=%u applied_unchanged=%d dirty_failure=%d",
        footer_ok, footer_diagnostics.generate_commands,
        footer_diagnostics.generator_calls, footer_ok, f5_ok,
        f5_diagnostics.generate_commands, f5_diagnostics.generator_calls,
        f5_ok, generate_hook_initial, generate_hook_region,
        failure_ok, failure_diagnostics.generator_calls,
        failure_ok, state->dirty && state->generation_failed);
    return footer_ok && f5_ok && failure_ok;
}

static void prepare_custom_commit_fixture(HWND hwnd) {
    ui_worldgen_control_state_init_fresh();
    ui_worldgen_control_state_set_tab(
        UI_WORLDGEN_TAB_HYDROLOGY_REGIONS);
    ui_worldgen_control_state_set_region_custom_value(49);
    ui_forms_write_world_setup_controls();
    ui_forms_layout(hwnd);
}

static int case_custom_commit_triggers(
    WorldgenControlsProbeReport *report, HWND hwnd) {
    const FormControls *form = ui_worldgen_legacy_forms_controls();
    RECT client = {0, 0, 1280, 800};
    UiWorldgenPanelLayout layout;
    HWND saved_focus = GetFocus();
    POINT tab;
    int enter_ok;
    int focused_before_loss;
    int focus_loss_ok;
    int tab_ok;

    prepare_custom_commit_fixture(hwnd);
    SetWindowTextA(form->region_custom_edit, "41");
    SetFocus(form->region_custom_edit);
    enter_ok = GetFocus() == form->region_custom_edit &&
               ui_worldgen_input_key_down(hwnd, VK_RETURN) &&
               ui_worldgen_config_get_field(
                   UI_WORLDGEN_FIELD_REGION_SIZE) == 41 &&
               text_equals(form->region_custom_edit, "41");

    SetWindowTextA(form->region_custom_edit, "57");
    SetFocus(form->region_custom_edit);
    focused_before_loss = GetFocus() == form->region_custom_edit;
    SetFocus(hwnd);
    focus_loss_ok = focused_before_loss &&
                    ui_forms_normalize_metric_edit(
                        ID_REGION_CUSTOM_EDIT) &&
                    ui_worldgen_config_get_field(
                        UI_WORLDGEN_FIELD_REGION_SIZE) == 57 &&
                    text_equals(form->region_custom_edit, "57");

    ui_worldgen_control_state_set_tab(
        UI_WORLDGEN_TAB_HYDROLOGY_REGIONS);
    ui_forms_layout(hwnd);
    SetWindowTextA(form->region_custom_edit, "73");
    SetFocus(form->region_custom_edit);
    ui_worldgen_view_build(client, 460, &layout);
    tab = rect_center(layout.tab_button[UI_WORLDGEN_TAB_PHYSICAL]);
    tab_ok = GetFocus() == form->region_custom_edit &&
             ui_worldgen_input_mouse_down(
                 hwnd, client, 460, tab.x, tab.y) &&
             ui_worldgen_input_mouse_up(
                 hwnd, client, 460, tab.x, tab.y) &&
             ui_worldgen_config_get_field(
                 UI_WORLDGEN_FIELD_REGION_SIZE) == 73 &&
             ui_worldgen_control_state_get()->tab ==
                 UI_WORLDGEN_TAB_PHYSICAL &&
             text_equals(form->region_custom_edit, "73");
    if (saved_focus && IsWindow(saved_focus)) SetFocus(saved_focus);
    else SetFocus(NULL);
    worldgen_controls_probe_record(
        report, "worldgen_controls_custom_commit_triggers",
        enter_ok && focus_loss_ok && tab_ok,
        "enter=%d focus_loss=%d tab_change=%d f5_covered_by_shared_command=1 values=41,57,73",
        enter_ok, focus_loss_ok, tab_ok);
    return enter_ok && focus_loss_ok && tab_ok;
}

static int expected_dimensions(WorldgenUiAssetId asset, int width,
                               int height) {
    const WorldgenUiAssetInfo *info = worldgen_ui_assets_info(asset);
    return info && width == info->expected_width &&
           height == info->expected_height;
}

static int case_asset_decode_surface_cache(
    WorldgenControlsProbeReport *report) {
    StaticPhysicalProbeCanvas canvas;
    WorldgenUiAssetDiagnostics totals;
    int ok = 1;
    int releases_ok;
    int asset;
    if (!static_physical_probe_canvas_open(&canvas, 640, 240)) {
        worldgen_controls_probe_record(
            report, "worldgen_controls_asset_decode_surface_cache", 0,
            "reason=memory_canvas_open_failed");
        return 0;
    }
    worldgen_ui_assets_reset_for_tests();
    static_physical_probe_canvas_clear(&canvas);
    for (asset = 0; asset < WORLDGEN_UI_ASSET_COUNT; asset++) {
        WorldgenUiAssetDiagnostics diagnostics;
        RECT destination = {
            4 + asset * 124, 8, 120 + asset * 124, 224
        };
        int width = 0;
        int height = 0;
        ok &= worldgen_ui_assets_draw_fit(
            canvas.dc, (WorldgenUiAssetId)asset, destination);
        ok &= worldgen_ui_assets_draw_fit(
            canvas.dc, (WorldgenUiAssetId)asset, destination);
        ok &= worldgen_ui_assets_dimensions(
            (WorldgenUiAssetId)asset, &width, &height);
        ok &= worldgen_ui_assets_get_diagnostics(
            (WorldgenUiAssetId)asset, &diagnostics);
        ok &= expected_dimensions((WorldgenUiAssetId)asset, width, height);
        ok &= diagnostics.attempts == 1 && diagnostics.decodes == 1 &&
              diagnostics.bitmap_allocations == 1 &&
              diagnostics.draw_calls == 2 && diagnostics.fallback_draws == 0 &&
              diagnostics.surface_builds == 1 &&
              diagnostics.surface_hits == 1 &&
              diagnostics.surface_allocations == 1 && diagnostics.loaded;
    }
    ok &= worldgen_ui_assets_preload_all();
    worldgen_ui_assets_get_totals(&totals);
    ok &= totals.attempts == WORLDGEN_UI_ASSET_COUNT &&
          totals.decodes == WORLDGEN_UI_ASSET_COUNT &&
          totals.bitmap_allocations == WORLDGEN_UI_ASSET_COUNT &&
          totals.surface_builds == WORLDGEN_UI_ASSET_COUNT &&
          totals.surface_hits == WORLDGEN_UI_ASSET_COUNT &&
          totals.fallback_draws == 0 &&
          totals.loaded == WORLDGEN_UI_ASSET_COUNT;
    worldgen_ui_assets_release();
    worldgen_ui_assets_get_totals(&totals);
    releases_ok = totals.bitmap_releases == WORLDGEN_UI_ASSET_COUNT &&
                  totals.surface_releases == WORLDGEN_UI_ASSET_COUNT &&
                  totals.loaded == 0;
    ok &= releases_ok;
    worldgen_controls_probe_record(
        report, "worldgen_controls_asset_decode_surface_cache", ok,
        "assets=%d attempts=%u decodes=%u builds=%u hits=%u bitmap_releases=%u surface_releases=%u stable_redraw=%d",
        WORLDGEN_UI_ASSET_COUNT, totals.attempts, totals.decodes,
        totals.surface_builds, totals.surface_hits,
        totals.bitmap_releases, totals.surface_releases, ok);
    worldgen_ui_assets_reset_for_tests();
    static_physical_probe_canvas_close(&canvas);
    return ok;
}

int worldgen_controls_probe_resources(WorldgenControlsProbeReport *report) {
    HWND hwnd = make_probe_window();
    int ok = 1;
    if (!hwnd) {
        worldgen_controls_probe_record(
            report, "worldgen_controls_hidden_form_window", 0,
            "reason=create_failed error=%lu", GetLastError());
        return 0;
    }
    ui_worldgen_control_state_init_fresh();
    ui_forms_create(hwnd);
    ok &= case_native_forms_preserved(report, hwnd);
    ok &= case_footer_f5_shared_command(report, hwnd);
    ok &= case_custom_commit_triggers(report, hwnd);
    ok &= case_asset_decode_surface_cache(report);
    ui_worldgen_command_set_generate_hook_for_tests(NULL);
    ui_worldgen_command_reset_diagnostics();
    ui_worldgen_input_reset_diagnostics();
    SetFocus(NULL);
    DestroyWindow(hwnd);
    return ok;
}
