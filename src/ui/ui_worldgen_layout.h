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
    RECT help;
    RECT hit;
} WorldgenSliderLayout;

typedef struct {
    RECT viewport;
    int scroll_offset;
    int max_scroll;
    int content_height;
    RECT title;
    RECT stage_section;
    RECT stage_row[4];
    RECT viewer_section;
    RECT viewer_row[3];
    RECT generation_section;
    RECT map_size_section;
    RECT map_size_buttons[MAP_SIZE_COUNT];
    RECT map_size_help;
    RECT initial_label;
    RECT initial_input_frame;
    RECT initial_input;
    RECT initial_help;
    RECT physical_section;
    RECT physical_random_button;
    RECT terrain_section;
    RECT terrain_random_button;
    RECT regions_section;
    RECT region_estimate;
    RECT region_warning;
    RECT generate_row;
    WorldgenSliderLayout sliders[UI_SLIDER_COUNT];
    RECT civ_section;
    RECT civ_random_button;
    RECT civ_hint;
    RECT civ_range_help;
    RECT name_label;
    RECT name_input_frame;
    RECT name_input;
    RECT symbol_label;
    RECT symbol_input_frame;
    RECT symbol_input;
    RECT civ_color_label;
    RECT civ_color_preview;
    RECT civ_color_swatch[CIV_COLOR_PALETTE_COUNT];
    RECT metric_label[WORLDGEN_CORE_METRIC_COUNT];
    RECT metric_input_frame[WORLDGEN_CORE_METRIC_COUNT];
    RECT metric_input[WORLDGEN_CORE_METRIC_COUNT];
    RECT metric_help[WORLDGEN_CORE_METRIC_COUNT];
    RECT add_button;
    RECT apply_button;
} WorldgenLayout;

void worldgen_layout_build(RECT client, int panel_width, int scroll_offset, WorldgenLayout *layout);
int worldgen_layout_clamp_scroll(RECT client, int panel_width, int scroll_offset);
int worldgen_rect_visible(RECT viewport, RECT rect);

#endif
