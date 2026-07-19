#ifndef WORLD_SIM_PANEL_MAP_WATER_LEGEND_H
#define WORLD_SIM_PANEL_MAP_WATER_LEGEND_H

#include <stdint.h>
#include <windows.h>

typedef struct {
    int ready;
    int width;
    int height;
    int unique_colors;
    uint32_t pixel_hash;
    RECT shallow_swatch;
    RECT deep_swatch;
    RECT lake_swatch;
    POINT shallow_text;
    POINT deep_text;
    POINT lake_text;
    int lake_border_drawn;
    int fallback_drawn;
} PanelMapWaterLegendPatternStats;

int panel_map_water_legend_draw(HDC hdc, int x, int group_y, int line_h);
int panel_map_water_legend_prewarm(HDC hdc);
const PanelMapWaterLegendPatternStats *panel_map_water_legend_pattern_stats(void);
const char *panel_map_water_legend_label_en(void);
const char *panel_map_water_legend_label_zh(void);
void panel_map_water_legend_release(void);

#endif
