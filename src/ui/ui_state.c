#include "ui_types.h"

int display_mode = DISPLAY_ALL;
int panel_tab = PANEL_INFO;
int ui_language = UI_LANG_EN;
int side_panel_w = DEFAULT_SIDE_PANEL_W;
int dragging_panel = 0;
int dragging_slider = -1;
int dragging_map = 0;
int last_mouse_x = 0;
int last_mouse_y = 0;
int hover_x = -1;
int hover_y = -1;
int ocean_slider = 45;
int continent_slider = 34;
int relief_slider = 28;
int moisture_slider = 17;
int drought_slider = 10;
int vegetation_slider = 93;
int bias_forest_slider = 93;
int bias_desert_slider = 10;
int bias_mountain_slider = 7;
int bias_wetland_slider = 17;
int initial_civ_count = 4;
int map_zoom_percent = 120;
int map_offset_x = 0;
int map_offset_y = 0;
int map_legend_collapsed = 0;

const int MAP_DISPLAY_MODES[4] = {DISPLAY_ALL, DISPLAY_CLIMATE, DISPLAY_GEOGRAPHY, DISPLAY_POLITICAL};
const char *MAP_DISPLAY_NAMES[4] = {"All", "Climate", "Geography", "Political"};
