#ifndef WORLD_SIM_UI_WORLDGEN_PANEL_LAYOUT_H
#define WORLD_SIM_UI_WORLDGEN_PANEL_LAYOUT_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "ui/ui_worldgen_control_state.h"

#define UI_WORLDGEN_PANEL_MAP_SIZE_COUNT 4
#define UI_WORLDGEN_PANEL_RELIEF_HANDLE_COUNT 2
#define UI_WORLDGEN_PANEL_RIVER_SLOT_COUNT 5
#define UI_WORLDGEN_PANEL_BIOME_LABEL_COUNT 7

typedef struct {
    RECT card;
    RECT title;
    RECT plot;
    RECT legend_default;
    RECT legend_current;
    RECT axis_label[UI_WORLDGEN_FINGERPRINT_AXIS_COUNT];
    POINT center;
    POINT axis_end[UI_WORLDGEN_FINGERPRINT_AXIS_COUNT];
    POINT default_point[UI_WORLDGEN_FINGERPRINT_AXIS_COUNT];
    POINT current_point[UI_WORLDGEN_FINGERPRINT_AXIS_COUNT];
    int radius;
} UiWorldgenFingerprintLayout;

typedef struct {
    RECT map_size_section;
    RECT map_size_label;
    RECT map_size_button[UI_WORLDGEN_PANEL_MAP_SIZE_COUNT];
    RECT xy_section;
    RECT xy_title;
    RECT xy_asset;
    RECT xy_plot;
    RECT xy_x_axis;
    RECT xy_y_axis;
    RECT xy_x_low_label;
    RECT xy_x_high_label;
    RECT xy_y_low_label;
    RECT xy_y_high_label;
    RECT xy_handle;
    RECT xy_handle_hit;
    RECT relief_section;
    RECT relief_title;
    RECT relief_asset;
    RECT relief_baseline;
    RECT relief_label[UI_WORLDGEN_PANEL_RELIEF_HANDLE_COUNT];
    RECT relief_handle[UI_WORLDGEN_PANEL_RELIEF_HANDLE_COUNT];
    RECT relief_handle_hit[UI_WORLDGEN_PANEL_RELIEF_HANDLE_COUNT];
} UiWorldgenPhysicalLayout;

typedef struct {
    RECT section;
    RECT title;
    RECT asset;
    RECT plot;
    RECT x_axis;
    RECT y_axis;
    RECT x_low_label;
    RECT x_high_label;
    RECT y_low_label;
    RECT y_high_label;
    RECT biome_label[UI_WORLDGEN_PANEL_BIOME_LABEL_COUNT];
    RECT corner_handle[UI_WORLDGEN_CLIMATE_CORNER_COUNT];
    RECT corner_handle_hit[UI_WORLDGEN_CLIMATE_CORNER_COUNT];
    RECT vegetation_section;
    RECT vegetation_label;
    RECT vegetation_value;
    RECT vegetation_track;
    RECT vegetation_handle;
    RECT vegetation_handle_hit;
} UiWorldgenClimateLayout;

typedef struct {
    RECT river_section;
    RECT river_title;
    RECT river_asset;
    RECT river_slot[UI_WORLDGEN_PANEL_RIVER_SLOT_COUNT];
    RECT river_slot_label[UI_WORLDGEN_PANEL_RIVER_SLOT_COUNT];
    RECT river_track;
    RECT river_handle;
    RECT river_handle_hit;
    RECT region_section;
    RECT region_title;
    RECT region_asset;
    RECT region_slot[UI_WORLDGEN_REGION_PRESET_COUNT];
    RECT region_slot_label[UI_WORLDGEN_REGION_PRESET_COUNT];
    RECT region_custom_label;
    RECT region_custom_input;
    RECT region_target_area;
    RECT region_estimated_count;
    RECT initial_civs_section;
    RECT initial_civs_label;
    RECT initial_civs_input_frame;
    RECT initial_civs_input;
} UiWorldgenHydrologyLayout;

typedef struct {
    RECT viewport;
    RECT content_bounds;
    POINT content_origin;
    int content_width;
} UiWorldgenLegacyLayout;

typedef struct {
    RECT client;
    int panel_width;
    UiWorldgenControlTab tab;
    int scroll_offsets[UI_WORLDGEN_TAB_COUNT];
    int legacy_content_height;
    const UiWorldgenEffectiveConfig *config;
    const UiWorldgenClimateCorners *climate_corners;
    const UiWorldgenFingerprint *fingerprint;
} UiWorldgenPanelLayoutInput;

typedef struct {
    RECT panel;
    RECT inner;
    RECT header;
    RECT title;
    RECT subtitle;
    RECT preset_row;
    RECT preset_label;
    RECT preset_selector;
    RECT dice_button;
    RECT reset_button;
    UiWorldgenFingerprintLayout fingerprint;
    RECT tabs_row;
    RECT tab_button[UI_WORLDGEN_TAB_COUNT];
    RECT tab_label[UI_WORLDGEN_TAB_COUNT];
    RECT content_viewport;
    RECT content_bounds;
    RECT footer;
    RECT generate_button;
    RECT status;
    UiWorldgenControlTab tab;
    int scroll_offset;
    int content_height;
    int max_scroll;
    int tab_scroll_offset[UI_WORLDGEN_TAB_COUNT];
    int tab_content_height[UI_WORLDGEN_TAB_COUNT];
    int tab_max_scroll[UI_WORLDGEN_TAB_COUNT];
    UiWorldgenPhysicalLayout physical;
    UiWorldgenClimateLayout climate;
    UiWorldgenHydrologyLayout hydrology;
    UiWorldgenLegacyLayout legacy;
} UiWorldgenPanelLayout;

void ui_worldgen_panel_layout_build(const UiWorldgenPanelLayoutInput *input,
                                    UiWorldgenPanelLayout *layout);

RECT ui_worldgen_panel_aspect_fit(RECT bounds, int source_width,
                                  int source_height);
RECT ui_worldgen_panel_clip_rect(RECT viewport, RECT rect);
int ui_worldgen_panel_point_in_viewport(RECT viewport, int x, int y);
int ui_worldgen_panel_hit_test(RECT viewport, RECT rect, int x, int y);

int ui_worldgen_panel_value_to_axis(int value, int value_min, int value_max,
                                    int axis_start, int axis_end);
int ui_worldgen_panel_axis_to_value(int position, int axis_start, int axis_end,
                                    int value_min, int value_max);
POINT ui_worldgen_panel_values_to_point(RECT plot, int x_value, int x_min,
                                        int x_max, int y_value, int y_min,
                                        int y_max);
void ui_worldgen_panel_point_to_values(RECT plot, POINT point, int x_min,
                                       int x_max, int y_min, int y_max,
                                       int *out_x, int *out_y);

#endif
