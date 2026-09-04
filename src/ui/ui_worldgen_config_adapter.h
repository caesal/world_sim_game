#ifndef WORLD_SIM_UI_WORLDGEN_CONFIG_ADAPTER_H
#define WORLD_SIM_UI_WORLDGEN_CONFIG_ADAPTER_H

#include <stdint.h>

typedef enum {
    UI_WORLDGEN_FIELD_PENDING_MAP_SIZE = 0,
    UI_WORLDGEN_FIELD_OCEAN,
    UI_WORLDGEN_FIELD_CONTINENT,
    UI_WORLDGEN_FIELD_RELIEF,
    UI_WORLDGEN_FIELD_MOISTURE,
    UI_WORLDGEN_FIELD_DROUGHT,
    UI_WORLDGEN_FIELD_VEGETATION,
    UI_WORLDGEN_FIELD_BIAS_FOREST,
    UI_WORLDGEN_FIELD_BIAS_DESERT,
    UI_WORLDGEN_FIELD_BIAS_MOUNTAIN,
    UI_WORLDGEN_FIELD_BIAS_WETLAND,
    UI_WORLDGEN_FIELD_REGION_SIZE,
    UI_WORLDGEN_FIELD_INITIAL_CIV_COUNT,
    UI_WORLDGEN_FIELD_COUNT
} UiWorldgenConfigField;

typedef uint32_t UiWorldgenFieldMask;

#define UI_WORLDGEN_FIELD_MASK(field) \
    (UINT32_C(1) << (unsigned int)(field))

typedef struct {
    int pending_map_size;
    int ocean_slider;
    int continent_slider;
    int relief_slider;
    int moisture_slider;
    int drought_slider;
    int vegetation_slider;
    int bias_forest_slider;
    int bias_desert_slider;
    int bias_mountain_slider;
    int bias_wetland_slider;
    int region_size_slider;
    int initial_civ_count;
} UiWorldgenEffectiveConfig;

typedef enum {
    UI_WORLDGEN_REGION_VERY_SMALL = 0,
    UI_WORLDGEN_REGION_SMALL,
    UI_WORLDGEN_REGION_MEDIUM,
    UI_WORLDGEN_REGION_LARGE,
    UI_WORLDGEN_REGION_VERY_LARGE,
    UI_WORLDGEN_REGION_CUSTOM,
    UI_WORLDGEN_REGION_CATEGORY_COUNT
} UiWorldgenRegionCategory;

#define UI_WORLDGEN_REGION_PRESET_COUNT 5

typedef enum {
    UI_WORLDGEN_CLIMATE_TOP_LEFT = 0,
    UI_WORLDGEN_CLIMATE_TOP_RIGHT,
    UI_WORLDGEN_CLIMATE_BOTTOM_RIGHT,
    UI_WORLDGEN_CLIMATE_BOTTOM_LEFT,
    UI_WORLDGEN_CLIMATE_CORNER_COUNT
} UiWorldgenClimateCornerId;

typedef struct {
    int x;
    int y;
} UiWorldgenPoint;

typedef struct {
    UiWorldgenPoint top_left;
    UiWorldgenPoint top_right;
    UiWorldgenPoint bottom_right;
    UiWorldgenPoint bottom_left;
} UiWorldgenClimateCorners;

typedef enum {
    UI_WORLDGEN_FINGERPRINT_OCEAN = 0,
    UI_WORLDGEN_FINGERPRINT_LANDMASS,
    UI_WORLDGEN_FINGERPRINT_RELIEF,
    UI_WORLDGEN_FINGERPRINT_TEMPERATURE,
    UI_WORLDGEN_FINGERPRINT_HUMIDITY,
    UI_WORLDGEN_FINGERPRINT_RIVERS,
    UI_WORLDGEN_FINGERPRINT_REGIONS,
    UI_WORLDGEN_FINGERPRINT_AXIS_COUNT
} UiWorldgenFingerprintAxis;

typedef struct {
    int values[UI_WORLDGEN_FINGERPRINT_AXIS_COUNT];
} UiWorldgenFingerprint;

void ui_worldgen_config_read(UiWorldgenEffectiveConfig *out_config);
void ui_worldgen_config_write(const UiWorldgenEffectiveConfig *config);
int ui_worldgen_config_get_field(UiWorldgenConfigField field);
int ui_worldgen_config_set_field(UiWorldgenConfigField field, int value);
int ui_worldgen_initial_civ_cap_for_map_size(int map_size);
int ui_worldgen_config_equal(const UiWorldgenEffectiveConfig *left,
                             const UiWorldgenEffectiveConfig *right);
uint64_t ui_worldgen_config_signature(const UiWorldgenEffectiveConfig *config);

void ui_worldgen_config_make_balanced(UiWorldgenEffectiveConfig *config);
void ui_worldgen_config_apply_balanced(void);
int ui_worldgen_config_is_balanced(const UiWorldgenEffectiveConfig *config);

int ui_worldgen_region_preset_value(int map_size,
                                    UiWorldgenRegionCategory category);
UiWorldgenRegionCategory ui_worldgen_region_category_for_value(int map_size,
                                                               int raw_value);
int ui_worldgen_region_normalized_percent(int map_size, int raw_value);

void ui_worldgen_climate_clamp_corner(UiWorldgenClimateCornerId corner,
                                      UiWorldgenPoint *point);
void ui_worldgen_climate_corners_from_config(
    const UiWorldgenEffectiveConfig *config,
    UiWorldgenClimateCorners *out_corners);
void ui_worldgen_climate_corners_to_config(
    const UiWorldgenClimateCorners *corners,
    UiWorldgenEffectiveConfig *in_out_config);

void ui_worldgen_config_fingerprint(const UiWorldgenEffectiveConfig *config,
                                    UiWorldgenFingerprint *out_fingerprint);

#endif
