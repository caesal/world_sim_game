#include "region_boundary.h"

#include "core/game_types.h"
#include "sim/regions.h"
#include "sim/regions_shape.h"
#include "world/terrain_query.h"

#include <stdio.h>

static int natural_barrier_bonus(Geography geography) {
    switch (geography) {
        case GEO_MOUNTAIN: return 28;
        case GEO_CANYON: return 24;
        case GEO_VOLCANO: return 26;
        case GEO_HILL: return 8;
        case GEO_PLATEAU: return 7;
        case GEO_WETLAND: return 5;
        default: return 0;
    }
}

int region_boundary_crossing_cost(int x, int y, int nx, int ny) {
    Tile *from = &world[y][x];
    Tile *to = &world[ny][nx];
    int cost = 6 + world_tile_cost(nx, ny) * 4;

    cost += abs(to->elevation - from->elevation) / 5;
    if (to->geography != from->geography) cost += 12;
    if (to->climate != from->climate) cost += 7;
    if (to->ecology != from->ecology) cost += 4;
    cost += natural_barrier_bonus(from->geography) + natural_barrier_bonus(to->geography);
    if (to->river != from->river) cost += 18;
    else if (to->river && from->river) cost -= 6;
    if (world_is_coastal_land_tile(x, y) != world_is_coastal_land_tile(nx, ny)) cost += 10;
    if ((from->geography == GEO_PLAIN || from->geography == GEO_BASIN) &&
        (to->geography == GEO_PLAIN || to->geography == GEO_BASIN)) cost -= 4;
    return clamp(cost, 2, 120);
}

int region_boundary_seed_score(int x, int y, int seed_count, const int *seed_x, const int *seed_y) {
    TerrainStats stats;
    int best = MAP_W * MAP_W + MAP_H * MAP_H;
    int i;

    if (!is_land(world[y][x].geography)) return -1000000;
    if (world[y][x].geography == GEO_MOUNTAIN || world[y][x].geography == GEO_CANYON ||
        world[y][x].geography == GEO_VOLCANO) return -1000000;
    stats = tile_stats(x, y);
    for (i = 0; i < seed_count; i++) {
        int dx = x - seed_x[i];
        int dy = y - seed_y[i];
        int dist = dx * dx + dy * dy;
        if (dist < best) best = dist;
    }
    if (seed_count == 0) best = 90000;
    return best / 7 + stats.habitability * 34 + stats.food * 22 + stats.water * 24 +
           stats.pop_capacity * 18 + stats.money * 8 + (world_is_coastal_land_tile(x, y) ? 18 : 0) -
           world_tile_cost(x, y) * 24;
}

int region_boundary_compactness_penalty(int seed_x, int seed_y, int x, int y, int target_size) {
    int dx = abs(x - seed_x);
    int dy = abs(y - seed_y);
    int major = max(dx, dy);
    int minor = min(dx, dy);
    int target_radius = clamp(target_size / 26, 8, 48);
    int penalty = 0;

    if (major > target_radius) penalty += (major - target_radius) * 6;
    if (major > minor * 3 + 10) penalty += (major - minor * 3 - 10) * 9;
    if (dx > target_radius * 2 || dy > target_radius * 2) penalty += 80;
    return penalty;
}

void region_boundary_debug_summary(void) {
    int i;
    int total = 0;
    int active = 0;
    int max_elongation = 0;
    char buffer[160];

    for (i = 0; i < region_count; i++) {
        NaturalRegion *region = &natural_regions[i];
        int min_x = MAP_W;
        int max_x = 0;
        int min_y = MAP_H;
        int max_y = 0;
        int x;
        int y;
        if (region->tile_count <= 0) continue;
        active++;
        total += region->tile_count;
        for (y = 0; y < MAP_H; y++) {
            for (x = 0; x < MAP_W; x++) {
                if (world[y][x].region_id != i) continue;
                min_x = min(min_x, x);
                max_x = max(max_x, x);
                min_y = min(min_y, y);
                max_y = max(max_y, y);
            }
        }
        {
            int w = max(1, max_x - min_x + 1);
            int h = max(1, max_y - min_y + 1);
            int elongation = max(w, h) * 100 / max(1, min(w, h));
            if (elongation > max_elongation) max_elongation = elongation;
        }
    }
    snprintf(buffer, sizeof(buffer),
             "World Sim: regions=%d avg_area=%d max_elongation=%d%% sliver_merges=%d\n",
             active, active > 0 ? total / active : 0, max_elongation, regions_shape_last_merge_count());
    OutputDebugStringA(buffer);
}
