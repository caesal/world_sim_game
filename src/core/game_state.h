#ifndef WORLD_SIM_GAME_STATE_H
#define WORLD_SIM_GAME_STATE_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

#include <stddef.h>

#define MAP_W 800
#define MAP_H 600
#define MAX_CIVS 16
#define MAX_CITIES 128
#define NAME_LEN 64
#define WINDOW_W 1920
#define WINDOW_H 1080
#define TOP_BAR_H 62
#define DEFAULT_SIDE_PANEL_W 460
#define MIN_SIDE_PANEL_W 420
#define MAX_SIDE_PANEL_W 640
#define BOTTOM_BAR_H 44
#define TIMER_ID 1
#define FORM_X_PAD 18
#define CITY_MIN_DISTANCE 12

#define ID_NAME_EDIT 101
#define ID_SYMBOL_EDIT 102
#define ID_AGGRESSION_EDIT 103
#define ID_EXPANSION_EDIT 104
#define ID_DEFENSE_EDIT 105
#define ID_CULTURE_EDIT 106
#define ID_ADD_BUTTON 107
#define ID_APPLY_BUTTON 108
#define ID_INITIAL_CIVS_EDIT 109

typedef enum {
    GEO_OCEAN,
    GEO_SHOAL,
    GEO_PLAIN,
    GEO_BASIN,
    GEO_HILL,
    GEO_MOUNTAIN
} Geography;

typedef enum {
    CLIMATE_GRASSLAND,
    CLIMATE_FOREST,
    CLIMATE_WETLAND,
    CLIMATE_SWAMP,
    CLIMATE_TUNDRA,
    CLIMATE_SNOWFIELD,
    CLIMATE_DESERT
} Climate;

typedef enum {
    DISPLAY_OVERVIEW,
    DISPLAY_CLIMATE,
    DISPLAY_GEOGRAPHY,
    DISPLAY_POLITICAL,
    DISPLAY_ALL
} DisplayMode;

typedef enum {
    PANEL_INFO,
    PANEL_CIV,
    PANEL_MAP
} PanelTab;

typedef struct {
    Geography geography;
    Climate climate;
    int owner;
    int province_id;
    int elevation;
    int moisture;
    int temperature;
    int resource_variation;
    int river;
} Tile;

typedef struct {
    char name[NAME_LEN];
    char symbol;
    COLORREF color;
    int alive;
    int population;
    int territory;
    int aggression;
    int expansion;
    int defense;
    int culture;
    int capital_city;
    int disorder;
} Civilization;

typedef struct {
    int alive;
    int owner;
    char name[NAME_LEN];
    int x;
    int y;
    int population;
    int radius;
    int capital;
} City;

typedef struct {
    int food;
    int livestock;
    int wood;
    int minerals;
    int water;
    int habitability;
    int attack;
    int defense;
} TerrainStats;

typedef struct {
    int city_id;
    int tiles;
    int population;
    int food;
    int livestock;
    int wood;
    int minerals;
    int water;
    int habitability;
    int attack;
    int defense;
} RegionSummary;

typedef struct {
    int map_x;
    int map_y;
    int tile_size;
    int draw_w;
    int draw_h;
} MapLayout;

typedef struct {
    HWND name_edit;
    HWND symbol_edit;
    HWND aggression_edit;
    HWND expansion_edit;
    HWND defense_edit;
    HWND culture_edit;
    HWND initial_civs_edit;
    HWND add_button;
    HWND apply_button;
} FormControls;

extern Tile world[MAP_H][MAP_W];
extern Civilization civs[MAX_CIVS];
extern City cities[MAX_CITIES];
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
extern int side_panel_w;
extern int dragging_panel;
extern int dragging_slider;
extern int dragging_map;
extern int last_mouse_x;
extern int last_mouse_y;
extern int hover_x;
extern int hover_y;
extern int ocean_slider;
extern int mountain_slider;
extern int desert_slider;
extern int forest_slider;
extern int wetland_slider;
extern int initial_civ_count;
extern int map_zoom_percent;
extern int map_offset_x;
extern int map_offset_y;
extern FormControls form;

extern const int SPEED_MS[3];
extern const char *SPEED_NAMES[3];
extern const COLORREF CIV_COLORS[MAX_CIVS];
extern const int MAP_DISPLAY_MODES[4];
extern const char *MAP_DISPLAY_NAMES[4];

int clamp(int value, int min, int max);
int rnd(int max);
void append_log(char *log, size_t log_size, const char *format, ...);
COLORREF blend_color(COLORREF base, COLORREF overlay, int percent);
RECT get_play_button_rect(RECT client);
RECT get_speed_button_rect(RECT client, int index);
RECT get_mode_button_rect(RECT client, int index);
RECT get_panel_tab_rect(RECT client, int index);
int point_in_rect(RECT rect, int x, int y);
const char *speed_seconds_text(int index);

#endif
