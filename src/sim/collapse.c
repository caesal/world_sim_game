#include "collapse.h"

#include "core/game_state.h"
#include "sim/diplomacy.h"
#include "sim/population.h"
#include "sim/regions.h"
#include "sim/simulation.h"

#include <stdio.h>
#include <string.h>

static int collapse_chance(int disorder) {
    if (disorder > 90) return 45;
    if (disorder > 85) return 30;
    if (disorder > 80) return 20;
    if (disorder > 75) return 10;
    return 0;
}

static int capital_region_for_civ(int civ_id) {
    int city_id = civs[civ_id].capital_city;
    if (city_id < 0 || city_id >= city_count || !cities[city_id].alive) return -1;
    return regions_region_for_city(city_id);
}

static int create_successor_civ(int parent, int index, int seed_region) {
    Civilization *child;
    int child_id;

    if (civ_count >= MAX_CIVS) return -1;
    child_id = civ_count++;
    child = &civs[child_id];
    *child = civs[parent];
    snprintf(child->name, sizeof(child->name), "%.40s Remnant %d", civs[parent].name, index + 1);
    child->symbol = (char)('a' + (child_id % 26));
    child->color = CIV_COLORS[child_id % MAX_CIVS];
    child->alive = 1;
    child->population = 0;
    child->territory = 0;
    child->disorder = clamp(civs[parent].disorder / 2, 20, 70);
    child->disorder_stability = 25;
    child->capital_city = -1;
    if (seed_region >= 0 && seed_region < region_count) {
        NaturalRegion *region = &natural_regions[seed_region];
        int city_id = region->city_id;
        if (city_id < 0 || city_id >= city_count || !cities[city_id].alive) {
            city_id = world_create_city(child_id, region->capital_x, region->capital_y,
                                        max(800, region->average_stats.pop_capacity * 600), 1);
        }
        if (city_id >= 0) {
            cities[city_id].owner = child_id;
            cities[city_id].capital = 1;
            child->capital_city = city_id;
        }
    }
    return child_id;
}

static void claim_region_direct(int region_id, int owner) {
    int x;
    int y;
    NaturalRegion *region = &natural_regions[region_id];
    int city_id = region->city_id;

    if (city_id < 0 || city_id >= city_count || !cities[city_id].alive) {
        city_id = world_create_city(owner, region->capital_x, region->capital_y,
                                    max(700, region->average_stats.pop_capacity * 420), 0);
        region->city_id = city_id;
    }
    if (city_id < 0) city_id = civs[owner].capital_city;
    region->owner_civ = owner;
    if (city_id >= 0 && city_id < city_count && cities[city_id].alive) cities[city_id].owner = owner;
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            if (world[y][x].region_id != region_id) continue;
            world[y][x].owner = owner;
            world[y][x].province_id = city_id >= 0 ? city_id : region_id;
        }
    }
}

static int add_neighbor_regions(int parent, int child, int seed_region, int cap_region) {
    int claimed = 0;
    int frontier[MAX_REGION_NEIGHBORS + 1];
    int front_count = 1;
    int i;

    frontier[0] = seed_region;
    for (i = 0; i < front_count && claimed < 6; i++) {
        NaturalRegion *region = &natural_regions[frontier[i]];
        int n;
        if (frontier[i] == cap_region || region->owner_civ != parent) continue;
        claim_region_direct(frontier[i], child);
        claimed++;
        for (n = 0; n < region->neighbor_count && front_count < MAX_REGION_NEIGHBORS + 1; n++) {
            int neighbor = region->neighbors[n];
            if (neighbor >= 0 && neighbor < region_count && natural_regions[neighbor].owner_civ == parent) {
                frontier[front_count++] = neighbor;
            }
        }
    }
    return claimed;
}

static void collapse_civ(int civ_id) {
    int cap_region = capital_region_for_civ(civ_id);
    int formed = 0;
    int i;

    for (i = 0; i < region_count && formed < 4 && civ_count < MAX_CIVS; i++) {
        int child;
        if (!natural_regions[i].alive || natural_regions[i].owner_civ != civ_id || i == cap_region) continue;
        if (natural_regions[i].development_score < 30 && natural_regions[i].tile_count < 40) continue;
        child = create_successor_civ(civ_id, formed, i);
        if (child < 0) break;
        if (add_neighbor_regions(civ_id, child, i, cap_region) <= 0) {
            civs[child].alive = 0;
            continue;
        }
        diplomacy_start_truce(civ_id, child, 45, 20);
        formed++;
    }
    civs[civ_id].disorder = clamp(civs[civ_id].disorder - 25, 0, 100);
    world_recalculate_territory();
    population_sync_all();
}

void collapse_update_decade(void) {
    int i;

    for (i = 0; i < civ_count; i++) {
        int chance;
        if (!civs[i].alive) continue;
        chance = collapse_chance(civs[i].disorder);
        if (chance > 0 && rnd(100) < chance) collapse_civ(i);
    }
}
