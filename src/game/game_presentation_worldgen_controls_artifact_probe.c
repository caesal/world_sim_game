#include "game/game_presentation_worldgen_controls_artifact_internal.h"

#include "core/constants.h"
#include "core/game_types.h"
#include "game/game_presentation_static_physical_artifacts.h"
#include "render/render_panel_internal.h"
#include "render/worldgen_ui_assets.h"
#include "ui/ui_forms.h"
#include "ui/ui_worldgen_command.h"
#include "ui/ui_worldgen_config_adapter.h"
#include "ui/ui_worldgen_control_state.h"
#include "ui/ui_worldgen_input.h"
#include "ui/ui_worldgen_legacy_forms.h"
#include "ui/ui_worldgen_view.h"

#include <string.h>

typedef struct {
    int panel_tab;
    int language;
    int panel_width;
    int expanded_width;
    int collapsed;
    int hover_x;
    int hover_y;
} WorldgenControlsArtifactGlobals;

#define WORLDGEN_CONTROLS_ARTIFACT_OWNER_CLASS \
    "WorldSimWorldgenArtifactForms"
#define WORLDGEN_CONTROLS_ARTIFACT_OFFSCREEN (-32000)

static LRESULT CALLBACK artifact_form_owner_proc(
    HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_PRINTCLIENT) {
        RECT client;
        GetClientRect(hwnd, &client);
        draw_side_panel((HDC)wparam, client);
        return 1;
    }
    if (message == WM_CTLCOLOREDIT) {
        HBRUSH brush = ui_forms_control_color(wparam, lparam);
        if (brush) return (LRESULT)brush;
    }
    return DefWindowProcA(hwnd, message, wparam, lparam);
}

static int register_artifact_form_owner_class(void) {
    WNDCLASSEXA window_class;
    ATOM atom;
    memset(&window_class, 0, sizeof(window_class));
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = artifact_form_owner_proc;
    window_class.hInstance = GetModuleHandleA(NULL);
    window_class.lpszClassName = WORLDGEN_CONTROLS_ARTIFACT_OWNER_CLASS;
    atom = RegisterClassExA(&window_class);
    return atom != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

static HWND make_artifact_form_owner(void) {
    HWND owner;
    if (!register_artifact_form_owner_class()) return NULL;
    owner = CreateWindowExA(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        WORLDGEN_CONTROLS_ARTIFACT_OWNER_CLASS,
        "worldgen-controls-artifact-forms", WS_POPUP,
        WORLDGEN_CONTROLS_ARTIFACT_OFFSCREEN,
        WORLDGEN_CONTROLS_ARTIFACT_OFFSCREEN,
        460, WORLDGEN_CONTROLS_ARTIFACT_HEIGHT,
        NULL, NULL, GetModuleHandleA(NULL), NULL);
    if (owner) ShowWindow(owner, SW_SHOWNOACTIVATE);
    return owner;
}

static int artifact_forms_created(void) {
    const FormControls *form = ui_worldgen_legacy_forms_controls();
    return form->name_edit && form->symbol_edit &&
           form->military_edit && form->logistics_edit &&
           form->governance_edit && form->cohesion_edit &&
           form->production_edit && form->commerce_edit &&
           form->innovation_edit && form->initial_civs_edit &&
           form->hydrology_initial_civs_edit &&
           form->region_custom_edit && form->add_button &&
           form->apply_button;
}

static int text_equals(HWND control, const char *expected) {
    char text[32];
    if (!control || !expected) return 0;
    GetWindowTextA(control, text, sizeof(text));
    return strcmp(text, expected) == 0;
}

static int child_rect_equals(HWND parent, HWND child, RECT expected) {
    RECT actual;
    if (!child || !GetWindowRect(child, &actual)) return 0;
    MapWindowPoints(HWND_DESKTOP, parent, (POINT *)&actual, 2);
    return actual.left == expected.left && actual.top == expected.top &&
           actual.right == expected.right && actual.bottom == expected.bottom;
}

static int child_style_visible(HWND child) {
    return child && (GetWindowLongPtrW(child, GWL_STYLE) & WS_VISIBLE) != 0;
}

static int case_initial_civs_two_way_sync(
    WorldgenControlsProbeReport *report, HWND hwnd) {
    const FormControls *form = ui_worldgen_legacy_forms_controls();
    RECT client;
    UiWorldgenPanelLayout panel;
    WorldgenLayout legacy;
    int distinct_ok;
    int geometry_ok;
    int sync_ok;
    int invalid_ok;
    int command_ok;
    int persistence_ok = 1;
    int tab;
    GetClientRect(hwnd, &client);
    panel_tab = PANEL_WORLD;
    side_panel_w = 460;
    side_panel_expanded_w = 460;
    side_panel_collapsed = 0;
    ui_worldgen_control_state_init_fresh();
    distinct_ok = form->initial_civs_edit &&
                  form->hydrology_initial_civs_edit &&
                  form->initial_civs_edit !=
                      form->hydrology_initial_civs_edit &&
                  GetDlgCtrlID(form->initial_civs_edit) ==
                      ID_INITIAL_CIVS_EDIT &&
                  GetDlgCtrlID(form->hydrology_initial_civs_edit) ==
                      ID_HYDROLOGY_INITIAL_CIVS_EDIT;

    ui_worldgen_control_state_set_tab(
        UI_WORLDGEN_TAB_HYDROLOGY_REGIONS);
    ui_worldgen_view_build(client, 460, &panel);
    ui_worldgen_control_state_set_scroll(
        UI_WORLDGEN_TAB_HYDROLOGY_REGIONS,
        panel.tab_max_scroll[UI_WORLDGEN_TAB_HYDROLOGY_REGIONS]);
    ui_forms_write_world_setup_controls();
    ui_forms_layout(hwnd);
    ui_worldgen_view_build(client, 460, &panel);
    geometry_ok = child_style_visible(
                      form->hydrology_initial_civs_edit) &&
                  !child_style_visible(form->initial_civs_edit) &&
                  child_rect_equals(
                      hwnd, form->hydrology_initial_civs_edit,
                      panel.hydrology.initial_civs_input);
    SetWindowTextA(form->hydrology_initial_civs_edit, "42");
    sync_ok = ui_forms_handle_metric_change(
                  ID_HYDROLOGY_INITIAL_CIVS_EDIT) &&
              ui_worldgen_config_get_field(
                  UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT) == 42 &&
              text_equals(form->initial_civs_edit, "42");

    ui_worldgen_control_state_set_tab(UI_WORLDGEN_TAB_LEGACY);
    ui_worldgen_control_state_set_scroll(UI_WORLDGEN_TAB_LEGACY, 0);
    ui_forms_layout(hwnd);
    ui_worldgen_view_build(client, 460, &panel);
    ui_worldgen_view_build_legacy_layout(&panel.legacy, &legacy);
    geometry_ok &= child_style_visible(form->initial_civs_edit) &&
                   !child_style_visible(
                       form->hydrology_initial_civs_edit) &&
                   child_rect_equals(
                       hwnd, form->initial_civs_edit, legacy.initial_input);
    SetWindowTextA(form->initial_civs_edit, "73");
    sync_ok &= ui_forms_handle_metric_change(ID_INITIAL_CIVS_EDIT) &&
               ui_worldgen_config_get_field(
                   UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT) == 73 &&
               text_equals(form->hydrology_initial_civs_edit, "73");

    ui_worldgen_control_state_set_tab(
        UI_WORLDGEN_TAB_HYDROLOGY_REGIONS);
    ui_forms_layout(hwnd);
    SetWindowTextA(form->hydrology_initial_civs_edit, "");
    invalid_ok = ui_forms_handle_metric_change(
                     ID_HYDROLOGY_INITIAL_CIVS_EDIT) &&
                 ui_worldgen_config_get_field(
                     UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT) == 73 &&
                 text_equals(form->initial_civs_edit, "73") &&
                 text_equals(form->hydrology_initial_civs_edit, "");
    invalid_ok &= ui_forms_normalize_metric_edit(
                      ID_HYDROLOGY_INITIAL_CIVS_EDIT) &&
                  text_equals(form->initial_civs_edit, "73") &&
                  text_equals(form->hydrology_initial_civs_edit, "73");
    SetWindowTextA(form->hydrology_initial_civs_edit, "201");
    invalid_ok &= ui_forms_handle_metric_change(
                      ID_HYDROLOGY_INITIAL_CIVS_EDIT) &&
                  ui_worldgen_config_get_field(
                      UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT) == 73 &&
                  text_equals(form->hydrology_initial_civs_edit, "201");
    SetFocus(form->hydrology_initial_civs_edit);
    invalid_ok &= GetFocus() == form->hydrology_initial_civs_edit &&
                  ui_worldgen_input_key_down(hwnd, VK_RETURN) &&
                  ui_worldgen_config_get_field(
                      UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT) == MAX_CIVS &&
                  text_equals(form->initial_civs_edit, "200") &&
                  text_equals(form->hydrology_initial_civs_edit, "200");

    ui_worldgen_control_state_set_field(
        UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT, 73);
    ui_forms_write_world_setup_controls();
    SetWindowTextA(form->initial_civs_edit, "19");
    SetFocus(hwnd);
    ui_forms_commit_worldgen_numeric_edits();
    command_ok = ui_worldgen_config_get_field(
                     UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT) == 73 &&
                 text_equals(form->initial_civs_edit, "73") &&
                 text_equals(form->hydrology_initial_civs_edit, "73");
    ui_worldgen_command_reset(hwnd);
    command_ok &= ui_worldgen_config_get_field(
                      UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT) == 73 &&
                  text_equals(form->initial_civs_edit, "73") &&
                  text_equals(form->hydrology_initial_civs_edit, "73");
    ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT, 88);
    ui_worldgen_command_resync_after_load(hwnd);
    command_ok &= text_equals(form->initial_civs_edit, "88") &&
                  text_equals(form->hydrology_initial_civs_edit, "88");
    ui_worldgen_control_state_set_field(
        UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT, 42);
    ui_worldgen_command_dice(hwnd);
    for (tab = 0; tab < UI_WORLDGEN_TAB_COUNT; tab++) {
        ui_worldgen_control_state_set_tab((UiWorldgenControlTab)tab);
        ui_worldgen_control_state_set_scroll(
            (UiWorldgenControlTab)tab, tab * 91);
        ui_forms_layout(hwnd);
        persistence_ok &= ui_worldgen_config_get_field(
                              UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT) == 42 &&
                          text_equals(form->initial_civs_edit, "42") &&
                          text_equals(
                              form->hydrology_initial_civs_edit, "42");
    }
    worldgen_controls_probe_record(
        report, "worldgen_controls_initial_civs_two_way_sync",
        distinct_ok && geometry_ok && sync_ok && invalid_ok &&
            command_ok && persistence_ok,
        "distinct=%d geometry=%d hydro42_legacy73=%d invalid_local_commit=%d reset_resync_hidden_guard=%d tab_scroll=%d",
        distinct_ok, geometry_ok, sync_ok, invalid_ok, command_ok,
        persistence_ok);
    return distinct_ok && geometry_ok && sync_ok && invalid_ok &&
           command_ok && persistence_ok;
}

static int point_equal(UiWorldgenPoint left, UiWorldgenPoint right) {
    return left.x == right.x && left.y == right.y;
}

static int corners_equal(const UiWorldgenClimateCorners *left,
                         const UiWorldgenClimateCorners *right) {
    return point_equal(left->top_left, right->top_left) &&
           point_equal(left->top_right, right->top_right) &&
           point_equal(left->bottom_right, right->bottom_right) &&
           point_equal(left->bottom_left, right->bottom_left);
}

static int target_equal(UiWorldgenControlTarget left,
                        UiWorldgenControlTarget right) {
    return left.kind == right.kind && left.index == right.index;
}

static int interaction_equal(const UiWorldgenInteractionState *left,
                             const UiWorldgenInteractionState *right) {
    return target_equal(left->hovered, right->hovered) &&
           target_equal(left->pressed, right->pressed) &&
           target_equal(left->focused, right->focused) &&
           target_equal(left->dragging, right->dragging);
}

static void restore_corners(const UiWorldgenClimateCorners *corners) {
    ui_worldgen_control_state_set_climate_corner(
        UI_WORLDGEN_CLIMATE_TOP_LEFT,
        corners->top_left.x, corners->top_left.y);
    ui_worldgen_control_state_set_climate_corner(
        UI_WORLDGEN_CLIMATE_TOP_RIGHT,
        corners->top_right.x, corners->top_right.y);
    ui_worldgen_control_state_set_climate_corner(
        UI_WORLDGEN_CLIMATE_BOTTOM_RIGHT,
        corners->bottom_right.x, corners->bottom_right.y);
    ui_worldgen_control_state_set_climate_corner(
        UI_WORLDGEN_CLIMATE_BOTTOM_LEFT,
        corners->bottom_left.x, corners->bottom_left.y);
}

static void restore_interaction(const UiWorldgenInteractionState *saved) {
    ui_worldgen_control_state_clear_interaction();
    ui_worldgen_control_state_set_hovered(saved->hovered);
    ui_worldgen_control_state_set_focused(saved->focused);
    if (saved->dragging.kind != UI_WORLDGEN_CONTROL_NONE) {
        ui_worldgen_control_state_begin_drag(saved->dragging);
    }
    ui_worldgen_control_state_set_pressed(saved->pressed);
}

static void restore_adapter_state(
    const UiWorldgenEffectiveConfig *saved_config,
    const UiWorldgenControlState *saved_state) {
    int tab;
    ui_worldgen_config_write(&saved_state->applied_config);
    ui_worldgen_control_state_resync(UI_WORLDGEN_RESYNC_MARK_APPLIED);
    ui_worldgen_config_write(saved_config);
    ui_worldgen_control_state_resync(UI_WORLDGEN_RESYNC_KEEP_APPLIED);
    restore_corners(&saved_state->climate_corners);
    if (saved_state->region_category == UI_WORLDGEN_REGION_CUSTOM) {
        ui_worldgen_control_state_set_region_category(
            UI_WORLDGEN_REGION_CUSTOM);
    } else {
        ui_worldgen_control_state_set_region_category(
            saved_state->region_category);
    }
    for (tab = 0; tab < UI_WORLDGEN_TAB_COUNT; tab++) {
        ui_worldgen_control_state_set_scroll(
            (UiWorldgenControlTab)tab, saved_state->scroll_offsets[tab]);
    }
    ui_worldgen_control_state_set_tab(saved_state->tab);
    restore_interaction(&saved_state->interaction);
    if (saved_state->generation_failed) {
        ui_worldgen_control_state_mark_generation_failed();
    }
}

static int state_equal_except_revision(
    const UiWorldgenControlState *current,
    const UiWorldgenControlState *saved) {
    int tab;
    int ok = current->initialized == saved->initialized &&
             current->tab == saved->tab &&
             corners_equal(&current->climate_corners,
                           &saved->climate_corners) &&
             current->region_category == saved->region_category &&
             current->preset == saved->preset &&
             ui_worldgen_config_equal(&current->applied_config,
                                      &saved->applied_config) &&
             current->has_applied_config == saved->has_applied_config &&
             current->dirty == saved->dirty &&
             current->generation_failed == saved->generation_failed &&
             interaction_equal(&current->interaction, &saved->interaction);
    for (tab = 0; tab < UI_WORLDGEN_TAB_COUNT; tab++) {
        ok &= current->scroll_offsets[tab] == saved->scroll_offsets[tab];
    }
    return ok;
}

static WorldgenControlsArtifactGlobals capture_globals(void) {
    WorldgenControlsArtifactGlobals saved = {
        panel_tab, ui_language, side_panel_w, side_panel_expanded_w,
        side_panel_collapsed, hover_x, hover_y
    };
    return saved;
}

static int restore_globals(
    const WorldgenControlsArtifactGlobals *saved) {
    panel_tab = saved->panel_tab;
    ui_language = saved->language;
    side_panel_w = saved->panel_width;
    side_panel_expanded_w = saved->expanded_width;
    side_panel_collapsed = saved->collapsed;
    hover_x = saved->hover_x;
    hover_y = saved->hover_y;
    return panel_tab == saved->panel_tab && ui_language == saved->language &&
           side_panel_w == saved->panel_width &&
           side_panel_expanded_w == saved->expanded_width &&
           side_panel_collapsed == saved->collapsed &&
           hover_x == saved->hover_x && hover_y == saved->hover_y;
}

int worldgen_controls_probe_artifacts(
    WorldgenControlsProbeReport *report) {
    const char *directory = static_physical_probe_artifact_dir();
    WorldgenControlsArtifactWriter writer;
    WorldgenControlsArtifactGlobals saved_globals;
    UiWorldgenEffectiveConfig saved_config;
    UiWorldgenEffectiveConfig restored_config;
    UiWorldgenControlState saved_state;
    const UiWorldgenControlState *restored_state;
    HWND form_owner;
    DWORD form_error;
    char manifest_path[MAX_PATH];
    uint32_t revision_delta;
    int sync_ok;
    int matrix_ok;
    int assets_ok;
    int adapter_ok;
    int state_ok;
    int globals_ok;
    int revision_advanced;
    int ok;
    if (!report || !static_physical_probe_join_path(
            manifest_path, sizeof(manifest_path), directory,
            "worldgen_controls_artifacts_manifest.txt")) return 0;
    memset(&writer, 0, sizeof(writer));
    writer.report = report;
    writer.directory = directory;
    writer.manifest = fopen(manifest_path, "w");
    if (!writer.manifest) {
        worldgen_controls_probe_record(
            report, "worldgen_controls_artifact_matrix", 0,
            "reason=manifest_open_failed path=%s", manifest_path);
        return 0;
    }
    ui_worldgen_config_read(&saved_config);
    saved_state = *ui_worldgen_control_state_get();
    saved_globals = capture_globals();
    fprintf(writer.manifest,
            "suite=worldgen_controls_artifacts renderer=draw_side_panel expected=%d height=%d\n",
            WORLDGEN_CONTROLS_ARTIFACT_EXPECTED_COUNT,
            WORLDGEN_CONTROLS_ARTIFACT_HEIGHT);
    if (!saved_state.initialized || !saved_state.has_applied_config) {
        fprintf(writer.manifest,
                "suite_ok=0 reason=unsupported_uninitialized_or_unapplied_state initialized=%d has_applied=%d\n",
                saved_state.initialized, saved_state.has_applied_config);
        fclose(writer.manifest);
        worldgen_controls_probe_record(
            report, "worldgen_controls_artifact_matrix", 0,
            "reason=state_restore_precondition initialized=%d has_applied=%d manifest=%s",
            saved_state.initialized, saved_state.has_applied_config,
            manifest_path);
        return 0;
    }

    form_owner = make_artifact_form_owner();
    if (!form_owner) {
        form_error = GetLastError();
        fprintf(writer.manifest,
                "suite_ok=0 reason=hidden_form_owner_create_failed error=%lu\n",
                form_error);
        fclose(writer.manifest);
        worldgen_controls_probe_record(
            report, "worldgen_controls_artifact_matrix", 0,
            "reason=hidden_form_owner_create_failed error=%lu manifest=%s",
            form_error, manifest_path);
        return 0;
    }
    ui_worldgen_legacy_forms_create(form_owner);
    if (!artifact_forms_created()) {
        fprintf(writer.manifest,
                "suite_ok=0 reason=native_child_create_failed\n");
        fclose(writer.manifest);
        DestroyWindow(form_owner);
        worldgen_controls_probe_record(
            report, "worldgen_controls_artifact_matrix", 0,
            "reason=native_child_create_failed manifest=%s", manifest_path);
        return 0;
    }
    writer.owner = form_owner;
    ui_forms_write_world_setup_controls();

    sync_ok = case_initial_civs_two_way_sync(report, form_owner);
    matrix_ok = worldgen_controls_artifact_matrix(&writer);
    assets_ok = worldgen_controls_artifact_assets(&writer);
    worldgen_ui_assets_validation_force_missing(
        WORLDGEN_UI_ASSET_CLIMATE_BIOME_ENVELOPE, 0);
    worldgen_ui_assets_reset_for_tests();
    ui_worldgen_legacy_forms_hide();
    DestroyWindow(form_owner);
    writer.owner = NULL;
    restore_adapter_state(&saved_config, &saved_state);
    globals_ok = restore_globals(&saved_globals);
    ui_worldgen_config_read(&restored_config);
    restored_state = ui_worldgen_control_state_get();
    adapter_ok = ui_worldgen_config_equal(&restored_config, &saved_config);
    state_ok = state_equal_except_revision(restored_state, &saved_state);
    revision_delta = restored_state->revision - saved_state.revision;
    revision_advanced = revision_delta > 0 &&
                        revision_delta < UINT32_C(0x80000000);
    ok = sync_ok && matrix_ok && assets_ok && adapter_ok && state_ok &&
         globals_ok &&
         revision_advanced && writer.artifact_count ==
             WORLDGEN_CONTROLS_ARTIFACT_EXPECTED_COUNT &&
         writer.failure_count == 0;
    fprintf(
        writer.manifest,
        "suite_ok=%d artifacts=%d expected=%d failures=%d sync_ok=%d matrix_ok=%d assets_ok=%d adapter_restored=%d state_restored=%d globals_restored=%d revision_before=%u revision_after=%u revision_delta=%u\n",
        ok, writer.artifact_count,
        WORLDGEN_CONTROLS_ARTIFACT_EXPECTED_COUNT, writer.failure_count,
        sync_ok, matrix_ok, assets_ok, adapter_ok, state_ok, globals_ok,
        saved_state.revision, restored_state->revision, revision_delta);
    fclose(writer.manifest);
    worldgen_controls_probe_record(
        report, "worldgen_controls_artifact_matrix", ok,
        "artifacts=%d expected=%d failures=%d sync_ok=%d renderer=hwnd_wm_printclient adapter_restored=%d state_restored=%d globals_restored=%d revision_advanced=%d manifest=%s",
        writer.artifact_count, WORLDGEN_CONTROLS_ARTIFACT_EXPECTED_COUNT,
        writer.failure_count, sync_ok, adapter_ok, state_ok, globals_ok,
        revision_advanced, manifest_path);
    return ok;
}
