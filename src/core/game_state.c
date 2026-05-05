#include "game_state.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Tile world[MAX_MAP_H][MAX_MAP_W];
RiverPath river_paths[MAX_RIVER_PATHS];
MaritimeRoute maritime_routes[MAX_MARITIME_ROUTES];
Civilization civs[MAX_CIVS];
City cities[MAX_CITIES];
int river_path_count = 0;
int maritime_route_count = 0;
int civ_count = 0;
int city_count = 0;
int year = 0;
int month = 1;
int selected_x = -1;
int selected_y = -1;
int selected_civ = -1;
int auto_run = 0;
int speed_index = 0;
int world_visual_revision = 1;
int map_w = DEFAULT_MAP_W;
int map_h = DEFAULT_MAP_H;
int map_size_index = MAP_SIZE_MEDIUM;
int pending_map_size = MAP_SIZE_MEDIUM;
int world_generated = 0;

GameState g_game = {
    world,
    civs,
    cities,
    &civ_count,
    &city_count,
    &year,
    &month,
    &selected_x,
    &selected_y,
    &selected_civ,
    &auto_run,
    &speed_index,
    &display_mode,
    &panel_tab,
    &ui_language,
    &side_panel_w,
    &dragging_panel,
    &dragging_slider,
    &dragging_map,
    &last_mouse_x,
    &last_mouse_y,
    &hover_x,
    &hover_y,
    &ocean_slider,
    &continent_slider,
    &relief_slider,
    &moisture_slider,
    &drought_slider,
    &vegetation_slider,
    &bias_forest_slider,
    &bias_desert_slider,
    &bias_mountain_slider,
    &bias_wetland_slider,
    &initial_civ_count,
    &map_zoom_percent,
    &map_offset_x,
    &map_offset_y,
    &map_legend_collapsed
};

const int SPEED_MS[3] = {1000, 250, 50};
const char *SPEED_NAMES[3] = {"Slow", "Normal", "Fast"};
const COLORREF CIV_COLORS[MAX_CIVS] = {
    RGB(232, 31, 39),
    RGB(34, 88, 230),
    RGB(18, 205, 42),
    RGB(244, 188, 40),
    RGB(168, 76, 210),
    RGB(232, 112, 37),
    RGB(20, 184, 170),
    RGB(220, 70, 145),
    RGB(165, 102, 44),
    RGB(72, 165, 224),
    RGB(145, 191, 71),
    RGB(205, 118, 150),
    RGB(114, 93, 189),
    RGB(209, 156, 44),
    RGB(78, 139, 122),
    RGB(191, 89, 89)
};

int clamp(int value, int min, int max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

int rnd(int max) {
    if (max <= 0) return 0;
    return rand() % max;
}

void append_log(char *log, size_t log_size, const char *format, ...) {
    size_t used = strlen(log);
    va_list args;

    if (used >= log_size) return;
    va_start(args, format);
    vsnprintf(log + used, log_size - used, format, args);
    va_end(args);
}

COLORREF blend_color(COLORREF base, COLORREF overlay, int percent) {
    int r = GetRValue(base) * (100 - percent) / 100 + GetRValue(overlay) * percent / 100;
    int g = GetGValue(base) * (100 - percent) / 100 + GetGValue(overlay) * percent / 100;
    int b = GetBValue(base) * (100 - percent) / 100 + GetBValue(overlay) * percent / 100;
    return RGB(r, g, b);
}

int point_in_rect(RECT rect, int x, int y) {
    return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
}

void map_size_dimensions(int size, int *out_w, int *out_h) {
    int width = DEFAULT_MAP_W;
    int height = DEFAULT_MAP_H;

    if (size == MAP_SIZE_SMALL) {
        width = 640;
        height = 360;
    } else if (size == MAP_SIZE_LARGE) {
        width = 960;
        height = 540;
    }
    if (out_w) *out_w = width;
    if (out_h) *out_h = height;
}

void set_active_map_size(int size) {
    map_size_index = clamp(size, 0, MAP_SIZE_COUNT - 1);
    map_size_dimensions(map_size_index, &map_w, &map_h);
}
