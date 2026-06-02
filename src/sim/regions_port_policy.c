#include "sim/regions_port_policy.h"

#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "sim/maritime.h"
#include "sim/regions.h"
#include "sim/regions_settlement.h"
#include "world/ports.h"
#include "world/terrain_query.h"

#include <string.h>

typedef struct {
    int has_candidate;
    int best_region;
    int best_score;
} PortComponentChoice;

typedef struct {
    int has_candidate;
    int x;
    int y;
    int score;
} PortCandidate;

static RegionPortPolicyStats last_stats;
static PortCandidate port_candidates[MAX_NATURAL_REGIONS];
static unsigned char forced_port[MAX_NATURAL_REGIONS];
static int component_id[MAX_MAP_H][MAX_MAP_W];
static int queue_x[MAX_MAP_W * MAX_MAP_H];
static int queue_y[MAX_MAP_W * MAX_MAP_H];

static unsigned int mix_hash(unsigned int hash, unsigned int value) {
    hash ^= value;
    return hash * 16777619u;
}

static int region_roll(const NaturalRegion *region) {
    unsigned int hash = 2166136261u;
    hash = mix_hash(hash, (unsigned int)(region->id + 1));
    hash = mix_hash(hash, (unsigned int)(region->tile_count + 17));
    hash = mix_hash(hash, (unsigned int)(region->capital_x + 37));
    hash = mix_hash(hash, (unsigned int)(region->capital_y + 53));
    hash = mix_hash(hash, (unsigned int)(region->development_score + 71));
    return (int)(hash % 100u);
}

static int water_access_score(int x, int y) {
    static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int i;
    int score = 0;

    for (i = 0; i < 4; i++) {
        int nx = x + dirs[i][0];
        int ny = y + dirs[i][1];
        if (nx < 0 || ny < 0 || nx >= MAP_W || ny >= MAP_H) continue;
        if (world_is_shallow_water(nx, ny)) score += 28;
        else if (world_is_deep_water(nx, ny)) score += 8;
    }
    return score;
}

static int candidate_tile_score(int x, int y) {
    TerrainStats stats;
    int sea_x;
    int sea_y;
    int score;

    if (!world_is_coastal_land_tile(x, y)) return -1000000;
    if (!ports_find_nearby_sea_entry(x, y, &sea_x, &sea_y)) return -1000000;
    stats = tile_stats(x, y);
    score = water_access_score(x, y) + stats.habitability * 5 + stats.money * 4 +
            stats.food * 2 + stats.water * 2 - world_tile_cost(x, y) * 4;
    if (world[y][x].river) score += 18;
    if (world[y][x].geography == GEO_DELTA) score += 28;
    if (world[y][x].geography == GEO_COAST) score += 12;
    if (world[y][x].geography == GEO_MOUNTAIN || world[y][x].geography == GEO_CANYON ||
        world[y][x].geography == GEO_VOLCANO) score -= 36;
    return score;
}

static void precompute_port_candidates(void) {
    int x;
    int y;

    memset(port_candidates, 0, sizeof(port_candidates));
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int region_id = world[y][x].region_id;
            int score;
            PortCandidate *candidate;
            if (region_id < 0 || region_id >= region_count) continue;
            candidate = &port_candidates[region_id];
            score = candidate_tile_score(x, y);
            if (score <= -1000000) continue;
            if (!candidate->has_candidate || score > candidate->score) {
                candidate->has_candidate = 1;
                candidate->x = x;
                candidate->y = y;
                candidate->score = score;
            }
        }
    }
    for (x = 0; x < region_count; x++) {
        if (port_candidates[x].has_candidate) last_stats.coastal_candidate_regions++;
    }
}

static int candidate_neighbor_count(int region_id) {
    const NaturalRegion *region = &natural_regions[region_id];
    int count = 0;
    int i;

    for (i = 0; i < region->neighbor_count; i++) {
        int neighbor_id = region->neighbors[i];
        if (neighbor_id >= 0 && neighbor_id < region_count &&
            port_candidates[neighbor_id].has_candidate) {
            count++;
        }
    }
    return count;
}

static int region_port_chance(const NaturalRegion *region, int best_score, int candidate_neighbors) {
    int chance = 45;
    int strong = region->average_stats.money >= 5 || region->development_score >= 48 ||
                 region->average_stats.water >= 6 || best_score >= 84;
    int weak = region->dominant_geography == GEO_MOUNTAIN || region->dominant_geography == GEO_CANYON ||
               region->dominant_geography == GEO_VOLCANO || region->habitability <= 3 || best_score < 45;

    if (strong) chance = 55;
    if (weak) chance = 25;
    if (candidate_neighbors >= 4) chance -= 15;
    else if (candidate_neighbors >= 2) chance -= 8;
    return clamp(chance, 15, 55);
}

static void set_region_city_port(int region_id, int is_port, int x, int y) {
    NaturalRegion *region = &natural_regions[region_id];
    int city_id = regions_local_city_id(region_id);
    int port_region = is_port ? ports_shallow_region_near_land(x, y) : -1;

    if (is_port && port_region < 0) is_port = 0;
    region->has_port_site = is_port;
    region->port_x = is_port ? x : -1;
    region->port_y = is_port ? y : -1;
    if (city_id >= 0) {
        City *city = &cities[city_id];
        if (city->port != is_port || city->port_x != region->port_x ||
            city->port_y != region->port_y || city->port_region != port_region) {
            dirty_mark_city();
        }
        city->port = is_port;
        city->port_x = region->port_x;
        city->port_y = region->port_y;
        city->port_region = port_region;
    }
}

static void apply_region_port_policy(int region_id) {
    NaturalRegion *region;
    const PortCandidate *candidate;
    int is_port = 0;

    if (region_id < 0 || region_id >= region_count) return;
    region = &natural_regions[region_id];
    candidate = &port_candidates[region_id];
    if (!region->alive) return;
    if (candidate->has_candidate) {
        if (forced_port[region_id]) {
            is_port = 1;
        } else {
            int chance = region_port_chance(region, candidate->score,
                                            candidate_neighbor_count(region_id));
            is_port = region_roll(region) < chance;
        }
        set_region_city_port(region_id, is_port, candidate->x, candidate->y);
    } else {
        set_region_city_port(region_id, 0, -1, -1);
    }
}

static void scan_component(int sx, int sy, int id, PortComponentChoice *choice) {
    static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int head = 0;
    int tail = 0;

    choice->has_candidate = 0;
    choice->best_region = -1;
    choice->best_score = -1000000;
    component_id[sy][sx] = id;
    queue_x[tail] = sx;
    queue_y[tail++] = sy;
    while (head < tail) {
        int x = queue_x[head];
        int y = queue_y[head++];
        int region_id = world[y][x].region_id;
        int i;

        if (region_id >= 0 && region_id < region_count &&
            port_candidates[region_id].has_candidate &&
            port_candidates[region_id].score > choice->best_score) {
            choice->has_candidate = 1;
            choice->best_region = region_id;
            choice->best_score = port_candidates[region_id].score;
        }
        for (i = 0; i < 4; i++) {
            int nx = x + dirs[i][0];
            int ny = y + dirs[i][1];
            if (nx < 0 || ny < 0 || nx >= MAP_W || ny >= MAP_H) continue;
            if (component_id[ny][nx] >= 0 || !is_land(world[ny][nx].geography)) continue;
            component_id[ny][nx] = id;
            queue_x[tail] = nx;
            queue_y[tail++] = ny;
        }
    }
}

static void choose_forced_island_ports(void) {
    int x;
    int y;
    int id = 0;

    memset(component_id, 0xff, sizeof(component_id));
    memset(forced_port, 0, sizeof(forced_port));
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            PortComponentChoice choice;
            if (component_id[y][x] >= 0 || !is_land(world[y][x].geography)) continue;
            scan_component(x, y, id, &choice);
            last_stats.island_components_checked++;
            if (choice.has_candidate && choice.best_region >= 0) {
                if (!forced_port[choice.best_region]) last_stats.forced_island_ports++;
                forced_port[choice.best_region] = 1;
                last_stats.island_components_with_port++;
            }
            id++;
        }
    }
}

void regions_port_policy_apply_all(void) {
    int i;

    memset(&last_stats, 0, sizeof(last_stats));
    precompute_port_candidates();
    choose_forced_island_ports();
    for (i = 0; i < region_count; i++) apply_region_port_policy(i);
    for (i = 0; i < region_count; i++) {
        int city_id = natural_regions[i].city_id;
        if (!natural_regions[i].alive || city_id < 0 || city_id >= city_count) continue;
        if (cities[city_id].port) last_stats.port_city_count++;
        else last_stats.normal_city_count++;
    }
    maritime_mark_routes_dirty();
    world_visual_revision++;
}

void regions_port_policy_last_stats(RegionPortPolicyStats *out_stats) {
    if (out_stats) *out_stats = last_stats;
}
