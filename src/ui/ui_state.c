#include "ui_types.h"
#include "world/world_gen.h"

int display_mode = DISPLAY_ALL;
int panel_tab = PANEL_WORLD;
int ui_language = UI_LANG_EN;
int side_panel_w = DEFAULT_SIDE_PANEL_W;
int side_panel_collapsed = 0;
int side_panel_expanded_w = DEFAULT_SIDE_PANEL_W;
int map_view_auto_centered = 1;
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
int map_zoom_percent = 100;
int map_offset_x = 0;
int map_offset_y = 0;
int map_interaction_preview = 0;
int map_legend_collapsed = 0;
int plague_fog_alpha = 45;
int region_size_slider = 50;
int worldgen_scroll_offset = 0;
int country_show_fallen = 0;
int country_list_scroll_offset = 0;
int country_detail_scroll_offset = 0;
int country_sort_column = COUNTRY_SORT_POPULATION;
int country_sort_descending = 1;
int country_detail_subtab = COUNTRY_DETAIL_OVERVIEW;
int country_detail_scroll_offsets[8] = {0};
int country_diplomacy_view = DIPLOMACY_VIEW_PEACE_TENSE;
int previous_selected_civ = -1;
int map_highlight_civ = -1;
int selected_civ_pulse_start_ms = 0;
int map_highlight_pulse_start_ms = 0;
int debug_event_filter = DEBUG_EVENT_FILTER_ALL;
int debug_event_log_scroll_offset = 0;
int debug_event_log_frozen = 0;
int debug_event_log_seen_total = 0;
int pause_menu_open = 0;
int selected_civ_color_index = -1;
Color32 selected_civ_color = COLOR32_RGB(232, 31, 39);

const Color32 UI_CIV_COLOR_PALETTE[CIV_COLOR_PALETTE_COUNT] = {
    COLOR32_RGB(232, 31, 39),
    COLOR32_RGB(34, 88, 230),
    COLOR32_RGB(18, 205, 42),
    COLOR32_RGB(244, 188, 40),
    COLOR32_RGB(168, 76, 210),
    COLOR32_RGB(232, 112, 37),
    COLOR32_RGB(20, 184, 170),
    COLOR32_RGB(220, 70, 145),
    COLOR32_RGB(165, 102, 44),
    COLOR32_RGB(72, 165, 224),
    COLOR32_RGB(145, 191, 71),
    COLOR32_RGB(205, 118, 150),
    COLOR32_RGB(114, 93, 189),
    COLOR32_RGB(209, 156, 44),
    COLOR32_RGB(78, 139, 122),
    COLOR32_RGB(191, 89, 89)
};

const int MAP_DISPLAY_MODES[MAP_DISPLAY_MODE_COUNT] = {DISPLAY_ALL, DISPLAY_CLIMATE, DISPLAY_GEOGRAPHY, DISPLAY_REGIONS, DISPLAY_POLITICAL, DISPLAY_ROUTE_POTENTIAL};
const char *MAP_DISPLAY_NAMES[MAP_DISPLAY_MODE_COUNT] = {"All", "Climate", "Geography", "Regions", "Political", "Routes"};
