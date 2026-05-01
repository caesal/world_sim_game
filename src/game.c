#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAP_W 160
#define MAP_H 90
#define MAX_CIVS 8
#define MAX_CITIES 128
#define NAME_LEN 32
#define WINDOW_W 1920
#define WINDOW_H 1080
#define TOP_BAR_H 62
#define DEFAULT_SIDE_PANEL_W 360
#define MIN_SIDE_PANEL_W 300
#define MAX_SIDE_PANEL_W 560
#define BOTTOM_BAR_H 44
#define TIMER_ID 1
#define FORM_X_PAD 18

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
    WATER,
    COAST,
    GRASS,
    FOREST,
    DESERT,
    ICE,
    MOUNTAIN,
    HILL,
    WETLAND
} Terrain;

typedef struct {
    Terrain terrain;
    int owner;
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
} Civilization;

typedef struct {
    int alive;
    int owner;
    int x;
    int y;
    int population;
    int radius;
    int capital;
} City;

typedef struct {
    int food;
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

static Tile world[MAP_H][MAP_W];
static Civilization civs[MAX_CIVS];
static City cities[MAX_CITIES];
static int civ_count = 0;
static int city_count = 0;
static int year = 0;
static int month = 1;
static int selected_x = -1;
static int selected_y = -1;
static int selected_civ = -1;
static int auto_run = 0;
static int speed_index = 0;
static int side_panel_w = DEFAULT_SIDE_PANEL_W;
static int dragging_panel = 0;
static int dragging_land_slider = 0;
static int ocean_slider = 55;
static int initial_civ_count = 4;
static int map_zoom_percent = 100;
static int map_offset_x = 0;
static int map_offset_y = 0;
static FormControls form;

static const int SPEED_MS[3] = {2000, 500, 100};
static const char *SPEED_NAMES[3] = {"Slow", "Normal", "Fast"};

static int read_int_control(HWND hwnd, int fallback);
static void select_tile_from_mouse(HWND hwnd, int mouse_x, int mouse_y);
static void set_speed(HWND hwnd, int index);

static RECT get_play_button_rect(RECT client) {
    RECT rect = {18, client.bottom - 38, 58, client.bottom - 8};
    return rect;
}

static RECT get_speed_button_rect(RECT client, int index) {
    RECT rect;
    rect.left = 68 + index * 46;
    rect.top = client.bottom - 38;
    rect.right = rect.left + 40;
    rect.bottom = client.bottom - 8;
    return rect;
}

static int point_in_rect(RECT rect, int x, int y) {
    return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
}

static const char *speed_seconds_text(int index) {
    switch (index) {
        case 0: return "2s";
        case 1: return "0.5s";
        default: return "0.1s";
    }
}

static const COLORREF CIV_COLORS[MAX_CIVS] = {
    RGB(232, 31, 39),
    RGB(34, 88, 230),
    RGB(18, 205, 42),
    RGB(244, 188, 40),
    RGB(168, 76, 210),
    RGB(232, 112, 37),
    RGB(20, 184, 170),
    RGB(220, 70, 145)
};

static int clamp(int value, int min, int max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static int rnd(int max) {
    if (max <= 0) return 0;
    return rand() % max;
}

static void append_log(char *log, size_t log_size, const char *format, ...) {
    size_t used = strlen(log);
    va_list args;

    if (used >= log_size) return;
    va_start(args, format);
    vsnprintf(log + used, log_size - used, format, args);
    va_end(args);
}

static COLORREF blend_color(COLORREF base, COLORREF overlay, int percent) {
    int r = GetRValue(base) * (100 - percent) / 100 + GetRValue(overlay) * percent / 100;
    int g = GetGValue(base) * (100 - percent) / 100 + GetGValue(overlay) * percent / 100;
    int b = GetBValue(base) * (100 - percent) / 100 + GetBValue(overlay) * percent / 100;
    return RGB(r, g, b);
}

static TerrainStats terrain_stats(Terrain terrain) {
    TerrainStats stats;
    memset(&stats, 0, sizeof(stats));

    switch (terrain) {
        case COAST:
            stats.food = 6; stats.wood = 1; stats.minerals = 1; stats.water = 8; stats.habitability = 7; stats.attack = 0; stats.defense = 1;
            break;
        case GRASS:
            stats.food = 8; stats.wood = 2; stats.minerals = 1; stats.water = 5; stats.habitability = 9; stats.attack = -1; stats.defense = 0;
            break;
        case FOREST:
            stats.food = 5; stats.wood = 9; stats.minerals = 2; stats.water = 5; stats.habitability = 6; stats.attack = 0; stats.defense = 3;
            break;
        case DESERT:
            stats.food = 1; stats.wood = 0; stats.minerals = 5; stats.water = 1; stats.habitability = 2; stats.attack = 3; stats.defense = 1;
            break;
        case ICE:
            stats.food = 1; stats.wood = 0; stats.minerals = 4; stats.water = 3; stats.habitability = 1; stats.attack = 3; stats.defense = 2;
            break;
        case MOUNTAIN:
            stats.food = 2; stats.wood = 2; stats.minerals = 9; stats.water = 3; stats.habitability = 3; stats.attack = 1; stats.defense = 4;
            break;
        case HILL:
            stats.food = 4; stats.wood = 4; stats.minerals = 6; stats.water = 3; stats.habitability = 5; stats.attack = 1; stats.defense = 2;
            break;
        case WETLAND:
            stats.food = 7; stats.wood = 4; stats.minerals = 1; stats.water = 9; stats.habitability = 5; stats.attack = -1; stats.defense = 3;
            break;
        default:
            stats.water = 10;
            break;
    }

    return stats;
}

static const char *terrain_name(Terrain terrain) {
    switch (terrain) {
        case WATER: return "Ocean";
        case COAST: return "Coast";
        case GRASS: return "Grassland";
        case FOREST: return "Forest";
        case DESERT: return "Desert";
        case ICE: return "Icefield";
        case MOUNTAIN: return "Mountain";
        case HILL: return "Hill";
        case WETLAND: return "Wetland";
        default: return "Unknown";
    }
}

static COLORREF terrain_color(Terrain terrain) {
    switch (terrain) {
        case WATER: return RGB(79, 160, 215);
        case COAST: return RGB(205, 234, 246);
        case GRASS: return RGB(149, 193, 88);
        case FOREST: return RGB(56, 144, 43);
        case DESERT: return RGB(250, 233, 112);
        case ICE: return RGB(224, 242, 246);
        case MOUNTAIN: return RGB(181, 112, 62);
        case HILL: return RGB(166, 151, 82);
        case WETLAND: return RGB(93, 159, 96);
        default: return RGB(0, 0, 0);
    }
}

static int is_land(Terrain terrain) {
    return terrain == GRASS || terrain == FOREST || terrain == DESERT ||
           terrain == ICE || terrain == MOUNTAIN || terrain == HILL || terrain == WETLAND;
}

static TerrainStats tile_stats(int x, int y) {
    TerrainStats stats = terrain_stats(world[y][x].terrain);
    int variation = world[y][x].resource_variation - 50;

    stats.food = clamp(stats.food + variation / 18 + (world[y][x].river ? 2 : 0), 0, 10);
    stats.wood = clamp(stats.wood + (world[y][x].moisture - 50) / 22, 0, 10);
    stats.minerals = clamp(stats.minerals + (world[y][x].elevation - 55) / 18, 0, 10);
    stats.water = clamp(stats.water + (world[y][x].moisture - 45) / 18 + (world[y][x].river ? 3 : 0), 0, 10);
    stats.habitability = clamp(stats.habitability + variation / 24 - abs(world[y][x].temperature - 55) / 35, 0, 10);
    stats.attack = clamp(stats.attack + (50 - stats.food * 5) / 20 + (world[y][x].elevation > 70 ? 1 : 0), -3, 6);
    stats.defense = clamp(stats.defense + (world[y][x].river ? 1 : 0) + (world[y][x].elevation > 70 ? 1 : 0), 0, 8);
    return stats;
}

static int nearby_land_count(int x, int y) {
    int count = 0;
    int dy, dx;

    for (dy = -1; dy <= 1; dy++) {
        for (dx = -1; dx <= 1; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            if (dx == 0 && dy == 0) continue;
            if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
            if (is_land(world[ny][nx].terrain)) count++;
        }
    }

    return count;
}

static int tile_cost(int x, int y) {
    TerrainStats stats = tile_stats(x, y);
    if (!is_land(world[y][x].terrain)) return 99;
    return clamp(10 - stats.habitability + stats.defense / 2, 2, 9);
}

static void recalculate_territory(void) {
    int i, y, x;

    for (i = 0; i < civ_count; i++) {
        civs[i].territory = 0;
    }

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int owner = world[y][x].owner;
            if (owner >= 0 && owner < civ_count) {
                civs[owner].territory++;
            }
        }
    }

    for (i = 0; i < civ_count; i++) {
        if (civs[i].territory <= 0 || civs[i].population <= 0) {
            civs[i].alive = 0;
        }
    }
}

static void seed_random(void) {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    srand((unsigned int)(time(NULL) ^ GetTickCount() ^ counter.LowPart ^ GetCurrentProcessId()));
}

static int compare_ints(const void *left, const void *right) {
    int a = *(const int *)left;
    int b = *(const int *)right;
    return (a > b) - (a < b);
}

static void smooth_field(int field[MAP_H][MAP_W], int passes) {
    int pass;

    for (pass = 0; pass < passes; pass++) {
        int next[MAP_H][MAP_W];
        int y, x;

        for (y = 0; y < MAP_H; y++) {
            for (x = 0; x < MAP_W; x++) {
                int total = 0;
                int count = 0;
                int dy, dx;

                for (dy = -1; dy <= 1; dy++) {
                    for (dx = -1; dx <= 1; dx++) {
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
                        total += field[ny][nx];
                        count++;
                    }
                }
                next[y][x] = total / count;
            }
        }
        memcpy(field, next, sizeof(next));
    }
}

static void mark_river_from(int start_x, int start_y) {
    int x = start_x;
    int y = start_y;
    int steps;

    for (steps = 0; steps < 180; steps++) {
        int best_x = x;
        int best_y = y;
        int best_score = world[y][x].elevation;
        int dy, dx;

        if (!is_land(world[y][x].terrain) || world[y][x].terrain == COAST) return;
        world[y][x].river = 1;

        for (dy = -1; dy <= 1; dy++) {
            for (dx = -1; dx <= 1; dx++) {
                int nx = x + dx;
                int ny = y + dy;
                int score;

                if (dx == 0 && dy == 0) continue;
                if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
                score = world[ny][nx].elevation + rnd(5);
                if (world[ny][nx].terrain == WATER || world[ny][nx].terrain == COAST) {
                    world[y][x].river = 1;
                    return;
                }
                if (score < best_score) {
                    best_score = score;
                    best_x = nx;
                    best_y = ny;
                }
            }
        }

        if (best_x == x && best_y == y) return;
        x = best_x;
        y = best_y;
    }
}

static void generate_world(void) {
    int y, x, pass;
    int elevation[MAP_H][MAP_W];
    int moisture[MAP_H][MAP_W];
    int temperature[MAP_H][MAP_W];
    int sorted_elevation[MAP_W * MAP_H];
    int target_land_percent = 75 - ocean_slider * 55 / 100;
    int target_land_tiles = MAP_W * MAP_H * target_land_percent / 100;
    int threshold_index = clamp(MAP_W * MAP_H - target_land_tiles, 0, MAP_W * MAP_H - 1);
    int sea_level;
    int index = 0;

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int edge_x = x < MAP_W / 2 ? x : MAP_W - 1 - x;
            int edge_y = y < MAP_H / 2 ? y : MAP_H - 1 - y;
            int edge = edge_x < edge_y ? edge_x : edge_y;
            int edge_falloff = clamp(edge * 3, 0, 45);

            world[y][x].terrain = WATER;
            world[y][x].owner = -1;
            world[y][x].river = 0;
            elevation[y][x] = rnd(101) + edge_falloff - 20;
            moisture[y][x] = rnd(101);
            temperature[y][x] = clamp(92 - abs(y - MAP_H / 2) * 140 / MAP_H + rnd(21) - 10, 0, 100);
        }
    }

    smooth_field(elevation, 11);
    smooth_field(moisture, 9);
    smooth_field(temperature, 7);

    for (pass = 0; pass < 18; pass++) {
        int cx = 12 + rnd(MAP_W - 24);
        int cy = 8 + rnd(MAP_H - 16);
        int radius = 8 + rnd(22);
        int lift = 12 + rnd(30);
        int yy, xx;

        for (yy = cy - radius; yy <= cy + radius; yy++) {
            for (xx = cx - radius; xx <= cx + radius; xx++) {
                int dx = xx - cx;
                int dy = (yy - cy) * 2;
                int dist = abs(dx) + abs(dy);
                if (xx < 0 || xx >= MAP_W || yy < 0 || yy >= MAP_H || dist > radius * 2) continue;
                elevation[yy][xx] += lift * (radius * 2 - dist) / (radius * 2);
            }
        }
    }
    smooth_field(elevation, 5);

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            sorted_elevation[index] = elevation[y][x];
            index++;
        }
    }

    qsort(sorted_elevation, MAP_W * MAP_H, sizeof(sorted_elevation[0]), compare_ints);
    sea_level = sorted_elevation[threshold_index];

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int elev = clamp(elevation[y][x], 0, 100);
            int moist = clamp(moisture[y][x] + (elev < sea_level + 8 ? 12 : 0), 0, 100);
            int temp = clamp(temperature[y][x] - elev / 9, 0, 100);

            world[y][x].elevation = elev;
            world[y][x].moisture = moist;
            world[y][x].temperature = temp;
            world[y][x].resource_variation = rnd(101);

            if (elevation[y][x] < sea_level) world[y][x].terrain = WATER;
            else if (elev > 82) world[y][x].terrain = MOUNTAIN;
            else if (elev > 68) world[y][x].terrain = HILL;
            else if (temp < 18) world[y][x].terrain = ICE;
            else if (moist < 24 && temp > 45) world[y][x].terrain = DESERT;
            else if (moist > 76 && elev < 58) world[y][x].terrain = WETLAND;
            else if (moist > 52) world[y][x].terrain = FOREST;
            else world[y][x].terrain = GRASS;
        }
    }

    for (pass = 0; pass < 2; pass++) {
        Tile next[MAP_H][MAP_W];
        memcpy(next, world, sizeof(world));
        for (y = 1; y < MAP_H - 1; y++) {
            for (x = 1; x < MAP_W - 1; x++) {
                int land = nearby_land_count(x, y);
                if (is_land(world[y][x].terrain) && land <= 2) next[y][x].terrain = WATER;
                if (world[y][x].terrain == WATER && land >= 6) next[y][x].terrain = GRASS;
            }
        }
        memcpy(world, next, sizeof(world));
    }

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            if (world[y][x].terrain == WATER && nearby_land_count(x, y) > 0) {
                world[y][x].terrain = COAST;
            } else if (world[y][x].terrain == WETLAND && nearby_land_count(x, y) == 8 && rnd(100) < 50) {
                world[y][x].terrain = FOREST;
            }
        }
    }

    for (pass = 0; pass < 42; pass++) {
        int tries;
        for (tries = 0; tries < 200; tries++) {
            int rx = rnd(MAP_W);
            int ry = rnd(MAP_H);
            if ((world[ry][rx].terrain == MOUNTAIN || world[ry][rx].terrain == HILL) &&
                world[ry][rx].moisture > 35 + rnd(30)) {
                mark_river_from(rx, ry);
                break;
            }
        }
    }

    year = 0;
    month = 1;
    civ_count = 0;
    city_count = 0;
    selected_x = -1;
    selected_y = -1;
    selected_civ = -1;
}

static int find_empty_land(int *out_x, int *out_y) {
    int tries;

    for (tries = 0; tries < 12000; tries++) {
        int x = rnd(MAP_W);
        int y = rnd(MAP_H);
        TerrainStats stats = tile_stats(x, y);

        if (is_land(world[y][x].terrain) && world[y][x].owner == -1 && stats.habitability > 1) {
            *out_x = x;
            *out_y = y;
            return 1;
        }
    }

    return 0;
}

static int city_radius_for_tile(int x, int y, int population) {
    TerrainStats stats = tile_stats(x, y);
    int resource_score = stats.food + stats.water + stats.habitability + stats.wood / 2 + stats.minerals / 2;
    return clamp(2 + population / 180 + resource_score / 8, 3, 10);
}

static int create_city(int owner, int x, int y, int population, int capital) {
    City *city;

    if (city_count >= MAX_CITIES) return -1;

    city = &cities[city_count];
    city->alive = 1;
    city->owner = owner;
    city->x = x;
    city->y = y;
    city->population = population;
    city->radius = city_radius_for_tile(x, y, population);
    city->capital = capital;
    return city_count++;
}

static void claim_city_region(int city_id, int owner) {
    int x, y;
    City *city;

    if (city_id < 0 || city_id >= city_count) return;
    city = &cities[city_id];
    if (!city->alive) return;

    city->owner = owner;
    for (y = city->y - city->radius; y <= city->y + city->radius; y++) {
        for (x = city->x - city->radius; x <= city->x + city->radius; x++) {
            int dx = x - city->x;
            int dy = y - city->y;
            if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) continue;
            if (!is_land(world[y][x].terrain)) continue;
            if (dx * dx + dy * dy <= city->radius * city->radius) {
                world[y][x].owner = owner;
            }
        }
    }
}

static int city_at(int x, int y) {
    int i;

    for (i = 0; i < city_count; i++) {
        if (cities[i].alive && cities[i].x == x && cities[i].y == y) return i;
    }

    return -1;
}

static int city_for_tile(int x, int y) {
    int i;
    int owner;

    if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) return -1;
    owner = world[y][x].owner;
    if (owner < 0) return -1;

    for (i = 0; i < city_count; i++) {
        int dx;
        int dy;
        if (!cities[i].alive || cities[i].owner != owner) continue;
        dx = x - cities[i].x;
        dy = y - cities[i].y;
        if (dx * dx + dy * dy <= cities[i].radius * cities[i].radius) return i;
    }

    return -1;
}

static RegionSummary summarize_city_region(int city_id) {
    RegionSummary summary;
    int x, y;
    City *city;

    memset(&summary, 0, sizeof(summary));
    summary.city_id = city_id;
    if (city_id < 0 || city_id >= city_count) return summary;
    city = &cities[city_id];
    summary.population = city->population;

    for (y = city->y - city->radius; y <= city->y + city->radius; y++) {
        for (x = city->x - city->radius; x <= city->x + city->radius; x++) {
            int dx = x - city->x;
            int dy = y - city->y;
            TerrainStats stats;

            if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) continue;
            if (world[y][x].owner != city->owner) continue;
            if (dx * dx + dy * dy > city->radius * city->radius) continue;
            stats = tile_stats(x, y);
            summary.tiles++;
            summary.food += stats.food;
            summary.wood += stats.wood;
            summary.minerals += stats.minerals;
            summary.water += stats.water;
            summary.habitability += stats.habitability;
            summary.attack += stats.attack;
            summary.defense += stats.defense;
        }
    }

    if (summary.tiles > 0) {
        summary.food /= summary.tiles;
        summary.wood /= summary.tiles;
        summary.minerals /= summary.tiles;
        summary.water /= summary.tiles;
        summary.habitability /= summary.tiles;
        summary.attack /= summary.tiles;
        summary.defense /= summary.tiles;
    }
    return summary;
}

static int add_civilization_at(const char *name, char symbol, int aggression, int expansion,
                               int defense, int culture, int preferred_x, int preferred_y) {
    int x = preferred_x;
    int y = preferred_y;
    int city_id;
    Civilization *civ;

    if (civ_count >= MAX_CIVS) return 0;
    if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H || !is_land(world[y][x].terrain) || world[y][x].owner != -1) {
        if (!find_empty_land(&x, &y)) return 0;
    }

    civ = &civs[civ_count];
    memset(civ, 0, sizeof(*civ));
    strncpy(civ->name, name, NAME_LEN - 1);
    civ->symbol = symbol;
    civ->color = CIV_COLORS[civ_count % MAX_CIVS];
    civ->alive = 1;
    civ->population = 120 + tile_stats(x, y).food * 10 + rnd(80);
    civ->territory = 1;
    civ->aggression = clamp(aggression, 0, 10);
    civ->expansion = clamp(expansion, 0, 10);
    civ->defense = clamp(defense, 0, 10);
    civ->culture = clamp(culture, 0, 10);
    civ->capital_city = -1;

    world[y][x].owner = civ_count;
    city_id = create_city(civ_count, x, y, civ->population / 2, 1);
    civ->capital_city = city_id;
    if (city_id >= 0) claim_city_region(city_id, civ_count);

    civ_count++;
    recalculate_territory();
    return 1;
}

static int add_civilization(const char *name, char symbol, int aggression, int expansion, int defense, int culture) {
    return add_civilization_at(name, symbol, aggression, expansion, defense, culture, -1, -1);
}

static void seed_default_civilizations(void) {
    const char *names[MAX_CIVS] = {
        "Red Dominion", "Blue League", "Green Pact", "Gold Union",
        "Violet Realm", "Orange Clans", "Teal Coast", "Rose March"
    };
    const char symbols[MAX_CIVS] = {'R', 'B', 'G', 'Y', 'V', 'O', 'T', 'M'};
    const int traits[MAX_CIVS][4] = {
        {8, 7, 4, 3}, {3, 4, 8, 6}, {5, 6, 5, 7}, {4, 6, 5, 8},
        {6, 5, 6, 6}, {7, 5, 5, 4}, {4, 7, 4, 7}, {5, 5, 7, 5}
    };
    int i;
    int count = clamp(initial_civ_count, 0, MAX_CIVS);

    for (i = 0; i < count; i++) {
        add_civilization(names[i], symbols[i], traits[i][0], traits[i][1], traits[i][2], traits[i][3]);
    }
}

static int pick_owned_border(int civ_id, int *to_x, int *to_y) {
    int tries;
    const int dir[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

    for (tries = 0; tries < 6000; tries++) {
        int x = rnd(MAP_W);
        int y = rnd(MAP_H);
        int d = rnd(4);
        int nx = x + dir[d][0];
        int ny = y + dir[d][1];

        if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
        if (world[y][x].owner != civ_id) continue;
        if (!is_land(world[ny][nx].terrain)) continue;
        if (world[ny][nx].owner == civ_id) continue;

        *to_x = nx;
        *to_y = ny;
        return 1;
    }

    return 0;
}

static void capture_city_if_present(int x, int y, int new_owner) {
    int city_id = city_at(x, y);

    if (city_id >= 0 && cities[city_id].owner != new_owner) {
        claim_city_region(city_id, new_owner);
    }
}

static void maybe_found_city(int civ_id) {
    int tries;

    if (city_count >= MAX_CITIES || !civs[civ_id].alive) return;
    if (civs[civ_id].territory < 16 || rnd(100) > 5 + civs[civ_id].culture) return;

    for (tries = 0; tries < 2500; tries++) {
        int x = rnd(MAP_W);
        int y = rnd(MAP_H);
        TerrainStats stats = tile_stats(x, y);

        if (world[y][x].owner != civ_id) continue;
        if (!is_land(world[y][x].terrain)) continue;
        if (city_at(x, y) >= 0) continue;
        if (stats.food + stats.water + stats.habitability < 14) continue;

        create_city(civ_id, x, y, 80 + stats.food * 8 + rnd(80), 0);
        return;
    }
}

static void resolve_expansion(int civ_id, char *log, size_t log_size) {
    int tx, ty;
    int target_owner;
    Civilization *civ = &civs[civ_id];
    TerrainStats target_stats;
    int drive;
    int cost;

    if (!civ->alive) return;
    if (!pick_owned_border(civ_id, &tx, &ty)) return;

    target_owner = world[ty][tx].owner;
    target_stats = tile_stats(tx, ty);
    cost = tile_cost(tx, ty);
    drive = civ->expansion * 8 + civ->culture * 2 + target_stats.habitability * 2 +
            rnd(45) + civ->population / 70 - cost * 5;

    if (target_owner == -1) {
        if (drive > 22) {
            world[ty][tx].owner = civ_id;
            append_log(log, log_size, "%s settled %s. ", civ->name, terrain_name(world[ty][tx].terrain));
        }
        return;
    }

    if (target_owner >= 0 && target_owner < civ_count && civs[target_owner].alive) {
        Civilization *enemy = &civs[target_owner];
        int attack = civ->aggression * 10 + target_stats.attack * 8 + civ->population / 45 + rnd(70);
        int defense = enemy->defense * 11 + target_stats.defense * 9 + enemy->population / 55 + rnd(70);

        if (attack > defense) {
            int loss = 6 + rnd(18);
            world[ty][tx].owner = civ_id;
            capture_city_if_present(tx, ty, civ_id);
            civ->population = clamp(civ->population - loss, 1, 999999);
            enemy->population = clamp(enemy->population - loss * 2, 0, 999999);
            append_log(log, log_size, "%s captured land from %s. ", civ->name, enemy->name);
        } else if (rnd(100) < 35 + civ->aggression * 3) {
            int loss = 4 + rnd(14);
            civ->population = clamp(civ->population - loss, 0, 999999);
            enemy->population = clamp(enemy->population - loss / 2, 0, 999999);
        }
    }
}

static void update_city_growth_and_regions(void) {
    int i;

    for (i = 0; i < city_count; i++) {
        City *city = &cities[i];
        TerrainStats stats;

        if (!city->alive || city->owner < 0 || city->owner >= civ_count || !civs[city->owner].alive) continue;
        stats = tile_stats(city->x, city->y);
        city->population = clamp(city->population + stats.food + stats.water / 2 + stats.habitability - 8, 1, 999999);
        city->radius = city_radius_for_tile(city->x, city->y, city->population);
        claim_city_region(i, city->owner);
    }
}

static void random_event(char *log, size_t log_size) {
    int id;
    Civilization *civ;

    if (civ_count == 0 || rnd(100) > 18) return;
    id = rnd(civ_count);
    civ = &civs[id];
    if (!civ->alive) return;

    switch (rnd(5)) {
        case 0:
            civ->population += 15 + civ->culture * 3;
            append_log(log, log_size, "Festival in %s. ", civ->name);
            break;
        case 1:
            civ->population = clamp(civ->population - (8 + rnd(28)), 0, 999999);
            append_log(log, log_size, "Plague hit %s. ", civ->name);
            break;
        case 2:
            civ->defense = clamp(civ->defense + 1, 0, 10);
            append_log(log, log_size, "%s fortified borders. ", civ->name);
            break;
        case 3:
            civ->aggression = clamp(civ->aggression + 1, 0, 10);
            append_log(log, log_size, "%s became ambitious. ", civ->name);
            break;
        default:
            civ->expansion = clamp(civ->expansion + 1, 0, 10);
            append_log(log, log_size, "%s started migrating. ", civ->name);
            break;
    }
}

static int living_civilizations(void) {
    int i;
    int living = 0;

    for (i = 0; i < civ_count; i++) {
        if (civs[i].alive) living++;
    }

    return living;
}

static int owned_resource_score(int civ_id) {
    int x, y;
    int score = 0;
    int count = 0;

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            if (world[y][x].owner == civ_id) {
                TerrainStats stats = tile_stats(x, y);
                score += stats.food + stats.water + stats.habitability;
                count++;
            }
        }
    }

    return count > 0 ? score / count : 0;
}

static void simulate_one_month(void) {
    int i;
    char log[512];
    log[0] = '\0';

    for (i = 0; i < civ_count; i++) {
        Civilization *civ = &civs[i];
        int resources;
        int growth;

        if (!civ->alive) continue;
        resources = owned_resource_score(i);
        growth = resources / 2 + civ->culture + civ->territory / 30 + rnd(6) - 4;
        civ->population = clamp(civ->population + growth, 0, 999999);
    }

    update_city_growth_and_regions();

    for (i = 0; i < civ_count; i++) {
        int attempts;
        if (!civs[i].alive) continue;
        attempts = 1 + civs[i].expansion / 4 + civs[i].aggression / 7;
        while (attempts-- > 0) resolve_expansion(i, log, sizeof(log));
        maybe_found_city(i);
    }

    random_event(log, sizeof(log));
    recalculate_territory();

    month++;
    if (month > 12) {
        month = 1;
        year = clamp(year + 1, 0, 99999);
    }

    if (living_civilizations() <= 1) auto_run = 0;
}

static void reset_simulation(void) {
    generate_world();
    seed_default_civilizations();
    recalculate_territory();
    auto_run = 0;
}

static void read_world_setup_controls(void) {
    if (form.initial_civs_edit) {
        initial_civ_count = clamp(read_int_control(form.initial_civs_edit, initial_civ_count), 0, MAX_CIVS);
    }
}

static MapLayout get_map_layout(RECT client) {
    MapLayout layout;
    int available_w = (client.right - client.left) - side_panel_w - 36;
    int available_h = (client.bottom - client.top) - TOP_BAR_H - BOTTOM_BAR_H - 24;
    int tile_w = available_w / MAP_W;
    int tile_h = available_h / MAP_H;
    int base_tile = clamp(tile_w < tile_h ? tile_w : tile_h, 4, 14);

    layout.tile_size = clamp(base_tile * map_zoom_percent / 100, 3, 42);
    layout.draw_w = layout.tile_size * MAP_W;
    layout.draw_h = layout.tile_size * MAP_H;
    layout.map_x = 18 + map_offset_x;
    layout.map_y = TOP_BAR_H + 10 + map_offset_y;
    return layout;
}

static void fill_rect(HDC hdc, RECT rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
}

static void draw_text_line(HDC hdc, int x, int y, const char *text, COLORREF color) {
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    TextOutA(hdc, x, y, text, (int)strlen(text));
}

static void draw_center_text(HDC hdc, RECT rect, const char *text, COLORREF color) {
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    DrawTextA(hdc, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void draw_tile(HDC hdc, MapLayout layout, int x, int y) {
    RECT rect;
    COLORREF color = terrain_color(world[y][x].terrain);
    int owner = world[y][x].owner;

    if (owner >= 0 && owner < civ_count && civs[owner].alive) {
        color = blend_color(color, civs[owner].color, 52);
    }

    rect.left = layout.map_x + x * layout.tile_size;
    rect.top = layout.map_y + y * layout.tile_size;
    rect.right = rect.left + layout.tile_size + 1;
    rect.bottom = rect.top + layout.tile_size + 1;
    fill_rect(hdc, rect, color);
}

static void draw_land_texture(HDC hdc, MapLayout layout, int x, int y) {
    int px = layout.map_x + x * layout.tile_size;
    int py = layout.map_y + y * layout.tile_size;
    int s = layout.tile_size;
    Terrain terrain = world[y][x].terrain;
    HBRUSH brush;
    COLORREF mark;

    if (!is_land(terrain) || s < 6) return;
    if (((x * 17 + y * 31) % 11) != 0) return;

    mark = terrain == FOREST ? RGB(35, 113, 38) :
           terrain == DESERT ? RGB(245, 174, 84) :
           terrain == ICE ? RGB(245, 252, 255) :
           terrain == WETLAND ? RGB(65, 135, 84) :
           terrain == MOUNTAIN ? RGB(143, 84, 48) :
           RGB(122, 176, 79);
    brush = CreateSolidBrush(mark);
    SelectObject(hdc, brush);
    SelectObject(hdc, GetStockObject(NULL_PEN));
    Ellipse(hdc, px + s / 4, py + s / 5, px + s, py + s * 4 / 5);
    DeleteObject(brush);
}

static void draw_rivers(HDC hdc, MapLayout layout) {
    int y, x;
    HPEN river_pen = CreatePen(PS_SOLID, layout.tile_size >= 10 ? 2 : 1, RGB(57, 139, 218));
    HPEN old_pen = SelectObject(hdc, river_pen);

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int cx;
            int cy;
            int s;

            if (!world[y][x].river) continue;
            cx = layout.map_x + x * layout.tile_size + layout.tile_size / 2;
            cy = layout.map_y + y * layout.tile_size + layout.tile_size / 2;
            s = layout.tile_size / 2;
            MoveToEx(hdc, cx - s, cy, NULL);
            LineTo(hdc, cx + s, cy);
            if (y + 1 < MAP_H && world[y + 1][x].river) {
                MoveToEx(hdc, cx, cy, NULL);
                LineTo(hdc, cx, cy + layout.tile_size);
            }
            if (x + 1 < MAP_W && world[y][x + 1].river) {
                MoveToEx(hdc, cx, cy, NULL);
                LineTo(hdc, cx + layout.tile_size, cy);
            }
        }
    }

    SelectObject(hdc, old_pen);
    DeleteObject(river_pen);
}

static void draw_borders(HDC hdc, MapLayout layout) {
    int x, y;
    HPEN country_pen = CreatePen(PS_SOLID, 2, RGB(24, 24, 24));
    HPEN coast_pen = CreatePen(PS_SOLID, 2, RGB(34, 34, 34));
    HPEN old_pen = SelectObject(hdc, country_pen);

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int owner = world[y][x].owner;
            int px = layout.map_x + x * layout.tile_size;
            int py = layout.map_y + y * layout.tile_size;
            int s = layout.tile_size;

            if (owner < 0) continue;
            if (x == 0 || world[y][x - 1].owner != owner) { MoveToEx(hdc, px, py, NULL); LineTo(hdc, px, py + s); }
            if (x == MAP_W - 1 || world[y][x + 1].owner != owner) { MoveToEx(hdc, px + s, py, NULL); LineTo(hdc, px + s, py + s); }
            if (y == 0 || world[y - 1][x].owner != owner) { MoveToEx(hdc, px, py, NULL); LineTo(hdc, px + s, py); }
            if (y == MAP_H - 1 || world[y + 1][x].owner != owner) { MoveToEx(hdc, px, py + s, NULL); LineTo(hdc, px + s, py + s); }
        }
    }

    SelectObject(hdc, coast_pen);
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int px = layout.map_x + x * layout.tile_size;
            int py = layout.map_y + y * layout.tile_size;
            int s = layout.tile_size;

            if (!is_land(world[y][x].terrain)) continue;
            if (x == 0 || !is_land(world[y][x - 1].terrain)) { MoveToEx(hdc, px, py, NULL); LineTo(hdc, px, py + s); }
            if (x == MAP_W - 1 || !is_land(world[y][x + 1].terrain)) { MoveToEx(hdc, px + s, py, NULL); LineTo(hdc, px + s, py + s); }
            if (y == 0 || !is_land(world[y - 1][x].terrain)) { MoveToEx(hdc, px, py, NULL); LineTo(hdc, px + s, py); }
            if (y == MAP_H - 1 || !is_land(world[y + 1][x].terrain)) { MoveToEx(hdc, px, py + s, NULL); LineTo(hdc, px + s, py + s); }
        }
    }

    SelectObject(hdc, old_pen);
    DeleteObject(country_pen);
    DeleteObject(coast_pen);
}

static void draw_city_icon(HDC hdc, int cx, int cy, int size, int capital) {
    POINT roof[3];
    HBRUSH white_brush = CreateSolidBrush(capital ? RGB(255, 248, 210) : RGB(245, 245, 235));
    HBRUSH black_brush = CreateSolidBrush(RGB(20, 20, 20));
    HPEN black_pen = CreatePen(PS_SOLID, 2, RGB(20, 20, 20));
    HPEN old_pen = SelectObject(hdc, black_pen);
    HBRUSH old_brush = SelectObject(hdc, white_brush);

    Rectangle(hdc, cx - size / 2, cy, cx + size / 2, cy + size / 2);
    roof[0].x = cx - size / 2 - 2; roof[0].y = cy;
    roof[1].x = cx; roof[1].y = cy - size / 2;
    roof[2].x = cx + size / 2 + 2; roof[2].y = cy;
    SelectObject(hdc, black_brush);
    Polygon(hdc, roof, 3);
    SelectObject(hdc, white_brush);
    Rectangle(hdc, cx - size / 10, cy + size / 5, cx + size / 8, cy + size / 2);

    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    DeleteObject(white_brush);
    DeleteObject(black_brush);
    DeleteObject(black_pen);
}

static void draw_cities(HDC hdc, MapLayout layout) {
    int i;
    int s = layout.tile_size;

    for (i = 0; i < city_count; i++) {
        City *city = &cities[i];
        if (!city->alive || city->owner < 0 || city->owner >= civ_count || !civs[city->owner].alive) continue;
        draw_city_icon(hdc,
                       layout.map_x + city->x * s + s / 2,
                       layout.map_y + city->y * s + s / 2,
                       clamp(s + (city->capital ? 7 : 4), 8, 18),
                       city->capital);
    }
}

static void draw_selected_tile(HDC hdc, MapLayout layout) {
    RECT rect;
    HPEN pen;
    HPEN old_pen;
    HBRUSH old_brush;

    if (selected_x < 0 || selected_y < 0) return;
    rect.left = layout.map_x + selected_x * layout.tile_size;
    rect.top = layout.map_y + selected_y * layout.tile_size;
    rect.right = rect.left + layout.tile_size;
    rect.bottom = rect.top + layout.tile_size;

    pen = CreatePen(PS_SOLID, 3, RGB(255, 255, 255));
    old_pen = SelectObject(hdc, pen);
    old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
}

static void draw_top_bar(HDC hdc, RECT client) {
    RECT bar = {0, 0, client.right, TOP_BAR_H};
    RECT year_box = {client.right / 2 - 112, 9, client.right / 2 + 112, 50};
    char text[80];
    HFONT title_font = CreateFontA(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET,
                                   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                   DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    HFONT old_font;

    fill_rect(hdc, bar, RGB(52, 153, 216));
    fill_rect(hdc, year_box, RGB(42, 42, 42));
    snprintf(text, sizeof(text), "Year %d  Month %d", year, month);
    old_font = SelectObject(hdc, title_font);
    draw_center_text(hdc, year_box, text, RGB(199, 230, 107));
    SelectObject(hdc, old_font);
    DeleteObject(title_font);
    draw_text_line(hdc, 18, 20, "World Sim Game Ver0.1.2", RGB(245, 250, 255));
}

static int selected_tile_owner(void) {
    if (selected_x < 0 || selected_y < 0) return -1;
    return world[selected_y][selected_x].owner;
}

static void draw_side_panel(HDC hdc, RECT client) {
    int x = client.right - side_panel_w + 18;
    int y = TOP_BAR_H + 18;
    int i;
    char text[180];
    RECT panel = {client.right - side_panel_w, TOP_BAR_H, client.right, client.bottom};
    RECT divider = {client.right - side_panel_w - 3, TOP_BAR_H, client.right - side_panel_w + 3, client.bottom};
    HFONT title_font = CreateFontA(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET,
                                   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                   DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    HFONT body_font = CreateFontA(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
                                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                  DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    HFONT old_font;

    fill_rect(hdc, panel, RGB(31, 37, 43));
    fill_rect(hdc, divider, RGB(71, 82, 92));

    old_font = SelectObject(hdc, title_font);
    draw_text_line(hdc, x, y, "Civilizations", RGB(245, 245, 245));
    y += 30;
    SelectObject(hdc, body_font);

    for (i = 0; i < civ_count; i++) {
        RECT swatch = {x, y + 3, x + 15, y + 18};
        fill_rect(hdc, swatch, civs[i].color);
        snprintf(text, sizeof(text), "%c  %s", civs[i].symbol, civs[i].name);
        draw_text_line(hdc, x + 24, y, text, i == selected_civ ? RGB(255, 238, 150) : RGB(238, 238, 238));
        y += 19;
        snprintf(text, sizeof(text), "Pop %d  Land %d  A%d E%d D%d C%d",
                 civs[i].population, civs[i].territory, civs[i].aggression,
                 civs[i].expansion, civs[i].defense, civs[i].culture);
        draw_text_line(hdc, x + 24, y, text, RGB(175, 186, 196));
        y += 25;
    }

    y += 8;
    SelectObject(hdc, title_font);
    draw_text_line(hdc, x, y, "Selected Tile", RGB(245, 245, 245));
    y += 28;
    SelectObject(hdc, body_font);
    if (selected_x >= 0 && selected_y >= 0) {
        TerrainStats stats = tile_stats(selected_x, selected_y);
        int owner = selected_tile_owner();
        int city_id = city_at(selected_x, selected_y);
        int region_id = city_for_tile(selected_x, selected_y);

        snprintf(text, sizeof(text), "Position: %d, %d", selected_x, selected_y);
        draw_text_line(hdc, x, y, text, RGB(220, 225, 230)); y += 20;
        snprintf(text, sizeof(text), "Terrain: %s", terrain_name(world[selected_y][selected_x].terrain));
        draw_text_line(hdc, x, y, text, RGB(220, 225, 230)); y += 20;
        snprintf(text, sizeof(text), "Elev %d Moist %d Temp %d%s",
                 world[selected_y][selected_x].elevation,
                 world[selected_y][selected_x].moisture,
                 world[selected_y][selected_x].temperature,
                 world[selected_y][selected_x].river ? " River" : "");
        draw_text_line(hdc, x, y, text, RGB(180, 190, 198)); y += 20;
        snprintf(text, sizeof(text), "Food %d Wood %d Ore %d Water %d Live %d",
                 stats.food, stats.wood, stats.minerals, stats.water, stats.habitability);
        draw_text_line(hdc, x, y, text, RGB(180, 190, 198)); y += 20;
        snprintf(text, sizeof(text), "Temper: Atk %+d Def %+d", stats.attack, stats.defense);
        draw_text_line(hdc, x, y, text, RGB(180, 190, 198)); y += 20;
        if (owner >= 0 && owner < civ_count) snprintf(text, sizeof(text), "Owner: %s", civs[owner].name);
        else snprintf(text, sizeof(text), "Owner: None");
        draw_text_line(hdc, x, y, text, RGB(220, 225, 230)); y += 20;
        if (city_id >= 0) {
            snprintf(text, sizeof(text), "City: Pop %d Radius %d", cities[city_id].population, cities[city_id].radius);
            draw_text_line(hdc, x, y, text, RGB(220, 225, 230)); y += 20;
        }
        if (region_id >= 0) {
            RegionSummary summary = summarize_city_region(region_id);
            snprintf(text, sizeof(text), "Admin: City %d Tiles %d Pop %d", region_id + 1, summary.tiles, summary.population);
            draw_text_line(hdc, x, y, text, RGB(220, 225, 230)); y += 20;
            snprintf(text, sizeof(text), "Avg F%d Wd%d Ore%d Wa%d Live%d",
                     summary.food, summary.wood, summary.minerals, summary.water, summary.habitability);
            draw_text_line(hdc, x, y, text, RGB(180, 190, 198)); y += 20;
            snprintf(text, sizeof(text), "Avg Atk %+d Def %+d", summary.attack, summary.defense);
            draw_text_line(hdc, x, y, text, RGB(180, 190, 198));
        }
    } else {
        draw_text_line(hdc, x, y, "Click a tile on the map.", RGB(180, 190, 198));
    }

    SelectObject(hdc, title_font);
    draw_text_line(hdc, x, client.bottom - 356, "World Setup", RGB(245, 245, 245));
    SelectObject(hdc, body_font);
    draw_text_line(hdc, x, client.bottom - 328, "Land", RGB(200, 210, 218));
    draw_text_line(hdc, x + 250, client.bottom - 328, "Ocean", RGB(200, 210, 218));
    {
        RECT track = {x + 48, client.bottom - 323, x + 242, client.bottom - 315};
        int knob_x = track.left + (track.right - track.left) * ocean_slider / 100;
        RECT knob = {knob_x - 6, client.bottom - 330, knob_x + 6, client.bottom - 308};
        char setup_text[80];
        fill_rect(hdc, track, RGB(92, 104, 114));
        fill_rect(hdc, knob, RGB(232, 238, 244));
        snprintf(setup_text, sizeof(setup_text), "Initial civilizations");
        draw_text_line(hdc, x, client.bottom - 292, setup_text, RGB(200, 210, 218));
        draw_text_line(hdc, x, client.bottom - 274, "F5 rebuilds with these settings.", RGB(160, 171, 180));
    }

    SelectObject(hdc, title_font);
    draw_text_line(hdc, x, client.bottom - 262, "Civilization Form", RGB(245, 245, 245));
    SelectObject(hdc, body_font);
    draw_text_line(hdc, x, client.bottom - 235, "Name", RGB(200, 210, 218));
    draw_text_line(hdc, x + 176, client.bottom - 235, "Symbol", RGB(200, 210, 218));
    draw_text_line(hdc, x, client.bottom - 175, "Aggression", RGB(200, 210, 218));
    draw_text_line(hdc, x + 82, client.bottom - 175, "Expansion", RGB(200, 210, 218));
    draw_text_line(hdc, x + 164, client.bottom - 175, "Defense", RGB(200, 210, 218));
    draw_text_line(hdc, x + 246, client.bottom - 175, "Culture", RGB(200, 210, 218));
    draw_text_line(hdc, x, client.bottom - 64, "F1 add civ. F2 apply selected. Select empty land to choose start city.", RGB(160, 171, 180));

    SelectObject(hdc, old_font);
    DeleteObject(title_font);
    DeleteObject(body_font);
}

static void draw_bottom_bar(HDC hdc, RECT client) {
    RECT bar = {0, client.bottom - BOTTOM_BAR_H, client.right - side_panel_w, client.bottom};
    RECT play = get_play_button_rect(client);
    char text[256];
    int i;

    fill_rect(hdc, bar, RGB(30, 34, 38));
    fill_rect(hdc, play, auto_run ? RGB(68, 88, 104) : RGB(42, 58, 72));
    draw_center_text(hdc, play, auto_run ? "||" : ">", RGB(245, 248, 250));

    for (i = 0; i < 3; i++) {
        RECT button = get_speed_button_rect(client, i);
        fill_rect(hdc, button, i == speed_index ? RGB(84, 110, 132) : RGB(42, 58, 72));
        draw_center_text(hdc, button, i == 0 ? ">" : (i == 1 ? ">>" : ">>>"), RGB(235, 240, 244));
    }

    snprintf(text, sizeof(text), "Space toggles run/pause    Speed: %s (%s/month)    Wheel zoom    F1 add    F2 apply    F5 new world",
             SPEED_NAMES[speed_index], speed_seconds_text(speed_index));
    draw_text_line(hdc, 216, client.bottom - 29, text, RGB(225, 230, 235));
}

static void render_world(HDC hdc, RECT client) {
    int y, x;
    MapLayout layout = get_map_layout(client);

    fill_rect(hdc, client, RGB(79, 160, 215));
    draw_top_bar(hdc, client);

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) draw_tile(hdc, layout, x, y);
    }

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) draw_land_texture(hdc, layout, x, y);
    }

    draw_rivers(hdc, layout);
    draw_borders(hdc, layout);
    draw_cities(hdc, layout);
    draw_selected_tile(hdc, layout);
    draw_bottom_bar(hdc, client);
    draw_side_panel(hdc, client);
}

static void paint_window(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT client;
    HDC mem_dc;
    HBITMAP bitmap;
    HBITMAP old_bitmap;

    GetClientRect(hwnd, &client);
    mem_dc = CreateCompatibleDC(hdc);
    bitmap = CreateCompatibleBitmap(hdc, client.right - client.left, client.bottom - client.top);
    old_bitmap = SelectObject(mem_dc, bitmap);
    render_world(mem_dc, client);
    BitBlt(hdc, 0, 0, client.right - client.left, client.bottom - client.top, mem_dc, 0, 0, SRCCOPY);
    SelectObject(mem_dc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(mem_dc);
    EndPaint(hwnd, &ps);
}

static int read_int_control(HWND hwnd, int fallback) {
    char buffer[32];
    char *end;
    long value;

    GetWindowTextA(hwnd, buffer, sizeof(buffer));
    value = strtol(buffer, &end, 10);
    if (end == buffer) return fallback;
    return clamp((int)value, 0, 10);
}

static void write_civ_to_form(int civ_id) {
    char buffer[32];

    if (civ_id < 0 || civ_id >= civ_count) return;
    SetWindowTextA(form.name_edit, civs[civ_id].name);
    snprintf(buffer, sizeof(buffer), "%c", civs[civ_id].symbol);
    SetWindowTextA(form.symbol_edit, buffer);
    snprintf(buffer, sizeof(buffer), "%d", civs[civ_id].aggression);
    SetWindowTextA(form.aggression_edit, buffer);
    snprintf(buffer, sizeof(buffer), "%d", civs[civ_id].expansion);
    SetWindowTextA(form.expansion_edit, buffer);
    snprintf(buffer, sizeof(buffer), "%d", civs[civ_id].defense);
    SetWindowTextA(form.defense_edit, buffer);
    snprintf(buffer, sizeof(buffer), "%d", civs[civ_id].culture);
    SetWindowTextA(form.culture_edit, buffer);
}

static void add_civ_from_form(HWND hwnd) {
    char name[NAME_LEN];
    char symbol_text[8];
    char symbol;
    int x = -1;
    int y = -1;
    int before_count = civ_count;
    int added;

    GetWindowTextA(form.name_edit, name, sizeof(name));
    GetWindowTextA(form.symbol_edit, symbol_text, sizeof(symbol_text));
    if (name[0] == '\0') snprintf(name, sizeof(name), "New Realm");
    symbol = symbol_text[0] ? symbol_text[0] : (char)('A' + civ_count);

    if (selected_x >= 0 && selected_y >= 0 && is_land(world[selected_y][selected_x].terrain) && world[selected_y][selected_x].owner == -1) {
        x = selected_x;
        y = selected_y;
    }

    added = add_civilization_at(name, symbol,
                                read_int_control(form.aggression_edit, 5),
                                read_int_control(form.expansion_edit, 5),
                                read_int_control(form.defense_edit, 5),
                                read_int_control(form.culture_edit, 5),
                                x, y);
    if (added) {
        selected_civ = before_count;
        write_civ_to_form(selected_civ);
    } else {
        MessageBoxA(hwnd,
                    "Could not add civilization. The world may already have 8 civilizations, or there is no valid empty land. Select an empty land tile or rebuild with more land.",
                    "Add Civilization",
                    MB_OK | MB_ICONINFORMATION);
    }
    InvalidateRect(hwnd, NULL, FALSE);
}

static void apply_form_to_selected_civ(HWND hwnd) {
    char name[NAME_LEN];
    char symbol_text[8];
    int civ_id = selected_civ;

    if (civ_id < 0 || civ_id >= civ_count) civ_id = selected_tile_owner();
    if (civ_id < 0 || civ_id >= civ_count) return;

    GetWindowTextA(form.name_edit, name, sizeof(name));
    GetWindowTextA(form.symbol_edit, symbol_text, sizeof(symbol_text));
    if (name[0] != '\0') {
        strncpy(civs[civ_id].name, name, NAME_LEN - 1);
        civs[civ_id].name[NAME_LEN - 1] = '\0';
    }
    if (symbol_text[0] != '\0') civs[civ_id].symbol = symbol_text[0];

    civs[civ_id].aggression = read_int_control(form.aggression_edit, civs[civ_id].aggression);
    civs[civ_id].expansion = read_int_control(form.expansion_edit, civs[civ_id].expansion);
    civs[civ_id].defense = read_int_control(form.defense_edit, civs[civ_id].defense);
    civs[civ_id].culture = read_int_control(form.culture_edit, civs[civ_id].culture);
    selected_civ = civ_id;
    InvalidateRect(hwnd, NULL, FALSE);
}

static void layout_form_controls(HWND hwnd) {
    RECT client;
    int panel_x;
    int y;

    GetClientRect(hwnd, &client);
    panel_x = client.right - side_panel_w + FORM_X_PAD;
    y = client.bottom - 214;

    MoveWindow(form.initial_civs_edit, panel_x + 150, client.bottom - 298, 48, 24, TRUE);

    MoveWindow(form.name_edit, panel_x, y, 160, 24, TRUE);
    MoveWindow(form.symbol_edit, panel_x + 176, y, 50, 24, TRUE);
    y += 60;
    MoveWindow(form.aggression_edit, panel_x, y, 58, 24, TRUE);
    MoveWindow(form.expansion_edit, panel_x + 82, y, 58, 24, TRUE);
    MoveWindow(form.defense_edit, panel_x + 164, y, 58, 24, TRUE);
    MoveWindow(form.culture_edit, panel_x + 246, y, 58, 24, TRUE);
    y += 42;
    MoveWindow(form.add_button, panel_x, y, 140, 30, TRUE);
    MoveWindow(form.apply_button, panel_x + 156, y, 150, 30, TRUE);
}

static HWND create_edit(HWND parent, const char *text) {
    return CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", text,
                           WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                           0, 0, 80, 24, parent, NULL, GetModuleHandle(NULL), NULL);
}

static void create_form_controls(HWND hwnd) {
    form.name_edit = create_edit(hwnd, "New Realm");
    form.symbol_edit = create_edit(hwnd, "N");
    form.aggression_edit = create_edit(hwnd, "5");
    form.expansion_edit = create_edit(hwnd, "5");
    form.defense_edit = create_edit(hwnd, "5");
    form.culture_edit = create_edit(hwnd, "5");
    form.initial_civs_edit = create_edit(hwnd, "4");
    form.add_button = CreateWindowA("BUTTON", "Add Civilization", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                    0, 0, 140, 30, hwnd, (HMENU)ID_ADD_BUTTON, GetModuleHandle(NULL), NULL);
    form.apply_button = CreateWindowA("BUTTON", "Apply Selected", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                      0, 0, 150, 30, hwnd, (HMENU)ID_APPLY_BUTTON, GetModuleHandle(NULL), NULL);
    layout_form_controls(hwnd);
}

static int divider_hit_test(HWND hwnd, int mouse_x, int mouse_y) {
    RECT client;
    int divider_x;

    GetClientRect(hwnd, &client);
    divider_x = client.right - side_panel_w;
    return mouse_y >= TOP_BAR_H && mouse_x >= divider_x - 6 && mouse_x <= divider_x + 6;
}

static int land_slider_hit_test(HWND hwnd, int mouse_x, int mouse_y) {
    RECT client;
    int panel_x;
    RECT hit;

    GetClientRect(hwnd, &client);
    panel_x = client.right - side_panel_w + 18;
    hit.left = panel_x + 42;
    hit.right = panel_x + 248;
    hit.top = client.bottom - 336;
    hit.bottom = client.bottom - 302;
    return mouse_x >= hit.left && mouse_x <= hit.right && mouse_y >= hit.top && mouse_y <= hit.bottom;
}

static void update_land_slider(HWND hwnd, int mouse_x) {
    RECT client;
    int panel_x;
    int left;
    int right;

    GetClientRect(hwnd, &client);
    panel_x = client.right - side_panel_w + 18;
    left = panel_x + 48;
    right = panel_x + 242;
    ocean_slider = clamp((mouse_x - left) * 100 / (right - left), 0, 100);
    InvalidateRect(hwnd, NULL, FALSE);
}

static void handle_mouse_down(HWND hwnd, int mouse_x, int mouse_y) {
    RECT client;
    int i;

    GetClientRect(hwnd, &client);
    if (point_in_rect(get_play_button_rect(client), mouse_x, mouse_y)) {
        auto_run = !auto_run;
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }
    for (i = 0; i < 3; i++) {
        if (point_in_rect(get_speed_button_rect(client, i), mouse_x, mouse_y)) {
            set_speed(hwnd, i);
            InvalidateRect(hwnd, NULL, FALSE);
            return;
        }
    }

    if (divider_hit_test(hwnd, mouse_x, mouse_y)) {
        dragging_panel = 1;
        SetCapture(hwnd);
        return;
    }
    if (land_slider_hit_test(hwnd, mouse_x, mouse_y)) {
        dragging_land_slider = 1;
        update_land_slider(hwnd, mouse_x);
        SetCapture(hwnd);
        return;
    }

    select_tile_from_mouse(hwnd, mouse_x, mouse_y);
}

static void handle_mouse_move(HWND hwnd, int mouse_x, int mouse_y) {
    RECT client;

    if (dragging_panel) {
        GetClientRect(hwnd, &client);
        side_panel_w = clamp(client.right - mouse_x, MIN_SIDE_PANEL_W, MAX_SIDE_PANEL_W);
        layout_form_controls(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }

    if (dragging_land_slider) {
        update_land_slider(hwnd, mouse_x);
        return;
    }

    if (divider_hit_test(hwnd, mouse_x, mouse_y)) {
        SetCursor(LoadCursor(NULL, IDC_SIZEWE));
    }
}

static void handle_mouse_up(HWND hwnd) {
    if (dragging_panel || dragging_land_slider) {
        dragging_panel = 0;
        dragging_land_slider = 0;
        ReleaseCapture();
        InvalidateRect(hwnd, NULL, FALSE);
    }
}

static void handle_mouse_wheel(HWND hwnd, int screen_x, int screen_y, int delta) {
    RECT client;
    POINT point;
    MapLayout before;
    int tile_x_scaled;
    int tile_y_scaled;
    int old_zoom = map_zoom_percent;

    point.x = screen_x;
    point.y = screen_y;
    ScreenToClient(hwnd, &point);
    GetClientRect(hwnd, &client);
    if (point.x >= client.right - side_panel_w || point.y < TOP_BAR_H || point.y > client.bottom - BOTTOM_BAR_H) return;

    before = get_map_layout(client);
    tile_x_scaled = (point.x - before.map_x) * 1000 / before.tile_size;
    tile_y_scaled = (point.y - before.map_y) * 1000 / before.tile_size;

    if (delta > 0) map_zoom_percent = clamp(map_zoom_percent + 20, 60, 320);
    else map_zoom_percent = clamp(map_zoom_percent - 20, 60, 320);
    if (map_zoom_percent == old_zoom) return;

    {
        MapLayout after = get_map_layout(client);
        map_offset_x += point.x - (after.map_x + tile_x_scaled * after.tile_size / 1000);
        map_offset_y += point.y - (after.map_y + tile_y_scaled * after.tile_size / 1000);
    }

    InvalidateRect(hwnd, NULL, FALSE);
}

static void select_tile_from_mouse(HWND hwnd, int mouse_x, int mouse_y) {
    RECT client;
    MapLayout layout;
    int x;
    int y;
    int owner;

    GetClientRect(hwnd, &client);
    layout = get_map_layout(client);
    if (mouse_x < layout.map_x || mouse_y < layout.map_y ||
        mouse_x >= layout.map_x + layout.draw_w || mouse_y >= layout.map_y + layout.draw_h) {
        return;
    }

    x = (mouse_x - layout.map_x) / layout.tile_size;
    y = (mouse_y - layout.map_y) / layout.tile_size;
    if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) return;

    selected_x = x;
    selected_y = y;
    owner = selected_tile_owner();
    if (owner >= 0 && owner < civ_count) {
        selected_civ = owner;
        write_civ_to_form(owner);
    }
    InvalidateRect(hwnd, NULL, FALSE);
}

static void set_speed(HWND hwnd, int index) {
    speed_index = clamp(index, 0, 2);
    KillTimer(hwnd, TIMER_ID);
    SetTimer(hwnd, TIMER_ID, SPEED_MS[speed_index], NULL);
}

static int handle_shortcut(HWND hwnd, WPARAM key) {
    if (key == VK_SPACE) {
        auto_run = !auto_run;
        InvalidateRect(hwnd, NULL, FALSE);
        return 1;
    }
    if (key == '1') {
        set_speed(hwnd, 0);
        InvalidateRect(hwnd, NULL, FALSE);
        return 1;
    }
    if (key == '2') {
        set_speed(hwnd, 1);
        InvalidateRect(hwnd, NULL, FALSE);
        return 1;
    }
    if (key == '3') {
        set_speed(hwnd, 2);
        InvalidateRect(hwnd, NULL, FALSE);
        return 1;
    }
    if (key == VK_F1) {
        add_civ_from_form(hwnd);
        return 1;
    }
    if (key == VK_F2) {
        apply_form_to_selected_civ(hwnd);
        return 1;
    }
    if (key == VK_F5 || key == 'R') {
        read_world_setup_controls();
        reset_simulation();
        InvalidateRect(hwnd, NULL, FALSE);
        return 1;
    }
    if (key == VK_ESCAPE) {
        DestroyWindow(hwnd);
        return 1;
    }
    return 0;
}

static int is_game_shortcut(WPARAM key) {
    return key == VK_SPACE || key == '1' || key == '2' || key == '3' ||
           key == VK_F1 || key == VK_F2 || key == VK_F5 || key == VK_ESCAPE;
}

static int is_game_char_shortcut(WPARAM key) {
    return key == ' ' || key == '1' || key == '2' || key == '3';
}

static int handle_char_shortcut(HWND hwnd, WPARAM key) {
    if (key == ' ') return handle_shortcut(hwnd, VK_SPACE);
    if (key == '1') return handle_shortcut(hwnd, '1');
    if (key == '2') return handle_shortcut(hwnd, '2');
    if (key == '3') return handle_shortcut(hwnd, '3');
    return 0;
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_CREATE:
            create_form_controls(hwnd);
            SetTimer(hwnd, TIMER_ID, SPEED_MS[speed_index], NULL);
            return 0;

        case WM_SIZE:
            layout_form_controls(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;

        case WM_COMMAND:
            if (LOWORD(wparam) == ID_ADD_BUTTON) add_civ_from_form(hwnd);
            else if (LOWORD(wparam) == ID_APPLY_BUTTON) apply_form_to_selected_civ(hwnd);
            return 0;

        case WM_TIMER:
            if (wparam == TIMER_ID && auto_run) {
                simulate_one_month();
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;

        case WM_LBUTTONDOWN:
            handle_mouse_down(hwnd, LOWORD(lparam), HIWORD(lparam));
            return 0;

        case WM_MOUSEMOVE:
            handle_mouse_move(hwnd, LOWORD(lparam), HIWORD(lparam));
            return 0;

        case WM_LBUTTONUP:
            handle_mouse_up(hwnd);
            return 0;

        case WM_MOUSEWHEEL:
            handle_mouse_wheel(hwnd, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam), GET_WHEEL_DELTA_WPARAM(wparam));
            return 0;

        case WM_KEYDOWN:
            handle_shortcut(hwnd, wparam);
            return 0;

        case WM_PAINT:
            paint_window(hwnd);
            return 0;

        case WM_DESTROY:
            KillTimer(hwnd, TIMER_ID);
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProc(hwnd, msg, wparam, lparam);
    }
}

int run_game(void) {
    HINSTANCE instance = GetModuleHandle(NULL);
    const char *class_name = "WorldSimGameWindow";
    WNDCLASSA wc;
    HWND hwnd;
    MSG msg;

    seed_random();
    reset_simulation();

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.lpszClassName = class_name;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClassA(&wc)) {
        MessageBoxA(NULL, "Failed to register window class.", "World Sim Game", MB_ICONERROR);
        return 1;
    }

    hwnd = CreateWindowExA(0,
                           class_name,
                           "World Sim Game",
                           WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT,
                           CW_USEDEFAULT,
                           WINDOW_W,
                           WINDOW_H,
                           NULL,
                           NULL,
                           instance,
                           NULL);

    if (!hwnd) {
        MessageBoxA(NULL, "Failed to create game window.", "World Sim Game", MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && (msg.hwnd == hwnd || IsChild(hwnd, msg.hwnd))) {
            if (is_game_shortcut(msg.wParam)) {
                if (handle_shortcut(hwnd, msg.wParam)) continue;
            }
        }
        if (msg.message == WM_CHAR && (msg.hwnd == hwnd || IsChild(hwnd, msg.hwnd))) {
            if (is_game_char_shortcut(msg.wParam)) {
                if (handle_char_shortcut(hwnd, msg.wParam)) continue;
            }
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
