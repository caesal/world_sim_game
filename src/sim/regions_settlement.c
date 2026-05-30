#include "sim/regions_settlement.h"

#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "data/province_names.h"
#include "sim/population.h"
#include "sim/regions.h"
#include "world/terrain_query.h"

#include <stdio.h>
#include <string.h>

static int region_tile_contains(int region_id, int x, int y) {
    return region_id >= 0 && region_id < region_count &&
           x >= 0 && x < MAP_W && y >= 0 && y < MAP_H &&
           world[y][x].region_id == region_id && is_land(world[y][x].geography);
}

int regions_raw_region_for_city(int city_id) {
    if (city_id < 0 || city_id >= city_count) return -1;
    if (cities[city_id].x < 0 || cities[city_id].x >= MAP_W ||
        cities[city_id].y < 0 || cities[city_id].y >= MAP_H) return -1;
    return world[cities[city_id].y][cities[city_id].x].region_id;
}

int regions_city_is_local_to_region(int city_id, int region_id) {
    return city_id >= 0 && city_id < city_count && region_tile_contains(region_id, cities[city_id].x, cities[city_id].y);
}

static void choose_region_city_site(int region_id, int *out_x, int *out_y) {
    const NaturalRegion *region = regions_get(region_id);
    int x;
    int y;

    if (region && region_tile_contains(region_id, region->capital_x, region->capital_y)) {
        *out_x = region->capital_x;
        *out_y = region->capital_y;
        return;
    }
    if (region && region_tile_contains(region_id, region->center_x, region->center_y)) {
        *out_x = region->center_x;
        *out_y = region->center_y;
        return;
    }
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            if (region_tile_contains(region_id, x, y)) {
                *out_x = x;
                *out_y = y;
                return;
            }
        }
    }
    *out_x = -1;
    *out_y = -1;
}

int regions_local_city_id(int region_id) {
    int i;
    const NaturalRegion *region = regions_get(region_id);

    if (!region) return -1;
    if (regions_city_is_local_to_region(region->city_id, region_id)) return region->city_id;
    for (i = 0; i < city_count; i++) {
        if (regions_city_is_local_to_region(i, region_id)) return i;
    }
    return -1;
}

static void initialize_city_slot(City *city, int region_id, int x, int y) {
    const char *name = province_display_name(region_id, 0);

    memset(city, 0, sizeof(*city));
    city->alive = 0;
    city->owner = -1;
    snprintf(city->name, sizeof(city->name), "%s", name && name[0] ? name : "Province City");
    city->x = x;
    city->y = y;
    city->population = 0;
    city->radius = 3;
    city->capital = 0;
    city->port = 0;
    city->port_x = -1;
    city->port_y = -1;
    city->port_region = -1;
    city->population_ready = 0;
}

static int allocate_region_city_slot(int region_id) {
    NaturalRegion *region;
    int city_id;
    int x;
    int y;

    if (region_id < 0 || region_id >= region_count) return -1;
    region = &natural_regions[region_id];
    if (!region->alive) return -1;
    if (city_count >= MAX_CITIES) return -1;
    choose_region_city_site(region_id, &x, &y);
    if (x < 0 || y < 0) return -1;
    city_id = city_count++;
    initialize_city_slot(&cities[city_id], region_id, x, y);
    region->city_id = city_id;
    dirty_mark_city();
    return city_id;
}

int regions_ensure_local_city_slot(int region_id) {
    int city_id = regions_local_city_id(region_id);

    if (city_id >= 0) {
        natural_regions[region_id].city_id = city_id;
        return city_id;
    }
    return allocate_region_city_slot(region_id);
}

static int find_unassigned_local_city(int region_id, const unsigned char *assigned) {
    int i;

    for (i = 0; i < city_count; i++) {
        if (assigned[i] || !regions_city_is_local_to_region(i, region_id)) continue;
        return i;
    }
    return -1;
}

int regions_activate_local_city(int region_id, int owner, int population, int capital, int allow_create) {
    int city_id = regions_local_city_id(region_id);
    City *city;
    int old_owner;

    if (city_id < 0 && allow_create) city_id = regions_ensure_local_city_slot(region_id);
    if (city_id < 0 || owner < 0 || owner >= MAX_CIVS) return -1;
    city = &cities[city_id];
    old_owner = city->owner;
    if (!city->alive || city->owner != owner) dirty_mark_city();
    city->alive = 1;
    city->owner = owner;
    if (old_owner != owner && !capital) city->capital = 0;
    if (capital) city->capital = 1;
    if (city->population <= 0 || !city->population_ready) {
        population_init_city(city_id, max(1, population));
    }
    natural_regions[region_id].city_id = city_id;
    return city_id;
}

void regions_deactivate_local_city(int region_id) {
    int city_id = regions_local_city_id(region_id);

    if (city_id < 0) return;
    if (cities[city_id].alive || cities[city_id].owner != -1 || cities[city_id].capital) dirty_mark_city();
    cities[city_id].alive = 0;
    cities[city_id].owner = -1;
    cities[city_id].capital = 0;
    natural_regions[region_id].city_id = city_id;
}

void regions_repair_local_city_slots(int repair_owned_regions) {
    unsigned char assigned[MAX_CITIES];
    int i;

    memset(assigned, 0, sizeof(assigned));
    for (i = 0; i < region_count; i++) {
        int city_id;
        if (!natural_regions[i].alive) continue;
        city_id = regions_city_is_local_to_region(natural_regions[i].city_id, i) &&
                  !assigned[natural_regions[i].city_id] ? natural_regions[i].city_id : -1;
        if (city_id < 0) city_id = find_unassigned_local_city(i, assigned);
        if (city_id < 0) city_id = allocate_region_city_slot(i);
        if (city_id < 0) continue;
        assigned[city_id] = 1;
        natural_regions[i].city_id = city_id;
        if (repair_owned_regions && natural_regions[i].owner_civ >= 0) {
            regions_activate_local_city(i, natural_regions[i].owner_civ,
                                        max(700, natural_regions[i].average_stats.pop_capacity * 420), 0, 1);
        } else if (natural_regions[i].owner_civ < 0) {
            regions_deactivate_local_city(i);
        }
    }
    for (i = 0; i < city_count; i++) {
        if (assigned[i]) continue;
        if (!cities[i].alive && cities[i].owner == -1) continue;
        cities[i].alive = 0;
        cities[i].owner = -1;
        cities[i].capital = 0;
        cities[i].port = 0;
        cities[i].port_region = -1;
        dirty_mark_city();
    }
}

void regions_refresh_province_ids_from_regions(void) {
    int x;
    int y;

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int region_id = world[y][x].region_id;
            int city_id = -1;
            if (region_id >= 0 && region_id < region_count && natural_regions[region_id].owner_civ >= 0) {
                city_id = regions_city_is_local_to_region(natural_regions[region_id].city_id, region_id) ?
                          natural_regions[region_id].city_id : -1;
            }
            world[y][x].province_id = city_id;
        }
    }
}

void regions_settlement_collect_stats(RegionSettlementStats *out_stats) {
    unsigned char seen[MAX_CITIES];
    int i;

    if (!out_stats) return;
    memset(out_stats, 0, sizeof(*out_stats));
    memset(seen, 0, sizeof(seen));
    out_stats->city_slots = city_count;
    for (i = 0; i < region_count; i++) {
        int city_id;
        if (!natural_regions[i].alive) continue;
        out_stats->regions_total++;
        city_id = natural_regions[i].city_id;
        if (city_id < 0 || city_id >= city_count) {
            out_stats->missing_or_invalid_city_id++;
            continue;
        }
        if (seen[city_id]) out_stats->duplicated_region_city_id++;
        seen[city_id] = 1;
        if (!regions_city_is_local_to_region(city_id, i)) out_stats->city_outside_region++;
        if (natural_regions[i].owner_civ >= 0 &&
            (!cities[city_id].alive || cities[city_id].owner != natural_regions[i].owner_civ)) {
            out_stats->owner_mismatch++;
        }
        if (natural_regions[i].owner_civ < 0 && !cities[city_id].alive && cities[city_id].owner < 0) {
            out_stats->neutral_city_count++;
        }
        if (cities[city_id].port) out_stats->port_city_count++;
        else out_stats->normal_city_count++;
    }
}
