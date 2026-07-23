#ifndef WORLD_SIM_UI_WORLDGEN_CONTROL_STATE_H
#define WORLD_SIM_UI_WORLDGEN_CONTROL_STATE_H

#include <stdint.h>

#include "ui/ui_worldgen_config_adapter.h"

typedef enum {
    UI_WORLDGEN_TAB_PHYSICAL = 0,
    UI_WORLDGEN_TAB_CLIMATE,
    UI_WORLDGEN_TAB_HYDROLOGY_REGIONS,
    UI_WORLDGEN_TAB_LEGACY,
    UI_WORLDGEN_TAB_COUNT
} UiWorldgenControlTab;

typedef enum {
    UI_WORLDGEN_PRESET_BALANCED = 0,
    UI_WORLDGEN_PRESET_CUSTOM
} UiWorldgenPreset;

typedef enum {
    UI_WORLDGEN_RESYNC_KEEP_APPLIED = 0,
    UI_WORLDGEN_RESYNC_MARK_APPLIED
} UiWorldgenResyncMode;

typedef enum {
    UI_WORLDGEN_CONTROL_NONE = 0,
    UI_WORLDGEN_CONTROL_PRESET,
    UI_WORLDGEN_CONTROL_DICE,
    UI_WORLDGEN_CONTROL_RESET,
    UI_WORLDGEN_CONTROL_TAB,
    UI_WORLDGEN_CONTROL_MAP_SIZE,
    UI_WORLDGEN_CONTROL_PHYSICAL_XY,
    UI_WORLDGEN_CONTROL_RELIEF_HANDLE,
    UI_WORLDGEN_CONTROL_CLIMATE_CORNER,
    UI_WORLDGEN_CONTROL_VEGETATION,
    UI_WORLDGEN_CONTROL_RIVER_DENSITY,
    UI_WORLDGEN_CONTROL_REGION_CATEGORY,
    UI_WORLDGEN_CONTROL_REGION_CUSTOM_INPUT,
    UI_WORLDGEN_CONTROL_LEGACY_SLIDER,
    UI_WORLDGEN_CONTROL_LEGACY_RANDOM,
    UI_WORLDGEN_CONTROL_FOOTER_GENERATE
} UiWorldgenControlKind;

typedef struct {
    UiWorldgenControlKind kind;
    int index;
} UiWorldgenControlTarget;

typedef struct {
    UiWorldgenControlTarget hovered;
    UiWorldgenControlTarget pressed;
    UiWorldgenControlTarget focused;
    UiWorldgenControlTarget dragging;
} UiWorldgenInteractionState;

typedef struct {
    int initialized;
    UiWorldgenControlTab tab;
    int scroll_offsets[UI_WORLDGEN_TAB_COUNT];
    UiWorldgenClimateCorners climate_corners;
    UiWorldgenRegionCategory region_category;
    UiWorldgenPreset preset;
    UiWorldgenEffectiveConfig applied_config;
    int has_applied_config;
    int dirty;
    int generation_failed;
    uint32_t revision;
    UiWorldgenInteractionState interaction;
} UiWorldgenControlState;

void ui_worldgen_control_state_init_fresh(void);
int ui_worldgen_control_state_resync(UiWorldgenResyncMode mode);
const UiWorldgenControlState *ui_worldgen_control_state_get(void);
void ui_worldgen_control_state_mark_applied(void);
void ui_worldgen_control_state_mark_generation_failed(void);

int ui_worldgen_control_state_set_tab(UiWorldgenControlTab tab);
int ui_worldgen_control_state_set_scroll(UiWorldgenControlTab tab,
                                         int scroll_offset);
int ui_worldgen_control_state_set_field(UiWorldgenConfigField field,
                                        int value);
int ui_worldgen_control_state_set_climate_corner(
    UiWorldgenClimateCornerId corner, int x, int y);
int ui_worldgen_control_state_set_region_category(
    UiWorldgenRegionCategory category);
int ui_worldgen_control_state_set_region_custom_value(int raw_value);
int ui_worldgen_control_state_apply_balanced(void);

int ui_worldgen_control_state_set_hovered(UiWorldgenControlTarget target);
int ui_worldgen_control_state_set_pressed(UiWorldgenControlTarget target);
int ui_worldgen_control_state_set_focused(UiWorldgenControlTarget target);
int ui_worldgen_control_state_begin_drag(UiWorldgenControlTarget target);
int ui_worldgen_control_state_end_drag(void);
int ui_worldgen_control_state_clear_interaction(void);

#endif
