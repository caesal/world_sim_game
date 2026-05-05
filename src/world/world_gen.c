#include "world/world_gen.h"

#include "data/game_tables.h"
#include "world/noise.h"
#include "world/rivers.h"
#include "world/terrain_query.h"
#include "world/world_smoothing.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

const WorldGenConfig DEFAULT_WORLD_GEN_CONFIG = {WORLD_GEN_DEFAULT_OCEAN, WORLD_GEN_DEFAULT_CONTINENT, WORLD_GEN_DEFAULT_RELIEF, WORLD_GEN_DEFAULT_MOISTURE, WORLD_GEN_DEFAULT_DROUGHT, WORLD_GEN_DEFAULT_VEGETATION, WORLD_GEN_DEFAULT_BIAS_FOREST, WORLD_GEN_DEFAULT_BIAS_DESERT, WORLD_GEN_DEFAULT_BIAS_MOUNTAIN, WORLD_GEN_DEFAULT_BIAS_WETLAND};
static WorldGenConfig gen_config;
#define COPY_ACTIVE_TILES(dst, src) do { int _row; for (_row = 0; _row < MAP_H; _row++) memcpy((dst)[_row], (src)[_row], (size_t)MAP_W * sizeof((dst)[_row][0])); } while (0)

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

static void smooth_field(int field[MAX_MAP_H][MAX_MAP_W], int passes) {
    int pass;

    for (pass = 0; pass < passes; pass++) {
        static int next[MAX_MAP_H][MAX_MAP_W];
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

static void compute_ocean_distance(int elevation[MAX_MAP_H][MAX_MAP_W], int sea_level,
                                   int distance[MAX_MAP_H][MAX_MAP_W]) {
    static int queue_x[MAX_MAP_W * MAX_MAP_H];
    static int queue_y[MAX_MAP_W * MAX_MAP_H];
    int head = 0;
    int tail = 0;
    int x;
    int y;

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            if (gen_config.ocean > 0 && elevation[y][x] < sea_level) {
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

static void compute_rain_shadow(int elevation[MAX_MAP_H][MAX_MAP_W], int sea_level,
                                int wind_x, int wind_y, int shadow[MAX_MAP_H][MAX_MAP_W]) {
    int x;
    int y;

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int block = 0;
            int step;

            if (elevation[y][x] < sea_level || gen_config.ocean <= 0) {
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

static Geography classify_geography(int elevation, int sea_level, int nearby_water, int relief, int moisture,
                                     int temperature, Climate climate) {
    int coast_band = 1 + gen_config.ocean * 5 / 100;
    int coast_water_limit = gen_config.ocean < 15 ? 7 : 4;
    int mountain_bias = (gen_config.relief + gen_config.bias_mountain) / 2;
    int wet_bias = (gen_config.moisture + gen_config.bias_wetland) / 2;

    (void)temperature;
    if (gen_config.ocean <= 0) {
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
    int forest_bias = (gen_config.vegetation - 50) / 2 + (gen_config.bias_forest - 50) / 3;
    int wetland_bias = (gen_config.moisture + gen_config.bias_wetland) / 8;

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
            return RESOURCE_FEATURE_FISHERY;
        }
        return RESOURCE_FEATURE_NONE;
    }
    if ((geography == GEO_MOUNTAIN || geography == GEO_HILL || geography == GEO_VOLCANO ||
         geography == GEO_CANYON || elevation > 78) && variation > 28) {
        return RESOURCE_FEATURE_MINE;
    }
    if (geography == GEO_VOLCANO && variation > 60) return RESOURCE_FEATURE_GEOTHERMAL;
    if ((geography == GEO_LAKE || geography == GEO_BASIN) &&
        (climate == CLIMATE_DESERT || climate == CLIMATE_SEMI_ARID) && variation > 50) {
        return RESOURCE_FEATURE_SALT_LAKE;
    }
    if ((geography == GEO_PLAIN || geography == GEO_DELTA || geography == GEO_BASIN ||
         geography == GEO_COAST || geography == GEO_OASIS) &&
        moisture > 42 && climate != CLIMATE_DESERT && climate != CLIMATE_ICE_CAP &&
        climate != CLIMATE_TUNDRA && variation > 18) {
        return RESOURCE_FEATURE_FARMLAND;
    }
    if ((geography == GEO_COAST || geography == GEO_ISLAND) && variation > 42) return RESOURCE_FEATURE_FISHERY;
    if (ecology == ECO_GRASSLAND || climate == CLIMATE_SEMI_ARID ||
        climate == CLIMATE_HIGHLAND_PLATEAU) {
        return RESOURCE_FEATURE_PASTURE;
    }
    if (ecology == ECO_FOREST || ecology == ECO_RAINFOREST || ecology == ECO_BAMBOO ||
        ecology == ECO_MANGROVE) {
        return RESOURCE_FEATURE_FOREST;
    }
    if (temperature > 68 && moisture > 76 && geography == GEO_DELTA) return RESOURCE_FEATURE_FISHERY;
    return RESOURCE_FEATURE_NONE;
}

static Climate classify_climate(int cold, int elevation, int moisture, int temperature, int near_ocean) {
    int desert_limit = 6 + (gen_config.drought + gen_config.bias_desert) * 36 / 100;
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

void generate_world_with_config(const WorldGenConfig *config) {
    int y;
    int x;
    int pass;
    static int elevation[MAX_MAP_H][MAX_MAP_W];
    static int moisture[MAX_MAP_H][MAX_MAP_W];
    static int temperature[MAX_MAP_H][MAX_MAP_W];
    static int sorted_elevation[MAX_MAP_W * MAX_MAP_H];
    static int ocean_distance[MAX_MAP_H][MAX_MAP_W];
    static int rain_shadow[MAX_MAP_H][MAX_MAP_W];
    int target_land_percent;
    int target_land_tiles;
    int threshold_index;
    int sea_level;
    int index = 0;
    int climate_axis;
    int wind_x;
    int wind_y;
    int elevation_seed;
    int moisture_seed;
    int temperature_seed;

    gen_config = config ? *config : DEFAULT_WORLD_GEN_CONFIG;
    target_land_percent = 100 - gen_config.ocean * 72 / 100;
    target_land_tiles = MAP_W * MAP_H * target_land_percent / 100;
    threshold_index = clamp(MAP_W * MAP_H - target_land_tiles, 0, MAP_W * MAP_H - 1);

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
            int dry_shift = gen_config.drought * 16 / 100 + gen_config.bias_desert / 6;
            int base_elevation = world_fractal_noise(x, y, elevation_seed);
            int broken_elevation = world_fractal_noise(x * 2 + elevation_seed % 97, y * 2 + elevation_seed % 53,
                                                       elevation_seed + 911);
            int fragment_weight = clamp(gen_config.continent, 0, 100);

            world[y][x].geography = GEO_OCEAN;
            world[y][x].climate = CLIMATE_OCEANIC;
            world[y][x].ecology = ECO_NONE;
            world[y][x].resource = RESOURCE_FEATURE_NONE;
            world[y][x].owner = -1;
            world[y][x].province_id = -1;
            world[y][x].river = 0;
            elevation[y][x] = (base_elevation * (100 - fragment_weight) + broken_elevation * fragment_weight) / 100 +
                              edge_falloff - 22 + (gen_config.relief - 50) / 5;
            moisture[y][x] = clamp(world_fractal_noise(x, y, moisture_seed) + gen_config.moisture / 3 - dry_shift, 0, 100);
            temperature[y][x] = world_fractal_noise(x, y, temperature_seed);
        }
    }

    smooth_field(elevation, clamp(7 - gen_config.continent / 20, 2, 7));
    smooth_field(moisture, clamp(5 - gen_config.continent / 35, 2, 5));
    smooth_field(temperature, 3);

    for (pass = 0; pass < 18 + gen_config.relief / 3 + gen_config.bias_mountain / 5; pass++) {
        int cx = 24 + rnd(MAP_W - 48);
        int cy = 18 + rnd(MAP_H - 36);
        int radius = 12 + rnd(28 + clamp(100 - gen_config.continent, 0, 100) / 2);
        int lift = 6 + gen_config.relief * 24 / 100 + gen_config.bias_mountain / 5 + rnd(18);
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
    smooth_field(elevation, clamp(6 - gen_config.continent / 25, 2, 6));

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
            int coast_humidity = gen_config.ocean <= 0 ? 0 : clamp(42 - ocean_distance[y][x], 0, 42);
            int inland_dryness = gen_config.ocean <= 0 ? 0 : clamp(ocean_distance[y][x] / 2, 0, 34);
            int shadow_dryness = rain_shadow[y][x];
            int moist = clamp(moisture[y][x] + coast_humidity - inland_dryness - shadow_dryness +
                              (elev < sea_level + 8 && gen_config.ocean > 0 ? 12 : 0), 0, 100);
            int latitude_cold = latitude_coldness(x, y, climate_axis);
            int cold = clamp(latitude_cold + elev / 4 + shadow_dryness / 5 + rnd(17) - 8, 0, 120);
            int ocean_moderation = gen_config.ocean <= 0 ? 0 : clamp(22 - ocean_distance[y][x] / 2, 0, 22);
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

    for (pass = 0; pass < (gen_config.ocean < 15 ? 5 : 2); pass++) {
        static Tile next[MAX_MAP_H][MAX_MAP_W];
        COPY_ACTIVE_TILES(next, world);
        for (y = 1; y < MAP_H - 1; y++) {
            for (x = 1; x < MAP_W - 1; x++) {
                int land = nearby_land_count(x, y);
                if (is_land(world[y][x].geography) && land <= 2 &&
                    world[y][x].geography != GEO_ISLAND) {
                    next[y][x].geography = gen_config.ocean <= 0 ? GEO_PLAIN : GEO_OCEAN;
                }
                if (!is_land(world[y][x].geography) && land >= (gen_config.ocean < 15 ? 4 : 6)) next[y][x].geography = GEO_PLAIN;
            }
        }
        COPY_ACTIVE_TILES(world, next);
    }

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            if (is_land(world[y][x].geography)) {
                world[y][x].climate = world_majority_neighbor_climate(x, y, world[y][x].climate);
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

    generate_rivers(gen_config.moisture, gen_config.bias_wetland);
}
