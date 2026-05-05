#ifndef WORLD_SIM_GAME_TYPES_H
#define WORLD_SIM_GAME_TYPES_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stddef.h>

#include "constants.h"
#include "world_types.h"
#include "sim_types.h"

typedef struct {
    Tile (*world)[MAX_MAP_W];
    Civilization *civs;
    City *cities;
    int *civ_count;
    int *city_count;
    int *year;
    int *month;
    int *selected_x;
    int *selected_y;
    int *selected_civ;
    int *auto_run;
    int *speed_index;
    int *display_mode;
    int *panel_tab;
    int *ui_language;
    int *side_panel_w;
    int *dragging_panel;
    int *dragging_slider;
    int *dragging_map;
    int *last_mouse_x;
    int *last_mouse_y;
    int *hover_x;
    int *hover_y;
    int *ocean_slider;
    int *continent_slider;
    int *relief_slider;
    int *moisture_slider;
    int *drought_slider;
    int *vegetation_slider;
    int *bias_forest_slider;
    int *bias_desert_slider;
    int *bias_mountain_slider;
    int *bias_wetland_slider;
    int *initial_civ_count;
    int *map_zoom_percent;
    int *map_offset_x;
    int *map_offset_y;
    int *map_legend_collapsed;
} GameState;

extern Tile world[MAX_MAP_H][MAX_MAP_W];
extern RiverPath river_paths[MAX_RIVER_PATHS];
extern MaritimeRoute maritime_routes[MAX_MARITIME_ROUTES];
extern Civilization civs[MAX_CIVS];
extern City cities[MAX_CITIES];
extern GameState g_game;
extern int river_path_count;
extern int maritime_route_count;
extern int civ_count;
extern int city_count;
extern int year;
extern int month;
extern int selected_x;
extern int selected_y;
extern int selected_civ;
extern int auto_run;
extern int speed_index;
extern int display_mode;
extern int panel_tab;
extern int ui_language;
extern int side_panel_w;
extern int dragging_panel;
extern int dragging_slider;
extern int dragging_map;
extern int last_mouse_x;
extern int last_mouse_y;
extern int hover_x;
extern int hover_y;
extern int ocean_slider;
extern int continent_slider;
extern int relief_slider;
extern int moisture_slider;
extern int drought_slider;
extern int vegetation_slider;
extern int bias_forest_slider;
extern int bias_desert_slider;
extern int bias_mountain_slider;
extern int bias_wetland_slider;
extern int initial_civ_count;
extern int map_zoom_percent;
extern int map_offset_x;
extern int map_offset_y;
extern int map_legend_collapsed;
extern int map_interaction_preview;
extern int world_visual_revision;
extern int map_w;
extern int map_h;
extern int map_size_index;
extern int pending_map_size;
extern int world_generated;

extern const int SPEED_MS[3];
extern const char *SPEED_NAMES[3];
extern const COLORREF CIV_COLORS[MAX_CIVS];
extern const int MAP_DISPLAY_MODES[4];
extern const char *MAP_DISPLAY_NAMES[4];

int clamp(int value, int min, int max);
int rnd(int max);
void append_log(char *log, size_t log_size, const char *format, ...);
COLORREF blend_color(COLORREF base, COLORREF overlay, int percent);
int point_in_rect(RECT rect, int x, int y);
void map_size_dimensions(int size, int *out_w, int *out_h);
void set_active_map_size(int size);

#endif
