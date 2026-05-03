#include "province.h"

#include "sim/simulation.h"
#include "world/ports.h"
#include "world/terrain_query.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    int x;
    int y;
} ProvinceFrontierNode;

static int province_cache[MAP_H][MAP_W];
static int province_cache_dirty = 1;
static RegionSummary region_summary_cache[MAX_CITIES];
static int region_summary_dirty = 1;
static int province_claim_cost[MAP_H][MAP_W];
static int province_border_distance[MAP_H][MAP_W];
static ProvinceFrontierNode province_frontier[MAP_W * MAP_H];

void province_invalidate_region_cache(void) {
    province_cache_dirty = 1;
    region_summary_dirty = 1;
}

static int compute_city_for_tile_uncached(int x, int y) {
    int province_id;

    if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) return -1;
    province_id = world[y][x].province_id;
    if (province_id < 0 || province_id >= city_count || !cities[province_id].alive) return -1;
    return province_id;
}

static void rebuild_province_cache(void) {
    int x;
    int y;

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            province_cache[y][x] = compute_city_for_tile_uncached(x, y);
        }
    }
    province_cache_dirty = 0;
    region_summary_dirty = 1;
}

static void ensure_province_cache(void) {
    if (province_cache_dirty) rebuild_province_cache();
}

int province_city_for_tile(int x, int y) {
    if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) return -1;
    ensure_province_cache();
    return province_cache[y][x];
}

static void rebuild_region_summary_cache(void) {
    int i;
    int x;
    int y;

    ensure_province_cache();
    memset(region_summary_cache, 0, sizeof(region_summary_cache));
    for (i = 0; i < MAX_CITIES; i++) region_summary_cache[i].city_id = i;
    for (i = 0; i < city_count; i++) {
        if (cities[i].alive) region_summary_cache[i].population = cities[i].population;
    }

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int city_id = province_cache[y][x];
            TerrainStats stats;

            if (city_id < 0 || city_id >= city_count || !cities[city_id].alive) continue;
            stats = tile_stats(x, y);
            region_summary_cache[city_id].tiles++;
            region_summary_cache[city_id].food += stats.food;
            region_summary_cache[city_id].livestock += stats.livestock;
            region_summary_cache[city_id].wood += stats.wood;
            region_summary_cache[city_id].stone += stats.stone;
            region_summary_cache[city_id].minerals += stats.minerals;
            region_summary_cache[city_id].water += stats.water;
            region_summary_cache[city_id].pop_capacity += stats.pop_capacity;
            region_summary_cache[city_id].money += stats.money;
            region_summary_cache[city_id].habitability += stats.habitability;
            region_summary_cache[city_id].attack += stats.attack;
            region_summary_cache[city_id].defense += stats.defense;
        }
    }

    for (i = 0; i < city_count; i++) {
        RegionSummary *summary = &region_summary_cache[i];
        if (summary->tiles <= 0) continue;
        summary->food /= summary->tiles;
        summary->livestock /= summary->tiles;
        summary->wood /= summary->tiles;
        summary->stone /= summary->tiles;
        summary->minerals /= summary->tiles;
        summary->water /= summary->tiles;
        summary->pop_capacity /= summary->tiles;
        summary->money /= summary->tiles;
        summary->habitability /= summary->tiles;
        summary->attack /= summary->tiles;
        summary->defense /= summary->tiles;
    }
    region_summary_dirty = 0;
}

RegionSummary province_summarize_city_region(int city_id) {
    RegionSummary summary;

    memset(&summary, 0, sizeof(summary));
    summary.city_id = city_id;
    if (city_id < 0 || city_id >= city_count) return summary;
    if (region_summary_dirty) rebuild_region_summary_cache();
    return region_summary_cache[city_id];
}

static int city_too_close_except(int x, int y, int min_distance, int except_city) {
    int i;
    int min_distance_sq = min_distance * min_distance;

    for (i = 0; i < city_count; i++) {
        int dx;
        int dy;
        if (i == except_city) continue;
        if (!cities[i].alive) continue;
        dx = x - cities[i].x;
        dy = y - cities[i].y;
        if (dx * dx + dy * dy < min_distance_sq) return 1;
    }
    return 0;
}

int province_city_too_close(int x, int y, int min_distance) {
    return city_too_close_except(x, y, min_distance, -1);
}

int province_city_site_has_room(int x, int y, int owner, int radius) {
    int open_tiles = 0;
    int needed_tiles = 0;
    int dy;
    int dx;

    for (dy = -radius; dy <= radius; dy++) {
        for (dx = -radius; dx <= radius; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            if (dx * dx + dy * dy > radius * radius) continue;
            needed_tiles++;
            if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
            if (!is_land(world[ny][nx].geography)) continue;
            if (world[ny][nx].province_id >= 0) continue;
            if (owner < 0 && world[ny][nx].owner != -1) continue;
            if (owner >= 0 && world[ny][nx].owner != -1 && world[ny][nx].owner != owner) continue;
            open_tiles++;
        }
    }
    return open_tiles * 100 >= needed_tiles * 68;
}

int province_city_radius_for_tile(int x, int y, int population) {
    TerrainStats stats = tile_stats(x, y);
    int resource_score = stats.food + stats.livestock + stats.water + stats.habitability + stats.wood / 2 + stats.minerals / 2;
    return clamp(2 + population / 180 + resource_score / 8, 3, 10);
}

static int province_entry_cost(int city_id, int x, int y, int from_x, int from_y) {
    TerrainStats stats = tile_stats(x, y);
    City *city = &cities[city_id];
    int cost;
    int elevation_step = abs(world[y][x].elevation - world[from_y][from_x].elevation) / 8;
    int elevation_from_city = abs(world[y][x].elevation - world[city->y][city->x].elevation) / 18;

    cost = 8 + world_tile_cost(x, y) + elevation_step + elevation_from_city;
    if (world[y][x].river) cost -= 2;
    if (world[y][x].geography == GEO_MOUNTAIN) cost += 8;
    else if (world[y][x].geography == GEO_HILL) cost += 3;
    else if (world[y][x].geography == GEO_BASIN) cost -= 1;
    if (stats.habitability >= 7) cost -= 2;
    if (stats.water >= 7) cost -= 1;
    cost += ((x * 17 + y * 31 + city_id * 13) % 7) - 3;
    return clamp(cost, 3, 28);
}

int province_nearby_enemy_border(int owner, int x, int y, int radius) {
    int dy;
    int dx;

    for (dy = -radius; dy <= radius; dy++) {
        for (dx = -radius; dx <= radius; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            int other_owner;
            if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
            other_owner = world[ny][nx].owner;
            if (other_owner >= 0 && other_owner != owner) return 1;
        }
    }
    return 0;
}

static int province_tile_is_border(int city_id, int x, int y) {
    static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int i;

    for (i = 0; i < 4; i++) {
        int nx = x + dirs[i][0];
        int ny = y + dirs[i][1];
        if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) return 1;
        if (world[ny][nx].province_id != city_id) return 1;
    }
    return 0;
}

static int compute_province_border_distance(int city_id) {
    static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int x;
    int y;
    int head = 0;
    int tail = 0;
    int max_distance = 0;

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) province_border_distance[y][x] = -1;
    }

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            if (world[y][x].province_id != city_id) continue;
            if (!province_tile_is_border(city_id, x, y)) continue;
            province_border_distance[y][x] = 0;
            province_frontier[tail].x = x;
            province_frontier[tail].y = y;
            tail++;
        }
    }

    while (head < tail) {
        ProvinceFrontierNode node = province_frontier[head++];
        int i;
        int distance = province_border_distance[node.y][node.x] + 1;

        for (i = 0; i < 4; i++) {
            int nx = node.x + dirs[i][0];
            int ny = node.y + dirs[i][1];
            if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
            if (world[ny][nx].province_id != city_id) continue;
            if (province_border_distance[ny][nx] >= 0) continue;
            province_border_distance[ny][nx] = distance;
            if (distance > max_distance) max_distance = distance;
            if (tail < MAP_W * MAP_H) {
                province_frontier[tail].x = nx;
                province_frontier[tail].y = ny;
                tail++;
            }
        }
    }
    return max_distance;
}

static int city_location_score(int city_id, int x, int y, int border_distance, int port_site) {
    City *city = &cities[city_id];
    TerrainStats stats = tile_stats(x, y);
    int score = border_distance * (port_site ? 10 : 36) + world_terrain_resource_value(stats);

    score += stats.water * 5 + stats.food * 5 + stats.pop_capacity * 4 + stats.habitability * 3;
    if (world[y][x].river) score += 18;
    if (world_is_coastal_land_tile(x, y)) score += port_site ? 34 : 5;
    score -= world_tile_cost(x, y) * 8;
    if (province_nearby_enemy_border(city->owner, x, y, 2)) score -= 90;
    return score;
}

int province_select_city_site(int city_id, int min_border_distance, int port_only,
                              int *out_x, int *out_y) {
    int x;
    int y;
    int best_score = -1000000;
    int found = 0;

    compute_province_border_distance(city_id);
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int distance;
            int score;
            if (world[y][x].province_id != city_id) continue;
            distance = province_border_distance[y][x];
            if (distance < min_border_distance) continue;
            if (port_only && !world_is_coastal_land_tile(x, y)) continue;
            if (city_too_close_except(x, y, CITY_MIN_DISTANCE, city_id)) continue;
            score = city_location_score(city_id, x, y, distance, port_only);
            if (score > best_score) {
                best_score = score;
                *out_x = x;
                *out_y = y;
                found = 1;
            }
        }
    }
    return found;
}

static void repair_city_location(int city_id, int min_border_distance) {
    City *city;
    int x;
    int y;

    if (city_id < 0 || city_id >= city_count) return;
    city = &cities[city_id];
    if (!city->alive || city->port) return;
    if (!province_select_city_site(city_id, min_border_distance, 0, &x, &y) &&
        !province_select_city_site(city_id, 2, 0, &x, &y) &&
        !province_select_city_site(city_id, 1, 0, &x, &y)) {
        return;
    }
    city->x = x;
    city->y = y;
    city->radius = province_city_radius_for_tile(x, y, city->population);
}

void province_claim_city_region(int city_id, int owner) {
    int x;
    int y;
    int has_existing_region = 0;
    int head = 0;
    int tail = 0;
    int budget;
    City *city;

    if (city_id < 0 || city_id >= city_count) return;
    city = &cities[city_id];
    if (!city->alive) return;

    city->owner = owner;
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            if (world[y][x].province_id == city_id) {
                world[y][x].owner = owner;
                has_existing_region = 1;
            }
        }
    }
    if (has_existing_region) {
        world_invalidate_region_cache();
        return;
    }

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) province_claim_cost[y][x] = -1;
    }

    budget = clamp(city->radius * 15 + tile_stats(city->x, city->y).habitability * 5 + city->population / 80, 64, 190);
    {
        int core_radius = clamp(city->radius / 2, 2, 3);
        int dy;
        int dx;

        for (dy = -core_radius; dy <= core_radius; dy++) {
            for (dx = -core_radius; dx <= core_radius; dx++) {
                int nx = city->x + dx;
                int ny = city->y + dy;
                int core_cost = (abs(dx) + abs(dy)) * 2;

                if (dx * dx + dy * dy > core_radius * core_radius) continue;
                if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
                if (!is_land(world[ny][nx].geography)) continue;
                if (world[ny][nx].province_id >= 0 && world[ny][nx].province_id != city_id) continue;
                if (world[ny][nx].owner >= 0 && world[ny][nx].owner != owner) continue;
                world[ny][nx].owner = owner;
                world[ny][nx].province_id = city_id;
                province_claim_cost[ny][nx] = core_cost;
                province_frontier[tail].x = nx;
                province_frontier[tail].y = ny;
                tail++;
            }
        }
    }
    if (tail == 0) {
        province_claim_cost[city->y][city->x] = 0;
        province_frontier[tail].x = city->x;
        province_frontier[tail].y = city->y;
        tail++;
    }

    while (head < tail) {
        static const int dirs[8][2] = {
            {1, 0}, {-1, 0}, {0, 1}, {0, -1},
            {1, 1}, {-1, 1}, {1, -1}, {-1, -1}
        };
        ProvinceFrontierNode node = province_frontier[head++];
        int direction;

        world[node.y][node.x].owner = owner;
        world[node.y][node.x].province_id = city_id;
        for (direction = 0; direction < 8; direction++) {
            int nx = node.x + dirs[direction][0];
            int ny = node.y + dirs[direction][1];
            int next_cost;

            if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
            if (!is_land(world[ny][nx].geography)) continue;
            if (world[ny][nx].province_id >= 0 && world[ny][nx].province_id != city_id) continue;
            if (world[ny][nx].owner >= 0 && world[ny][nx].owner != owner) continue;
            next_cost = province_claim_cost[node.y][node.x] + province_entry_cost(city_id, nx, ny, node.x, node.y);
            if (next_cost > budget) continue;
            if (province_claim_cost[ny][nx] >= 0 && province_claim_cost[ny][nx] <= next_cost) continue;
            province_claim_cost[ny][nx] = next_cost;
            if (tail < MAP_W * MAP_H) {
                province_frontier[tail].x = nx;
                province_frontier[tail].y = ny;
                tail++;
            }
        }
    }
    repair_city_location(city_id, city->capital ? 4 : 3);
    ports_maybe_make_city_port(city_id);
    world_invalidate_region_cache();
}
