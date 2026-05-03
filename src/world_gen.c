#include "world_gen.h"

#include "data/game_tables.h"
#include "world/noise.h"
#include "world/ports.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

static void add_table_stats(TerrainStats *stats, TableStats delta) {
    stats->food += delta.food;
    stats->livestock += delta.livestock;
    stats->wood += delta.wood;
    stats->stone += delta.stone;
    stats->minerals += delta.minerals;
    stats->water += delta.water;
    stats->pop_capacity += delta.population;
    stats->money += delta.money;
    stats->habitability += delta.habitability;
    stats->attack += delta.attack;
    stats->defense += delta.defense;
}

static void clamp_terrain_stats(TerrainStats *stats) {
    stats->food = clamp(stats->food, 0, 10);
    stats->livestock = clamp(stats->livestock, 0, 10);
    stats->wood = clamp(stats->wood, 0, 10);
    stats->stone = clamp(stats->stone, 0, 10);
    stats->minerals = clamp(stats->minerals, 0, 10);
    stats->water = clamp(stats->water, 0, 10);
    stats->pop_capacity = clamp(stats->pop_capacity, 0, 10);
    stats->money = clamp(stats->money, 0, 10);
    stats->habitability = clamp(stats->habitability, 0, 10);
    stats->attack = clamp(stats->attack, -3, 6);
    stats->defense = clamp(stats->defense, 0, 8);
}

static TerrainStats terrain_stats_base(Geography geography, Climate climate, int river) {
    TerrainStats stats;

    memset(&stats, 0, sizeof(stats));
    if (geography >= 0 && geography < GEO_COUNT) add_table_stats(&stats, GEOGRAPHY_RULES[geography].stats);
    if (climate >= 0 && climate < CLIMATE_COUNT) add_table_stats(&stats, CLIMATE_RULES[climate].stats);

    if (river) {
        stats.food += 2;
        stats.water += 3;
        stats.money += 1;
        stats.pop_capacity += 1;
        stats.defense += 1;
    }

    clamp_terrain_stats(&stats);
    return stats;
}

static void apply_ecology_stats(TerrainStats *stats, Ecology ecology) {
    if (ecology >= 0 && ecology < ECO_COUNT) add_table_stats(stats, ECOLOGY_RULES[ecology].stats);
}

static void apply_resource_stats(TerrainStats *stats, ResourceFeature resource) {
    if (resource >= 0 && resource < RESOURCE_COUNT) add_table_stats(stats, RESOURCE_FEATURE_RULES[resource].stats);
}

const char *geography_name(Geography geography) {
    if (geography < 0 || geography >= GEO_COUNT) return ui_language == UI_LANG_ZH ? "未知" : "Unknown";
    return localized_text(GEOGRAPHY_RULES[geography].name, ui_language);
}

const char *climate_name(Climate climate) {
    if (climate < 0 || climate >= CLIMATE_COUNT) return ui_language == UI_LANG_ZH ? "未知" : "Unknown";
    return localized_text(CLIMATE_RULES[climate].name, ui_language);
}

const char *ecology_name(Ecology ecology) {
    if (ecology < 0 || ecology >= ECO_COUNT) return ui_language == UI_LANG_ZH ? "未知" : "Unknown";
    return localized_text(ECOLOGY_RULES[ecology].name, ui_language);
}

const char *resource_name(ResourceFeature resource) {
    if (resource < 0 || resource >= RESOURCE_COUNT) return ui_language == UI_LANG_ZH ? "未知" : "Unknown";
    return localized_text(RESOURCE_FEATURE_RULES[resource].name, ui_language);
}

COLORREF geography_color(Geography geography) {
    switch (geography) {
        case GEO_OCEAN: return RGB(74, 139, 201);
        case GEO_COAST: return RGB(199, 224, 201);
        case GEO_PLAIN: return RGB(189, 190, 157);
        case GEO_HILL: return RGB(140, 125, 88);
        case GEO_MOUNTAIN: return RGB(101, 92, 81);
        case GEO_PLATEAU: return RGB(154, 141, 106);
        case GEO_BASIN: return RGB(165, 166, 135);
        case GEO_CANYON: return RGB(158, 104, 74);
        case GEO_VOLCANO: return RGB(83, 71, 68);
        case GEO_LAKE: return RGB(87, 154, 207);
        case GEO_BAY: return RGB(98, 171, 215);
        case GEO_DELTA: return RGB(134, 177, 111);
        case GEO_WETLAND: return RGB(106, 154, 112);
        case GEO_OASIS: return RGB(118, 178, 118);
        case GEO_ISLAND: return RGB(154, 184, 112);
        case GEO_COUNT: return RGB(0, 0, 0);
        default: return RGB(0, 0, 0);
    }
}

COLORREF climate_color(Climate climate) {
    switch (climate) {
        case CLIMATE_TROPICAL_RAINFOREST: return RGB(30, 126, 52);
        case CLIMATE_TROPICAL_MONSOON: return RGB(74, 164, 62);
        case CLIMATE_TROPICAL_SAVANNA: return RGB(170, 188, 75);
        case CLIMATE_DESERT: return RGB(224, 174, 74);
        case CLIMATE_SEMI_ARID: return RGB(194, 176, 94);
        case CLIMATE_MEDITERRANEAN: return RGB(151, 183, 93);
        case CLIMATE_OCEANIC: return RGB(83, 150, 93);
        case CLIMATE_TEMPERATE_MONSOON: return RGB(96, 170, 83);
        case CLIMATE_CONTINENTAL: return RGB(149, 176, 96);
        case CLIMATE_SUBARCTIC: return RGB(87, 137, 103);
        case CLIMATE_TUNDRA: return RGB(178, 185, 153);
        case CLIMATE_ICE_CAP: return RGB(233, 238, 233);
        case CLIMATE_ALPINE: return RGB(172, 168, 150);
        case CLIMATE_HIGHLAND_PLATEAU: return RGB(164, 154, 120);
        case CLIMATE_COUNT: return RGB(0, 0, 0);
        default: return RGB(0, 0, 0);
    }
}

COLORREF overview_color(int x, int y) {
    COLORREF base = geography_color(world[y][x].geography);
    COLORREF climate = climate_color(world[y][x].climate);
    int blend = is_land(world[y][x].geography) ? 48 : 18;
    COLORREF color = blend_color(base, climate, blend);
    int elev = world[y][x].elevation;

    if (!is_land(world[y][x].geography)) return color;
    if (elev > 55) return blend_color(color, RGB(38, 35, 32), clamp((elev - 55) / 3, 0, 18));
    return blend_color(color, RGB(236, 230, 198), clamp((55 - elev) / 4, 0, 12));
}

int is_land(Geography geography) {
    return geography != GEO_OCEAN && geography != GEO_LAKE && geography != GEO_BAY;
}

static int is_sea_water(Geography geography) {
    return geography == GEO_OCEAN || geography == GEO_BAY;
}

static int sea_water_near_land(int x, int y, int radius) {
    int dy;
    int dx;

    for (dy = -radius; dy <= radius; dy++) {
        for (dx = -radius; dx <= radius; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            if (dx == 0 && dy == 0) continue;
            if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
            if (is_sea_water(world[ny][nx].geography)) return 1;
        }
    }
    return 0;
}

static int is_coastal_land_tile(int x, int y) {
    if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) return 0;
    return is_land(world[y][x].geography) && sea_water_near_land(x, y, 2);
}

TerrainStats tile_stats(int x, int y) {
    TerrainStats stats = terrain_stats_base(world[y][x].geography, world[y][x].climate, world[y][x].river);
    int variation = world[y][x].resource_variation - 50;
    int base_habitability = stats.habitability;
    int resource_habitability;

    stats.food = clamp(stats.food + variation / 18, 0, 10);
    stats.livestock = clamp(stats.livestock + variation / 22 + (world[y][x].moisture - 45) / 30, 0, 10);
    stats.wood = clamp(stats.wood + (world[y][x].moisture - 50) / 24, 0, 10);
    stats.stone = clamp(stats.stone + (world[y][x].elevation - 48) / 22, 0, 10);
    stats.minerals = clamp(stats.minerals + (world[y][x].elevation - 55) / 18, 0, 10);
    stats.water = clamp(stats.water + (world[y][x].moisture - 45) / 18, 0, 10);
    stats.pop_capacity = clamp(stats.pop_capacity + stats.food / 3 + stats.water / 4, 0, 10);
    stats.money = clamp(stats.money + stats.livestock / 5 + stats.minerals / 5 + stats.water / 6, 0, 10);
    apply_ecology_stats(&stats, world[y][x].ecology);
    apply_resource_stats(&stats, world[y][x].resource);
    clamp_terrain_stats(&stats);
    resource_habitability = (stats.food * 2 + stats.livestock + stats.wood + stats.stone / 2 +
                             stats.minerals / 2 + stats.water * 2 + stats.pop_capacity) / 7;
    stats.habitability = clamp((base_habitability + resource_habitability) / 2 +
                               variation / 24 - abs(world[y][x].temperature - 55) / 35, 0, 10);
    if (world[y][x].river) stats.defense = clamp(stats.defense + 1, 0, 8);
    clamp_terrain_stats(&stats);
    return stats;
}

static int nearby_land_count(int x, int y) {
    int count = 0;
    int dy;
    int dx;

    for (dy = -1; dy <= 1; dy++) {
        for (dx = -1; dx <= 1; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            if (dx == 0 && dy == 0) continue;
            if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
            if (is_land(world[ny][nx].geography)) count++;
        }
    }
    return count;
}

static int tile_cost(int x, int y) {
    TerrainStats stats = tile_stats(x, y);
    int resource_relief;
    if (!is_land(world[y][x].geography)) return 99;
    resource_relief = stats.food + stats.water + stats.wood + stats.stone + stats.minerals + stats.money;
    return clamp(12 - stats.habitability / 2 - stats.water / 4 - stats.food / 5 -
                 resource_relief / 18 + stats.defense / 3, 2, 12);
}

static int terrain_resource_value(TerrainStats stats) {
    return stats.food * 4 + stats.water * 4 + stats.pop_capacity * 3 + stats.money * 3 +
           stats.livestock * 2 + stats.wood * 2 + stats.stone * 2 + stats.minerals * 3 +
           stats.habitability * 2;
}

int world_tile_cost(int x, int y) {
    return tile_cost(x, y);
}

int world_terrain_resource_value(TerrainStats stats) {
    return terrain_resource_value(stats);
}

int world_is_coastal_land_tile(int x, int y) {
    return is_coastal_land_tile(x, y);
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
        static int next[MAP_H][MAP_W];
        int y;
        int x;

        for (y = 0; y < MAP_H; y++) {
            for (x = 0; x < MAP_W; x++) {
                int total = 0;
                int count = 0;
                int dy;
                int dx;

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

static int latitude_coldness(int x, int y, int axis) {
    int pos;
    int max_pos;
    int edge_dist;

    switch (axis) {
        case 1:
            pos = x;
            max_pos = MAP_W - 1;
            break;
        case 2:
            pos = x * MAP_H / MAP_W + y;
            max_pos = (MAP_W - 1) * MAP_H / MAP_W + MAP_H - 1;
            break;
        case 3:
            pos = (MAP_W - 1 - x) * MAP_H / MAP_W + y;
            max_pos = (MAP_W - 1) * MAP_H / MAP_W + MAP_H - 1;
            break;
        case 0:
        default:
            pos = y;
            max_pos = MAP_H - 1;
            break;
    }

    edge_dist = pos < max_pos - pos ? pos : max_pos - pos;
    return clamp(100 - edge_dist * 200 / (max_pos > 0 ? max_pos : 1), 0, 100);
}

static void compute_ocean_distance(int elevation[MAP_H][MAP_W], int sea_level,
                                   int distance[MAP_H][MAP_W]) {
    static int queue_x[MAP_W * MAP_H];
    static int queue_y[MAP_W * MAP_H];
    int head = 0;
    int tail = 0;
    int x;
    int y;

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            if (ocean_slider > 0 && elevation[y][x] < sea_level) {
                distance[y][x] = 0;
                queue_x[tail] = x;
                queue_y[tail] = y;
                tail++;
            } else {
                distance[y][x] = 9999;
            }
        }
    }

    if (tail == 0) {
        for (y = 0; y < MAP_H; y++) {
            for (x = 0; x < MAP_W; x++) distance[y][x] = 28;
        }
        return;
    }

    while (head < tail) {
        static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        int cx = queue_x[head];
        int cy = queue_y[head];
        int i;
        head++;

        for (i = 0; i < 4; i++) {
            int nx = cx + dirs[i][0];
            int ny = cy + dirs[i][1];
            if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
            if (distance[ny][nx] <= distance[cy][cx] + 1) continue;
            distance[ny][nx] = distance[cy][cx] + 1;
            queue_x[tail] = nx;
            queue_y[tail] = ny;
            tail++;
        }
    }
}

static void compute_rain_shadow(int elevation[MAP_H][MAP_W], int sea_level,
                                int wind_x, int wind_y, int shadow[MAP_H][MAP_W]) {
    int x;
    int y;

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int block = 0;
            int step;

            if (elevation[y][x] < sea_level || ocean_slider <= 0) {
                shadow[y][x] = 0;
                continue;
            }

            for (step = 1; step <= 34; step++) {
                int sx = x - wind_x * step;
                int sy = y - wind_y * step;

                if (sx < 0 || sx >= MAP_W || sy < 0 || sy >= MAP_H) break;
                if (elevation[sy][sx] < sea_level) {
                    block = block > 5 ? block - 5 : 0;
                    break;
                }
                if (elevation[sy][sx] > sea_level + 42) block += 5;
                else if (elevation[sy][sx] > sea_level + 30) block += 3;
                else if (elevation[sy][sx] > sea_level + 20) block += 1;
            }
            shadow[y][x] = clamp(block, 0, 38);
        }
    }
}

static Climate majority_neighbor_climate(int x, int y, Climate fallback) {
    int counts[CLIMATE_COUNT] = {0};
    int best = fallback;
    int best_count = -1;
    int dy;
    int dx;

    for (dy = -1; dy <= 1; dy++) {
        for (dx = -1; dx <= 1; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            Climate climate;
            if (dx == 0 && dy == 0) continue;
            if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
            if (!is_land(world[ny][nx].geography)) continue;
            climate = world[ny][nx].climate;
            counts[climate]++;
        }
    }

    for (dx = 0; dx < CLIMATE_COUNT; dx++) {
        if (counts[dx] > best_count) {
            best = (Climate)dx;
            best_count = counts[dx];
        }
    }
    return best;
}

static void mark_river_from(int start_x, int start_y) {
    int x = start_x;
    int y = start_y;
    int steps;

    for (steps = 0; steps < 180; steps++) {
        int best_x = x;
        int best_y = y;
        int best_score = world[y][x].elevation;
        int dy;
        int dx;

        if (!is_land(world[y][x].geography)) return;
        world[y][x].river = 1;

        for (dy = -1; dy <= 1; dy++) {
            for (dx = -1; dx <= 1; dx++) {
                int nx = x + dx;
                int ny = y + dy;
                int score;

                if (dx == 0 && dy == 0) continue;
                if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
                if (!is_land(world[ny][nx].geography)) return;
                score = world[ny][nx].elevation + rnd(5);
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

static Geography classify_geography(int elevation, int sea_level, int nearby_water, int relief, int moisture,
                                    int temperature, Climate climate) {
    int coast_band = 1 + ocean_slider * 5 / 100;
    int coast_water_limit = ocean_slider < 15 ? 7 : 4;
    int mountain_bias = (relief_slider + bias_mountain_slider) / 2;
    int wet_bias = (moisture_slider + bias_wetland_slider) / 2;

    (void)temperature;
    if (ocean_slider <= 0) {
        if (elevation > 88 && rnd(1000) < 4 + mountain_bias / 4) return GEO_VOLCANO;
        if (elevation > 82 && relief < 9) return GEO_PLATEAU;
        if (elevation > 84 - mountain_bias / 12) return GEO_MOUNTAIN;
        if (elevation > 66) return GEO_HILL;
        if (relief > 28 && elevation < 62) return GEO_CANYON;
        if (elevation < sea_level + 14) return GEO_BASIN;
        return GEO_PLAIN;
    }
    if (elevation < sea_level) {
        if (nearby_water >= 5 && elevation > sea_level - 5) return GEO_BAY;
        return GEO_OCEAN;
    }
    if (nearby_water >= 5 && elevation < sea_level + 10) {
        if (moisture > 74 && (climate == CLIMATE_TROPICAL_MONSOON || climate == CLIMATE_TEMPERATE_MONSOON)) return GEO_DELTA;
        return GEO_ISLAND;
    }
    if (elevation < sea_level + coast_band || nearby_water >= coast_water_limit) {
        return GEO_COAST;
    }
    if ((climate == CLIMATE_DESERT || climate == CLIMATE_SEMI_ARID) && moisture > 50 &&
        elevation < sea_level + 22 && rnd(100) < 14) return GEO_OASIS;
    if (moisture > 86 - wet_bias / 5 && elevation < sea_level + 18) return GEO_WETLAND;
    if (moisture > 82 - wet_bias / 6 && elevation < sea_level + 12 && rnd(100) < 9) return GEO_LAKE;
    if (elevation > 88 && rnd(1000) < 4 + mountain_bias / 4) return GEO_VOLCANO;
    if (elevation > 82 && relief < 9) return GEO_PLATEAU;
    if (elevation > 84 - mountain_bias / 12) return GEO_MOUNTAIN;
    if (relief > 32 - mountain_bias / 8 && elevation < 65) return GEO_CANYON;
    if (elevation > 66) return GEO_HILL;
    if (elevation < sea_level + 12) return GEO_BASIN;
    return GEO_PLAIN;
}

static Ecology classify_ecology(Geography geography, Climate climate, int moisture, int temperature) {
    int forest_bias = (vegetation_slider - 50) / 2 + (bias_forest_slider - 50) / 3;
    int wetland_bias = (moisture_slider + bias_wetland_slider) / 8;

    if (!is_land(geography)) return ECO_NONE;
    if (geography == GEO_DELTA && temperature > 65) return ECO_MANGROVE;
    if (geography == GEO_WETLAND || geography == GEO_BASIN) {
        if (moisture + wetland_bias > 82) return ECO_SWAMP;
    }
    if (geography == GEO_OASIS) return ECO_GRASSLAND;

    switch (climate) {
        case CLIMATE_TROPICAL_RAINFOREST:
            if (moisture + forest_bias > 82 && temperature > 72) return ECO_RAINFOREST;
            return ECO_FOREST;
        case CLIMATE_TROPICAL_MONSOON:
            if (moisture > 72 && rnd(100) < 24) return ECO_BAMBOO;
            if (moisture + forest_bias > 64) return ECO_FOREST;
            return ECO_GRASSLAND;
        case CLIMATE_TROPICAL_SAVANNA:
        case CLIMATE_SEMI_ARID:
            return ECO_GRASSLAND;
        case CLIMATE_DESERT:
            return ECO_DESERT;
        case CLIMATE_MEDITERRANEAN:
        case CLIMATE_CONTINENTAL:
            if (moisture + forest_bias > 64) return ECO_FOREST;
            return ECO_GRASSLAND;
        case CLIMATE_OCEANIC:
        case CLIMATE_TEMPERATE_MONSOON:
            if (moisture + forest_bias > 72) return ECO_FOREST;
            return ECO_GRASSLAND;
        case CLIMATE_SUBARCTIC:
            if (moisture + forest_bias > 48) return ECO_FOREST;
            return ECO_TUNDRA;
        case CLIMATE_TUNDRA:
        case CLIMATE_ICE_CAP:
        case CLIMATE_ALPINE:
            return ECO_TUNDRA;
        case CLIMATE_HIGHLAND_PLATEAU:
            return moisture > 58 ? ECO_GRASSLAND : ECO_TUNDRA;
        case CLIMATE_COUNT:
            break;
    }
    return ECO_NONE;
}

static ResourceFeature classify_resource(Geography geography, Climate climate, Ecology ecology,
                                         int moisture, int temperature, int elevation, int variation) {
    if (!is_land(geography)) {
        if (geography == GEO_BAY || geography == GEO_LAKE) {
            return RESOURCE_FISHERY;
        }
        return RESOURCE_NONE;
    }
    if ((geography == GEO_MOUNTAIN || geography == GEO_HILL || geography == GEO_VOLCANO ||
         geography == GEO_CANYON || elevation > 78) && variation > 28) {
        return RESOURCE_MINE;
    }
    if (geography == GEO_VOLCANO && variation > 60) return RESOURCE_GEOTHERMAL;
    if ((geography == GEO_LAKE || geography == GEO_BASIN) &&
        (climate == CLIMATE_DESERT || climate == CLIMATE_SEMI_ARID) && variation > 50) {
        return RESOURCE_SALT_LAKE;
    }
    if ((geography == GEO_PLAIN || geography == GEO_DELTA || geography == GEO_BASIN ||
         geography == GEO_COAST || geography == GEO_OASIS) &&
        moisture > 42 && climate != CLIMATE_DESERT && climate != CLIMATE_ICE_CAP &&
        climate != CLIMATE_TUNDRA && variation > 18) {
        return RESOURCE_FARMLAND;
    }
    if ((geography == GEO_COAST || geography == GEO_ISLAND) && variation > 42) return RESOURCE_FISHERY;
    if (ecology == ECO_GRASSLAND || climate == CLIMATE_SEMI_ARID ||
        climate == CLIMATE_HIGHLAND_PLATEAU) {
        return RESOURCE_PASTURE;
    }
    if (ecology == ECO_FOREST || ecology == ECO_RAINFOREST || ecology == ECO_BAMBOO ||
        ecology == ECO_MANGROVE) {
        return RESOURCE_FOREST;
    }
    if (temperature > 68 && moisture > 76 && geography == GEO_DELTA) return RESOURCE_FISHERY;
    return RESOURCE_NONE;
}

static Climate classify_climate(int cold, int elevation, int moisture, int temperature, int near_ocean) {
    int desert_limit = 6 + (drought_slider + bias_desert_slider) * 36 / 100;
    int semi_arid_limit = desert_limit + 16;

    if (elevation > 84 && temperature < 48) return CLIMATE_ALPINE;
    if (elevation > 72 && temperature < 68) return CLIMATE_HIGHLAND_PLATEAU;
    if (cold > 98 || temperature < 12) return CLIMATE_ICE_CAP;
    if (cold > 86 || temperature < 25) return CLIMATE_TUNDRA;
    if (cold > 74 || temperature < 36) return CLIMATE_SUBARCTIC;
    if (moisture < desert_limit && temperature > 32) return CLIMATE_DESERT;
    if (moisture < semi_arid_limit && temperature > 28) return CLIMATE_SEMI_ARID;
    if (temperature > 72 && moisture > 78) return CLIMATE_TROPICAL_RAINFOREST;
    if (temperature > 68 && moisture > 58) return CLIMATE_TROPICAL_MONSOON;
    if (temperature > 64) return CLIMATE_TROPICAL_SAVANNA;
    if (near_ocean && temperature > 47 && moisture > 38 && moisture < 70) return CLIMATE_MEDITERRANEAN;
    if (near_ocean && moisture > 52) return CLIMATE_OCEANIC;
    if (moisture > 66 && temperature > 42) return CLIMATE_TEMPERATE_MONSOON;
    return CLIMATE_CONTINENTAL;
}

void generate_world(void) {
    int y;
    int x;
    int pass;
    static int elevation[MAP_H][MAP_W];
    static int moisture[MAP_H][MAP_W];
    static int temperature[MAP_H][MAP_W];
    static int sorted_elevation[MAP_W * MAP_H];
    static int ocean_distance[MAP_H][MAP_W];
    static int rain_shadow[MAP_H][MAP_W];
    int target_land_percent = 100 - ocean_slider * 72 / 100;
    int target_land_tiles = MAP_W * MAP_H * target_land_percent / 100;
    int threshold_index = clamp(MAP_W * MAP_H - target_land_tiles, 0, MAP_W * MAP_H - 1);
    int sea_level;
    int index = 0;
    int climate_axis;
    int wind_x;
    int wind_y;
    int elevation_seed;
    int moisture_seed;
    int temperature_seed;

    seed_random();
    climate_axis = rnd(4);
    wind_x = rnd(2) ? 1 : -1;
    wind_y = rnd(3) - 1;
    elevation_seed = rnd(1000000);
    moisture_seed = rnd(1000000);
    temperature_seed = rnd(1000000);
    if (wind_y == 0 && rnd(2)) wind_y = rnd(2) ? 1 : -1;

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int edge_x = x < MAP_W / 2 ? x : MAP_W - 1 - x;
            int edge_y = y < MAP_H / 2 ? y : MAP_H - 1 - y;
            int edge = edge_x < edge_y ? edge_x : edge_y;
            int edge_falloff = clamp(edge * 3, 0, 42);
            int dry_shift = drought_slider * 16 / 100 + bias_desert_slider / 6;
            int base_elevation = world_fractal_noise(x, y, elevation_seed);
            int broken_elevation = world_fractal_noise(x * 2 + elevation_seed % 97, y * 2 + elevation_seed % 53,
                                                       elevation_seed + 911);
            int fragment_weight = clamp(continent_slider, 0, 100);

            world[y][x].geography = GEO_OCEAN;
            world[y][x].climate = CLIMATE_OCEANIC;
            world[y][x].ecology = ECO_NONE;
            world[y][x].resource = RESOURCE_NONE;
            world[y][x].owner = -1;
            world[y][x].province_id = -1;
            world[y][x].river = 0;
            elevation[y][x] = (base_elevation * (100 - fragment_weight) + broken_elevation * fragment_weight) / 100 +
                              edge_falloff - 22 + (relief_slider - 50) / 5;
            moisture[y][x] = clamp(world_fractal_noise(x, y, moisture_seed) + moisture_slider / 3 - dry_shift, 0, 100);
            temperature[y][x] = world_fractal_noise(x, y, temperature_seed);
        }
    }

    smooth_field(elevation, clamp(7 - continent_slider / 20, 2, 7));
    smooth_field(moisture, clamp(5 - continent_slider / 35, 2, 5));
    smooth_field(temperature, 3);

    for (pass = 0; pass < 18 + relief_slider / 3 + bias_mountain_slider / 5; pass++) {
        int cx = 24 + rnd(MAP_W - 48);
        int cy = 18 + rnd(MAP_H - 36);
        int radius = 12 + rnd(28 + clamp(100 - continent_slider, 0, 100) / 2);
        int lift = 6 + relief_slider * 24 / 100 + bias_mountain_slider / 5 + rnd(18);
        int yy;
        int xx;

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
    smooth_field(elevation, clamp(6 - continent_slider / 25, 2, 6));

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) sorted_elevation[index++] = elevation[y][x];
    }
    qsort(sorted_elevation, MAP_W * MAP_H, sizeof(sorted_elevation[0]), compare_ints);
    sea_level = sorted_elevation[threshold_index];
    compute_ocean_distance(elevation, sea_level, ocean_distance);
    compute_rain_shadow(elevation, sea_level, wind_x, wind_y, rain_shadow);

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int elev = clamp(elevation[y][x], 0, 100);
            int coast_humidity = ocean_slider <= 0 ? 0 : clamp(42 - ocean_distance[y][x], 0, 42);
            int inland_dryness = ocean_slider <= 0 ? 0 : clamp(ocean_distance[y][x] / 2, 0, 34);
            int shadow_dryness = rain_shadow[y][x];
            int moist = clamp(moisture[y][x] + coast_humidity - inland_dryness - shadow_dryness +
                              (elev < sea_level + 8 && ocean_slider > 0 ? 12 : 0), 0, 100);
            int latitude_cold = latitude_coldness(x, y, climate_axis);
            int cold = clamp(latitude_cold + elev / 4 + shadow_dryness / 5 + rnd(17) - 8, 0, 120);
            int ocean_moderation = ocean_slider <= 0 ? 0 : clamp(22 - ocean_distance[y][x] / 2, 0, 22);
            int temp = clamp(100 - cold + temperature[y][x] / 8 - elev / 7 + ocean_moderation / 2, 0, 100);
            int nearby_water = 0;
            int local_min = 999;
            int local_max = -999;
            int dy;
            int dx;

            for (dy = -1; dy <= 1; dy++) {
                for (dx = -1; dx <= 1; dx++) {
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
                    if (elevation[ny][nx] < sea_level) nearby_water++;
                    if (elevation[ny][nx] < local_min) local_min = elevation[ny][nx];
                    if (elevation[ny][nx] > local_max) local_max = elevation[ny][nx];
                }
            }

            world[y][x].elevation = elev;
            world[y][x].moisture = moist;
            world[y][x].temperature = temp;
            world[y][x].resource_variation = rnd(101);
            world[y][x].climate = classify_climate(cold, elev, moist, temp, ocean_distance[y][x] <= 18);
            world[y][x].geography = classify_geography(elev, sea_level, nearby_water, local_max - local_min,
                                                       moist, temp, world[y][x].climate);
        }
    }

    for (pass = 0; pass < (ocean_slider < 15 ? 5 : 2); pass++) {
        static Tile next[MAP_H][MAP_W];
        memcpy(next, world, sizeof(world));
        for (y = 1; y < MAP_H - 1; y++) {
            for (x = 1; x < MAP_W - 1; x++) {
                int land = nearby_land_count(x, y);
                if (is_land(world[y][x].geography) && land <= 2 &&
                    world[y][x].geography != GEO_ISLAND) {
                    next[y][x].geography = ocean_slider <= 0 ? GEO_PLAIN : GEO_OCEAN;
                }
                if (!is_land(world[y][x].geography) && land >= (ocean_slider < 15 ? 4 : 6)) next[y][x].geography = GEO_PLAIN;
            }
        }
        memcpy(world, next, sizeof(world));
    }

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            if (is_land(world[y][x].geography)) {
                world[y][x].climate = majority_neighbor_climate(x, y, world[y][x].climate);
            } else {
                world[y][x].climate = CLIMATE_OCEANIC;
            }
            world[y][x].ecology = classify_ecology(world[y][x].geography, world[y][x].climate,
                                                   world[y][x].moisture, world[y][x].temperature);
            world[y][x].resource = classify_resource(world[y][x].geography, world[y][x].climate,
                                                     world[y][x].ecology, world[y][x].moisture,
                                                     world[y][x].temperature, world[y][x].elevation,
                                                     world[y][x].resource_variation);
        }
    }

    for (pass = 0; pass < 72 + (moisture_slider + bias_wetland_slider) / 2; pass++) {
        int tries;
        for (tries = 0; tries < 250; tries++) {
            int rx = rnd(MAP_W);
            int ry = rnd(MAP_H);
            if ((world[ry][rx].geography == GEO_MOUNTAIN || world[ry][rx].geography == GEO_HILL ||
                 world[ry][rx].geography == GEO_PLATEAU) &&
                world[ry][rx].elevation > 52 && world[ry][rx].moisture > 28 + rnd(28)) {
                mark_river_from(rx, ry);
                break;
            }
        }
    }

    ports_reset_regions();
}
