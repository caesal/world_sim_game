#include "game_state.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sim/simulation.h"

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
    &side_panel_collapsed,
    &side_panel_expanded_w,
    &map_view_auto_centered,
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
    &map_legend_collapsed,
    &plague_fog_alpha,
    &region_size_slider,
    &worldgen_scroll_offset,
    &country_show_fallen,
    &country_list_scroll_offset,
    &country_detail_scroll_offset,
    &debug_subtab,
    &debug_system_scroll_offset,
    &debug_event_filter,
    &pause_menu_open,
    &selected_civ_color_index,
    &selected_civ_color
};

const int SPEED_MS[SPEED_COUNT] = {10000, 5000, 1000, 250, 100};
const char *SPEED_NAMES[SPEED_COUNT] = {"Observe 10s/mo", "Slow 5s/mo", "Normal 1s/mo", "Fast 0.25s/mo", "Max 0.1s/mo"};
const Color32 CIV_COLORS[MAX_CIVS] = {
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
    COLOR32_RGB(191, 89, 89),
    COLOR32_RGB(102, 173, 206),
    COLOR32_RGB(196, 130, 55),
    COLOR32_RGB(90, 176, 112),
    COLOR32_RGB(202, 94, 127),
    COLOR32_RGB(142, 152, 71),
    COLOR32_RGB(98, 112, 200),
    COLOR32_RGB(184, 86, 194),
    COLOR32_RGB(86, 156, 150),
    COLOR32_RGB(215, 142, 105),
    COLOR32_RGB(124, 177, 82),
    COLOR32_RGB(183, 103, 76),
    COLOR32_RGB(72, 139, 191),
    COLOR32_RGB(169, 137, 205),
    COLOR32_RGB(189, 166, 83),
    COLOR32_RGB(91, 157, 102),
    COLOR32_RGB(214, 104, 104)
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
        width = 576;
        height = 400;
    } else if (size == MAP_SIZE_LARGE) {
        width = 864;
        height = 600;
    }
    if (out_w) *out_w = width;
    if (out_h) *out_h = height;
}

void set_active_map_size(int size) {
    map_size_index = clamp(size, 0, MAP_SIZE_COUNT - 1);
    map_size_dimensions(map_size_index, &map_w, &map_h);
}
