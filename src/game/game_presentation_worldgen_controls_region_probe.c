#include "game/game_presentation_worldgen_controls_probe_internal.h"

#include "core/constants.h"
#include "ui/ui_worldgen_command.h"
#include "ui/ui_worldgen_config_adapter.h"
#include "ui/ui_worldgen_control_state.h"

static const int EXPECTED_REGIONS[MAP_SIZE_COUNT]
                                 [UI_WORLDGEN_REGION_PRESET_COUNT] = {
    {5, 20, 40, 60, 80},
    {10, 30, 50, 70, 90},
    {20, 40, 60, 80, 100},
    {30, 50, 70, 85, 100}
};

static int case_scalar_one_to_one(WorldgenControlsProbeReport *report) {
    static const int vegetation_values[] = {0, 1, 50, 99, 100};
    static const int river_values[] = {0, 13, 25, 50, 62, 75, 99, 100};
    int vegetation_ok = 1;
    int river_ok = 1;
    int i;
    ui_worldgen_control_state_init_fresh();
    for (i = 0; i < (int)(sizeof(vegetation_values) /
                           sizeof(vegetation_values[0])); i++) {
        ui_worldgen_control_state_set_field(
            UI_WORLDGEN_FIELD_VEGETATION, vegetation_values[i]);
        vegetation_ok &= ui_worldgen_config_get_field(
            UI_WORLDGEN_FIELD_VEGETATION) == vegetation_values[i];
    }
    for (i = 0; i < (int)(sizeof(river_values) /
                           sizeof(river_values[0])); i++) {
        ui_worldgen_control_state_set_field(
            UI_WORLDGEN_FIELD_BIAS_WETLAND, river_values[i]);
        river_ok &= ui_worldgen_config_get_field(
            UI_WORLDGEN_FIELD_BIAS_WETLAND) == river_values[i];
    }
    worldgen_controls_probe_record(
        report, "worldgen_controls_vegetation_river_exact",
        vegetation_ok && river_ok,
        "vegetation_samples=%d vegetation_ok=%d river_samples=%d river_ok=%d",
        (int)(sizeof(vegetation_values) / sizeof(vegetation_values[0])),
        vegetation_ok,
        (int)(sizeof(river_values) / sizeof(river_values[0])), river_ok);
    return vegetation_ok && river_ok;
}

static int case_region_matrix(WorldgenControlsProbeReport *report) {
    int ok = 1;
    int size;
    int category;
    int samples = 0;
    for (size = MAP_SIZE_SMALL; size < MAP_SIZE_COUNT; size++) {
        for (category = UI_WORLDGEN_REGION_VERY_SMALL;
             category <= UI_WORLDGEN_REGION_VERY_LARGE; category++) {
            int expected = EXPECTED_REGIONS[size][category];
            int actual = ui_worldgen_region_preset_value(
                size, (UiWorldgenRegionCategory)category);
            ok &= actual == expected;
            ok &= ui_worldgen_region_category_for_value(size, actual) ==
                  (UiWorldgenRegionCategory)category;
            ui_worldgen_control_state_init_fresh();
            ui_worldgen_control_state_set_field(
                UI_WORLDGEN_FIELD_PENDING_MAP_SIZE, size);
            ui_worldgen_control_state_set_region_category(
                (UiWorldgenRegionCategory)category);
            ok &= ui_worldgen_config_get_field(
                      UI_WORLDGEN_FIELD_REGION_SIZE) == expected;
            ok &= ui_worldgen_control_state_get()->region_category ==
                  (UiWorldgenRegionCategory)category;
            samples++;
        }
    }
    worldgen_controls_probe_record(
        report, "worldgen_controls_region_4x5_matrix", ok,
        "samples=%d table_exact=%d auto_selection=%d",
        samples, ok, ok);
    return ok;
}

static int case_region_custom(WorldgenControlsProbeReport *report) {
    static const int raw_values[] = {0, 1, 49, 50, 99, 100};
    int ok = 1;
    int auto_named = 0;
    int custom = 0;
    int size;
    int i;
    for (size = MAP_SIZE_SMALL; size < MAP_SIZE_COUNT; size++) {
        for (i = 0; i < (int)(sizeof(raw_values) /
                              sizeof(raw_values[0])); i++) {
            UiWorldgenRegionCategory expected;
            ui_worldgen_control_state_init_fresh();
            ui_worldgen_control_state_set_field(
                UI_WORLDGEN_FIELD_PENDING_MAP_SIZE, size);
            ui_worldgen_control_state_set_region_custom_value(raw_values[i]);
            expected = ui_worldgen_region_category_for_value(
                size, raw_values[i]);
            ok &= ui_worldgen_config_get_field(
                      UI_WORLDGEN_FIELD_REGION_SIZE) == raw_values[i];
            ok &= ui_worldgen_control_state_get()->region_category == expected;
            if (expected == UI_WORLDGEN_REGION_CUSTOM) custom++;
            else auto_named++;
        }
    }
    worldgen_controls_probe_record(
        report, "worldgen_controls_region_custom_auto_select", ok,
        "samples=%d named_matches=%d custom_matches=%d exact_raw=%d",
        MAP_SIZE_COUNT *
            (int)(sizeof(raw_values) / sizeof(raw_values[0])),
        auto_named, custom, ok);
    return ok;
}

static int case_region_map_size_preservation(
    WorldgenControlsProbeReport *report) {
    int named_ok = 1;
    int custom_ok = 1;
    int category;
    int source_size;
    int target_size;
    for (category = UI_WORLDGEN_REGION_VERY_SMALL;
         category <= UI_WORLDGEN_REGION_VERY_LARGE; category++) {
        for (source_size = MAP_SIZE_SMALL;
             source_size < MAP_SIZE_COUNT; source_size++) {
            for (target_size = MAP_SIZE_SMALL;
                 target_size < MAP_SIZE_COUNT; target_size++) {
                ui_worldgen_control_state_init_fresh();
                ui_worldgen_control_state_set_field(
                    UI_WORLDGEN_FIELD_PENDING_MAP_SIZE, source_size);
                ui_worldgen_control_state_set_region_category(
                    (UiWorldgenRegionCategory)category);
                ui_worldgen_control_state_set_field(
                    UI_WORLDGEN_FIELD_PENDING_MAP_SIZE, target_size);
                named_ok &= ui_worldgen_control_state_get()->region_category ==
                            (UiWorldgenRegionCategory)category;
                named_ok &= ui_worldgen_config_get_field(
                    UI_WORLDGEN_FIELD_REGION_SIZE) ==
                    EXPECTED_REGIONS[target_size][category];
            }
        }
    }
    for (source_size = MAP_SIZE_SMALL;
         source_size < MAP_SIZE_COUNT; source_size++) {
        for (target_size = MAP_SIZE_SMALL;
             target_size < MAP_SIZE_COUNT; target_size++) {
            ui_worldgen_control_state_init_fresh();
            ui_worldgen_control_state_set_field(
                UI_WORLDGEN_FIELD_PENDING_MAP_SIZE, source_size);
            ui_worldgen_control_state_set_region_custom_value(49);
            custom_ok &= ui_worldgen_control_state_get()->region_category ==
                         UI_WORLDGEN_REGION_CUSTOM;
            ui_worldgen_control_state_set_field(
                UI_WORLDGEN_FIELD_PENDING_MAP_SIZE, target_size);
            custom_ok &= ui_worldgen_config_get_field(
                             UI_WORLDGEN_FIELD_REGION_SIZE) == 49;
            custom_ok &= ui_worldgen_control_state_get()->region_category ==
                         UI_WORLDGEN_REGION_CUSTOM;
        }
    }
    worldgen_controls_probe_record(
        report, "worldgen_controls_region_map_size_preservation",
        named_ok && custom_ok,
        "named_transitions=%d named_ok=%d custom_transitions=%d custom_ok=%d",
        UI_WORLDGEN_REGION_PRESET_COUNT * MAP_SIZE_COUNT * MAP_SIZE_COUNT,
        named_ok, MAP_SIZE_COUNT * MAP_SIZE_COUNT, custom_ok);
    return named_ok && custom_ok;
}

static int case_external_resync(WorldgenControlsProbeReport *report) {
    UiWorldgenEffectiveConfig external = {
        MAP_SIZE_MEDIUM, 9, 19, 29, 41, 63, 73,
        35, 57, 81, 67, 49, 31
    };
    const UiWorldgenControlState *state;
    int ok;
    ui_worldgen_control_state_init_fresh();
    ui_worldgen_config_write(&external);
    ui_worldgen_control_state_resync(UI_WORLDGEN_RESYNC_MARK_APPLIED);
    state = ui_worldgen_control_state_get();
    ok = state->region_category == UI_WORLDGEN_REGION_CUSTOM &&
         state->preset == UI_WORLDGEN_PRESET_CUSTOM &&
         state->has_applied_config && !state->dirty &&
         state->climate_corners.top_left.x == -18 &&
         state->climate_corners.bottom_left.x == -17 &&
         state->climate_corners.top_right.x == 29 &&
         state->climate_corners.bottom_right.x == 28 &&
         state->climate_corners.top_left.y == 21 &&
         state->climate_corners.top_right.y == 20 &&
         state->climate_corners.bottom_left.y == -32 &&
         state->climate_corners.bottom_right.y == -31;
    worldgen_controls_probe_record(
        report, "worldgen_controls_external_load_resync", ok,
        "category=%d preset=%d dirty=%d climate_inverse_exact=%d",
        state->region_category, state->preset, state->dirty, ok);
    return ok;
}

static int case_independent_tabs(WorldgenControlsProbeReport *report) {
    static const int offsets[UI_WORLDGEN_TAB_COUNT] = {11, 22, 33, 44};
    UiWorldgenEffectiveConfig before;
    UiWorldgenEffectiveConfig after;
    const UiWorldgenControlState *state;
    int ok = 1;
    int tab;
    ui_worldgen_control_state_init_fresh();
    ui_worldgen_control_state_set_field(UI_WORLDGEN_FIELD_OCEAN, 38);
    ui_worldgen_control_state_set_field(UI_WORLDGEN_FIELD_RELIEF, 74);
    ui_worldgen_config_read(&before);
    for (tab = 0; tab < UI_WORLDGEN_TAB_COUNT; tab++) {
        ui_worldgen_control_state_set_scroll(
            (UiWorldgenControlTab)tab, offsets[tab]);
    }
    for (tab = UI_WORLDGEN_TAB_COUNT - 1; tab >= 0; tab--) {
        ui_worldgen_control_state_set_tab((UiWorldgenControlTab)tab);
        state = ui_worldgen_control_state_get();
        ok &= state->tab == (UiWorldgenControlTab)tab;
        ok &= state->scroll_offsets[tab] == offsets[tab];
    }
    ui_worldgen_config_read(&after);
    ok &= ui_worldgen_config_equal(&before, &after);
    worldgen_controls_probe_record(
        report, "worldgen_controls_independent_tab_scroll_values", ok,
        "tabs=%d offsets=%d,%d,%d,%d config_preserved=%d",
        UI_WORLDGEN_TAB_COUNT, offsets[0], offsets[1], offsets[2], offsets[3],
        ui_worldgen_config_equal(&before, &after));
    return ok;
}

static int case_region_deferred(WorldgenControlsProbeReport *report) {
    UiWorldgenCommandDiagnostics diagnostics;
    int ok;
    ui_worldgen_control_state_init_fresh();
    ui_worldgen_command_reset_diagnostics();
    ui_worldgen_control_state_set_region_category(UI_WORLDGEN_REGION_LARGE);
    ui_worldgen_control_state_set_region_custom_value(49);
    ui_worldgen_control_state_set_field(UI_WORLDGEN_FIELD_PENDING_MAP_SIZE,
                                        MAP_SIZE_SMALL);
    ui_worldgen_command_get_diagnostics(&diagnostics);
    ok = diagnostics.generate_commands == 0 &&
         diagnostics.generator_calls == 0 &&
         diagnostics.successful_generations == 0;
    worldgen_controls_probe_record(
        report, "worldgen_controls_region_edit_deferred", ok,
        "generate_commands=%u generator_calls=%u successes=%u",
        diagnostics.generate_commands, diagnostics.generator_calls,
        diagnostics.successful_generations);
    return ok;
}

int worldgen_controls_probe_regions(WorldgenControlsProbeReport *report) {
    int ok = 1;
    ok &= case_scalar_one_to_one(report);
    ok &= case_region_matrix(report);
    ok &= case_region_custom(report);
    ok &= case_region_map_size_preservation(report);
    ok &= case_external_resync(report);
    ok &= case_independent_tabs(report);
    ok &= case_region_deferred(report);
    return ok;
}
