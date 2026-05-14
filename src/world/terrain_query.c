#include "terrain_query.h"

#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "data/game_tables.h"

#include <stdlib.h>
#include <string.h>

static TerrainStats tile_stats_cache[MAX_MAP_H][MAX_MAP_W];
static int tile_stats_cache_valid = 0;
static unsigned char water_depth_cache[MAX_MAP_H][MAX_MAP_W];
static unsigned char water_land_dist[MAX_MAP_H][MAX_MAP_W];
static int water_depth_cache_revision = -1;
static int water_depth_cache_w = 0;
static int water_depth_cache_h = 0;

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
    int x;
    int y;
    int pass;
    const unsigned char max_dist = 60;

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int water_tile = is_water_geography(world[y][x].geography);
            water_land_dist[y][x] = water_tile ? max_dist : 0;
            water_depth_cache[y][x] = WATER_DEPTH_NONE;
        }
    }
    for (pass = 0; pass < 2; pass++) {
        for (y = 0; y < MAP_H; y++) {
            for (x = 0; x < MAP_W; x++) {
                unsigned char *dist = &water_land_dist[y][x];
                if (!is_water_geography(world[y][x].geography)) continue;
                if (x > 0) *dist = min(*dist, (unsigned char)(water_land_dist[y][x - 1] + 1));
                if (y > 0) *dist = min(*dist, (unsigned char)(water_land_dist[y - 1][x] + 1));
                if (x > 0 && y > 0) *dist = min(*dist, (unsigned char)(water_land_dist[y - 1][x - 1] + 1));
                if (x + 1 < MAP_W && y > 0) *dist = min(*dist, (unsigned char)(water_land_dist[y - 1][x + 1] + 1));
            }
        }
        for (y = MAP_H - 1; y >= 0; y--) {
            for (x = MAP_W - 1; x >= 0; x--) {
                unsigned char *dist = &water_land_dist[y][x];
                if (!is_water_geography(world[y][x].geography)) continue;
                if (x + 1 < MAP_W) *dist = min(*dist, (unsigned char)(water_land_dist[y][x + 1] + 1));
                if (y + 1 < MAP_H) *dist = min(*dist, (unsigned char)(water_land_dist[y + 1][x] + 1));
                if (x + 1 < MAP_W && y + 1 < MAP_H) *dist = min(*dist, (unsigned char)(water_land_dist[y + 1][x + 1] + 1));
                if (x > 0 && y + 1 < MAP_H) *dist = min(*dist, (unsigned char)(water_land_dist[y + 1][x - 1] + 1));
            }
        }
    }
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            Geography g = world[y][x].geography;
            unsigned char dist = water_land_dist[y][x];
            if (!is_water_geography(g)) {
                water_depth_cache[y][x] = WATER_DEPTH_NONE;
            } else if (dist <= SHALLOW_WATER_LOGIC_MAX_DIST) {
                water_depth_cache[y][x] = WATER_DEPTH_SHALLOW;
            } else {
                water_depth_cache[y][x] = WATER_DEPTH_DEEP;
            }
        }
    }
    water_depth_cache_revision = dirty_revision_coast();
    water_depth_cache_w = MAP_W;
    water_depth_cache_h = MAP_H;
}

static void ensure_water_depth_cache(void) {
    if (water_depth_cache_revision != dirty_revision_coast() ||
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
    int dist;
    int range = WATER_VISUAL_BLEND_END_DIST - WATER_VISUAL_BLEND_START_DIST;

    if (world_water_depth_at(x, y) == WATER_DEPTH_NONE) return 0;
    dist = world_water_distance_to_land(x, y);
    if (dist <= WATER_VISUAL_BLEND_START_DIST) return 0;
    if (dist >= WATER_VISUAL_BLEND_END_DIST) return 100;
    return clamp((dist - WATER_VISUAL_BLEND_START_DIST) * 100 / range, 0, 100);
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
