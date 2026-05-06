#include "regions.h"

#include "core/game_types.h"
#include "sim/population.h"
#include "sim/ports.h"
#include "sim/simulation.h"
#include "world/terrain_query.h"

#define REGION_SPAWN_HARD_DISTANCE 30

typedef struct {
    int region_id;
    int score;
} RegionPick;

static int region_distance_penalty(const NaturalRegion *region) {
    int best = MAP_W * MAP_W + MAP_H * MAP_H;
    int i;

    for (i = 0; i < city_count; i++) {
        int dx;
        int dy;
        int dist;
        if (!cities[i].alive) continue;
        dx = region->capital_x - cities[i].x;
        dy = region->capital_y - cities[i].y;
        dist = dx * dx + dy * dy;
        if (dist < best) best = dist;
    }
    if (city_count <= 0) return 0;
    if (best < REGION_SPAWN_HARD_DISTANCE * REGION_SPAWN_HARD_DISTANCE) return 1000000;
    if (best >= 60 * 60) return 0;
    return (60 * 60 - best) / 14;
}

static int is_special_cradle(const NaturalRegion *region) {
    if (region->has_port_site && region->viable_direction_count >= 1 &&
        region->average_stats.water >= 3 && region->development_score >= 38) return 1;
    if (region->dominant_geography == GEO_ISLAND && region->tile_count >= 28 &&
        region->has_port_site && region->average_stats.food >= 2) return 1;
    if ((region->dominant_geography == GEO_MOUNTAIN || region->dominant_geography == GEO_PLATEAU ||
         region->dominant_geography == GEO_HILL) &&
        region->viable_direction_count >= 1 && region->average_stats.water >= 3 &&
        region->average_stats.minerals + region->average_stats.stone >= 6) return 1;
    if (region->dominant_geography == GEO_OASIS && region->average_stats.water >= 5 &&
        region->average_stats.money >= 3) return 1;
    return 0;
}

static int is_auto_spawn_candidate(const NaturalRegion *region) {
    if (!region->alive || region->tile_count < 20 || region->owner_civ >= 0 || region->capital_x < 0) return 0;
    if (region_distance_penalty(region) >= 1000000) return 0;
    if (is_special_cradle(region)) return 1;
    if (region->average_stats.food < 3 || region->average_stats.water < 3) return 0;
    if (region->habitability < 3 || region->resource_diversity < 3) return 0;
    if (region->viable_direction_count < 2) return 0;
    return 1;
}

static int select_weighted_region(RegionPick *picks, int count, int *out_region_id) {
    int limit = min(count, 20);
    int total = 0;
    int lowest;
    int roll;
    int i;

    if (count <= 0) return 0;
    lowest = picks[limit - 1].score;
    for (i = 0; i < limit; i++) total += max(1, picks[i].score - lowest + 1);
    roll = rnd(total);
    for (i = 0; i < limit; i++) {
        roll -= max(1, picks[i].score - lowest + 1);
        if (roll < 0) {
            *out_region_id = picks[i].region_id;
            return 1;
        }
    }
    *out_region_id = picks[0].region_id;
    return 1;
}

int regions_select_spawn_region(int preferred_x, int preferred_y, int *out_region_id, int *out_x, int *out_y) {
    RegionPick picks[MAX_NATURAL_REGIONS];
    int pick_count = 0;
    int i;

    if (!out_region_id || !out_x || !out_y || region_count <= 0) return 0;
    if (preferred_x >= 0 && preferred_x < MAP_W && preferred_y >= 0 && preferred_y < MAP_H) {
        int id = world[preferred_y][preferred_x].region_id;
        const NaturalRegion *region = regions_get(id);
        if (region && region->owner_civ < 0 && region_distance_penalty(region) < 1000000) {
            *out_region_id = id;
            *out_x = region->capital_x;
            *out_y = region->capital_y;
            return 1;
        }
    }
    for (i = 0; i < region_count; i++) {
        const NaturalRegion *region = &natural_regions[i];
        int penalty;
        int score;
        int j;
        if (!is_auto_spawn_candidate(region)) continue;
        penalty = region_distance_penalty(region);
        score = region->cradle_score + region->development_score + region->viable_direction_count * 30 +
                region->resource_diversity * 15 + region->neighbor_count * 8 +
                (region->has_port_site ? 18 : 0) + rnd(30) - penalty;
        if (region->neighbor_count <= 0) score -= 80;
        for (j = pick_count; j > 0 && picks[j - 1].score < score; j--) picks[j] = picks[j - 1];
        picks[j].region_id = i;
        picks[j].score = score;
        if (pick_count < MAX_NATURAL_REGIONS - 1) pick_count++;
    }
    if (!select_weighted_region(picks, pick_count, out_region_id)) return 0;
    *out_x = natural_regions[*out_region_id].capital_x;
    *out_y = natural_regions[*out_region_id].capital_y;
    return 1;
}

int regions_region_for_city(int city_id) {
    City *city;

    if (city_id < 0 || city_id >= city_count || !cities[city_id].alive) return -1;
    city = &cities[city_id];
    if (city->x < 0 || city->x >= MAP_W || city->y < 0 || city->y >= MAP_H) return -1;
    return world[city->y][city->x].region_id;
}

int regions_region_has_owner_neighbor(int region_id, int owner) {
    const NaturalRegion *region = regions_get(region_id);
    int i;

    if (!region || owner < 0) return 0;
    for (i = 0; i < region->neighbor_count; i++) {
        const NaturalRegion *neighbor = regions_get(region->neighbors[i]);
        if (neighbor && neighbor->owner_civ == owner) return 1;
    }
    return 0;
}

static int city_can_admin_region(int city_id, int owner, int region_id) {
    if (city_id < 0 || city_id >= city_count || !cities[city_id].alive) return 0;
    if (cities[city_id].owner != owner) return 0;
    return regions_region_for_city(city_id) == region_id;
}

static int city_is_in_region(int city_id, int region_id) {
    if (city_id < 0 || city_id >= city_count || !cities[city_id].alive) return 0;
    return regions_region_for_city(city_id) == region_id;
}

static int find_region_city(int region_id, int owner) {
    int i;

    for (i = 0; i < city_count; i++) {
        if (city_can_admin_region(i, owner, region_id)) return i;
    }
    return -1;
}

static int find_nearest_owner_city(const NaturalRegion *region, int owner) {
    int best_city = -1;
    int best_dist = MAP_W * MAP_W + MAP_H * MAP_H;
    int i;

    for (i = 0; i < city_count; i++) {
        int dx;
        int dy;
        int dist;
        if (!cities[i].alive || cities[i].owner != owner) continue;
        dx = cities[i].x - region->capital_x;
        dy = cities[i].y - region->capital_y;
        dist = dx * dx + dy * dy;
        if (dist < best_dist) {
            best_dist = dist;
            best_city = i;
        }
    }
    return best_city;
}

static int create_region_admin_city(const NaturalRegion *region, int owner) {
    TerrainStats stats;
    int source_city;
    int desired;
    int seed_population;
    int city_id;

    if (!region || region->capital_x < 0 || region->capital_y < 0 || city_count >= MAX_CITIES) return -1;
    if (city_at(region->capital_x, region->capital_y) >= 0) return -1;
    stats = tile_stats(region->capital_x, region->capital_y);
    desired = 1600 + stats.food * 420 + stats.water * 420 + stats.pop_capacity * 520 +
              stats.habitability * 280 + stats.money * 180 + min(region->tile_count, 1200) * 2;
    desired = clamp(desired + rnd(1200), 1200, 18000);
    source_city = find_nearest_owner_city(region, owner);
    if (source_city >= 0) desired = clamp(desired, 800, max(900, cities[source_city].population / 9));
    seed_population = source_city >= 0 ? min(500, desired) : desired;
    city_id = world_create_city(owner, region->capital_x, region->capital_y, seed_population, 0);
    if (city_id >= 0 && source_city >= 0 && desired > seed_population) {
        population_migrate_between_cities(source_city, city_id, desired - seed_population);
    }
    return city_id;
}

int regions_claim_for_civ(int region_id, int owner, int preferred_city_id, int create_city) {
    NaturalRegion *region;
    int admin_city;
    int x;
    int y;

    if (region_id < 0 || region_id >= region_count || owner < 0 || owner >= MAX_CIVS) return 0;
    region = &natural_regions[region_id];
    if (!region->alive) return 0;
    admin_city = city_is_in_region(preferred_city_id, region_id) ? preferred_city_id : -1;
    if (admin_city < 0) admin_city = find_region_city(region_id, owner);
    if (admin_city < 0 && region->owner_civ == owner && city_can_admin_region(region->city_id, owner, region_id)) {
        admin_city = region->city_id;
    }
    if (admin_city < 0 && create_city) {
        admin_city = create_region_admin_city(region, owner);
    }
    if (admin_city < 0) admin_city = find_nearest_owner_city(region, owner);
    if (admin_city < 0) return 0;

    if (admin_city >= 0 && admin_city < city_count) {
        cities[admin_city].owner = owner;
        cities[admin_city].alive = 1;
    }
    region->owner_civ = owner;
    region->city_id = admin_city;
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            if (world[y][x].region_id != region_id) continue;
            world[y][x].owner = owner;
            world[y][x].province_id = admin_city;
        }
    }
    ports_maybe_make_city_port(admin_city);
    world_invalidate_region_cache();
    return 1;
}

int regions_claim_as_province(int region_id, int owner, int city_id) {
    return regions_claim_for_civ(region_id, owner, city_id, 0);
}
