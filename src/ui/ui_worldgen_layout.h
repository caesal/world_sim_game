#ifndef WORLD_SIM_UI_WORLDGEN_LAYOUT_H
#define WORLD_SIM_UI_WORLDGEN_LAYOUT_H

#include <windows.h>

#include "ui/ui_types.h"

#define WORLDGEN_CORE_METRIC_COUNT 7

typedef enum {
    WORLDGEN_METRIC_MILITARY,
    WORLDGEN_METRIC_LOGISTICS,
    WORLDGEN_METRIC_GOVERNANCE,
    WORLDGEN_METRIC_COHESION,
    WORLDGEN_METRIC_PRODUCTION,
    WORLDGEN_METRIC_COMMERCE,
    WORLDGEN_METRIC_INNOVATION
} WorldgenMetricIndex;

typedef struct {
    RECT label;
    RECT value;
    RECT track;
    RECT hit;
} WorldgenSliderLayout;

typedef struct {
    RECT viewport;
    int scroll_offset;
    int max_scroll;
    int content_height;
    RECT title;
    RECT map_size_section;
    RECT map_size_buttons[MAP_SIZE_COUNT];
    RECT initial_label;
    RECT initial_input;
    RECT physical_section;
    RECT terrain_section;
    RECT regions_section;
    RECT generate_row;
    WorldgenSliderLayout sliders[UI_SLIDER_COUNT];
    RECT civ_section;
    RECT civ_hint;
    RECT name_label;
    RECT name_input;
    RECT symbol_label;
    RECT symbol_input;
    RECT metric_label[WORLDGEN_CORE_METRIC_COUNT];
    RECT metric_input[WORLDGEN_CORE_METRIC_COUNT];
    RECT add_button;
    RECT apply_button;
} WorldgenLayout;

void worldgen_layout_build(RECT client, int panel_width, int scroll_offset, WorldgenLayout *layout);
int worldgen_layout_clamp_scroll(RECT client, int panel_width, int scroll_offset);
int worldgen_rect_visible(RECT viewport, RECT rect);

#endif
