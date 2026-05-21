#include "regions_shape.h"

#include "core/game_types.h"
#include "sim/region_boundary.h"
#include "sim/regions.h"
#include "world/terrain_query.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define SHAPE_PASSES 3
#define SHAPE_SPLIT_PARTS 4

typedef struct {
    int tile_count;
    int min_x, min_y, max_x, max_y;
    int sum_x, sum_y;
    int aspect;
    int fill_percent;
    int perimeter;
    int perimeter_area;
    int avg_width_x100;
    int coastal_tiles;
    int river_tiles;
    int mountain_tiles;
    int protected_tiles;
    int diagonal_links;
} ShapeMetrics;

typedef struct { int x, y, part, cost; } ShapeNode;

static int shape_tile_count[MAX_NATURAL_REGIONS];
static ShapeMetrics shape_metrics[MAX_NATURAL_REGIONS];
static RegionShapeClass shape_class[MAX_NATURAL_REGIONS];
static int split_assign[MAX_MAP_H][MAX_MAP_W];
static int split_cost[MAX_MAP_H][MAX_MAP_W];
static ShapeNode split_heap[MAX_MAP_W * MAX_MAP_H];
static int split_heap_count;
static int last_merge_count, last_ugly_count, last_repair_tiles, last_max_aspect_before, last_max_aspect_after;
static int last_ribbon_count, last_low_fill_count, last_diagonal_count, last_resplit_count, last_shape_merge_count, last_local_regrow_count;
static int last_worst_fill_percent, last_worst_perimeter_area;
static void rebuild_shape_counts(void) {
    int x, y;
    memset(shape_tile_count, 0, sizeof(shape_tile_count));
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int id = world[y][x].region_id;
            if (id >= 0 && id < MAX_NATURAL_REGIONS) shape_tile_count[id]++;
        }
    }
}
static int terrain_transition_cost(int x, int y, int nx, int ny) {
    int cost = region_boundary_crossing_cost(x, y, nx, ny);
    if (world[y][x].geography == world[ny][nx].geography) cost -= 6;
    if (world[y][x].climate == world[ny][nx].climate) cost -= 3;
    return clamp(cost, 2, 120);
}
static int neighbor_support(int x, int y, int region_id, int four_only) {
    int count = 0, dy, dx;
    for (dy = -1; dy <= 1; dy++) {
        for (dx = -1; dx <= 1; dx++) {
            int nx = x + dx, ny = y + dy;
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
    int best = -1, best_score = -1000000, i;
    for (i = 0; i < 4; i++) {
        int nx = x + dirs[i][0], ny = y + dirs[i][1];
        int id, score;
        if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
        id = world[ny][nx].region_id;
        if (id < 0 || id == current) continue;
        score = neighbor_support(x, y, id, 0) * 28 + neighbor_support(x, y, id, 1) * 18;
        score += shape_tile_count[id] / 40 - terrain_transition_cost(x, y, nx, ny) * 2;
        if (score > best_score) {
            best_score = score;
            best = id;
        }
    }
    return best;
}
static void count_diagonal_link(int x, int y, int id, ShapeMetrics *m) {
    static const int dirs[4][2] = {{1, 1}, {-1, 1}, {1, -1}, {-1, -1}};
    int i;
    for (i = 0; i < 4; i++) {
        int dx = dirs[i][0], dy = dirs[i][1];
        int nx = x + dx, ny = y + dy;
        int ox = x + dx, oy = y;
        int px = x, py = y + dy;
        if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
        if (world[ny][nx].region_id != id) continue;
        if ((ox < 0 || ox >= MAP_W || world[oy][ox].region_id != id) &&
            (py < 0 || py >= MAP_H || world[py][px].region_id != id)) {
            m->diagonal_links++;
        }
    }
}
static void compute_shape_metrics(void) {
    static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int i, x, y, d;
    memset(shape_metrics, 0, sizeof(shape_metrics));
    memset(shape_class, 0, sizeof(shape_class));
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
            m->sum_x += x; m->sum_y += y;
            if (x < m->min_x) m->min_x = x;
            if (y < m->min_y) m->min_y = y;
            if (x > m->max_x) m->max_x = x;
            if (y > m->max_y) m->max_y = y;
            if (world_is_coastal_land_tile(x, y)) m->coastal_tiles++;
            if (world[y][x].river) m->river_tiles++;
            if (world[y][x].geography == GEO_MOUNTAIN || world[y][x].geography == GEO_CANYON ||
                world[y][x].geography == GEO_VOLCANO) m->mountain_tiles++;
            if (world[y][x].river || world_is_coastal_land_tile(x, y) ||
                world[y][x].geography == GEO_WETLAND || world[y][x].geography == GEO_DELTA ||
                world[y][x].geography == GEO_ISLAND || world[y][x].geography == GEO_MOUNTAIN ||
                world[y][x].geography == GEO_CANYON) m->protected_tiles++;
            count_diagonal_link(x, y, id, m);
            for (d = 0; d < 4; d++) {
                int nx = x + dirs[d][0], ny = y + dirs[d][1];
                if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H ||
                    world[ny][nx].region_id != id || !is_land(world[ny][nx].geography)) m->perimeter++;
            }
        }
    }
    last_max_aspect_after = 0;
    last_worst_fill_percent = 100;
    last_worst_perimeter_area = 0;
    for (i = 0; i < region_count; i++) {
        ShapeMetrics *m = &shape_metrics[i];
        int w, h, area, major;
        if (m->tile_count <= 0) continue;
        w = max(1, m->max_x - m->min_x + 1);
        h = max(1, m->max_y - m->min_y + 1);
        area = max(1, w * h);
        major = max(w, h);
        m->aspect = major * 100 / max(1, min(w, h));
        m->fill_percent = m->tile_count * 100 / area;
        m->perimeter_area = m->perimeter * 100 / max(1, m->tile_count);
        m->avg_width_x100 = m->tile_count * 100 / max(1, major);
        if (m->aspect > last_max_aspect_after) last_max_aspect_after = m->aspect;
        if (m->fill_percent < last_worst_fill_percent) last_worst_fill_percent = m->fill_percent;
        if (m->perimeter_area > last_worst_perimeter_area) last_worst_perimeter_area = m->perimeter_area;
    }
}

static int protected_shape_is_coherent(const ShapeMetrics *m) {
    int protected_ratio = m->protected_tiles * 100 / max(1, m->tile_count);
    int river_ratio = m->river_tiles * 100 / max(1, m->tile_count);
    int coast_ratio = m->coastal_tiles * 100 / max(1, m->tile_count);
    if (protected_ratio < 36) return 0;
    if (m->avg_width_x100 < 450 || m->fill_percent < 26 || m->perimeter_area > 132) return 0;
    if (m->mountain_tiles * 100 / max(1, m->tile_count) > 65 && m->avg_width_x100 < 700) return 0;
    return river_ratio >= 8 || coast_ratio >= 12 || protected_ratio >= 52;
}

static RegionShapeClass classify_region_shape(int id, int target_size) {
    ShapeMetrics *m = &shape_metrics[id];
    int coherent = protected_shape_is_coherent(m);
    int diagonal_percent;
    if (m->tile_count <= 0) return REGION_SHAPE_OK;
    diagonal_percent = m->diagonal_links * 100 / max(1, m->tile_count);
    if (m->tile_count < max(16, target_size / 5)) return REGION_SHAPE_TINY;
    if (m->tile_count > max(target_size * 5 / 2, target_size + 90)) return REGION_SHAPE_HUGE;
    if (m->avg_width_x100 < 300 && m->tile_count > 24) return REGION_SHAPE_SLIVER;
    if (diagonal_percent > 18 && m->fill_percent < 38 && m->perimeter_area > 96) return REGION_SHAPE_ARTIFICIAL_DIAGONAL;
    if (m->fill_percent < (coherent ? 18 : 25) && m->tile_count > target_size / 3) return REGION_SHAPE_LOW_FILL;
    if (m->aspect > (coherent ? 480 : 320)) return REGION_SHAPE_RIBBON;
    if (m->aspect > 240 && m->avg_width_x100 < (coherent ? 520 : 620)) return REGION_SHAPE_RIBBON;
    if (!coherent && m->perimeter_area > 132 && m->fill_percent < 34) return REGION_SHAPE_LOW_FILL;
    return REGION_SHAPE_OK;
}

static int best_merge_neighbor_for_region(int id, int target_size) {
    static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int scores[MAX_NATURAL_REGIONS];
    int best = -1, best_score = INT_MIN;
    int x, y, d, i;
    memset(scores, 0, sizeof(scores));
    for (y = shape_metrics[id].min_y; y <= shape_metrics[id].max_y; y++) {
        for (x = shape_metrics[id].min_x; x <= shape_metrics[id].max_x; x++) {
            if (world[y][x].region_id != id) continue;
            for (d = 0; d < 4; d++) {
                int nx = x + dirs[d][0], ny = y + dirs[d][1], other, cost;
                if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
                other = world[ny][nx].region_id;
                if (other < 0 || other == id || other >= region_count) continue;
                cost = terrain_transition_cost(x, y, nx, ny);
                if (cost >= 94) continue;
                scores[other] += 110 - cost;
                if (natural_regions[id].dominant_geography == natural_regions[other].dominant_geography) scores[other] += 16;
                if (natural_regions[id].dominant_climate == natural_regions[other].dominant_climate) scores[other] += 8;
            }
        }
    }
    for (i = 0; i < region_count; i++) {
        int size_penalty = 0;
        if (i == id || shape_metrics[i].tile_count <= 0) continue;
        if (shape_metrics[i].tile_count > target_size * 2) size_penalty = (shape_metrics[i].tile_count - target_size * 2) / 8;
        if (scores[i] - size_penalty > best_score) {
            best_score = scores[i] - size_penalty;
            best = i;
        }
    }
    return best_score > 0 ? best : -1;
}

static int merge_bad_region(int id, int target_size) {
    int to = best_merge_neighbor_for_region(id, target_size);
    int x, y, changed = 0;
    if (to < 0) return 0;
    for (y = shape_metrics[id].min_y; y <= shape_metrics[id].max_y; y++) {
        for (x = shape_metrics[id].min_x; x <= shape_metrics[id].max_x; x++) {
            if (world[y][x].region_id == id) {
                world[y][x].region_id = to;
                changed++;
            }
        }
    }
    if (changed) {
        last_shape_merge_count++;
        last_merge_count++;
    }
    return changed;
}

static void split_heap_push(ShapeNode node) {
    int i = split_heap_count++;
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (split_heap[parent].cost <= node.cost) break;
        split_heap[i] = split_heap[parent];
        i = parent;
    }
    split_heap[i] = node;
}

static int split_heap_pop(ShapeNode *out) {
    ShapeNode last;
    int i = 0;
    if (split_heap_count <= 0) return 0;
    *out = split_heap[0];
    last = split_heap[--split_heap_count];
    while (1) {
        int child = i * 2 + 1;
        if (child >= split_heap_count) break;
        if (child + 1 < split_heap_count && split_heap[child + 1].cost < split_heap[child].cost) child++;
        if (last.cost <= split_heap[child].cost) break;
        split_heap[i] = split_heap[child];
        i = child;
    }
    if (split_heap_count > 0) split_heap[i] = last;
    return 1;
}

static int split_seed_score(int x, int y, const int *seed_x, const int *seed_y, int seed_count) {
    int closest = INT_MAX, i;
    TerrainStats stats = tile_stats(x, y);
    if (world[y][x].geography == GEO_VOLCANO) return -1;
    for (i = 0; i < seed_count; i++) {
        int dx = x - seed_x[i], dy = y - seed_y[i];
        int dist = dx * dx + dy * dy;
        if (dist < closest) closest = dist;
    }
    return closest + stats.habitability * 40 + stats.food * 18 + stats.water * 18 - world_tile_cost(x, y) * 35 -
           (world[y][x].geography == GEO_MOUNTAIN || world[y][x].geography == GEO_CANYON ? 500 : 0);
}

static int choose_split_seeds(int id, int pieces, int *seed_x, int *seed_y) {
    ShapeMetrics *m = &shape_metrics[id];
    int p, x, y;
    for (p = 0; p < pieces; p++) {
        int best_score = INT_MIN, best_x = -1, best_y = -1;
        for (y = m->min_y; y <= m->max_y; y++) {
            for (x = m->min_x; x <= m->max_x; x++) {
                int score;
                if (world[y][x].region_id != id) continue;
                score = p == 0 ? -((x - m->sum_x / max(1, m->tile_count)) * (x - m->sum_x / max(1, m->tile_count)) +
                                   (y - m->sum_y / max(1, m->tile_count)) * (y - m->sum_y / max(1, m->tile_count))) :
                                  split_seed_score(x, y, seed_x, seed_y, p);
                if (score > best_score) {
                    best_score = score; best_x = x; best_y = y;
                }
            }
        }
        if (best_x < 0) return 0;
        seed_x[p] = best_x; seed_y[p] = best_y;
    }
    return 1;
}

static int split_bad_region(int id, int target_size) {
    static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    ShapeMetrics *m = &shape_metrics[id];
    int pieces = clamp(m->tile_count / max(1, target_size) + 1, 2, SHAPE_SPLIT_PARTS);
    int available = MAX_NATURAL_REGIONS - region_count;
    int seed_x[SHAPE_SPLIT_PARTS], seed_y[SHAPE_SPLIT_PARTS];
    int part_count[SHAPE_SPLIT_PARTS] = {0};
    int part_region[SHAPE_SPLIT_PARTS];
    int preserve_part = 0, x, y, p;
    ShapeNode node;
    if (available <= 0) return 0;
    pieces = min(pieces, available + 1);
    if (pieces < 2 || !choose_split_seeds(id, pieces, seed_x, seed_y)) return 0;
    split_heap_count = 0;
    for (y = m->min_y; y <= m->max_y; y++) {
        for (x = m->min_x; x <= m->max_x; x++) {
            split_assign[y][x] = -1;
            split_cost[y][x] = INT_MAX;
        }
    }
    for (p = 0; p < pieces; p++) {
        split_assign[seed_y[p]][seed_x[p]] = p;
        split_cost[seed_y[p]][seed_x[p]] = 0;
        node.x = seed_x[p]; node.y = seed_y[p]; node.part = p; node.cost = 0;
        split_heap_push(node);
    }
    while (split_heap_pop(&node)) {
        int d;
        if (node.cost != split_cost[node.y][node.x] || split_assign[node.y][node.x] != node.part) continue;
        for (d = 0; d < 4; d++) {
            int nx = node.x + dirs[d][0], ny = node.y + dirs[d][1], next_cost;
            if (nx < m->min_x || nx > m->max_x || ny < m->min_y || ny > m->max_y) continue;
            if (world[ny][nx].region_id != id) continue;
            next_cost = node.cost + terrain_transition_cost(node.x, node.y, nx, ny);
            if (next_cost >= split_cost[ny][nx]) continue;
            split_cost[ny][nx] = next_cost;
            split_assign[ny][nx] = node.part;
            { ShapeNode next = {nx, ny, node.part, next_cost}; split_heap_push(next); }
        }
    }
    for (y = m->min_y; y <= m->max_y; y++)
        for (x = m->min_x; x <= m->max_x; x++)
            if (world[y][x].region_id == id && split_assign[y][x] >= 0) part_count[split_assign[y][x]]++;
    if (natural_regions[id].capital_x >= m->min_x && natural_regions[id].capital_x <= m->max_x &&
        natural_regions[id].capital_y >= m->min_y && natural_regions[id].capital_y <= m->max_y) {
        int part = split_assign[natural_regions[id].capital_y][natural_regions[id].capital_x];
        if (part >= 0 && part < pieces) preserve_part = part;
    }
    part_region[preserve_part] = id;
    for (p = 0; p < pieces; p++) {
        if (p == preserve_part) continue;
        if (part_count[p] < max(16, target_size / 8)) part_region[p] = id;
        else {
            part_region[p] = region_count++;
            natural_regions[part_region[p]].id = part_region[p];
        }
    }
    for (y = m->min_y; y <= m->max_y; y++) {
        for (x = m->min_x; x <= m->max_x; x++) {
            int part;
            if (world[y][x].region_id != id) continue;
            part = split_assign[y][x];
            if (part >= 0 && part < pieces) world[y][x].region_id = part_region[part];
        }
    }
    last_resplit_count++;
    last_local_regrow_count += pieces - 1;
    return 1;
}

static int best_tail_neighbor(int x, int y, int current, int same4) {
    int best = best_supported_neighbor(x, y, current);
    if (best < 0) return -1;
    if (neighbor_support(x, y, best, 0) < 2 || neighbor_support(x, y, best, 1) < same4) return -1;
    return best;
}

static int repair_ugly_region_edges(int id, int target_size) {
    ShapeMetrics *m = &shape_metrics[id];
    int cx = m->sum_x / max(1, m->tile_count);
    int cy = m->sum_y / max(1, m->tile_count);
    int major_x = (m->max_x - m->min_x) >= (m->max_y - m->min_y);
    int changed = 0, limit = max(8, min(target_size / 5, m->tile_count / 10));
    int x, y;
    for (y = m->min_y; y <= m->max_y && changed < limit; y++) {
        for (x = m->min_x; x <= m->max_x && changed < limit; x++) {
            int same4, far_axis, replacement;
            if (world[y][x].region_id != id || !is_land(world[y][x].geography)) continue;
            if (natural_regions[id].capital_x == x && natural_regions[id].capital_y == y) continue;
            same4 = neighbor_support(x, y, id, 1);
            far_axis = major_x ? abs(x - cx) > (m->max_x - m->min_x) / 4 :
                                 abs(y - cy) > (m->max_y - m->min_y) / 4;
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
    if (same4 == 2 && ((left && right) || (up && down))) return !(left || right ? up || down : left || right);
    return 0;
}

static int smooth_sliver_pass(void) {
    int changed = 0, x, y;
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
    int i, x, y;
    for (i = 0; i < region_count; i++) {
        if (shape_tile_count[i] <= 0 || shape_tile_count[i] >= min_size) continue;
        if (natural_regions[i].neighbor_count != 1) continue;
        for (y = 0; y < MAP_H; y++)
            for (x = 0; x < MAP_W; x++)
                if (world[y][x].region_id == i) world[y][x].region_id = natural_regions[i].neighbors[0];
        last_merge_count++;
    }
}

void regions_shape_refine(int target_size) {
    int pass; last_merge_count = 0;
    for (pass = 0; pass < SHAPE_PASSES; pass++) { rebuild_shape_counts(); if (!smooth_sliver_pass()) break; }
    rebuild_shape_counts(); merge_small_islands(max(18, target_size / 7)); }

void regions_shape_repair_ugly(int target_size) {
    int pass, i;
    last_ugly_count = last_repair_tiles = 0;
    last_ribbon_count = last_low_fill_count = last_diagonal_count = 0;
    last_resplit_count = last_shape_merge_count = last_local_regrow_count = 0;
    compute_shape_metrics();
    last_max_aspect_before = last_max_aspect_after;
    for (pass = 0; pass < 3; pass++) {
        int changed = 0;
        rebuild_shape_counts();
        compute_shape_metrics();
        for (i = 0; i < region_count; i++) {
            RegionShapeClass cls = classify_region_shape(i, target_size);
            shape_class[i] = cls;
            if (cls == REGION_SHAPE_OK || cls == REGION_SHAPE_HUGE || cls == REGION_SHAPE_DISCONNECTED) continue;
            if (pass == 0) {
                last_ugly_count++;
                if (cls == REGION_SHAPE_RIBBON || cls == REGION_SHAPE_SLIVER) last_ribbon_count++;
                if (cls == REGION_SHAPE_LOW_FILL) last_low_fill_count++;
                if (cls == REGION_SHAPE_ARTIFICIAL_DIAGONAL) last_diagonal_count++;
            }
            if ((cls == REGION_SHAPE_TINY || cls == REGION_SHAPE_SLIVER) && merge_bad_region(i, target_size) > 0) {
                changed++;
            } else if ((shape_metrics[i].tile_count > target_size * 135 / 100 ||
                        shape_metrics[i].aspect > 520 || cls == REGION_SHAPE_ARTIFICIAL_DIAGONAL) &&
                       split_bad_region(i, target_size) > 0) {
                changed++;
            } else {
                changed += repair_ugly_region_edges(i, target_size);
            }
        }
        last_repair_tiles += changed;
        if (!changed) break;
    }
    compute_shape_metrics();
}

int regions_shape_last_merge_count(void) { return last_merge_count; } int regions_shape_last_ugly_count(void) { return last_ugly_count; } int regions_shape_last_repair_tiles(void) { return last_repair_tiles; } int regions_shape_last_max_aspect_before(void) { return last_max_aspect_before; }
int regions_shape_last_max_aspect_after(void) { return last_max_aspect_after; } int regions_shape_last_ribbon_count(void) { return last_ribbon_count; } int regions_shape_last_low_fill_count(void) { return last_low_fill_count; } int regions_shape_last_diagonal_count(void) { return last_diagonal_count; }
int regions_shape_last_resplit_count(void) { return last_resplit_count; } int regions_shape_last_shape_merge_count(void) { return last_shape_merge_count; } int regions_shape_last_local_regrow_count(void) { return last_local_regrow_count; } int regions_shape_last_worst_fill_percent(void) { return last_worst_fill_percent; } int regions_shape_last_worst_perimeter_area(void) { return last_worst_perimeter_area; }
