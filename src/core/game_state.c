#include "game_state.h"

#include "ui/ui_types.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Tile world[MAP_H][MAP_W];
Civilization civs[MAX_CIVS];
City cities[MAX_CITIES];
int civ_count = 0;
int city_count = 0;
int year = 0;
int month = 1;
int selected_x = -1;
int selected_y = -1;
int selected_civ = -1;
int auto_run = 0;
int speed_index = 0;
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
const int MAP_DISPLAY_MODES[4] = {DISPLAY_ALL, DISPLAY_CLIMATE, DISPLAY_GEOGRAPHY, DISPLAY_POLITICAL};
const char *MAP_DISPLAY_NAMES[4] = {"All", "Climate", "Geography", "Political"};

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
