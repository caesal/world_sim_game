#include "game/game_presentation_worldgen_controls_probe_internal.h"

#include "core/constants.h"
#include "ui/ui_worldgen_config_adapter.h"
#include "ui/ui_worldgen_control_state.h"
#include "ui/ui_worldgen_random.h"

#include <string.h>

static UiWorldgenFieldMask randomized_mask(void) {
    UiWorldgenFieldMask mask = 0;
    int field;
    for (field = UI_WORLDGEN_FIELD_OCEAN;
         field <= UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT; field++) {
        mask |= UI_WORLDGEN_FIELD_MASK(field);
    }
    return mask;
}

static int case_balanced(WorldgenControlsProbeReport *report) {
    UiWorldgenEffectiveConfig config;
    const UiWorldgenControlState *state;
    int initial = 37;
    ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT, initial);
    ui_worldgen_control_state_init_fresh();
    ui_worldgen_config_read(&config);
    state = ui_worldgen_control_state_get();
    {
        int ok = ui_worldgen_config_is_balanced(&config) &&
                 config.pending_map_size == MAP_SIZE_EXTREME &&
                 config.region_size_slider == 70 &&
                 config.initial_civ_count == initial &&
                 state->tab == UI_WORLDGEN_TAB_PHYSICAL &&
                 state->region_category == UI_WORLDGEN_REGION_MEDIUM &&
                 state->preset == UI_WORLDGEN_PRESET_BALANCED &&
                 state->has_applied_config && !state->dirty;
        worldgen_controls_probe_record(
            report, "worldgen_controls_balanced_startup", ok,
            "map_size=%d region=%d initial=%d tab=%d preset=%d dirty=%d",
            config.pending_map_size, config.region_size_slider,
            config.initial_civ_count, state->tab, state->preset, state->dirty);
        return ok;
    }
}

static int case_balanced_custom(WorldgenControlsProbeReport *report) {
    const UiWorldgenControlState *state;
    int custom_ok;
    int balanced_ok;
    ui_worldgen_control_state_init_fresh();
    ui_worldgen_control_state_set_field(UI_WORLDGEN_FIELD_OCEAN, 51);
    state = ui_worldgen_control_state_get();
    custom_ok = state->preset == UI_WORLDGEN_PRESET_CUSTOM && state->dirty;
    ui_worldgen_control_state_apply_balanced();
    state = ui_worldgen_control_state_get();
    balanced_ok = state->preset == UI_WORLDGEN_PRESET_BALANCED &&
                  !state->dirty;
    worldgen_controls_probe_record(
        report, "worldgen_controls_balanced_custom_transition",
        custom_ok && balanced_ok,
        "custom_ok=%d balanced_ok=%d", custom_ok, balanced_ok);
    return custom_ok && balanced_ok;
}

static int case_dice_reset_scope(WorldgenControlsProbeReport *report) {
    UiWorldgenEffectiveConfig before;
    UiWorldgenEffectiveConfig after;
    UiWorldgenFieldMask changed;
    UiWorldgenFieldMask expected = randomized_mask();
    int diced_initial;
    int dice_ok;
    int reset_ok;
    ui_worldgen_control_state_init_fresh();
    ui_worldgen_control_state_set_field(UI_WORLDGEN_FIELD_PENDING_MAP_SIZE,
                                        MAP_SIZE_LARGE);
    ui_worldgen_control_state_set_field(UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT,
                                        43);
    ui_worldgen_config_read(&before);
    ui_worldgen_random_validation_set_state(0x31415926u);
    changed = ui_worldgen_randomize_all();
    ui_worldgen_control_state_resync(UI_WORLDGEN_RESYNC_KEEP_APPLIED);
    ui_worldgen_config_read(&after);
    dice_ok = changed == expected &&
              after.pending_map_size == before.pending_map_size &&
              after.initial_civ_count >= 1 &&
              after.initial_civ_count <=
                  ui_worldgen_initial_civ_cap_for_map_size(
                      after.pending_map_size) &&
              after.initial_civ_count != before.initial_civ_count;
    diced_initial = after.initial_civ_count;
    ui_worldgen_control_state_apply_balanced();
    ui_worldgen_config_read(&after);
    reset_ok = ui_worldgen_config_is_balanced(&after) &&
               after.initial_civ_count == diced_initial;
    worldgen_controls_probe_record(
        report, "worldgen_controls_dice_reset_scope", dice_ok && reset_ok,
        "dice_ok=%d changed_mask=%08x expected_mask=%08x map_before=%d map_after_dice=%d initial_before=%d initial_after=%d reset_ok=%d",
        dice_ok, (unsigned int)changed, (unsigned int)expected,
        before.pending_map_size, dice_ok ? before.pending_map_size :
        after.pending_map_size, before.initial_civ_count, diced_initial,
        reset_ok);
    return dice_ok && reset_ok;
}

static int case_xy_and_relief(WorldgenControlsProbeReport *report) {
    static const int values[] = {0, 1, 49, 50, 51, 99, 100};
    UiWorldgenEffectiveConfig config;
    int xy_ok = 1;
    int relief_ok;
    int x;
    int y;
    for (x = 0; x < (int)(sizeof(values) / sizeof(values[0])); x++) {
        for (y = 0; y < (int)(sizeof(values) / sizeof(values[0])); y++) {
            ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_CONTINENT,
                                         values[x]);
            ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_OCEAN, values[y]);
            ui_worldgen_config_read(&config);
            xy_ok &= config.continent_slider == values[x] &&
                     config.ocean_slider == values[y];
        }
    }
    ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_RELIEF, 20);
    ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_BIAS_MOUNTAIN, 80);
    ui_worldgen_config_read(&config);
    relief_ok = config.relief_slider == 20 &&
                config.bias_mountain_slider == 80;
    ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_RELIEF, 80);
    ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_BIAS_MOUNTAIN, 20);
    ui_worldgen_config_read(&config);
    relief_ok &= config.relief_slider == 80 &&
                 config.bias_mountain_slider == 20;
    worldgen_controls_probe_record(
        report, "worldgen_controls_xy_relief_identity", xy_ok && relief_ok,
        "xy_samples=%d xy_ok=%d relief_crossed_ok=%d",
        (int)(sizeof(values) / sizeof(values[0])) *
            (int)(sizeof(values) / sizeof(values[0])),
        xy_ok, relief_ok);
    return xy_ok && relief_ok;
}

static int point_equal(UiWorldgenPoint point, int x, int y) {
    return point.x == x && point.y == y;
}

static int case_climate_defaults_limits(WorldgenControlsProbeReport *report) {
    UiWorldgenEffectiveConfig config;
    UiWorldgenClimateCorners corners;
    UiWorldgenPoint limits[UI_WORLDGEN_CLIMATE_CORNER_COUNT] = {
        {-100, 100}, {100, 100}, {100, -100}, {-100, -100}
    };
    int limits_ok = 1;
    int i;
    ui_worldgen_config_make_balanced(&config);
    ui_worldgen_climate_corners_from_config(&config, &corners);
    {
        int defaults_ok = point_equal(corners.top_left, -25, 25) &&
                          point_equal(corners.top_right, 25, 25) &&
                          point_equal(corners.bottom_right, 25, -25) &&
                          point_equal(corners.bottom_left, -25, -25);
        for (i = 0; i < UI_WORLDGEN_CLIMATE_CORNER_COUNT; i++) {
            ui_worldgen_climate_clamp_corner(
                (UiWorldgenClimateCornerId)i, &limits[i]);
        }
        limits_ok = point_equal(limits[0], -50, 50) &&
                    point_equal(limits[1], 50, 50) &&
                    point_equal(limits[2], 50, -50) &&
                    point_equal(limits[3], -50, -50);
        worldgen_controls_probe_record(
            report, "worldgen_controls_climate_defaults_limits",
            defaults_ok && limits_ok,
            "defaults_ok=%d quadrant_limits_ok=%d", defaults_ok, limits_ok);
        return defaults_ok && limits_ok;
    }
}

static int case_climate_forward_irregular(
    WorldgenControlsProbeReport *report) {
    UiWorldgenClimateCorners irregular = {
        {-41, 37}, {12, 48}, {45, -7}, {-8, -33}
    };
    UiWorldgenEffectiveConfig config;
    const UiWorldgenControlState *state;
    int forward_ok;
    int state_ok;
    ui_worldgen_config_make_balanced(&config);
    ui_worldgen_climate_corners_to_config(&irregular, &config);
    forward_ok = config.bias_forest_slider == 49 &&
                 config.bias_desert_slider == 57 &&
                 config.moisture_slider == 85 &&
                 config.drought_slider == 40;
    ui_worldgen_control_state_init_fresh();
    ui_worldgen_control_state_set_climate_corner(
        UI_WORLDGEN_CLIMATE_TOP_LEFT, -41, 37);
    ui_worldgen_control_state_set_climate_corner(
        UI_WORLDGEN_CLIMATE_TOP_RIGHT, 12, 48);
    ui_worldgen_control_state_set_climate_corner(
        UI_WORLDGEN_CLIMATE_BOTTOM_RIGHT, 45, -7);
    ui_worldgen_control_state_set_climate_corner(
        UI_WORLDGEN_CLIMATE_BOTTOM_LEFT, -8, -33);
    state = ui_worldgen_control_state_get();
    state_ok = memcmp(&state->climate_corners, &irregular,
                      sizeof(irregular)) == 0;
    ui_worldgen_config_read(&config);
    state_ok &= config.bias_forest_slider == 49 &&
                config.bias_desert_slider == 57 &&
                config.moisture_slider == 85 &&
                config.drought_slider == 40;
    worldgen_controls_probe_record(
        report, "worldgen_controls_climate_irregular_forward",
        forward_ok && state_ok,
        "forward_ok=%d preserved_components=%d forest=%d desert=%d moisture=%d drought=%d",
        forward_ok, state_ok, config.bias_forest_slider,
        config.bias_desert_slider, config.moisture_slider,
        config.drought_slider);
    return forward_ok && state_ok;
}

static int case_climate_inverse_all(WorldgenControlsProbeReport *report) {
    UiWorldgenEffectiveConfig config;
    UiWorldgenEffectiveConfig round_trip;
    UiWorldgenClimateCorners corners;
    int ok = 1;
    int value;
    for (value = 0; value <= 100; value++) {
        ui_worldgen_config_make_balanced(&config);
        config.bias_forest_slider = value;
        config.bias_desert_slider = value;
        config.moisture_slider = value;
        config.drought_slider = value;
        ui_worldgen_climate_corners_from_config(&config, &corners);
        ok &= point_equal(corners.top_left, -(value + 1) / 2,
                          (value + 1) / 2);
        ok &= point_equal(corners.top_right, (value + 1) / 2, value / 2);
        ok &= point_equal(corners.bottom_right, value / 2, -(value / 2));
        ok &= point_equal(corners.bottom_left, -(value / 2),
                          -(value + 1) / 2);
        round_trip = config;
        ui_worldgen_climate_corners_to_config(&corners, &round_trip);
        ok &= round_trip.bias_forest_slider == value &&
              round_trip.bias_desert_slider == value &&
              round_trip.moisture_slider == value &&
              round_trip.drought_slider == value;
    }
    worldgen_controls_probe_record(
        report, "worldgen_controls_climate_inverse_0_100", ok,
        "integer_values=101 odd_remainder_order=ceil_then_floor round_trip=%d",
        ok);
    return ok;
}

static UiWorldgenEffectiveConfig round_trip_fixture(int offset) {
    UiWorldgenEffectiveConfig config = {
        MAP_SIZE_LARGE, 3, 17, 29, 41, 53, 67,
        79, 83, 91, 97, 43, 26
    };
    config.ocean_slider += offset;
    config.moisture_slider += offset;
    config.region_size_slider += offset;
    return config;
}

static void write_new_origin(const UiWorldgenEffectiveConfig *config) {
    const int values[UI_WORLDGEN_FIELD_COUNT] = {
        config->pending_map_size, config->ocean_slider,
        config->continent_slider, config->relief_slider,
        config->moisture_slider, config->drought_slider,
        config->vegetation_slider, config->bias_forest_slider,
        config->bias_desert_slider, config->bias_mountain_slider,
        config->bias_wetland_slider, config->region_size_slider,
        config->initial_civ_count
    };
    int field;
    for (field = 0; field < UI_WORLDGEN_FIELD_COUNT; field++) {
        ui_worldgen_control_state_set_field(
            (UiWorldgenConfigField)field, values[field]);
    }
}

static int round_trip_matches(const UiWorldgenEffectiveConfig *expected,
                              UiWorldgenControlTab expected_tab) {
    UiWorldgenEffectiveConfig observed;
    UiWorldgenClimateCorners expected_corners;
    const UiWorldgenControlState *state = ui_worldgen_control_state_get();
    ui_worldgen_config_read(&observed);
    ui_worldgen_climate_corners_from_config(expected, &expected_corners);
    return state->tab == expected_tab &&
           ui_worldgen_config_equal(expected, &observed) &&
           memcmp(&state->climate_corners, &expected_corners,
                  sizeof(expected_corners)) == 0 &&
           state->region_category == ui_worldgen_region_category_for_value(
               expected->pending_map_size, expected->region_size_slider);
}

static int case_bidirectional_round_trips(
    WorldgenControlsProbeReport *report) {
    UiWorldgenEffectiveConfig new_values = round_trip_fixture(0);
    UiWorldgenEffectiveConfig legacy_values = round_trip_fixture(1);
    int new_legacy_new_ok;
    int legacy_new_legacy_ok;
    ui_worldgen_control_state_init_fresh();
    write_new_origin(&new_values);
    ui_worldgen_control_state_set_tab(UI_WORLDGEN_TAB_LEGACY);
    ui_worldgen_control_state_set_tab(UI_WORLDGEN_TAB_PHYSICAL);
    new_legacy_new_ok = round_trip_matches(
        &new_values, UI_WORLDGEN_TAB_PHYSICAL);

    ui_worldgen_control_state_init_fresh();
    ui_worldgen_control_state_set_tab(UI_WORLDGEN_TAB_LEGACY);
    ui_worldgen_config_write(&legacy_values);
    ui_worldgen_control_state_resync(UI_WORLDGEN_RESYNC_KEEP_APPLIED);
    ui_worldgen_control_state_set_tab(UI_WORLDGEN_TAB_CLIMATE);
    ui_worldgen_control_state_set_tab(UI_WORLDGEN_TAB_LEGACY);
    legacy_new_legacy_ok = round_trip_matches(
        &legacy_values, UI_WORLDGEN_TAB_LEGACY);
    worldgen_controls_probe_record(
        report, "worldgen_controls_bidirectional_round_trips",
        new_legacy_new_ok && legacy_new_legacy_ok,
        "new_origin_legacy_physical=%d legacy_origin_climate_legacy=%d tuple_and_decomposition=%d",
        new_legacy_new_ok, legacy_new_legacy_ok,
        new_legacy_new_ok && legacy_new_legacy_ok);
    return new_legacy_new_ok && legacy_new_legacy_ok;
}

static int expected_signed_round(int numerator, int denominator) {
    if (numerator >= 0) return (numerator + denominator / 2) / denominator;
    return -((-numerator + denominator / 2) / denominator);
}

static int case_fingerprint_exact_axes(
    WorldgenControlsProbeReport *report) {
    UiWorldgenEffectiveConfig config = {
        MAP_SIZE_LARGE, 73, 31, 20, 12, 67, 76,
        83, 17, 81, 63, 50, 26
    };
    UiWorldgenFingerprint fingerprint;
    UiWorldgenEffectiveConfig low = config;
    UiWorldgenEffectiveConfig high = config;
    UiWorldgenFingerprint low_result;
    UiWorldgenFingerprint high_result;
    static const int expected[UI_WORLDGEN_FINGERPRINT_AXIS_COUNT] = {
        73, 69, 51, 33, 36, 63, 38
    };
    int exact_ok = 1;
    int clamp_ok;
    int axis;
    ui_worldgen_config_fingerprint(&config, &fingerprint);
    for (axis = 0; axis < UI_WORLDGEN_FINGERPRINT_AXIS_COUNT; axis++) {
        exact_ok &= fingerprint.values[axis] == expected[axis];
    }
    low.ocean_slider = -7;
    low.continent_slider = 109;
    low.relief_slider = -80;
    low.bias_mountain_slider = -80;
    low.bias_wetland_slider = -3;
    low.region_size_slider = -9;
    high.ocean_slider = 107;
    high.continent_slider = -9;
    high.relief_slider = 120;
    high.bias_mountain_slider = 120;
    high.bias_wetland_slider = 107;
    high.region_size_slider = 121;
    ui_worldgen_config_fingerprint(&low, &low_result);
    ui_worldgen_config_fingerprint(&high, &high_result);
    clamp_ok = low_result.values[UI_WORLDGEN_FINGERPRINT_OCEAN] == 0 &&
               low_result.values[UI_WORLDGEN_FINGERPRINT_LANDMASS] == 0 &&
               low_result.values[UI_WORLDGEN_FINGERPRINT_RELIEF] == 0 &&
               low_result.values[UI_WORLDGEN_FINGERPRINT_RIVERS] == 0 &&
               low_result.values[UI_WORLDGEN_FINGERPRINT_REGIONS] == 0 &&
               high_result.values[UI_WORLDGEN_FINGERPRINT_OCEAN] == 100 &&
               high_result.values[UI_WORLDGEN_FINGERPRINT_LANDMASS] == 100 &&
               high_result.values[UI_WORLDGEN_FINGERPRINT_RELIEF] == 100 &&
               high_result.values[UI_WORLDGEN_FINGERPRINT_RIVERS] == 100 &&
               high_result.values[UI_WORLDGEN_FINGERPRINT_REGIONS] == 100;
    worldgen_controls_probe_record(
        report, "worldgen_controls_fingerprint_exact_axes",
        exact_ok && clamp_ok,
        "axes=%d exact=%d clamps=%d values=%d,%d,%d,%d,%d,%d,%d",
        UI_WORLDGEN_FINGERPRINT_AXIS_COUNT, exact_ok, clamp_ok,
        fingerprint.values[0], fingerprint.values[1],
        fingerprint.values[2], fingerprint.values[3],
        fingerprint.values[4], fingerprint.values[5],
        fingerprint.values[6]);
    return exact_ok && clamp_ok;
}

static int case_fingerprint_signed_rounding(
    WorldgenControlsProbeReport *report) {
    UiWorldgenEffectiveConfig config;
    UiWorldgenFingerprint fingerprint;
    int ok = 1;
    int difference;
    for (difference = -100; difference <= 100; difference++) {
        int expected = 50 + expected_signed_round(difference, 4);
        ui_worldgen_config_make_balanced(&config);
        config.bias_desert_slider = difference > 0 ? difference : 0;
        config.bias_forest_slider = difference < 0 ? -difference : 0;
        config.moisture_slider = difference > 0 ? difference : 0;
        config.drought_slider = difference < 0 ? -difference : 0;
        ui_worldgen_config_fingerprint(&config, &fingerprint);
        ok &= fingerprint.values[UI_WORLDGEN_FINGERPRINT_TEMPERATURE] ==
                  expected &&
              fingerprint.values[UI_WORLDGEN_FINGERPRINT_HUMIDITY] ==
                  expected;
    }
    worldgen_controls_probe_record(
        report, "worldgen_controls_fingerprint_signed_rounding", ok,
        "differences=201 divisor=4 symmetric_half_away_from_zero=%d", ok);
    return ok;
}

static int expected_region_percent(const int *anchors, int raw_value) {
    int interval;
    if (raw_value <= anchors[0]) return 0;
    if (raw_value >= anchors[UI_WORLDGEN_REGION_PRESET_COUNT - 1]) return 100;
    for (interval = 0; interval < UI_WORLDGEN_REGION_PRESET_COUNT - 1;
         interval++) {
        int low = anchors[interval];
        int high = anchors[interval + 1];
        if (raw_value <= high) {
            return interval * 25 +
                   ((raw_value - low) * 25 + (high - low) / 2) /
                       (high - low);
        }
    }
    return 100;
}

static int case_fingerprint_region_mapping(
    WorldgenControlsProbeReport *report) {
    static const int anchors[MAP_SIZE_COUNT]
                            [UI_WORLDGEN_REGION_PRESET_COUNT] = {
        {5, 20, 40, 60, 80},
        {10, 30, 50, 70, 90},
        {20, 40, 60, 80, 100},
        {30, 50, 70, 85, 100}
    };
    UiWorldgenEffectiveConfig config;
    UiWorldgenFingerprint fingerprint;
    int anchors_ok = 1;
    int interpolation_ok = 1;
    int map_size;
    int category;
    int raw_value;
    for (map_size = 0; map_size < MAP_SIZE_COUNT; map_size++) {
        for (category = 0; category < UI_WORLDGEN_REGION_PRESET_COUNT;
             category++) {
            ui_worldgen_config_make_balanced(&config);
            config.pending_map_size = map_size;
            config.region_size_slider = anchors[map_size][category];
            ui_worldgen_config_fingerprint(&config, &fingerprint);
            anchors_ok &= ui_worldgen_region_preset_value(
                              map_size,
                              (UiWorldgenRegionCategory)category) ==
                              anchors[map_size][category] &&
                          fingerprint.values[
                              UI_WORLDGEN_FINGERPRINT_REGIONS] ==
                              category * 25;
        }
        for (raw_value = -10; raw_value <= 110; raw_value++) {
            ui_worldgen_config_make_balanced(&config);
            config.pending_map_size = map_size;
            config.region_size_slider = raw_value;
            ui_worldgen_config_fingerprint(&config, &fingerprint);
            interpolation_ok &= fingerprint.values[
                                    UI_WORLDGEN_FINGERPRINT_REGIONS] ==
                                expected_region_percent(
                                    anchors[map_size], raw_value);
        }
    }
    worldgen_controls_probe_record(
        report, "worldgen_controls_fingerprint_region_mapping",
        anchors_ok && interpolation_ok,
        "map_sizes=%d anchors=%d raw_samples=%d anchors_ok=%d interpolation_clamps=%d",
        MAP_SIZE_COUNT,
        MAP_SIZE_COUNT * UI_WORLDGEN_REGION_PRESET_COUNT,
        MAP_SIZE_COUNT * 121, anchors_ok, interpolation_ok);
    return anchors_ok && interpolation_ok;
}

int worldgen_controls_probe_adapter(WorldgenControlsProbeReport *report) {
    int ok = 1;
    ok &= case_balanced(report);
    ok &= case_balanced_custom(report);
    ok &= case_dice_reset_scope(report);
    ok &= case_xy_and_relief(report);
    ok &= case_climate_defaults_limits(report);
    ok &= case_climate_forward_irregular(report);
    ok &= case_climate_inverse_all(report);
    ok &= case_bidirectional_round_trips(report);
    ok &= case_fingerprint_exact_axes(report);
    ok &= case_fingerprint_signed_rounding(report);
    ok &= case_fingerprint_region_mapping(report);
    return ok;
}
