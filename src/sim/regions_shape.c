#include "regions_shape.h"

#include "core/game_types.h"
#include "sim/regions.h"
#include "world/terrain_query.h"

#include <string.h>

#define SHAPE_PASSES 3

static int shape_tile_count[MAX_NATURAL_REGIONS];
static int last_merge_count;

static void rebuild_shape_counts(void) {
    int x;
    int y;

    memset(shape_tile_count, 0, sizeof(shape_tile_count));
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int id = world[y][x].region_id;
            if (id >= 0 && id < MAX_NATURAL_REGIONS) shape_tile_count[id]++;
        }
    }
}

static int terrain_transition_cost(int x, int y, int nx, int ny) {
    Tile *a = &world[y][x];
    Tile *b = &world[ny][nx];
    int cost = abs(a->elevation - b->elevation) / 9 + world_tile_cost(nx, ny);

    if (a->geography != b->geography) cost += 8;
    if (a->climate != b->climate) cost += 4;
    if (a->ecology != b->ecology) cost += 3;
    if (a->river != b->river) cost += 10;
    if (a->geography == GEO_MOUNTAIN || b->geography == GEO_MOUNTAIN) cost += 14;
    if (a->geography == GEO_CANYON || b->geography == GEO_CANYON) cost += 12;
    return cost;
}

static int neighbor_support(int x, int y, int region_id, int four_only) {
    int count = 0;
    int dy;
    int dx;

    for (dy = -1; dy <= 1; dy++) {
        for (dx = -1; dx <= 1; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            if (dx == 0 && dy == 0) continue;
            if (four_only && abs(dx) + abs(dy) != 1) continue;
            if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
            if (world[ny][nx].region_id == region_id) count++;
        }
    }
    return count;
}

static int best_supported_neighbor(int x, int y, int current) {
    static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int best = -1;
    int best_score = -1000000;
    int i;

    for (i = 0; i < 4; i++) {
        int nx = x + dirs[i][0];
        int ny = y + dirs[i][1];
        int id;
        int score;
        if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
        id = world[ny][nx].region_id;
        if (id < 0 || id == current) continue;
        score = neighbor_support(x, y, id, 0) * 28 + neighbor_support(x, y, id, 1) * 18;
        score += shape_tile_count[id] / 40;
        score -= terrain_transition_cost(x, y, nx, ny) * 2;
        if (score > best_score) {
            best_score = score;
            best = id;
        }
    }
    return best;
}

static int is_thin_bridge(int x, int y, int id) {
    int left = x > 0 && world[y][x - 1].region_id == id;
    int right = x + 1 < MAP_W && world[y][x + 1].region_id == id;
    int up = y > 0 && world[y - 1][x].region_id == id;
    int down = y + 1 < MAP_H && world[y + 1][x].region_id == id;
    int same4 = left + right + up + down;

    if (same4 <= 1) return 1;
    if (same4 == 2 && ((left && right) || (up && down))) {
        int side_a = left || right ? up : left;
        int side_b = left || right ? down : right;
        return !side_a && !side_b;
    }
    return 0;
}

static int smooth_sliver_pass(void) {
    int changed = 0;
    int x;
    int y;

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int id = world[y][x].region_id;
            int replacement;
            if (id < 0 || !is_land(world[y][x].geography)) continue;
            if (!is_thin_bridge(x, y, id) && neighbor_support(x, y, id, 1) >= 2) continue;
            replacement = best_supported_neighbor(x, y, id);
            if (replacement >= 0 && replacement != id && neighbor_support(x, y, replacement, 1) >= 1) {
                world[y][x].region_id = replacement;
                changed++;
                last_merge_count++;
            }
        }
    }
    return changed;
}

static void merge_small_islands(int min_size) {
    int i;

    for (i = 0; i < region_count; i++) {
        if (shape_tile_count[i] <= 0 || shape_tile_count[i] >= min_size) continue;
        if (natural_regions[i].neighbor_count == 1) {
            int to = natural_regions[i].neighbors[0];
            int x;
            int y;
            if (to < 0 || to >= region_count) continue;
            for (y = 0; y < MAP_H; y++) {
                for (x = 0; x < MAP_W; x++) {
                    if (world[y][x].region_id == i) world[y][x].region_id = to;
                }
            }
            last_merge_count++;
        }
    }
}

void regions_shape_refine(int target_size) {
    int pass;

    last_merge_count = 0;
    for (pass = 0; pass < SHAPE_PASSES; pass++) {
        rebuild_shape_counts();
        if (!smooth_sliver_pass()) break;
    }
    rebuild_shape_counts();
    merge_small_islands(max(18, target_size / 7));
}

int regions_shape_last_merge_count(void) {
    return last_merge_count;
}
