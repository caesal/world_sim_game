#include "ui_types.h"
#include "world/world_gen.h"

int display_mode = DISPLAY_ALL;
int panel_tab = PANEL_WORLD;
int ui_language = UI_LANG_EN;
int side_panel_w = DEFAULT_SIDE_PANEL_W;
int dragging_panel = 0;
int dragging_slider = -1;
int dragging_map = 0;
int last_mouse_x = 0;
int last_mouse_y = 0;
int hover_x = -1;
int hover_y = -1;
int ocean_slider = WORLD_GEN_DEFAULT_OCEAN;
int continent_slider = WORLD_GEN_DEFAULT_CONTINENT;
int relief_slider = WORLD_GEN_DEFAULT_RELIEF;
int moisture_slider = WORLD_GEN_DEFAULT_MOISTURE;
int drought_slider = WORLD_GEN_DEFAULT_DROUGHT;
int vegetation_slider = WORLD_GEN_DEFAULT_VEGETATION;
int bias_forest_slider = WORLD_GEN_DEFAULT_BIAS_FOREST;
int bias_desert_slider = WORLD_GEN_DEFAULT_BIAS_DESERT;
int bias_mountain_slider = WORLD_GEN_DEFAULT_BIAS_MOUNTAIN;
int bias_wetland_slider = WORLD_GEN_DEFAULT_BIAS_WETLAND;
int initial_civ_count = 0;
int map_zoom_percent = 120;
int map_offset_x = 0;
int map_offset_y = 0;
int map_interaction_preview = 0;
int map_legend_collapsed = 0;
int plague_fog_alpha = 45;
int region_size_slider = 50;
int worldgen_scroll_offset = 0;

const int MAP_DISPLAY_MODES[MAP_DISPLAY_MODE_COUNT] = {DISPLAY_ALL, DISPLAY_CLIMATE, DISPLAY_GEOGRAPHY, DISPLAY_REGIONS, DISPLAY_POLITICAL};
const char *MAP_DISPLAY_NAMES[MAP_DISPLAY_MODE_COUNT] = {"All", "Climate", "Geography", "Regions", "Political"};
