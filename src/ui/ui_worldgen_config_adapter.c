#include "ui/ui_worldgen_config_adapter.h"

#include "core/constants.h"
#include "core/game_types.h"

#include <stddef.h>

static const int REGION_PRESET_VALUES[MAP_SIZE_COUNT][UI_WORLDGEN_REGION_PRESET_COUNT] = {
    {5, 20, 40, 60, 80},
    {10, 30, 50, 70, 90},
    {20, 40, 60, 80, 100},
    {30, 50, 70, 85, 100}
};

static int clamp_local(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static int *field_address(UiWorldgenConfigField field) {
    switch (field) {
        case UI_WORLDGEN_FIELD_PENDING_MAP_SIZE: return &pending_map_size;
        case UI_WORLDGEN_FIELD_OCEAN: return &ocean_slider;
        case UI_WORLDGEN_FIELD_CONTINENT: return &continent_slider;
        case UI_WORLDGEN_FIELD_RELIEF: return &relief_slider;
        case UI_WORLDGEN_FIELD_MOISTURE: return &moisture_slider;
        case UI_WORLDGEN_FIELD_DROUGHT: return &drought_slider;
        case UI_WORLDGEN_FIELD_VEGETATION: return &vegetation_slider;
        case UI_WORLDGEN_FIELD_BIAS_FOREST: return &bias_forest_slider;
        case UI_WORLDGEN_FIELD_BIAS_DESERT: return &bias_desert_slider;
        case UI_WORLDGEN_FIELD_BIAS_MOUNTAIN: return &bias_mountain_slider;
        case UI_WORLDGEN_FIELD_BIAS_WETLAND: return &bias_wetland_slider;
        case UI_WORLDGEN_FIELD_REGION_SIZE: return &region_size_slider;
        case UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT: return &initial_civ_count;
        default: return NULL;
    }
}

static int clamp_field_value(UiWorldgenConfigField field, int value) {
    if (field == UI_WORLDGEN_FIELD_PENDING_MAP_SIZE) {
        return clamp_local(value, MAP_SIZE_SMALL, MAP_SIZE_COUNT - 1);
    }
    if (field == UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT) {
        return clamp_local(value, 0, MAX_CIVS);
    }
    return clamp_local(value, 0, 100);
}

void ui_worldgen_config_read(UiWorldgenEffectiveConfig *out_config) {
    if (!out_config) return;
    out_config->pending_map_size = pending_map_size;
    out_config->ocean_slider = ocean_slider;
    out_config->continent_slider = continent_slider;
    out_config->relief_slider = relief_slider;
    out_config->moisture_slider = moisture_slider;
    out_config->drought_slider = drought_slider;
    out_config->vegetation_slider = vegetation_slider;
    out_config->bias_forest_slider = bias_forest_slider;
    out_config->bias_desert_slider = bias_desert_slider;
    out_config->bias_mountain_slider = bias_mountain_slider;
    out_config->bias_wetland_slider = bias_wetland_slider;
    out_config->region_size_slider = region_size_slider;
    out_config->initial_civ_count = initial_civ_count;
}

void ui_worldgen_config_write(const UiWorldgenEffectiveConfig *config) {
    if (!config) return;
    ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_PENDING_MAP_SIZE,
                                 config->pending_map_size);
    ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_OCEAN,
                                 config->ocean_slider);
    ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_CONTINENT,
                                 config->continent_slider);
    ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_RELIEF,
                                 config->relief_slider);
    ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_MOISTURE,
                                 config->moisture_slider);
    ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_DROUGHT,
                                 config->drought_slider);
    ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_VEGETATION,
                                 config->vegetation_slider);
    ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_BIAS_FOREST,
                                 config->bias_forest_slider);
    ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_BIAS_DESERT,
                                 config->bias_desert_slider);
    ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_BIAS_MOUNTAIN,
                                 config->bias_mountain_slider);
    ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_BIAS_WETLAND,
                                 config->bias_wetland_slider);
    ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_REGION_SIZE,
                                 config->region_size_slider);
    ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT,
                                 config->initial_civ_count);
}

int ui_worldgen_config_get_field(UiWorldgenConfigField field) {
    int *address = field_address(field);
    return address ? *address : 0;
}

int ui_worldgen_config_set_field(UiWorldgenConfigField field, int value) {
    int *address = field_address(field);
    if (!address) return 0;
    value = clamp_field_value(field, value);
    if (*address == value) return 0;
    *address = value;
    return 1;
}

int ui_worldgen_config_equal(const UiWorldgenEffectiveConfig *left,
                             const UiWorldgenEffectiveConfig *right) {
    if (!left || !right) return 0;
    return left->pending_map_size == right->pending_map_size &&
           left->ocean_slider == right->ocean_slider &&
           left->continent_slider == right->continent_slider &&
           left->relief_slider == right->relief_slider &&
           left->moisture_slider == right->moisture_slider &&
           left->drought_slider == right->drought_slider &&
           left->vegetation_slider == right->vegetation_slider &&
           left->bias_forest_slider == right->bias_forest_slider &&
           left->bias_desert_slider == right->bias_desert_slider &&
           left->bias_mountain_slider == right->bias_mountain_slider &&
           left->bias_wetland_slider == right->bias_wetland_slider &&
           left->region_size_slider == right->region_size_slider &&
           left->initial_civ_count == right->initial_civ_count;
}

static uint64_t signature_mix(uint64_t hash, int value) {
    uint32_t bits = (uint32_t)value;
    int byte_index;
    for (byte_index = 0; byte_index < 4; byte_index++) {
        hash ^= (uint8_t)(bits >> (byte_index * 8));
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

uint64_t ui_worldgen_config_signature(const UiWorldgenEffectiveConfig *config) {
    uint64_t hash = UINT64_C(1469598103934665603);
    if (!config) return 0;
    hash = signature_mix(hash, config->pending_map_size);
    hash = signature_mix(hash, config->ocean_slider);
    hash = signature_mix(hash, config->continent_slider);
    hash = signature_mix(hash, config->relief_slider);
    hash = signature_mix(hash, config->moisture_slider);
    hash = signature_mix(hash, config->drought_slider);
    hash = signature_mix(hash, config->vegetation_slider);
    hash = signature_mix(hash, config->bias_forest_slider);
    hash = signature_mix(hash, config->bias_desert_slider);
    hash = signature_mix(hash, config->bias_mountain_slider);
    hash = signature_mix(hash, config->bias_wetland_slider);
    hash = signature_mix(hash, config->region_size_slider);
    hash = signature_mix(hash, config->initial_civ_count);
    return hash ? hash : UINT64_C(1);
}

void ui_worldgen_config_make_balanced(UiWorldgenEffectiveConfig *config) {
    if (!config) return;
    config->pending_map_size = MAP_SIZE_EXTREME;
    config->ocean_slider = 50;
    config->continent_slider = 50;
    config->relief_slider = 50;
    config->moisture_slider = 50;
    config->drought_slider = 50;
    config->vegetation_slider = 50;
    config->bias_forest_slider = 50;
    config->bias_desert_slider = 50;
    config->bias_mountain_slider = 50;
    config->bias_wetland_slider = 50;
    config->region_size_slider = ui_worldgen_region_preset_value(
        MAP_SIZE_EXTREME, UI_WORLDGEN_REGION_MEDIUM);
}

void ui_worldgen_config_apply_balanced(void) {
    UiWorldgenEffectiveConfig config;
    ui_worldgen_config_read(&config);
    ui_worldgen_config_make_balanced(&config);
    ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_PENDING_MAP_SIZE,
                                 config.pending_map_size);
    ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_OCEAN,
                                 config.ocean_slider);
    ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_CONTINENT,
                                 config.continent_slider);
    ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_RELIEF,
                                 config.relief_slider);
    ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_MOISTURE,
                                 config.moisture_slider);
    ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_DROUGHT,
                                 config.drought_slider);
    ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_VEGETATION,
                                 config.vegetation_slider);
    ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_BIAS_FOREST,
                                 config.bias_forest_slider);
    ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_BIAS_DESERT,
                                 config.bias_desert_slider);
    ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_BIAS_MOUNTAIN,
                                 config.bias_mountain_slider);
    ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_BIAS_WETLAND,
                                 config.bias_wetland_slider);
    ui_worldgen_config_set_field(UI_WORLDGEN_FIELD_REGION_SIZE,
                                 config.region_size_slider);
}

int ui_worldgen_config_is_balanced(const UiWorldgenEffectiveConfig *config) {
    if (!config) return 0;
    return config->pending_map_size == MAP_SIZE_EXTREME &&
           config->ocean_slider == 50 &&
           config->continent_slider == 50 &&
           config->relief_slider == 50 &&
           config->moisture_slider == 50 &&
           config->drought_slider == 50 &&
           config->vegetation_slider == 50 &&
           config->bias_forest_slider == 50 &&
           config->bias_desert_slider == 50 &&
           config->bias_mountain_slider == 50 &&
           config->bias_wetland_slider == 50 &&
           config->region_size_slider == ui_worldgen_region_preset_value(
               MAP_SIZE_EXTREME, UI_WORLDGEN_REGION_MEDIUM);
}

int ui_worldgen_region_preset_value(int map_size,
                                    UiWorldgenRegionCategory category) {
    if (map_size < MAP_SIZE_SMALL || map_size >= MAP_SIZE_COUNT ||
        category < UI_WORLDGEN_REGION_VERY_SMALL ||
        category > UI_WORLDGEN_REGION_VERY_LARGE) return -1;
    return REGION_PRESET_VALUES[map_size][category];
}

UiWorldgenRegionCategory ui_worldgen_region_category_for_value(int map_size,
                                                               int raw_value) {
    int category;
    if (map_size < MAP_SIZE_SMALL || map_size >= MAP_SIZE_COUNT) {
        return UI_WORLDGEN_REGION_CUSTOM;
    }
    for (category = 0; category < UI_WORLDGEN_REGION_PRESET_COUNT; category++) {
        if (REGION_PRESET_VALUES[map_size][category] == raw_value) {
            return (UiWorldgenRegionCategory)category;
        }
    }
    return UI_WORLDGEN_REGION_CUSTOM;
}

int ui_worldgen_region_normalized_percent(int map_size, int raw_value) {
    const int *anchors;
    int interval;
    int low;
    int high;
    int span;
    map_size = clamp_local(map_size, MAP_SIZE_SMALL, MAP_SIZE_COUNT - 1);
    raw_value = clamp_local(raw_value, 0, 100);
    anchors = REGION_PRESET_VALUES[map_size];
    if (raw_value <= anchors[0]) return 0;
    if (raw_value >= anchors[UI_WORLDGEN_REGION_PRESET_COUNT - 1]) return 100;
    for (interval = 0; interval < UI_WORLDGEN_REGION_PRESET_COUNT - 1;
         interval++) {
        low = anchors[interval];
        high = anchors[interval + 1];
        if (raw_value <= high) {
            span = high - low;
            return interval * 25 +
                   ((raw_value - low) * 25 + span / 2) / span;
        }
    }
    return 100;
}

void ui_worldgen_climate_clamp_corner(UiWorldgenClimateCornerId corner,
                                      UiWorldgenPoint *point) {
    if (!point || corner < UI_WORLDGEN_CLIMATE_TOP_LEFT ||
        corner >= UI_WORLDGEN_CLIMATE_CORNER_COUNT) return;
    if (corner == UI_WORLDGEN_CLIMATE_TOP_LEFT ||
        corner == UI_WORLDGEN_CLIMATE_BOTTOM_LEFT) {
        point->x = clamp_local(point->x, -50, 0);
    } else {
        point->x = clamp_local(point->x, 0, 50);
    }
    if (corner == UI_WORLDGEN_CLIMATE_TOP_LEFT ||
        corner == UI_WORLDGEN_CLIMATE_TOP_RIGHT) {
        point->y = clamp_local(point->y, 0, 50);
    } else {
        point->y = clamp_local(point->y, -50, 0);
    }
}

void ui_worldgen_climate_corners_from_config(
    const UiWorldgenEffectiveConfig *config,
    UiWorldgenClimateCorners *out_corners) {
    int forest;
    int desert;
    int moisture;
    int drought;
    if (!config || !out_corners) return;
    forest = clamp_local(config->bias_forest_slider, 0, 100);
    desert = clamp_local(config->bias_desert_slider, 0, 100);
    moisture = clamp_local(config->moisture_slider, 0, 100);
    drought = clamp_local(config->drought_slider, 0, 100);
    out_corners->top_left.x = -(forest + 1) / 2;
    out_corners->bottom_left.x = -(forest / 2);
    out_corners->top_right.x = (desert + 1) / 2;
    out_corners->bottom_right.x = desert / 2;
    out_corners->top_left.y = (moisture + 1) / 2;
    out_corners->top_right.y = moisture / 2;
    out_corners->bottom_left.y = -(drought + 1) / 2;
    out_corners->bottom_right.y = -(drought / 2);
}

void ui_worldgen_climate_corners_to_config(
    const UiWorldgenClimateCorners *corners,
    UiWorldgenEffectiveConfig *in_out_config) {
    UiWorldgenClimateCorners clamped;
    if (!corners || !in_out_config) return;
    clamped = *corners;
    ui_worldgen_climate_clamp_corner(UI_WORLDGEN_CLIMATE_TOP_LEFT,
                                     &clamped.top_left);
    ui_worldgen_climate_clamp_corner(UI_WORLDGEN_CLIMATE_TOP_RIGHT,
                                     &clamped.top_right);
    ui_worldgen_climate_clamp_corner(UI_WORLDGEN_CLIMATE_BOTTOM_RIGHT,
                                     &clamped.bottom_right);
    ui_worldgen_climate_clamp_corner(UI_WORLDGEN_CLIMATE_BOTTOM_LEFT,
                                     &clamped.bottom_left);
    in_out_config->bias_forest_slider =
        -clamped.top_left.x - clamped.bottom_left.x;
    in_out_config->bias_desert_slider =
        clamped.top_right.x + clamped.bottom_right.x;
    in_out_config->moisture_slider =
        clamped.top_left.y + clamped.top_right.y;
    in_out_config->drought_slider =
        -clamped.bottom_left.y - clamped.bottom_right.y;
}

static int rounded_signed_divide(int numerator, int denominator) {
    if (numerator >= 0) return (numerator + denominator / 2) / denominator;
    return -((-numerator + denominator / 2) / denominator);
}

void ui_worldgen_config_fingerprint(const UiWorldgenEffectiveConfig *config,
                                    UiWorldgenFingerprint *out_fingerprint) {
    int relief_sum;
    if (!config || !out_fingerprint) return;
    relief_sum = config->relief_slider + config->bias_mountain_slider;
    out_fingerprint->values[UI_WORLDGEN_FINGERPRINT_OCEAN] =
        clamp_local(config->ocean_slider, 0, 100);
    out_fingerprint->values[UI_WORLDGEN_FINGERPRINT_LANDMASS] =
        clamp_local(100 - config->continent_slider, 0, 100);
    out_fingerprint->values[UI_WORLDGEN_FINGERPRINT_RELIEF] =
        clamp_local((relief_sum + 1) / 2, 0, 100);
    out_fingerprint->values[UI_WORLDGEN_FINGERPRINT_TEMPERATURE] = clamp_local(
        50 + rounded_signed_divide(config->bias_desert_slider -
                                   config->bias_forest_slider, 4),
        0, 100);
    out_fingerprint->values[UI_WORLDGEN_FINGERPRINT_HUMIDITY] = clamp_local(
        50 + rounded_signed_divide(config->moisture_slider -
                                   config->drought_slider, 4),
        0, 100);
    out_fingerprint->values[UI_WORLDGEN_FINGERPRINT_RIVERS] =
        clamp_local(config->bias_wetland_slider, 0, 100);
    out_fingerprint->values[UI_WORLDGEN_FINGERPRINT_REGIONS] =
        ui_worldgen_region_normalized_percent(config->pending_map_size,
                                              config->region_size_slider);
}
