#include "game/game_presentation_worldgen_initial_civs_probe.h"

#include "core/constants.h"
#include "core/game_types.h"
#include "ui/ui_forms.h"
#include "ui/ui_types.h"
#include "ui/ui_worldgen_command.h"
#include "ui/ui_worldgen_config_adapter.h"
#include "ui/ui_worldgen_control_state.h"
#include "ui/ui_worldgen_input.h"
#include "ui/ui_worldgen_legacy_forms.h"
#include "ui/ui_worldgen_random.h"
#include "ui/ui_worldgen_view.h"

#include <stdio.h>
#include <string.h>

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

static int child_visible(HWND child) {
    return child && (GetWindowLongPtrW(child, GWL_STYLE) & WS_VISIBLE) != 0;
}

static UiWorldgenFieldMask field_mask(int first, int last) {
    UiWorldgenFieldMask mask = 0;
    int field;
    for (field = first; field <= last; field++) {
        mask |= UI_WORLDGEN_FIELD_MASK(field);
    }
    return mask;
}

static void set_config(int map_size, int initial) {
    ui_worldgen_control_state_apply_balanced();
    ui_worldgen_control_state_set_field(UI_WORLDGEN_FIELD_PENDING_MAP_SIZE,
                                        map_size);
    ui_worldgen_control_state_set_field(UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT,
                                        initial);
}

static int case_caps_and_dice(WorldgenControlsProbeReport *report) {
    static const int caps[MAP_SIZE_COUNT] = {50, 80, 115, 200};
    UiWorldgenEffectiveConfig before;
    UiWorldgenEffectiveConfig first;
    UiWorldgenEffectiveConfig second;
    UiWorldgenFieldMask expected = field_mask(
        UI_WORLDGEN_FIELD_OCEAN, UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT);
    int cap_ok = 1;
    int dice_ok = 1;
    int map_size;
    for (map_size = MAP_SIZE_SMALL; map_size < MAP_SIZE_COUNT; map_size++) {
        UiWorldgenFieldMask first_mask;
        UiWorldgenFieldMask second_mask;
        UiWorldgenEffectiveConfig capped;
        int old_value = caps[map_size] / 2;
        cap_ok &= ui_worldgen_initial_civ_cap_for_map_size(map_size) ==
                  caps[map_size];
        set_config(map_size, caps[map_size] + 1);
        ui_worldgen_config_read(&capped);
        cap_ok &= capped.pending_map_size == map_size &&
                  capped.initial_civ_count == caps[map_size];
        set_config(map_size, old_value);
        ui_worldgen_config_read(&before);
        ui_worldgen_random_validation_set_state(
            0x51a7c000u + (unsigned int)map_size);
        first_mask = ui_worldgen_randomize_all();
        ui_worldgen_control_state_resync(UI_WORLDGEN_RESYNC_KEEP_APPLIED);
        ui_worldgen_config_read(&first);
        ui_worldgen_config_write(&before);
        ui_worldgen_control_state_resync(UI_WORLDGEN_RESYNC_KEEP_APPLIED);
        ui_worldgen_random_validation_set_state(
            0x51a7c000u + (unsigned int)map_size);
        second_mask = ui_worldgen_randomize_all();
        ui_worldgen_control_state_resync(UI_WORLDGEN_RESYNC_KEEP_APPLIED);
        ui_worldgen_config_read(&second);
        dice_ok &= first_mask == expected && second_mask == expected &&
                   first.pending_map_size == map_size &&
                   first.initial_civ_count >= 1 &&
                   first.initial_civ_count <= caps[map_size] &&
                   first.initial_civ_count != old_value &&
                   ui_worldgen_config_equal(&first, &second);
    }
    worldgen_controls_probe_record(
        report, "worldgen_initial_civs_caps_dice", cap_ok && dice_ok,
        "caps=50,80,115,200 map_sizes=4 cap_plus_one_clamped=%d deterministic=%d changed_mask=%08x map_unchanged=1 different=1",
        cap_ok,
        dice_ok, (unsigned int)expected);
    return cap_ok && dice_ok;
}

static int case_section_dice_scope(WorldgenControlsProbeReport *report) {
    UiWorldgenEffectiveConfig before;
    UiWorldgenEffectiveConfig after;
    UiWorldgenFieldMask physical;
    UiWorldgenFieldMask advanced;
    UiWorldgenFieldMask expected_physical = field_mask(
        UI_WORLDGEN_FIELD_OCEAN, UI_WORLDGEN_FIELD_VEGETATION);
    UiWorldgenFieldMask expected_advanced = field_mask(
        UI_WORLDGEN_FIELD_BIAS_FOREST, UI_WORLDGEN_FIELD_REGION_SIZE);
    int physical_ok;
    int advanced_ok;
    set_config(MAP_SIZE_LARGE, 43);
    ui_worldgen_config_read(&before);
    ui_worldgen_random_validation_set_state(0x13579bdfu);
    physical = ui_worldgen_randomize_physical();
    ui_worldgen_config_read(&after);
    physical_ok = physical == expected_physical &&
                  after.pending_map_size == before.pending_map_size &&
                  after.initial_civ_count == before.initial_civ_count;
    ui_worldgen_config_write(&before);
    ui_worldgen_control_state_resync(UI_WORLDGEN_RESYNC_KEEP_APPLIED);
    ui_worldgen_random_validation_set_state(0x2468ace0u);
    advanced = ui_worldgen_randomize_advanced();
    ui_worldgen_config_read(&after);
    advanced_ok = advanced == expected_advanced &&
                  after.pending_map_size == before.pending_map_size &&
                  after.initial_civ_count == before.initial_civ_count;
    worldgen_controls_probe_record(
        report, "worldgen_initial_civs_section_dice_scope",
        physical_ok && advanced_ok,
        "physical_mask=%08x advanced_mask=%08x map_and_initial_unchanged=%d",
        (unsigned int)physical, (unsigned int)advanced,
        physical_ok && advanced_ok);
    return physical_ok && advanced_ok;
}

static int case_cap_transitions(WorldgenControlsProbeReport *report) {
    UiWorldgenEffectiveConfig config;
    int shrink_ok;
    int expand_ok;
    int programmatic_ok;
    int balanced_ok;
    set_config(MAP_SIZE_EXTREME, 200);
    ui_worldgen_control_state_set_field(UI_WORLDGEN_FIELD_PENDING_MAP_SIZE,
                                        MAP_SIZE_SMALL);
    ui_worldgen_config_read(&config);
    shrink_ok = config.initial_civ_count == 50;
    ui_worldgen_control_state_set_field(UI_WORLDGEN_FIELD_PENDING_MAP_SIZE,
                                        MAP_SIZE_EXTREME);
    ui_worldgen_config_read(&config);
    expand_ok = config.initial_civ_count == 50;
    config.pending_map_size = MAP_SIZE_MEDIUM;
    config.initial_civ_count = 200;
    ui_worldgen_config_write(&config);
    ui_worldgen_control_state_resync(UI_WORLDGEN_RESYNC_KEEP_APPLIED);
    ui_worldgen_config_read(&config);
    programmatic_ok = config.pending_map_size == MAP_SIZE_MEDIUM &&
                      config.initial_civ_count == 80;
    ui_worldgen_control_state_set_field(UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT,
                                        37);
    ui_worldgen_control_state_apply_balanced();
    ui_worldgen_config_read(&config);
    balanced_ok = config.pending_map_size == MAP_SIZE_EXTREME &&
                  config.initial_civ_count == 37;
    worldgen_controls_probe_record(
        report, "worldgen_initial_civs_cap_transitions",
        shrink_ok && expand_ok && programmatic_ok && balanced_ok,
        "shrink_200_to_50=%d expand_stays_50=%d programmatic_cap_80=%d balanced_preserves_37=%d",
        shrink_ok, expand_ok, programmatic_ok, balanced_ok);
    return shrink_ok && expand_ok && programmatic_ok && balanced_ok;
}

static int case_native_sync(WorldgenControlsProbeReport *report, HWND owner) {
    const FormControls *form = ui_worldgen_legacy_forms_controls();
    RECT client;
    UiWorldgenPanelLayout panel;
    WorldgenLayout legacy;
    int geometry_ok;
    int sync_ok;
    int empty_ok;
    int clamp_ok;
    int zero_ok;
    int transition_ok;
    int command_ok;
    int value;
    GetClientRect(owner, &client);
    panel_tab = PANEL_WORLD;
    side_panel_w = 460;
    side_panel_expanded_w = 460;
    side_panel_collapsed = 0;
    set_config(MAP_SIZE_MEDIUM, 20);
    ui_worldgen_control_state_set_tab(UI_WORLDGEN_TAB_HYDROLOGY_REGIONS);
    ui_worldgen_view_build(client, 460, &panel);
    ui_worldgen_control_state_set_scroll(
        UI_WORLDGEN_TAB_HYDROLOGY_REGIONS,
        panel.tab_max_scroll[UI_WORLDGEN_TAB_HYDROLOGY_REGIONS]);
    ui_forms_write_world_setup_controls();
    ui_forms_layout(owner);
    ui_worldgen_view_build(client, 460, &panel);
    geometry_ok = form->initial_civs_edit &&
                  form->hydrology_initial_civs_edit &&
                  form->initial_civs_edit !=
                      form->hydrology_initial_civs_edit &&
                  GetDlgCtrlID(form->initial_civs_edit) ==
                      ID_INITIAL_CIVS_EDIT &&
                  GetDlgCtrlID(form->hydrology_initial_civs_edit) ==
                      ID_HYDROLOGY_INITIAL_CIVS_EDIT &&
                  child_visible(form->hydrology_initial_civs_edit) &&
                  !child_visible(form->initial_civs_edit) &&
                  child_rect_equals(owner, form->hydrology_initial_civs_edit,
                                    panel.hydrology.initial_civs_input);
    SetWindowTextA(form->hydrology_initial_civs_edit, "42");
    sync_ok = ui_forms_handle_metric_change(ID_HYDROLOGY_INITIAL_CIVS_EDIT) &&
              ui_worldgen_config_get_field(
                  UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT) == 42 &&
              text_equals(form->initial_civs_edit, "42");
    ui_worldgen_control_state_set_tab(UI_WORLDGEN_TAB_LEGACY);
    ui_worldgen_control_state_set_scroll(UI_WORLDGEN_TAB_LEGACY, 0);
    ui_forms_layout(owner);
    ui_worldgen_view_build(client, 460, &panel);
    ui_worldgen_view_build_legacy_layout(&panel.legacy, &legacy);
    geometry_ok &= child_visible(form->initial_civs_edit) &&
                   !child_visible(form->hydrology_initial_civs_edit) &&
                   child_rect_equals(owner, form->initial_civs_edit,
                                     legacy.initial_input);
    SetWindowTextA(form->initial_civs_edit, "73");
    sync_ok &= ui_forms_handle_metric_change(ID_INITIAL_CIVS_EDIT) &&
               ui_worldgen_config_get_field(
                   UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT) == 73 &&
               text_equals(form->hydrology_initial_civs_edit, "73");
    ui_worldgen_control_state_set_tab(UI_WORLDGEN_TAB_HYDROLOGY_REGIONS);
    ui_forms_layout(owner);
    SetWindowTextA(form->hydrology_initial_civs_edit, "");
    empty_ok = ui_forms_handle_metric_change(ID_HYDROLOGY_INITIAL_CIVS_EDIT) &&
               ui_worldgen_config_get_field(
                   UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT) == 73 &&
               text_equals(form->initial_civs_edit, "73") &&
               text_equals(form->hydrology_initial_civs_edit, "");
    empty_ok &= ui_forms_normalize_metric_edit(
                    ID_HYDROLOGY_INITIAL_CIVS_EDIT) &&
                text_equals(form->initial_civs_edit, "73") &&
                text_equals(form->hydrology_initial_civs_edit, "73");
    ui_worldgen_control_state_set_field(UI_WORLDGEN_FIELD_PENDING_MAP_SIZE,
                                        MAP_SIZE_SMALL);
    ui_forms_write_world_setup_controls();
    SetWindowTextA(form->hydrology_initial_civs_edit, "51");
    clamp_ok = ui_forms_handle_metric_change(ID_HYDROLOGY_INITIAL_CIVS_EDIT) &&
               ui_worldgen_config_get_field(
                   UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT) == 50 &&
               text_equals(form->initial_civs_edit, "50") &&
               text_equals(form->hydrology_initial_civs_edit, "50");
    SetWindowTextA(form->hydrology_initial_civs_edit, "0");
    zero_ok = ui_forms_handle_metric_change(ID_HYDROLOGY_INITIAL_CIVS_EDIT) &&
              ui_worldgen_config_get_field(
                  UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT) == 0 &&
              text_equals(form->initial_civs_edit, "0") &&
              text_equals(form->hydrology_initial_civs_edit, "0");
    set_config(MAP_SIZE_EXTREME, 200);
    ui_forms_write_world_setup_controls();
    ui_worldgen_control_state_set_field(UI_WORLDGEN_FIELD_PENDING_MAP_SIZE,
                                        MAP_SIZE_SMALL);
    ui_forms_write_world_setup_controls();
    transition_ok = text_equals(form->initial_civs_edit, "50") &&
                    text_equals(form->hydrology_initial_civs_edit, "50");
    ui_worldgen_control_state_set_field(UI_WORLDGEN_FIELD_PENDING_MAP_SIZE,
                                        MAP_SIZE_EXTREME);
    ui_forms_write_world_setup_controls();
    transition_ok &= text_equals(form->initial_civs_edit, "50") &&
                     text_equals(form->hydrology_initial_civs_edit, "50");
    set_config(MAP_SIZE_SMALL, 17);
    ui_forms_write_world_setup_controls();
    ui_worldgen_random_validation_set_state(0x4a11ce55u);
    ui_worldgen_command_dice(owner);
    value = ui_worldgen_config_get_field(UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT);
    command_ok = value >= 1 && value <= 50 && value != 17;
    {
        char expected[16];
        snprintf(expected, sizeof(expected), "%d", value);
        command_ok &= text_equals(form->initial_civs_edit, expected) &&
                      text_equals(form->hydrology_initial_civs_edit, expected);
    }
    ui_worldgen_command_reset(owner);
    {
        char expected[16];
        snprintf(expected, sizeof(expected), "%d", value);
        command_ok &= ui_worldgen_config_get_field(
                          UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT) == value &&
                      text_equals(form->initial_civs_edit, expected) &&
                      text_equals(form->hydrology_initial_civs_edit,
                                  expected);
    }
    pending_map_size = MAP_SIZE_SMALL;
    initial_civ_count = 200;
    ui_worldgen_command_resync_after_load(owner);
    command_ok &= ui_worldgen_config_get_field(
                      UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT) == 50 &&
                  text_equals(form->initial_civs_edit, "50") &&
                  text_equals(form->hydrology_initial_civs_edit, "50");
    worldgen_controls_probe_record(
        report, "worldgen_initial_civs_native_sync",
        geometry_ok && sync_ok && empty_ok && clamp_ok && zero_ok &&
            transition_ok && command_ok,
        "geometry=%d two_way=%d temporary_empty=%d cap_plus_one_immediate=%d zero=%d shrink_expand=%d dice_reset_load=%d",
        geometry_ok, sync_ok, empty_ok, clamp_ok, zero_ok, transition_ok,
        command_ok);
    return geometry_ok && sync_ok && empty_ok && clamp_ok && zero_ok &&
           transition_ok && command_ok;
}

int worldgen_initial_civs_probe_run(WorldgenControlsProbeReport *report,
                                    HWND owner) {
    int ok = 1;
    ok &= case_caps_and_dice(report);
    ok &= case_section_dice_scope(report);
    ok &= case_cap_transitions(report);
    ok &= case_native_sync(report, owner);
    return ok;
}

static int require_native_mask(WorldgenControlsArtifactWriter *writer,
                               const char *label, uint32_t expected) {
    int ok = writer->last_native_mask == expected &&
             writer->last_native_visible > 0 &&
             writer->last_native_composited > 0 &&
             writer->last_native_edit_text_expected ==
                 writer->last_native_edit_text_rendered &&
             writer->last_native_edit_text_pixels > 0;
    fprintf(writer->manifest,
            "native_case=%s ok=%d expected_mask=%08x actual_mask=%08x visible=%u composited=%u text=%u/%u pixels=%u\n",
            label, ok, expected, writer->last_native_mask,
            writer->last_native_visible, writer->last_native_composited,
            writer->last_native_edit_text_rendered,
            writer->last_native_edit_text_expected,
            writer->last_native_edit_text_pixels);
    if (!ok) writer->failure_count++;
    return ok;
}

static void prepare_artifact_state(int map_size, UiWorldgenControlTab tab,
                                   int width) {
    RECT client = {0, 0, width, WORLDGEN_CONTROLS_ARTIFACT_HEIGHT};
    UiWorldgenPanelLayout layout;
    int cap = ui_worldgen_initial_civ_cap_for_map_size(map_size);
    set_config(map_size, cap);
    ui_worldgen_control_state_set_tab(tab);
    ui_worldgen_control_state_clear_interaction();
    hover_x = -1;
    hover_y = -1;
    ui_forms_write_world_setup_controls();
    if (tab == UI_WORLDGEN_TAB_HYDROLOGY_REGIONS) {
        ui_worldgen_view_build(client, width, &layout);
        ui_worldgen_control_state_set_scroll(
            tab, layout.tab_max_scroll[UI_WORLDGEN_TAB_HYDROLOGY_REGIONS]);
    } else {
        ui_worldgen_control_state_set_scroll(tab, 0);
    }
}

int worldgen_initial_civs_artifact_matrix(
    WorldgenControlsArtifactWriter *writer) {
    static const int widths[] = {500, 720};
    static const int languages[] = {UI_LANG_EN, UI_LANG_ZH};
    static const UiWorldgenControlTab tabs[] = {
        UI_WORLDGEN_TAB_HYDROLOGY_REGIONS, UI_WORLDGEN_TAB_LEGACY
    };
    static const char *map_names[] = {"small", "medium", "large", "extreme"};
    static const char *tab_names[] = {"hydrology", "legacy"};
    static const char *language_names[] = {"en", "zh"};
    char filename[128];
    char native_label[128];
    int ok = 1;
    int map_size;
    int tab_index;
    int language_index;
    int width_index;
    for (map_size = MAP_SIZE_SMALL; map_size < MAP_SIZE_COUNT; map_size++) {
        for (tab_index = 0; tab_index < 2; tab_index++) {
            for (language_index = 0; language_index < 2; language_index++) {
                for (width_index = 0; width_index < 2; width_index++) {
                    uint32_t mask = tab_index == 0 ?
                        WORLDGEN_CONTROLS_NATIVE_HYDROLOGY_INITIAL :
                        WORLDGEN_CONTROLS_NATIVE_INITIAL;
                    prepare_artifact_state(map_size, tabs[tab_index],
                                           widths[width_index]);
                    snprintf(filename, sizeof(filename),
                             "worldgen_initial_civs_%s_%s_%s_%d.bmp",
                             map_names[map_size], tab_names[tab_index],
                             language_names[language_index], widths[width_index]);
                    ok &= worldgen_controls_artifact_render(
                        writer, filename, widths[width_index],
                        languages[language_index]);
                    snprintf(native_label, sizeof(native_label), "%s_%s_%s_%d",
                             map_names[map_size], tab_names[tab_index],
                             language_names[language_index], widths[width_index]);
                    ok &= require_native_mask(writer, native_label, mask);
                }
            }
        }
    }
    return ok;
}
