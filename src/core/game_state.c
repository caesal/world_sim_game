#include "game_state.h"

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
int side_panel_w = DEFAULT_SIDE_PANEL_W;
int dragging_panel = 0;
int dragging_slider = -1;
int dragging_map = 0;
int last_mouse_x = 0;
int last_mouse_y = 0;
int hover_x = -1;
int hover_y = -1;
int ocean_slider = 45;
int mountain_slider = 50;
int desert_slider = 45;
int forest_slider = 55;
int wetland_slider = 40;
int initial_civ_count = 4;
int map_zoom_percent = 120;
int map_offset_x = 0;
int map_offset_y = 0;
FormControls form;

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

RECT get_play_button_rect(RECT client) {
    RECT rect = {18, client.bottom - 38, 58, client.bottom - 8};
    return rect;
}

RECT get_speed_button_rect(RECT client, int index) {
    RECT rect;
    rect.left = 68 + index * 46;
    rect.top = client.bottom - 38;
    rect.right = rect.left + 40;
    rect.bottom = client.bottom - 8;
    return rect;
}

RECT get_mode_button_rect(RECT client, int index) {
    RECT rect;
    int panel_x = client.right - side_panel_w + FORM_X_PAD;
    rect.left = panel_x;
    rect.top = TOP_BAR_H + 74 + index * 34;
    rect.right = panel_x + side_panel_w - FORM_X_PAD * 2;
    rect.bottom = rect.top + 28;
    return rect;
}

RECT get_panel_tab_rect(RECT client, int index) {
    RECT rect;
    int panel_x = client.right - side_panel_w + 12;
    int tab_w = (side_panel_w - 24) / 3;
    rect.left = panel_x + index * tab_w;
    rect.top = TOP_BAR_H + 10;
    rect.right = index == 2 ? client.right - 12 : rect.left + tab_w - 4;
    rect.bottom = rect.top + 30;
    return rect;
}

int point_in_rect(RECT rect, int x, int y) {
    return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
}

const char *speed_seconds_text(int index) {
    switch (index) {
        case 0: return "1s";
        case 1: return "0.25s";
        default: return "0.05s";
    }
}
