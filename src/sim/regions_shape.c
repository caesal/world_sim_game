#include "regions_shape.h"

#include "core/game_types.h"
#include "sim/regions.h"
#include "world/terrain_query.h"

#include <string.h>

#define SHAPE_PASSES 3

static int shape_tile_count[MAX_NATURAL_REGIONS];
static int last_merge_count;
static int last_ugly_count;
static int last_repair_tiles;
static int last_max_aspect_before;
static int last_max_aspect_after;

typedef struct {
    int tile_count;
    int min_x, min_y, max_x, max_y;
    int sum_x, sum_y;
    int aspect;
    int fill_percent;
    int perimeter;
    int coastal_tiles;
    int protected_tiles;
} ShapeMetrics;

static ShapeMetrics shape_metrics[MAX_NATURAL_REGIONS];

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

static void compute_shape_metrics(void) {
    static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int i, x, y, d;
    memset(shape_metrics, 0, sizeof(shape_metrics));
    for (i = 0; i < MAX_NATURAL_REGIONS; i++) {
        shape_metrics[i].min_x = MAP_W;
        shape_metrics[i].min_y = MAP_H;
        shape_metrics[i].max_x = -1;
        shape_metrics[i].max_y = -1;
    }
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int id = world[y][x].region_id;
            ShapeMetrics *m;
            if (id < 0 || id >= region_count || !is_land(world[y][x].geography)) continue;
            m = &shape_metrics[id];
            m->tile_count++;
            m->sum_x += x;
            m->sum_y += y;
            if (x < m->min_x) m->min_x = x;
            if (y < m->min_y) m->min_y = y;
            if (x > m->max_x) m->max_x = x;
            if (y > m->max_y) m->max_y = y;
            if (world_is_coastal_land_tile(x, y)) m->coastal_tiles++;
            if (world[y][x].river || world[y][x].geography == GEO_MOUNTAIN ||
                world[y][x].geography == GEO_CANYON || world[y][x].geography == GEO_WETLAND ||
                world[y][x].geography == GEO_DELTA) m->protected_tiles++;
            for (d = 0; d < 4; d++) {
                int nx = x + dirs[d][0], ny = y + dirs[d][1];
                if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H ||
                    world[ny][nx].region_id != id || !is_land(world[ny][nx].geography)) {
                    m->perimeter++;
                }
            }
        }
    }
    last_max_aspect_after = 0;
    for (i = 0; i < region_count; i++) {
        ShapeMetrics *m = &shape_metrics[i];
        int w, h, area;
        if (m->tile_count <= 0) continue;
        w = max(1, m->max_x - m->min_x + 1);
        h = max(1, m->max_y - m->min_y + 1);
        area = max(1, w * h);
        m->aspect = max(w, h) * 100 / max(1, min(w, h));
        m->fill_percent = m->tile_count * 100 / area;
        if (m->aspect > last_max_aspect_after) last_max_aspect_after = m->aspect;
    }
}

static int region_shape_is_ugly(int id, int target_size) {
    ShapeMetrics *m = &shape_metrics[id];
    int protected_shape;
    int aspect_limit;
    if (m->tile_count < max(24, target_size / 3)) return 0;
    protected_shape = (m->coastal_tiles + m->protected_tiles) * 100 / max(1, m->tile_count) > 42;
    aspect_limit = protected_shape ? 440 : 320;
    if (m->aspect > aspect_limit) return 1;
    if (m->aspect > 250 && m->fill_percent < (protected_shape ? 24 : 32)) return 1;
    if (!protected_shape && m->perimeter * 100 / max(1, m->tile_count) > 90 && m->aspect > 240) return 1;
    return 0;
}

static int best_tail_neighbor(int x, int y, int current, int same4) {
    static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int best = -1;
    int best_score = -1000000;
    int i;
    for (i = 0; i < 4; i++) {
        int nx = x + dirs[i][0], ny = y + dirs[i][1];
        int id, support, cost, score;
        if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
        id = world[ny][nx].region_id;
        if (id < 0 || id == current || id >= region_count) continue;
        cost = terrain_transition_cost(x, y, nx, ny);
        if (cost >= 88) continue;
        support = neighbor_support(x, y, id, 0);
        if (support < 2 || support < same4) continue;
        score = support * 40 - cost * 2 + shape_tile_count[id] / 80;
        if (score > best_score) {
            best_score = score;
            best = id;
        }
    }
    return best;
}

static int repair_ugly_region(int id, int target_size) {
    ShapeMetrics *m = &shape_metrics[id];
    int cx = m->sum_x / max(1, m->tile_count);
    int cy = m->sum_y / max(1, m->tile_count);
    int major_x = (m->max_x - m->min_x) >= (m->max_y - m->min_y);
    int changed = 0;
    int limit = max(6, min(target_size / 7, m->tile_count / 18));
    int x, y;
    for (y = m->min_y; y <= m->max_y && changed < limit; y++) {
        for (x = m->min_x; x <= m->max_x && changed < limit; x++) {
            int same4;
            int far_axis;
            int replacement;
            if (world[y][x].region_id != id || !is_land(world[y][x].geography)) continue;
            if (natural_regions[id].capital_x == x && natural_regions[id].capital_y == y) continue;
            same4 = neighbor_support(x, y, id, 1);
            far_axis = major_x ? abs(x - cx) > (m->max_x - m->min_x) / 3 :
                                 abs(y - cy) > (m->max_y - m->min_y) / 3;
            if (same4 > 2 || (!far_axis && same4 > 1)) continue;
            replacement = best_tail_neighbor(x, y, id, same4);
            if (replacement < 0) continue;
            world[y][x].region_id = replacement;
            changed++;
        }
    }
    return changed;
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

void regions_shape_repair_ugly(int target_size) {
    int pass, i;
    last_ugly_count = 0;
    last_repair_tiles = 0;
    compute_shape_metrics();
    last_max_aspect_before = last_max_aspect_after;
    for (pass = 0; pass < 3; pass++) {
        int changed = 0;
        rebuild_shape_counts();
        compute_shape_metrics();
        for (i = 0; i < region_count; i++) {
            if (!region_shape_is_ugly(i, target_size)) continue;
            if (pass == 0) last_ugly_count++;
            changed += repair_ugly_region(i, target_size);
        }
        last_repair_tiles += changed;
        if (!changed) break;
    }
    compute_shape_metrics();
}

int regions_shape_last_merge_count(void) {
    return last_merge_count;
}

int regions_shape_last_ugly_count(void) { return last_ugly_count; }
int regions_shape_last_repair_tiles(void) { return last_repair_tiles; }
int regions_shape_last_max_aspect_before(void) { return last_max_aspect_before; }
int regions_shape_last_max_aspect_after(void) { return last_max_aspect_after; }
