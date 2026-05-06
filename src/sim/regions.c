#include "regions.h"

#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "sim/region_boundary.h"
#include "sim/regions_shape.h"
#include "world/terrain_query.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#define REGION_SEED_TRIES 80
#define REGION_TINY_DIVISOR 5
#define REGION_CAPITAL_EDGE_PAD 2

typedef struct {
    int x;
    int y;
    int region;
    int cost;
} RegionNode;

NaturalRegion natural_regions[MAX_NATURAL_REGIONS];
int region_count = 0;

static int grow_cost[MAX_MAP_H][MAX_MAP_W];
static RegionNode heap_nodes[MAX_MAP_W * MAX_MAP_H * 2];
static int heap_count;

static void stats_add(TerrainStats *total, TerrainStats value) {
    total->food += value.food;
    total->livestock += value.livestock;
    total->wood += value.wood;
    total->stone += value.stone;
    total->minerals += value.minerals;
    total->water += value.water;
    total->pop_capacity += value.pop_capacity;
    total->money += value.money;
    total->habitability += value.habitability;
    total->attack += value.attack;
    total->defense += value.defense;
}

static TerrainStats stats_average(TerrainStats total, int count) {
    TerrainStats avg;

    memset(&avg, 0, sizeof(avg));
    if (count <= 0) return avg;
    avg.food = total.food / count;
    avg.livestock = total.livestock / count;
    avg.wood = total.wood / count;
    avg.stone = total.stone / count;
    avg.minerals = total.minerals / count;
    avg.water = total.water / count;
    avg.pop_capacity = total.pop_capacity / count;
    avg.money = total.money / count;
    avg.habitability = total.habitability / count;
    avg.attack = total.attack / count;
    avg.defense = total.defense / count;
    return avg;
}

static void heap_push(RegionNode node) {
    int i;

    if (heap_count >= (int)(sizeof(heap_nodes) / sizeof(heap_nodes[0]))) return;
    i = heap_count++;
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (heap_nodes[parent].cost <= node.cost) break;
        heap_nodes[i] = heap_nodes[parent];
        i = parent;
    }
    heap_nodes[i] = node;
}

static int heap_pop(RegionNode *out) {
    RegionNode last;
    int i = 0;

    if (heap_count <= 0) return 0;
    *out = heap_nodes[0];
    last = heap_nodes[--heap_count];
    while (1) {
        int child = i * 2 + 1;
        if (child >= heap_count) break;
        if (child + 1 < heap_count && heap_nodes[child + 1].cost < heap_nodes[child].cost) child++;
        if (last.cost <= heap_nodes[child].cost) break;
        heap_nodes[i] = heap_nodes[child];
        i = child;
    }
    if (heap_count > 0) heap_nodes[i] = last;
    return 1;
}

static int region_crossing_cost(int x, int y, int nx, int ny) {
    return region_boundary_crossing_cost(x, y, nx, ny);
}

static int land_tile_count(void) {
    int count = 0;
    int x;
    int y;

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            if (is_land(world[y][x].geography)) count++;
        }
    }
    return count;
}

void regions_reset(void) {
    int x;
    int y;

    region_count = 0;
    memset(natural_regions, 0, sizeof(natural_regions));
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) world[y][x].region_id = -1;
    }
}

static int seed_spacing_score(int x, int y, int seed_count, const int *seed_x, const int *seed_y) {
    return region_boundary_seed_score(x, y, seed_count, seed_x, seed_y);
}

static void choose_region_seeds(int target_count, int *seed_x, int *seed_y) {
    int i;

    for (i = 0; i < target_count; i++) {
        int best_x = -1;
        int best_y = -1;
        int best_score = -1000000;
        int t;

        for (t = 0; t < REGION_SEED_TRIES; t++) {
            int x = rnd(MAP_W);
            int y = rnd(MAP_H);
            int score = seed_spacing_score(x, y, i, seed_x, seed_y);
            if (score > best_score) {
                best_score = score;
                best_x = x;
                best_y = y;
            }
        }
        if (best_x < 0) {
            for (best_y = 0; best_y < MAP_H && best_x < 0; best_y++) {
                for (best_x = 0; best_x < MAP_W; best_x++) {
                    if (is_land(world[best_y][best_x].geography)) break;
                }
                if (best_x >= MAP_W) best_x = -1;
            }
        }
        seed_x[i] = best_x < 0 ? 0 : best_x;
        seed_y[i] = best_y < 0 ? 0 : best_y;
    }
}

static void grow_regions_from_seeds(int target_count, int target_size, const int *seed_x, const int *seed_y) {
    static const int dirs[8][2] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1},
        {1, 1}, {-1, 1}, {1, -1}, {-1, -1}
    };
    int x, y, i;
    RegionNode node;

    heap_count = 0;
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) grow_cost[y][x] = INT_MAX;
    }
    for (i = 0; i < target_count; i++) {
        RegionNode seed = {seed_x[i], seed_y[i], i, 0};
        natural_regions[i].id = i;
        natural_regions[i].owner_civ = -1;
        natural_regions[i].city_id = -1;
        grow_cost[seed.y][seed.x] = 0;
        heap_push(seed);
    }
    region_count = target_count;
    while (heap_pop(&node)) {
        if (node.x < 0 || node.x >= MAP_W || node.y < 0 || node.y >= MAP_H) continue;
        if (!is_land(world[node.y][node.x].geography)) continue;
        if (node.cost != grow_cost[node.y][node.x]) continue;
        world[node.y][node.x].region_id = node.region;
        for (i = 0; i < 8; i++) {
            int nx = node.x + dirs[i][0];
            int ny = node.y + dirs[i][1];
            int next_cost;
            if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
            if (!is_land(world[ny][nx].geography)) continue;
            next_cost = node.cost + region_crossing_cost(node.x, node.y, nx, ny) + (i >= 4 ? 6 : 0) +
                        region_boundary_compactness_penalty(seed_x[node.region], seed_y[node.region], nx, ny, target_size);
            if (next_cost >= grow_cost[ny][nx]) continue;
            grow_cost[ny][nx] = next_cost;
            {
                RegionNode next = {nx, ny, node.region, next_cost};
                heap_push(next);
            }
        }
    }
}

static void add_neighbor(NaturalRegion *region, int neighbor) {
    int i;

    if (neighbor < 0 || neighbor == region->id) return;
    for (i = 0; i < region->neighbor_count; i++) {
        if (region->neighbors[i] == neighbor) return;
    }
    if (region->neighbor_count < MAX_REGION_NEIGHBORS) region->neighbors[region->neighbor_count++] = neighbor;
}

static int same_region_depth(int x, int y, int region_id) {
    int r;

    for (r = 1; r <= 7; r++) {
        int dx;
        int dy;
        for (dy = -r; dy <= r; dy++) {
            for (dx = -r; dx <= r; dx++) {
                int nx = x + dx;
                int ny = y + dy;
                if (abs(dx) != r && abs(dy) != r) continue;
                if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) return r - 1;
                if (world[ny][nx].region_id != region_id) return r - 1;
            }
        }
    }
    return 7;
}

static void consider_region_site(NaturalRegion *region, int x, int y, int *best_score) {
    TerrainStats stats = tile_stats(x, y);
    int depth;
    int score;

    if (x < REGION_CAPITAL_EDGE_PAD || y < REGION_CAPITAL_EDGE_PAD ||
        x >= MAP_W - REGION_CAPITAL_EDGE_PAD || y >= MAP_H - REGION_CAPITAL_EDGE_PAD) return;
    if (world[y][x].geography == GEO_MOUNTAIN || world[y][x].geography == GEO_CANYON ||
        world[y][x].geography == GEO_VOLCANO) return;
    if (stats.water < 2 || stats.habitability < 2) return;
    depth = same_region_depth(x, y, region->id);
    score = depth * 26 + stats.food * 5 + stats.water * 5 + stats.habitability * 6 +
            stats.pop_capacity * 5 + stats.money * 2 - world_tile_cost(x, y) * 5;
    if (world[y][x].river) score += 22;
    if (score > *best_score) {
        *best_score = score;
        region->capital_x = x;
        region->capital_y = y;
    }
}

static void rebuild_region_metadata(void) {
    static int geo_counts[MAX_NATURAL_REGIONS][GEO_COUNT];
    static int climate_counts[MAX_NATURAL_REGIONS][CLIMATE_COUNT];
    static int ecology_counts[MAX_NATURAL_REGIONS][ECO_COUNT];
    int x;
    int y;
    int i;

    memset(geo_counts, 0, sizeof(geo_counts));
    memset(climate_counts, 0, sizeof(climate_counts));
    memset(ecology_counts, 0, sizeof(ecology_counts));
    for (i = 0; i < region_count; i++) {
        int id = natural_regions[i].id;
        memset(&natural_regions[i], 0, sizeof(natural_regions[i]));
        natural_regions[i].id = id;
        natural_regions[i].alive = 1;
        natural_regions[i].owner_civ = -1;
        natural_regions[i].city_id = -1;
        natural_regions[i].port_x = -1;
        natural_regions[i].port_y = -1;
        natural_regions[i].capital_x = -1;
        natural_regions[i].capital_y = -1;
    }
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int id = world[y][x].region_id;
            TerrainStats stats;
            NaturalRegion *region;
            if (id < 0 || id >= region_count) continue;
            region = &natural_regions[id];
            stats = tile_stats(x, y);
            region->tile_count++;
            region->center_x += x;
            region->center_y += y;
            stats_add(&region->total_stats, stats);
            geo_counts[id][world[y][x].geography]++;
            climate_counts[id][world[y][x].climate]++;
            ecology_counts[id][world[y][x].ecology]++;
            consider_region_site(region, x, y, &region->cradle_score);
            if (world_is_coastal_land_tile(x, y)) {
                int score = stats.water * 4 + stats.money * 3 + stats.food + stats.habitability - world_tile_cost(x, y);
                if (!region->has_port_site || score > region->natural_defense) {
                    region->has_port_site = 1;
                    region->port_x = x;
                    region->port_y = y;
                    region->natural_defense = score;
                }
            }
        }
    }
    for (i = 0; i < region_count; i++) {
        NaturalRegion *region = &natural_regions[i];
        int g, c, e;
        int best_geo = 0;
        int best_climate = 0;
        int best_ecology = 0;
        if (region->tile_count <= 0) continue;
        region->center_x /= region->tile_count;
        region->center_y /= region->tile_count;
        region->average_stats = stats_average(region->total_stats, region->tile_count);
        for (g = 1; g < GEO_COUNT; g++) if (geo_counts[i][g] > geo_counts[i][best_geo]) best_geo = g;
        for (c = 1; c < CLIMATE_COUNT; c++) if (climate_counts[i][c] > climate_counts[i][best_climate]) best_climate = c;
        for (e = 1; e < ECO_COUNT; e++) if (ecology_counts[i][e] > ecology_counts[i][best_ecology]) best_ecology = e;
        region->dominant_geography = (Geography)best_geo;
        region->dominant_climate = (Climate)best_climate;
        region->dominant_ecology = (Ecology)best_ecology;
        region->resource_diversity = (region->average_stats.food >= 3) + (region->average_stats.water >= 3) +
                                     (region->average_stats.livestock >= 3) + (region->average_stats.wood >= 3) +
                                     (region->average_stats.stone >= 3) + (region->average_stats.minerals >= 3) + (region->average_stats.money >= 3);
        region->movement_difficulty = clamp(12 - region->average_stats.habitability + region->average_stats.defense, 1, 20);
        region->habitability = region->average_stats.habitability;
        region->natural_defense = region->average_stats.defense +
                                  (region->average_stats.stone + region->average_stats.minerals) / 3;
        region->development_score = region->average_stats.food * 4 + region->average_stats.water * 4 +
                                    region->average_stats.pop_capacity * 4 + region->average_stats.money * 3 +
                                    region->average_stats.wood + region->average_stats.minerals;
        if (region->capital_x < 0) {
            region->capital_x = region->center_x;
            region->capital_y = region->center_y;
        }
        region->cradle_score += region->development_score + region->tile_count / 4;
    }
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int id = world[y][x].region_id;
            if (id < 0) continue;
            if (x + 1 < MAP_W && world[y][x + 1].region_id != id) {
                int other = world[y][x + 1].region_id;
                add_neighbor(&natural_regions[id], other);
                if (other >= 0 && other < region_count) add_neighbor(&natural_regions[other], id);
            }
            if (y + 1 < MAP_H && world[y + 1][x].region_id != id) {
                int other = world[y + 1][x].region_id;
                add_neighbor(&natural_regions[id], other);
                if (other >= 0 && other < region_count) add_neighbor(&natural_regions[other], id);
            }
        }
    }
}

static int direction_sector(int dx, int dy) {
    if (dy < 0 && abs(dy) >= abs(dx) * 2) return 0;
    if (dy < 0 && dx > 0) return 1;
    if (dx > 0 && abs(dx) >= abs(dy) * 2) return 2;
    if (dy > 0 && dx > 0) return 3;
    if (dy > 0 && abs(dy) >= abs(dx) * 2) return 4;
    if (dy > 0 && dx < 0) return 5;
    if (dx < 0 && abs(dx) >= abs(dy) * 2) return 6;
    return 7;
}

static void compute_direction_scores(void) {
    int i;

    for (i = 0; i < region_count; i++) {
        NaturalRegion *region = &natural_regions[i];
        int n;
        int d;

        if (region->tile_count <= 0) continue;
        memset(region->direction_score, 0, sizeof(region->direction_score));
        region->viable_direction_count = 0;
        for (n = 0; n < region->neighbor_count; n++) {
            int id = region->neighbors[n];
            const NaturalRegion *neighbor;
            int sector;
            int score;
            if (id < 0 || id >= region_count) continue;
            neighbor = &natural_regions[id];
            if (neighbor->tile_count <= 0) continue;
            sector = direction_sector(neighbor->center_x - region->center_x,
                                      neighbor->center_y - region->center_y);
            score = neighbor->development_score / 5 + neighbor->resource_diversity * 8 +
                    neighbor->average_stats.food * 2 + neighbor->average_stats.water * 2 +
                    neighbor->average_stats.pop_capacity * 2 + (neighbor->has_port_site ? 8 : 0) -
                    neighbor->movement_difficulty;
            region->direction_score[sector] += max(0, score);
        }
        for (d = 0; d < REGION_DIR_COUNT; d++) {
            if (region->direction_score[d] >= 28) region->viable_direction_count++;
        }
    }
}

static void assign_unreached_land(void) {
    int x;
    int y;

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            if (!is_land(world[y][x].geography) || world[y][x].region_id >= 0) continue;
            if (region_count < MAX_NATURAL_REGIONS) {
                world[y][x].region_id = region_count;
                natural_regions[region_count].id = region_count;
                natural_regions[region_count].owner_civ = -1;
                natural_regions[region_count].city_id = -1;
                region_count++;
            } else {
                world[y][x].region_id = 0;
            }
        }
    }
}

static void merge_tiny_regions(int min_size) {
    int i;

    rebuild_region_metadata();
    for (i = 0; i < region_count; i++) {
        NaturalRegion *region = &natural_regions[i];
        int best = -1;
        int best_tiles = -1;
        int n;
        if (region->tile_count <= 0 || region->tile_count >= min_size || region->neighbor_count <= 0) continue;
        for (n = 0; n < region->neighbor_count; n++) {
            int neighbor = region->neighbors[n];
            if (neighbor >= 0 && neighbor < region_count && natural_regions[neighbor].tile_count > best_tiles) {
                best = neighbor;
                best_tiles = natural_regions[neighbor].tile_count;
            }
        }
        if (best >= 0) {
            int x;
            int y;
            for (y = 0; y < MAP_H; y++) {
                for (x = 0; x < MAP_W; x++) {
                    if (world[y][x].region_id == i) world[y][x].region_id = best;
                }
            }
        }
    }
}

void regions_generate(int region_size_value) {
    static int seed_x[MAX_NATURAL_REGIONS];
    static int seed_y[MAX_NATURAL_REGIONS];
    int land = land_tile_count();
    int target_size = 220 + clamp(region_size_value, 0, 100) * 18;
    int target_count;

    regions_reset();
    if (land <= 0) {
        dirty_mark_territory();
        return;
    }
    target_count = clamp(land / target_size, 1, MAX_NATURAL_REGIONS);
    choose_region_seeds(target_count, seed_x, seed_y);
    grow_regions_from_seeds(target_count, target_size, seed_x, seed_y);
    assign_unreached_land();
    merge_tiny_regions(max(24, target_size / REGION_TINY_DIVISOR));
    regions_shape_refine(target_size);
    rebuild_region_metadata();
    compute_direction_scores();
    region_boundary_debug_summary();
    dirty_mark_territory();
    world_visual_revision++;
}

const NaturalRegion *regions_get(int region_id) {
    if (region_id < 0 || region_id >= region_count || natural_regions[region_id].tile_count <= 0) return NULL;
    return &natural_regions[region_id];
}
