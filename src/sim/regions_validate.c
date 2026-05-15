#include "regions_validate.h"

#include "core/game_types.h"
#include "sim/region_boundary.h"
#include "sim/regions.h"
#include "world/terrain_query.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SPLIT_PARTS 4
#define MAX_COMPONENTS 96

typedef struct {
    int tile_count;
    int min_x, min_y, max_x, max_y;
    int sum_x, sum_y;
    int perimeter;
    int components;
    int largest_component;
    int elongation;
    int compactness;
    int protected_shape;
} RegionMeasure;

static RegionValidationStats last_stats;
static RegionMeasure measure[MAX_NATURAL_REGIONS];
static unsigned char seen[MAX_MAP_H][MAX_MAP_W];
static int work_assign[MAX_MAP_W * MAX_MAP_H];
static int queue_cells[MAX_MAP_W * MAX_MAP_H];

static int cell_index(int x, int y) { return y * MAX_MAP_W + x; }
static int in_map(int x, int y) { return x >= 0 && y >= 0 && x < MAP_W && y < MAP_H; }

static int region_tile(int x, int y, int id) {
    return in_map(x, y) && world[y][x].region_id == id && is_land(world[y][x].geography);
}

static int is_protected_geography(Geography g) {
    return g == GEO_ISLAND || g == GEO_DELTA || g == GEO_MOUNTAIN ||
           g == GEO_CANYON || g == GEO_WETLAND || g == GEO_COAST;
}

static void reset_measure(void) {
    int i;
    memset(measure, 0, sizeof(measure));
    for (i = 0; i < MAX_NATURAL_REGIONS; i++) {
        measure[i].min_x = MAP_W;
        measure[i].min_y = MAP_H;
        measure[i].max_x = -1;
        measure[i].max_y = -1;
    }
}

static void measure_basic(void) {
    static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int x, y, d, i;

    reset_measure();
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int id = world[y][x].region_id;
            RegionMeasure *m;
            if (id < 0 || id >= region_count || !is_land(world[y][x].geography)) continue;
            m = &measure[id];
            m->tile_count++;
            m->sum_x += x;
            m->sum_y += y;
            if (x < m->min_x) m->min_x = x;
            if (y < m->min_y) m->min_y = y;
            if (x > m->max_x) m->max_x = x;
            if (y > m->max_y) m->max_y = y;
            if (world[y][x].river || world_is_coastal_land_tile(x, y) ||
                is_protected_geography(world[y][x].geography)) {
                m->protected_shape = 1;
            }
            for (d = 0; d < 4; d++) {
                int nx = x + dirs[d][0], ny = y + dirs[d][1];
                if (!region_tile(nx, ny, id)) m->perimeter++;
            }
        }
    }
    for (i = 0; i < region_count; i++) {
        RegionMeasure *m = &measure[i];
        int w, h, area;
        if (m->tile_count <= 0) continue;
        w = m->max_x - m->min_x + 1;
        h = m->max_y - m->min_y + 1;
        area = w * h;
        m->elongation = max(w, h) * 100 / max(1, min(w, h));
        m->compactness = m->tile_count * 100 / max(1, area);
    }
}

static int add_neighbor_score(int *ids, int *scores, int *count, int id, int score) {
    int i;
    if (id < 0 || id >= region_count) return 0;
    for (i = 0; i < *count; i++) {
        if (ids[i] == id) {
            scores[i] += score;
            return 1;
        }
    }
    if (*count >= MAX_REGION_NEIGHBORS * 3) return 0;
    ids[*count] = id;
    scores[*count] = score;
    (*count)++;
    return 1;
}

static int best_neighbor_for_region(int id, int allow_strong) {
    static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int ids[MAX_REGION_NEIGHBORS * 3];
    int scores[MAX_REGION_NEIGHBORS * 3];
    int count = 0;
    int x, y, d, i;
    int best = -1, best_score = INT_MIN;
    RegionMeasure *m = &measure[id];

    memset(ids, 0, sizeof(ids));
    memset(scores, 0, sizeof(scores));
    for (y = m->min_y; y <= m->max_y; y++) {
        for (x = m->min_x; x <= m->max_x; x++) {
            if (!region_tile(x, y, id)) continue;
            for (d = 0; d < 4; d++) {
                int nx = x + dirs[d][0], ny = y + dirs[d][1];
                int other, cost, score;
                if (!in_map(nx, ny)) continue;
                other = world[ny][nx].region_id;
                if (other < 0 || other == id || other >= region_count) continue;
                cost = region_boundary_crossing_cost(x, y, nx, ny);
                if (!allow_strong && cost >= 90) continue;
                score = 100 - cost + measure[other].tile_count / 12;
                add_neighbor_score(ids, scores, &count, other, score);
            }
        }
    }
    for (i = 0; i < count; i++) {
        if (scores[i] > best_score) {
            best_score = scores[i];
            best = ids[i];
        }
    }
    return best;
}

static int best_neighbor_for_component(int id, int comp, int allow_strong) {
    static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int ids[MAX_REGION_NEIGHBORS * 3];
    int scores[MAX_REGION_NEIGHBORS * 3];
    int count = 0;
    int x, y, d, i;
    int best = -1, best_score = INT_MIN;
    RegionMeasure *m = &measure[id];

    memset(ids, 0, sizeof(ids));
    memset(scores, 0, sizeof(scores));
    for (y = m->min_y; y <= m->max_y; y++) {
        for (x = m->min_x; x <= m->max_x; x++) {
            if (world[y][x].region_id != id || work_assign[cell_index(x, y)] != comp) continue;
            for (d = 0; d < 4; d++) {
                int nx = x + dirs[d][0], ny = y + dirs[d][1];
                int other, cost;
                if (!in_map(nx, ny)) continue;
                other = world[ny][nx].region_id;
                if (other < 0 || other == id || other >= region_count) continue;
                cost = region_boundary_crossing_cost(x, y, nx, ny);
                if (!allow_strong && cost >= 90) continue;
                add_neighbor_score(ids, scores, &count, other, 100 - cost + measure[other].tile_count / 14);
            }
        }
    }
    for (i = 0; i < count; i++) {
        if (scores[i] > best_score) {
            best_score = scores[i];
            best = ids[i];
        }
    }
    return best;
}

static int merge_tiny_regions_pass(int target_size) {
    int tiny_limit = max(16, target_size / 4);
    int protected_limit = max(10, target_size / 8);
    int changed = 0;
    int i, x, y;

    measure_basic();
    for (i = 0; i < region_count; i++) {
        int best;
        if (measure[i].tile_count <= 0 || measure[i].tile_count >= tiny_limit) continue;
        last_stats.tiny_regions++;
        if (measure[i].protected_shape && measure[i].tile_count >= protected_limit) continue;
        best = best_neighbor_for_region(i, measure[i].tile_count < protected_limit);
        if (best < 0) continue;
        for (y = measure[i].min_y; y <= measure[i].max_y; y++) {
            for (x = measure[i].min_x; x <= measure[i].max_x; x++) {
                if (world[y][x].region_id == i) world[y][x].region_id = best;
            }
        }
        changed++;
        last_stats.tiny_merged++;
    }
    return changed;
}

static int flood_component(int sx, int sy, int id, int comp) {
    static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int head = 0, tail = 0, count = 0;
    int idx = cell_index(sx, sy);
    seen[sy][sx] = 1;
    work_assign[idx] = comp;
    queue_cells[tail++] = idx;
    while (head < tail) {
        int cur = queue_cells[head++];
        int x = cur % MAX_MAP_W;
        int y = cur / MAX_MAP_W;
        int d;
        count++;
        for (d = 0; d < 4; d++) {
            int nx = x + dirs[d][0], ny = y + dirs[d][1];
            int nidx;
            if (!region_tile(nx, ny, id) || seen[ny][nx]) continue;
            nidx = cell_index(nx, ny);
            seen[ny][nx] = 1;
            work_assign[nidx] = comp;
            queue_cells[tail++] = nidx;
        }
    }
    return count;
}

static int reassign_disconnected_components(void) {
    int changed = 0;
    int id, x, y;

    measure_basic();
    memset(seen, 0, sizeof(seen));
    for (id = 0; id < region_count; id++) {
        int comp_count = 0;
        int comp_size[MAX_COMPONENTS];
        int best_comp = 0, best_size = 0;
        if (measure[id].tile_count <= 0) continue;
        for (y = measure[id].min_y; y <= measure[id].max_y; y++) {
            for (x = measure[id].min_x; x <= measure[id].max_x; x++) {
                if (!region_tile(x, y, id) || seen[y][x]) continue;
                if (comp_count >= MAX_COMPONENTS) continue;
                comp_size[comp_count] = flood_component(x, y, id, comp_count);
                if (comp_size[comp_count] > best_size) {
                    best_size = comp_size[comp_count];
                    best_comp = comp_count;
                }
                comp_count++;
            }
        }
        measure[id].components = comp_count;
        measure[id].largest_component = best_size;
        if (comp_count <= 1) continue;
        last_stats.disconnected_regions++;
        for (y = measure[id].min_y; y <= measure[id].max_y; y++) {
            for (x = measure[id].min_x; x <= measure[id].max_x; x++) {
                int comp, best;
                if (world[y][x].region_id != id) continue;
                comp = work_assign[cell_index(x, y)];
                if (comp == best_comp || comp < 0 || comp >= comp_count) continue;
                best = best_neighbor_for_component(id, comp, comp_size[comp] < 10);
                if (best >= 0) {
                    world[y][x].region_id = best;
                    changed++;
                }
            }
        }
    }
    last_stats.disconnected_reassigned += changed;
    return changed;
}

static int choose_center_seed(int id, int *out_x, int *out_y) {
    RegionMeasure *m = &measure[id];
    int cx = m->sum_x / max(1, m->tile_count);
    int cy = m->sum_y / max(1, m->tile_count);
    int best_dist = INT_MAX, x, y;
    *out_x = m->min_x;
    *out_y = m->min_y;
    for (y = m->min_y; y <= m->max_y; y++) {
        for (x = m->min_x; x <= m->max_x; x++) {
            int dx, dy, dist;
            if (!region_tile(x, y, id)) continue;
            dx = x - cx; dy = y - cy; dist = dx * dx + dy * dy;
            if (dist < best_dist) {
                best_dist = dist;
                *out_x = x;
                *out_y = y;
            }
        }
    }
    return best_dist < INT_MAX;
}

static void choose_far_seed(int id, int part, int *seed_x, int *seed_y) {
    RegionMeasure *m = &measure[id];
    int best_score = -1, best_x = seed_x[0], best_y = seed_y[0];
    int x, y, p;
    for (y = m->min_y; y <= m->max_y; y++) {
        for (x = m->min_x; x <= m->max_x; x++) {
            int closest = INT_MAX;
            if (!region_tile(x, y, id)) continue;
            for (p = 0; p < part; p++) {
                int dx = x - seed_x[p], dy = y - seed_y[p];
                int dist = dx * dx + dy * dy;
                if (dist < closest) closest = dist;
            }
            if (closest > best_score) {
                best_score = closest;
                best_x = x;
                best_y = y;
            }
        }
    }
    seed_x[part] = best_x;
    seed_y[part] = best_y;
}

static int split_huge_region(int id, int target_size) {
    static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int pieces = clamp(measure[id].tile_count / max(1, target_size), 2, MAX_SPLIT_PARTS);
    int available = MAX_NATURAL_REGIONS - region_count;
    int seed_x[MAX_SPLIT_PARTS], seed_y[MAX_SPLIT_PARTS];
    int part_count[MAX_SPLIT_PARTS] = {0};
    int part_region[MAX_SPLIT_PARTS];
    int head = 0, tail = 0;
    int x, y, p;

    if (available <= 0) {
        last_stats.cap_reached = 1;
        return 0;
    }
    pieces = min(pieces, available + 1);
    if (pieces < 2 || !choose_center_seed(id, &seed_x[0], &seed_y[0])) return 0;
    for (p = 1; p < pieces; p++) choose_far_seed(id, p, seed_x, seed_y);
    for (y = measure[id].min_y; y <= measure[id].max_y; y++)
        for (x = measure[id].min_x; x <= measure[id].max_x; x++)
            work_assign[cell_index(x, y)] = -1;
    for (p = 0; p < pieces; p++) {
        int idx = cell_index(seed_x[p], seed_y[p]);
        work_assign[idx] = p;
        queue_cells[tail++] = idx;
        part_count[p]++;
    }
    while (head < tail) {
        int cur = queue_cells[head++];
        int cx = cur % MAX_MAP_W, cy = cur / MAX_MAP_W;
        int part = work_assign[cur], d;
        for (d = 0; d < 4; d++) {
            int nx = cx + dirs[d][0], ny = cy + dirs[d][1], nidx;
            if (!region_tile(nx, ny, id)) continue;
            nidx = cell_index(nx, ny);
            if (work_assign[nidx] >= 0) continue;
            work_assign[nidx] = part;
            part_count[part]++;
            queue_cells[tail++] = nidx;
        }
    }
    part_region[0] = id;
    for (p = 1; p < pieces; p++) {
        part_region[p] = part_count[p] > 0 ? region_count++ : id;
        natural_regions[part_region[p]].id = part_region[p];
    }
    for (y = measure[id].min_y; y <= measure[id].max_y; y++) {
        for (x = measure[id].min_x; x <= measure[id].max_x; x++) {
            int part;
            if (world[y][x].region_id != id) continue;
            part = work_assign[cell_index(x, y)];
            if (part >= 0 && part < pieces) world[y][x].region_id = part_region[part];
        }
    }
    last_stats.huge_split += pieces - 1;
    return pieces - 1;
}

static int split_huge_regions_pass(int target_size) {
    int huge_limit = max(target_size * 5 / 2, target_size + 80);
    int changed = 0, i;
    measure_basic();
    for (i = 0; i < region_count; i++) {
        if (measure[i].tile_count <= huge_limit) continue;
        last_stats.huge_regions++;
        if (split_huge_region(i, target_size) > 0) changed++;
        if (region_count >= MAX_NATURAL_REGIONS) {
            last_stats.cap_reached = 1;
            break;
        }
    }
    return changed;
}

static int smooth_slivers_pass(void) {
    static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int changed = 0, x, y, d;
    measure_basic();
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int id = world[y][x].region_id;
            int same = 0, best = -1, best_support = 0;
            if (id < 0 || !is_land(world[y][x].geography)) continue;
            for (d = 0; d < 4; d++) {
                int nx = x + dirs[d][0], ny = y + dirs[d][1];
                if (region_tile(nx, ny, id)) same++;
            }
            if (same >= 2 && measure[id].elongation < 420) continue;
            last_stats.thin_regions += same <= 1;
            for (d = 0; d < 4; d++) {
                int nx = x + dirs[d][0], ny = y + dirs[d][1];
                int other, support, cost;
                if (!in_map(nx, ny)) continue;
                other = world[ny][nx].region_id;
                if (other < 0 || other == id) continue;
                cost = region_boundary_crossing_cost(x, y, nx, ny);
                if (cost >= 86) continue;
                support = measure[other].tile_count / 80 + 4 - cost / 24;
                if (support > best_support) {
                    best_support = support;
                    best = other;
                }
            }
            if (best >= 0) {
                world[y][x].region_id = best;
                changed++;
            }
        }
    }
    last_stats.sliver_smoothed += changed;
    return changed;
}

void regions_validate_postprocess(int target_size) {
    char buffer[256];
    memset(&last_stats, 0, sizeof(last_stats));
    last_stats.target_size = target_size;
    reassign_disconnected_components();
    merge_tiny_regions_pass(target_size);
    split_huge_regions_pass(target_size);
    smooth_slivers_pass();
    merge_tiny_regions_pass(target_size);
    measure_basic();
    snprintf(buffer, sizeof(buffer),
             "World Sim: region_validation target=%d tiny=%d merged=%d huge=%d split=%d disconnected=%d reassigned=%d slivers=%d cap=%d\n",
             target_size, last_stats.tiny_regions, last_stats.tiny_merged,
             last_stats.huge_regions, last_stats.huge_split,
             last_stats.disconnected_regions, last_stats.disconnected_reassigned,
             last_stats.sliver_smoothed, last_stats.cap_reached);
    OutputDebugStringA(buffer);
}

const RegionValidationStats *regions_validate_last_stats(void) {
    return &last_stats;
}
