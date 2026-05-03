#ifndef WORLD_SIM_GAME_TYPES_H
#define WORLD_SIM_GAME_TYPES_H

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
#define DEFAULT_SIDE_PANEL_W 520
#define MIN_SIDE_PANEL_W 500
#define MAX_SIDE_PANEL_W 720
#define BOTTOM_BAR_H 44
#define TIMER_ID 1
#define FORM_X_PAD 18
#define CITY_MIN_DISTANCE 18
#define MAX_POPULATION 2000000000

typedef enum {
    GEO_OCEAN,
    GEO_COAST,
    GEO_PLAIN,
    GEO_HILL,
    GEO_MOUNTAIN,
    GEO_PLATEAU,
    GEO_BASIN,
    GEO_CANYON,
    GEO_VOLCANO,
    GEO_LAKE,
    GEO_BAY,
    GEO_DELTA,
    GEO_WETLAND,
    GEO_OASIS,
    GEO_ISLAND,
    GEO_COUNT
} Geography;

typedef enum {
    CLIMATE_TROPICAL_RAINFOREST,
    CLIMATE_TROPICAL_MONSOON,
    CLIMATE_TROPICAL_SAVANNA,
    CLIMATE_DESERT,
    CLIMATE_SEMI_ARID,
    CLIMATE_MEDITERRANEAN,
    CLIMATE_OCEANIC,
    CLIMATE_TEMPERATE_MONSOON,
    CLIMATE_CONTINENTAL,
    CLIMATE_SUBARCTIC,
    CLIMATE_TUNDRA,
    CLIMATE_ICE_CAP,
    CLIMATE_ALPINE,
    CLIMATE_HIGHLAND_PLATEAU,
    CLIMATE_COUNT
} Climate;

typedef enum {
    ECO_NONE,
    ECO_FOREST,
    ECO_RAINFOREST,
    ECO_GRASSLAND,
    ECO_DESERT,
    ECO_TUNDRA,
    ECO_SWAMP,
    ECO_BAMBOO,
    ECO_MANGROVE,
    ECO_COUNT
} Ecology;

typedef enum {
    RESOURCE_FEATURE_NONE,
    RESOURCE_FEATURE_FARMLAND,
    RESOURCE_FEATURE_PASTURE,
    RESOURCE_FEATURE_FOREST,
    RESOURCE_FEATURE_MINE,
    RESOURCE_FEATURE_FISHERY,
    RESOURCE_FEATURE_SALT_LAKE,
    RESOURCE_FEATURE_GEOTHERMAL,
    RESOURCE_FEATURE_COUNT
} ResourceFeature;

typedef enum {
    RESOURCE_FOOD,
    RESOURCE_LIVESTOCK,
    RESOURCE_WOOD,
    RESOURCE_STONE,
    RESOURCE_ORE,
    RESOURCE_WATER,
    RESOURCE_POPULATION,
    RESOURCE_MONEY,
    RESOURCE_TYPE_COUNT
} ResourceType;

typedef struct {
    int value[RESOURCE_TYPE_COUNT];
} ResourceVector;

typedef enum {
    CIV_METRIC_GOVERNANCE,
    CIV_METRIC_COHESION,
    CIV_METRIC_PRODUCTION,
    CIV_METRIC_MILITARY,
    CIV_METRIC_COMMERCE,
    CIV_METRIC_LOGISTICS,
    CIV_METRIC_INNOVATION,
    CIV_METRIC_COUNT
} CivilizationMetric;

typedef struct {
    Geography geography;
    Climate climate;
    Ecology ecology;
    ResourceFeature resource;
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
    int governance;
    int cohesion;
    int production;
    int military;
    int commerce;
    int logistics;
    int innovation;
    int adaptation;
    int capital_city;
    int disorder;
    int disorder_resource;
    int disorder_plague;
    int disorder_migration;
    int disorder_stability;
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
    int port;
    int port_x;
    int port_y;
    int port_region;
} City;

typedef struct {
    int food;
    int livestock;
    int wood;
    int stone;
    int minerals;
    int water;
    int pop_capacity;
    int money;
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
    int stone;
    int minerals;
    int water;
    int pop_capacity;
    int money;
    int habitability;
    int attack;
    int defense;
} RegionSummary;

typedef struct {
    int population;
    int territory;
    int cities;
    int ports;
    int food;
    int livestock;
    int wood;
    int stone;
    int minerals;
    int water;
    int pop_capacity;
    int money;
    int habitability;
    int resource_score;
} CountrySummary;

typedef struct {
    Tile (*world)[MAP_W];
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

extern Tile world[MAP_H][MAP_W];
extern Civilization civs[MAX_CIVS];
extern City cities[MAX_CITIES];
extern GameState g_game;
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

#endif
