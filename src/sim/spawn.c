#include "spawn.h"

#include "core/game_types.h"
#include "sim/province.h"
#include "sim/regions.h"
#include "world/terrain_query.h"

#include <string.h>

#define SPAWN_CORE_RADIUS 5
#define SPAWN_EXPANSION_BUDGET 60
#define SPAWN_KEEP_CANDIDATES 80
#define SPAWN_HARD_DISTANCE 30
#define SPAWN_SOFT_DISTANCE 60

typedef struct {
    int x;
    int y;
    int score;
} SpawnCandidate;

typedef struct {
    int usable_land;
    int good_land;
    int food_total;
    int water_total;
    int habitability_total;
    int diversity_mask;
    int mountain_neighbors;
} CoreArea;

typedef struct {
    int land;
    int good;
    int food_total;
    int water_total;
    int diversity_mask;
    int direction_score[8];
    int direction_good[8];
    int viable_directions;
} ReachArea;

static int flood_cost[MAX_MAP_H][MAX_MAP_W];
static int flood_seen[MAX_MAP_H][MAX_MAP_W];
static int flood_stamp = 1;
static int queue_x[MAX_MAP_W * MAX_MAP_H];
static int queue_y[MAX_MAP_W * MAX_MAP_H];

static int resource_mask_for_stats(TerrainStats stats) {
    int mask = 0;
    if (stats.food >= 5) mask |= 1 << 0;
    if (stats.water >= 5) mask |= 1 << 1;
    if (stats.livestock >= 5) mask |= 1 << 2;
    if (stats.wood >= 5) mask |= 1 << 3;
    if (stats.stone >= 5) mask |= 1 << 4;
    if (stats.minerals >= 5) mask |= 1 << 5;
    if (stats.money >= 5) mask |= 1 << 6;
    return mask;
}

static int bit_count(int value) {
    int count = 0;
    while (value) {
        count += value & 1;
        value >>= 1;
    }
    return count;
}

static int is_basic_cradle_tile(int x, int y) {
    TerrainStats stats;

    if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) return 0;
    if (!is_land(world[y][x].geography)) return 0;
    if (world[y][x].owner != -1 || world[y][x].province_id != -1) return 0;
    if (world[y][x].geography == GEO_MOUNTAIN || world[y][x].geography == GEO_CANYON ||
        world[y][x].geography == GEO_VOLCANO) {
        return 0;
    }
    if (province_city_too_close(x, y, SPAWN_HARD_DISTANCE)) return 0;
    if (!province_city_site_has_room(x, y, -1, 3)) return 0;
    if (world_tile_cost(x, y) > 9) return 0;
    stats = tile_stats(x, y);
    return stats.habitability >= 4 && stats.food >= 3 && stats.water >= 3 && stats.pop_capacity >= 3;
}

static int nearest_city_distance_sq(int x, int y) {
    int best = MAP_W * MAP_W + MAP_H * MAP_H;
    int i;

    for (i = 0; i < city_count; i++) {
        int dx;
        int dy;
        int dist;
        if (!cities[i].alive) continue;
        dx = x - cities[i].x;
        dy = y - cities[i].y;
        dist = dx * dx + dy * dy;
        if (dist < best) best = dist;
    }
    return best;
}

static int distance_penalty(int x, int y) {
    int dist_sq = nearest_city_distance_sq(x, y);
    int hard_sq = SPAWN_HARD_DISTANCE * SPAWN_HARD_DISTANCE;
    int soft_sq = SPAWN_SOFT_DISTANCE * SPAWN_SOFT_DISTANCE;

    if (city_count <= 0) return 0;
    if (dist_sq < hard_sq) return 1000000;
    if (dist_sq >= soft_sq) return 0;
    return (soft_sq - dist_sq) / 18;
}

static int good_tile(TerrainStats stats, int x, int y) {
    return stats.habitability >= 4 && stats.food >= 3 && stats.water >= 3 && world_tile_cost(x, y) <= 9;
}

static CoreArea evaluate_core_area(int cx, int cy) {
    CoreArea area;
    int dy;
    int dx;

    memset(&area, 0, sizeof(area));
    for (dy = -SPAWN_CORE_RADIUS; dy <= SPAWN_CORE_RADIUS; dy++) {
        for (dx = -SPAWN_CORE_RADIUS; dx <= SPAWN_CORE_RADIUS; dx++) {
            int x = cx + dx;
            int y = cy + dy;
            TerrainStats stats;
            if (dx * dx + dy * dy > SPAWN_CORE_RADIUS * SPAWN_CORE_RADIUS) continue;
            if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) continue;
            if (!is_land(world[y][x].geography)) continue;
            if (world[y][x].geography == GEO_MOUNTAIN) area.mountain_neighbors++;
            if (world[y][x].owner != -1 || world[y][x].province_id != -1) continue;
            stats = tile_stats(x, y);
            area.usable_land++;
            area.food_total += stats.food;
            area.water_total += stats.water;
            area.habitability_total += stats.habitability;
            area.diversity_mask |= resource_mask_for_stats(stats);
            if (good_tile(stats, x, y)) area.good_land++;
        }
    }
    return area;
}

static int direction_sector(int dx, int dy) {
    int sx = dx > 0 ? 1 : (dx < 0 ? -1 : 0);
    int sy = dy > 0 ? 1 : (dy < 0 ? -1 : 0);
    if (sy < 0 && sx == 0) return 0;
    if (sy < 0 && sx > 0) return 1;
    if (sy == 0 && sx > 0) return 2;
    if (sy > 0 && sx > 0) return 3;
    if (sy > 0 && sx == 0) return 4;
    if (sy > 0 && sx < 0) return 5;
    if (sy == 0 && sx < 0) return 6;
    return 7;
}

static void push_reachable(int x, int y, int cost, int *tail) {
    int max_nodes = MAP_W * MAP_H;
    if (*tail >= max_nodes) return;
    flood_seen[y][x] = flood_stamp;
    flood_cost[y][x] = cost;
    queue_x[*tail] = x;
    queue_y[*tail] = y;
    (*tail)++;
}

static ReachArea evaluate_reachable_area(int cx, int cy) {
    static const int dirs[8][2] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1},
        {1, 1}, {-1, 1}, {1, -1}, {-1, -1}
    };
    ReachArea area;
    int head = 0;
    int tail = 0;
    int i;

    memset(&area, 0, sizeof(area));
    if (++flood_stamp <= 0) {
        memset(flood_seen, 0, sizeof(flood_seen));
        flood_stamp = 1;
    }
    push_reachable(cx, cy, 0, &tail);
    while (head < tail) {
        int x = queue_x[head];
        int y = queue_y[head];
        int cost = flood_cost[y][x];
        TerrainStats stats = tile_stats(x, y);
        int sector = x == cx && y == cy ? -1 : direction_sector(x - cx, y - cy);
        int value = world_terrain_resource_value(stats);

        head++;
        area.land++;
        area.food_total += stats.food;
        area.water_total += stats.water;
        area.diversity_mask |= resource_mask_for_stats(stats);
        if (good_tile(stats, x, y)) area.good++;
        if (sector >= 0) {
            area.direction_score[sector] += value / 8 + stats.food * 2 + stats.water * 2 + max(0, 12 - cost);
            if (good_tile(stats, x, y)) area.direction_good[sector]++;
        }
        for (i = 0; i < 8; i++) {
            int nx = x + dirs[i][0];
            int ny = y + dirs[i][1];
            int step;
            int next_cost;
            if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
            if (!is_land(world[ny][nx].geography)) continue;
            if (world[ny][nx].owner != -1 || world[ny][nx].province_id != -1) continue;
            step = world_tile_cost(nx, ny) + (i >= 4 ? 2 : 0);
            next_cost = cost + step;
            if (next_cost > SPAWN_EXPANSION_BUDGET) continue;
            if (flood_seen[ny][nx] == flood_stamp && flood_cost[ny][nx] <= next_cost) continue;
            push_reachable(nx, ny, next_cost, &tail);
        }
    }
    for (i = 0; i < 8; i++) {
        if (area.direction_good[i] >= 3 && area.direction_score[i] >= 70) area.viable_directions++;
    }
    return area;
}

static int special_one_direction_start(int x, int y, CoreArea core, ReachArea reach) {
    int diversity = bit_count(core.diversity_mask | reach.diversity_mask);
    if (reach.viable_directions < 1) return 0;
    if (world_is_coastal_land_tile(x, y) && reach.land >= 45 && reach.good >= 16 && diversity >= 3) return 1;
    if (world[y][x].geography == GEO_OASIS && core.water_total >= core.usable_land * 5 && reach.good >= 14) return 1;
    if (core.mountain_neighbors >= 8 && core.good_land >= 8 && diversity >= 3) return 1;
    return 0;
}

static int score_candidate(int x, int y, int *out_score) {
    TerrainStats stats;
    CoreArea core;
    ReachArea reach;
    int diversity;
    int penalty;
    int avg_food;
    int avg_water;
    int avg_habitability;
    int capital_score;
    int score;

    if (!is_basic_cradle_tile(x, y)) return 0;
    penalty = distance_penalty(x, y);
    if (penalty >= 1000000) return 0;
    core = evaluate_core_area(x, y);
    if (core.usable_land < 20 || core.good_land < 8) return 0;
    avg_food = core.food_total / core.usable_land;
    avg_water = core.water_total / core.usable_land;
    avg_habitability = core.habitability_total / core.usable_land;
    if (avg_food < 3 || avg_water < 3 || avg_habitability < 3) return 0;
    reach = evaluate_reachable_area(x, y);
    diversity = bit_count(core.diversity_mask | reach.diversity_mask);
    if (reach.land < 60 || reach.good < 20 || diversity < 3) return 0;
    if (reach.food_total < reach.land * 3 || reach.water_total < reach.land * 3) return 0;
    if (reach.viable_directions < 2 && !special_one_direction_start(x, y, core, reach)) return 0;

    stats = tile_stats(x, y);
    capital_score = stats.habitability * 5 + stats.food * 4 + stats.water * 4 +
                    stats.pop_capacity * 4 + stats.money * 2 - world_tile_cost(x, y) * 4;
    if (world[y][x].river) capital_score += 24;
    if (world_is_coastal_land_tile(x, y)) capital_score += 14;
    score = capital_score + core.good_land * 5 + core.usable_land + reach.good * 2 + reach.land / 2 +
            reach.viable_directions * 30 + diversity * 15 - penalty;
    if (reach.viable_directions < 2) score -= 35;
    *out_score = score;
    return 1;
}

static void insert_candidate(SpawnCandidate *candidates, int *count, SpawnCandidate candidate) {
    int i;
    int limit = *count < SPAWN_KEEP_CANDIDATES ? *count : SPAWN_KEEP_CANDIDATES - 1;

    if (*count >= SPAWN_KEEP_CANDIDATES && candidate.score <= candidates[limit].score) return;
    if (*count < SPAWN_KEEP_CANDIDATES) (*count)++;
    for (i = *count - 1; i > 0 && candidates[i - 1].score < candidate.score; i--) {
        candidates[i] = candidates[i - 1];
    }
    candidates[i] = candidate;
}

static int choose_weighted_candidate(SpawnCandidate *candidates, int count, int *out_x, int *out_y) {
    int limit;
    int lowest;
    int total = 0;
    int roll;
    int i;

    if (count <= 0) return 0;
    limit = min(count, 20);
    if (count >= 10) limit = min(limit, max(5, count / 10));
    lowest = candidates[limit - 1].score;
    for (i = 0; i < limit; i++) total += max(1, candidates[i].score - lowest + 1);
    roll = rnd(total);
    for (i = 0; i < limit; i++) {
        roll -= max(1, candidates[i].score - lowest + 1);
        if (roll < 0) {
            *out_x = candidates[i].x;
            *out_y = candidates[i].y;
            return 1;
        }
    }
    *out_x = candidates[0].x;
    *out_y = candidates[0].y;
    return 1;
}

int spawn_select_civilization_cradle(int preferred_x, int preferred_y, int *out_x, int *out_y) {
    SpawnCandidate candidates[SPAWN_KEEP_CANDIDATES];
    int candidate_count = 0;
    int samples = clamp(MAP_W * MAP_H / 180, 1000, 3000);
    int i;
    int score;
    int region_id;

    if (!out_x || !out_y) return 0;
    if (regions_select_spawn_region(preferred_x, preferred_y, &region_id, out_x, out_y)) return 1;
    if (score_candidate(preferred_x, preferred_y, &score)) {
        *out_x = preferred_x;
        *out_y = preferred_y;
        return 1;
    }
    for (i = 0; i < samples; i++) {
        SpawnCandidate candidate;
        candidate.x = rnd(MAP_W);
        candidate.y = rnd(MAP_H);
        if (!score_candidate(candidate.x, candidate.y, &candidate.score)) continue;
        insert_candidate(candidates, &candidate_count, candidate);
    }
    return choose_weighted_candidate(candidates, candidate_count, out_x, out_y);
}
