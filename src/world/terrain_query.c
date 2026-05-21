#include "terrain_query.h"

#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "data/game_tables.h"

#include <stdlib.h>
#include <string.h>

static TerrainStats tile_stats_cache[MAX_MAP_H][MAX_MAP_W];
static int tile_stats_cache_valid = 0;
static unsigned char water_depth_cache[MAX_MAP_H][MAX_MAP_W];
static unsigned char water_depth_score[MAX_MAP_H][MAX_MAP_W];
static unsigned char water_land_dist[MAX_MAP_H][MAX_MAP_W];
static unsigned char water_shelf_width[MAX_MAP_H][MAX_MAP_W];
static unsigned char water_shelf_scratch[MAX_MAP_H][MAX_MAP_W];
static unsigned char water_score_scratch[MAX_MAP_H][MAX_MAP_W];
static unsigned short water_dist_cost[MAX_MAP_H][MAX_MAP_W];
static unsigned char water_in_queue[MAX_MAP_H][MAX_MAP_W];
static int water_queue[MAX_MAP_W * MAX_MAP_H];
static int water_depth_cache_revision = -1;
static int water_depth_cache_w = 0;
static int water_depth_cache_h = 0;
static int water_depth_rebuild_ms;
static int water_shallow_tiles;
static int water_deep_tiles;
static int water_shelf_min;
static int water_shelf_max;
static int water_shelf_avg;

static int in_bounds(int x, int y) {
    return x >= 0 && x < MAP_W && y >= 0 && y < MAP_H;
}

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
    if (resource >= 0 && resource < RESOURCE_FEATURE_COUNT) add_table_stats(stats, RESOURCE_FEATURE_RULES[resource].stats);
}

int is_land(Geography geography) {
    return geography != GEO_OCEAN && geography != GEO_LAKE && geography != GEO_BAY;
}

static int is_water_geography(Geography geography) {
    return geography == GEO_OCEAN || geography == GEO_LAKE || geography == GEO_BAY;
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

static int water_cache_revision_key(void) {
    return dirty_revision_coast() * 1009 + dirty_revision_terrain();
}

static int nearby_land_count(int x, int y, int radius) {
    int dx, dy, count = 0;
    for (dy = -radius; dy <= radius; dy++) {
        for (dx = -radius; dx <= radius; dx++) {
            int nx = x + dx, ny = y + dy;
            if (!in_bounds(nx, ny)) continue;
            if (is_land(world[ny][nx].geography)) count++;
        }
    }
    return count;
}

static int relief_penalty(int x, int y) {
    int dx, dy;
    int min_e = world[y][x].elevation;
    int max_e = min_e;
    for (dy = -1; dy <= 1; dy++) {
        for (dx = -1; dx <= 1; dx++) {
            int nx = x + dx, ny = y + dy;
            if (!in_bounds(nx, ny) || !is_land(world[ny][nx].geography)) continue;
            min_e = min(min_e, world[ny][nx].elevation);
            max_e = max(max_e, world[ny][nx].elevation);
        }
    }
    if (max_e - min_e > 35) return 3;
    if (max_e - min_e > 22) return 2;
    if (max_e - min_e > 14) return 1;
    return 0;
}

static int land_shelf_width(int x, int y) {
    int shelf = 7;
    Geography g = world[y][x].geography;
    switch (g) {
        case GEO_DELTA: shelf += 5; break;
        case GEO_WETLAND: shelf += 4; break;
        case GEO_PLAIN:
        case GEO_OASIS: shelf += 3; break;
        case GEO_BASIN:
        case GEO_COAST: shelf += 2; break;
        case GEO_ISLAND: shelf += 1; break;
        case GEO_PLATEAU: shelf -= 3; break;
        case GEO_MOUNTAIN:
        case GEO_CANYON: shelf -= 4; break;
        case GEO_VOLCANO: shelf -= 5; break;
        default: break;
    }
    if (world[y][x].river) shelf += 4;
    if (world[y][x].elevation <= 38) shelf += 2;
    else if (world[y][x].elevation <= 50) shelf += 1;
    else if (world[y][x].elevation >= 75) shelf -= 4;
    else if (world[y][x].elevation >= 62) shelf -= 2;
    if (world[y][x].moisture > 72) shelf += 1;
    shelf -= relief_penalty(x, y);
    return clamp(shelf, 3, 16);
}

static int adjacent_land_shelf_width(int x, int y) {
    int dx, dy;
    int sum = 0;
    int count = 0;
    for (dy = -1; dy <= 1; dy++) {
        for (dx = -1; dx <= 1; dx++) {
            int nx = x + dx, ny = y + dy;
            if (!in_bounds(nx, ny) || !is_land(world[ny][nx].geography)) continue;
            sum += land_shelf_width(nx, ny);
            count++;
        }
    }
    if (count <= 0) return 0;
    sum = (sum + count / 2) / count;
    if (nearby_land_count(x, y, 2) >= 12) sum += 3;
    else if (nearby_land_count(x, y, 2) >= 8) sum += 2;
    else if (nearby_land_count(x, y, 2) >= 5) sum += 1;
    return clamp(sum, 3, 16);
}

static int smoothstep_1000(int numerator, int denominator) {
    int t;
    long tt;
    if (denominator <= 0) return 1000;
    t = clamp(numerator * 1000 / denominator, 0, 1000);
    tt = (long)t * t;
    return (int)(tt * (3000 - 2 * t) / 1000000L);
}

static int water_depth_score_from_dist(int x, int y) {
    int dist_cost = water_dist_cost[y][x];
    int shelf_cost = max(3, water_shelf_width[y][x]) * 10;
    int deep_fade_cost = clamp(water_shelf_width[y][x] + 5, 8, 16) * 10;
    if (dist_cost >= 30000) return 100;
    if (dist_cost <= shelf_cost) {
        int smooth = smoothstep_1000(dist_cost, shelf_cost);
        return clamp(smooth * WATER_DEPTH_SHALLOW_MAX_SCORE / 1000, 0,
                     WATER_DEPTH_SHALLOW_MAX_SCORE);
    }
    return clamp(WATER_DEPTH_DEEP_MIN_SCORE +
                 smoothstep_1000(dist_cost - shelf_cost, deep_fade_cost) * 44 / 1000,
                 WATER_DEPTH_DEEP_MIN_SCORE, 100);
}

static TerrainStats compute_tile_stats_uncached(int x, int y) {
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

void terrain_stats_invalidate_cache(void) {
    tile_stats_cache_valid = 0;
    water_depth_cache_revision = -1;
}

static void rebuild_water_depth_cache(void) {
    const unsigned short inf = 30000;
    DWORD start = GetTickCount();
    int x, y, pass;
    int head = 0, tail = 0, queued = 0;
    int capacity = MAP_W * MAP_H;
    int shelf_sum = 0, shelf_count = 0;

#define PUSH_WATER(idx) do { \
        if (!water_in_queue[(idx) / MAP_W][(idx) % MAP_W]) { \
            water_queue[tail] = (idx); tail = (tail + 1) % capacity; queued++; \
            water_in_queue[(idx) / MAP_W][(idx) % MAP_W] = 1; \
        } \
    } while (0)

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int water_tile = is_water_geography(world[y][x].geography);
            water_land_dist[y][x] = water_tile ? 255 : 0;
            water_shelf_width[y][x] = 0;
            water_shelf_scratch[y][x] = 0;
            water_depth_score[y][x] = 0;
            water_depth_cache[y][x] = WATER_DEPTH_NONE;
            water_dist_cost[y][x] = water_tile ? inf : 0;
            water_in_queue[y][x] = 0;
            if (water_tile) {
                int shelf = adjacent_land_shelf_width(x, y);
                if (shelf > 0) {
                    int idx = y * MAP_W + x;
                    water_land_dist[y][x] = 1;
                    water_shelf_width[y][x] = (unsigned char)shelf;
                    water_dist_cost[y][x] = 10;
                    PUSH_WATER(idx);
                }
            }
        }
    }
    while (queued > 0) {
        int idx = water_queue[head];
        int cx = idx % MAP_W;
        int cy = idx / MAP_W;
        int dir;
        head = (head + 1) % capacity;
        queued--;
        water_in_queue[cy][cx] = 0;
        for (dir = 0; dir < 8; dir++) {
            static const int dxs[8] = {-1, 1, 0, 0, -1, 1, -1, 1};
            static const int dys[8] = {0, 0, -1, 1, -1, -1, 1, 1};
            int nx = cx + dxs[dir];
            int ny = cy + dys[dir];
            int step = dir < 4 ? 10 : 14;
            unsigned short candidate;
            if (!in_bounds(nx, ny) || !is_water_geography(world[ny][nx].geography)) continue;
            candidate = (unsigned short)min(inf, water_dist_cost[cy][cx] + step);
            if (candidate < water_dist_cost[ny][nx]) {
                water_dist_cost[ny][nx] = candidate;
                water_shelf_width[ny][nx] = water_shelf_width[cy][cx];
                PUSH_WATER(ny * MAP_W + nx);
            }
        }
    }
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            if (!is_water_geography(world[y][x].geography)) continue;
            water_land_dist[y][x] = (unsigned char)min(255, (water_dist_cost[y][x] + 5) / 10);
            if (water_shelf_width[y][x] <= 0) water_shelf_width[y][x] = 7;
        }
    }
    for (pass = 0; pass < 4; pass++) {
        for (y = 0; y < MAP_H; y++) {
            for (x = 0; x < MAP_W; x++) {
                int dx, dy, sum, weight;
                if (!is_water_geography(world[y][x].geography)) continue;
                sum = water_shelf_width[y][x] * 4;
                weight = 4;
                for (dy = -1; dy <= 1; dy++) {
                    for (dx = -1; dx <= 1; dx++) {
                        int nx = x + dx, ny = y + dy;
                        if ((dx == 0 && dy == 0) || !in_bounds(nx, ny)) continue;
                        if (!is_water_geography(world[ny][nx].geography)) continue;
                        sum += water_shelf_width[ny][nx];
                        weight++;
                    }
                }
                water_shelf_scratch[y][x] = (unsigned char)clamp((sum + weight / 2) / weight, 3, 16);
            }
        }
        for (y = 0; y < MAP_H; y++) {
            for (x = 0; x < MAP_W; x++) {
                if (!is_water_geography(world[y][x].geography)) continue;
                water_shelf_width[y][x] = water_shelf_scratch[y][x];
            }
        }
    }
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            Geography g = world[y][x].geography;
            water_depth_score[y][x] = is_water_geography(g) ?
                (unsigned char)water_depth_score_from_dist(x, y) : 0;
        }
    }
    for (pass = 0; pass < 2; pass++) {
        for (y = 0; y < MAP_H; y++) {
            for (x = 0; x < MAP_W; x++) {
                int dx, dy, sum, weight;
                if (!is_water_geography(world[y][x].geography)) continue;
                sum = water_depth_score[y][x] * 4;
                weight = 4;
                for (dy = -1; dy <= 1; dy++) {
                    for (dx = -1; dx <= 1; dx++) {
                        int nx = x + dx, ny = y + dy;
                        if ((dx == 0 && dy == 0) || !in_bounds(nx, ny)) continue;
                        if (!is_water_geography(world[ny][nx].geography)) continue;
                        sum += water_depth_score[ny][nx];
                        weight++;
                    }
                }
                water_score_scratch[y][x] = (unsigned char)clamp((sum + weight / 2) / weight, 0, 100);
            }
        }
        for (y = 0; y < MAP_H; y++) {
            for (x = 0; x < MAP_W; x++) {
                if (is_water_geography(world[y][x].geography)) water_depth_score[y][x] = water_score_scratch[y][x];
            }
        }
    }
    water_shallow_tiles = 0;
    water_deep_tiles = 0;
    water_shelf_min = 99;
    water_shelf_max = 0;
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            if (!is_water_geography(world[y][x].geography)) {
                water_depth_cache[y][x] = WATER_DEPTH_NONE;
                continue;
            }
            if (water_depth_score[y][x] <= WATER_DEPTH_SHALLOW_MAX_SCORE) {
                water_depth_cache[y][x] = WATER_DEPTH_SHALLOW;
                water_shallow_tiles++;
            } else {
                water_depth_cache[y][x] = WATER_DEPTH_DEEP;
                water_deep_tiles++;
            }
            water_shelf_min = min(water_shelf_min, water_shelf_width[y][x]);
            water_shelf_max = max(water_shelf_max, water_shelf_width[y][x]);
            shelf_sum += water_shelf_width[y][x];
            shelf_count++;
        }
    }
    water_shelf_avg = shelf_count > 0 ? shelf_sum / shelf_count : 0;
    if (water_shelf_min == 99) water_shelf_min = 0;
    water_depth_rebuild_ms = (int)(GetTickCount() - start);
    water_depth_cache_revision = water_cache_revision_key();
    water_depth_cache_w = MAP_W;
    water_depth_cache_h = MAP_H;
#undef PUSH_WATER
}

static void ensure_water_depth_cache(void) {
    if (water_depth_cache_revision != water_cache_revision_key() ||
        water_depth_cache_w != MAP_W || water_depth_cache_h != MAP_H) {
        rebuild_water_depth_cache();
    }
}

WaterDepth world_water_depth_at(int x, int y) {
    if (!in_bounds(x, y)) return WATER_DEPTH_NONE;
    ensure_water_depth_cache();
    return (WaterDepth)water_depth_cache[y][x];
}

int world_is_shallow_water(int x, int y) {
    return world_water_depth_at(x, y) == WATER_DEPTH_SHALLOW;
}

int world_is_deep_water(int x, int y) {
    return world_water_depth_at(x, y) == WATER_DEPTH_DEEP;
}

int world_water_distance_to_land(int x, int y) {
    if (!in_bounds(x, y)) return 0;
    ensure_water_depth_cache();
    return water_land_dist[y][x];
}

int world_water_visual_deep_percent(int x, int y) {
    if (world_water_depth_at(x, y) == WATER_DEPTH_NONE) return 0;
    return water_depth_score[y][x];
}

int world_water_shelf_width_at(int x, int y) {
    if (!in_bounds(x, y)) return 0;
    ensure_water_depth_cache();
    return water_shelf_width[y][x];
}

int world_water_depth_rebuild_ms(void) {
    ensure_water_depth_cache();
    return water_depth_rebuild_ms;
}

int world_water_shallow_tile_count(void) {
    ensure_water_depth_cache();
    return water_shallow_tiles;
}

int world_water_deep_tile_count(void) {
    ensure_water_depth_cache();
    return water_deep_tiles;
}

int world_water_shelf_min(void) {
    ensure_water_depth_cache();
    return water_shelf_min;
}

int world_water_shelf_max(void) {
    ensure_water_depth_cache();
    return water_shelf_max;
}

int world_water_shelf_avg(void) {
    ensure_water_depth_cache();
    return water_shelf_avg;
}

void terrain_stats_rebuild_cache(void) {
    int x;
    int y;

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            tile_stats_cache[y][x] = compute_tile_stats_uncached(x, y);
        }
    }
    tile_stats_cache_valid = 1;
}

TerrainStats tile_stats(int x, int y) {
    TerrainStats empty;

    memset(&empty, 0, sizeof(empty));
    if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) return empty;
    if (!tile_stats_cache_valid) terrain_stats_rebuild_cache();
    return tile_stats_cache[y][x];
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
