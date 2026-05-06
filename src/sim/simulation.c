#include "sim/simulation.h"

#include "core/dirty_flags.h"
#include "sim/diplomacy.h"
#include "sim/civilization_metrics.h"
#include "sim/expansion.h"
#include "sim/maritime.h"
#include "sim/plague.h"
#include "sim/population.h"
#include "sim/ports.h"
#include "sim/regions.h"
#include "sim/province.h"
#include "sim/province_partition.h"
#include "sim/spawn.h"
#include "sim/war.h"
#include "world/terrain_query.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static CountrySummary country_summary_cache[MAX_CIVS];
static int country_summary_dirty = 1;
static unsigned int territory_contact_hash = 0;

static void recalculate_territory(void) {
    int i;
    int y;
    int x;
    unsigned int next_hash = 2166136261u;

    for (i = 0; i < civ_count; i++) civs[i].territory = 0;
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int owner = world[y][x].owner;
            if (owner >= 0 && owner < civ_count && civs[owner].alive) civs[owner].territory++;
        }
    }
    population_sync_all();
    for (i = 0; i < civ_count; i++) {
        if (civs[i].territory <= 0 || civs[i].population <= 0) civs[i].alive = 0;
    }
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int owner = world[y][x].owner;
            if (owner >= 0 && (owner >= civ_count || !civs[owner].alive)) {
                world[y][x].owner = -1;
                world[y][x].province_id = -1;
            }
        }
    }
    for (i = 0; i < region_count; i++) {
        int owner = natural_regions[i].owner_civ;
        if (owner >= 0 && (owner >= civ_count || !civs[owner].alive)) {
            natural_regions[i].owner_civ = -1;
            natural_regions[i].city_id = -1;
        }
    }
    for (i = 0; i < city_count; i++) {
        int owner = cities[i].owner;
        if (cities[i].alive && owner >= 0 && (owner >= civ_count || !civs[owner].alive)) {
            cities[i].alive = 0;
            cities[i].owner = -1;
            cities[i].capital = 0;
        }
    }
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            next_hash = (next_hash ^ (unsigned int)(world[y][x].owner + 2)) * 16777619u;
            next_hash = (next_hash ^ (unsigned int)(world[y][x].province_id + 2)) * 16777619u;
        }
    }
    if (next_hash != territory_contact_hash) {
        territory_contact_hash = next_hash;
        diplomacy_mark_contacts_dirty();
    }
    world_invalidate_region_cache();
}

static void make_city_name(char *buffer, size_t buffer_size, int owner, int x, int y, int capital) {
    const char *prefixes[] = {
        "North", "South", "East", "West", "High", "Low", "River", "Stone",
        "Green", "Red", "Gold", "Silver", "Old", "New", "Lake", "Hill"
    };
    const char *roots[] = {
        "ford", "mere", "hold", "watch", "vale", "crest", "harbor", "field",
        "march", "gate", "fall", "haven", "wick", "brook", "den", "reach"
    };

    if (capital && owner >= 0 && owner < civ_count) snprintf(buffer, buffer_size, "%s Capital", civs[owner].name);
    else snprintf(buffer, buffer_size, "%s%s Province", prefixes[(x * 3 + y) % 16], roots[(x + y * 5) % 16]);
}

static int create_city(int owner, int x, int y, int population, int capital) {
    City *city;
    int city_id;

    if (city_count >= MAX_CITIES) {
        OutputDebugStringA("World Sim: MAX_CITIES reached; city creation skipped.\n");
        return -1;
    }
    if (province_city_too_close(x, y, CITY_MIN_DISTANCE)) return -1;
    city = &cities[city_count];
    city->alive = 1;
    city->owner = owner;
    make_city_name(city->name, sizeof(city->name), owner, x, y, capital);
    city->x = x;
    city->y = y;
    city->population = clamp(population, 1, MAX_POPULATION);
    city->radius = province_city_radius_for_tile(x, y, population);
    city->capital = capital;
    city->port = 0;
    city->port_x = -1;
    city->port_y = -1;
    city->port_region = -1;
    city->population_ready = 0;
    city_id = city_count++;
    population_init_city(city_id, population);
    return city_id;
}

static int starting_population_for_region(int x, int y, int region_id) {
    TerrainStats stats = tile_stats(x, y);
    const NaturalRegion *region = regions_get(region_id);
    int base = 6000 + stats.food * 1200 + stats.water * 1200 + stats.pop_capacity * 1400 +
               stats.habitability * 900 + stats.money * 450;

    if (region) {
        base += min(region->tile_count, 1400) * 7 + region->development_score * 90;
        if (region->has_port_site) base += 2500;
    }
    return clamp(base + rnd(max(1, base / 5)), 7000, 70000);
}

int city_at(int x, int y) {
    int i;

    for (i = 0; i < city_count; i++) {
        if (cities[i].alive && cities[i].x == x && cities[i].y == y) return i;
    }
    return -1;
}

int world_city_site_has_room(int x, int y, int owner, int radius) {
    return province_city_site_has_room(x, y, owner, radius);
}

int world_nearby_enemy_border(int owner, int x, int y, int radius) {
    return province_nearby_enemy_border(owner, x, y, radius);
}

int world_city_radius_for_tile(int x, int y, int population) {
    return province_city_radius_for_tile(x, y, population);
}

int world_select_city_site_in_province(int city_id, int min_border_distance, int port_only,
                                       int *out_x, int *out_y) {
    return province_select_city_site(city_id, min_border_distance, port_only, out_x, out_y);
}

int world_create_city(int owner, int x, int y, int population, int capital) {
    return create_city(owner, x, y, population, capital);
}

void world_claim_city_region(int city_id, int owner) {
    int region_id = regions_region_for_city(city_id);

    if (regions_claim_for_civ(region_id, owner, city_id, 0)) return;
    /* Legacy fallback for debug/old saves where a city is not inside a generated natural region. */
    province_claim_city_region(city_id, owner);
}

void world_mark_province_partition_dirty(int owner) {
    province_mark_partition_dirty(owner);
}

void world_recalculate_territory(void) {
    recalculate_territory();
}

void world_invalidate_region_cache(void) {
    province_invalidate_region_cache();
    country_summary_dirty = 1;
    population_mark_dirty();
    dirty_mark_territory();
    world_visual_revision++;
}

void world_invalidate_population_cache(void) {
    province_invalidate_region_summary();
    country_summary_dirty = 1;
    population_mark_dirty();
}

int city_for_tile(int x, int y) {
    return province_city_for_tile(x, y);
}

RegionSummary summarize_city_region(int city_id) {
    return province_summarize_city_region(city_id);
}

static void rebuild_country_summary_cache(void) {
    int x;
    int y;
    int i;

    memset(country_summary_cache, 0, sizeof(country_summary_cache));
    for (i = 0; i < civ_count; i++) {
        country_summary_cache[i].population = 0;
        country_summary_cache[i].territory = civs[i].territory;
    }
    for (i = 0; i < city_count; i++) {
        if (cities[i].alive && cities[i].owner >= 0 && cities[i].owner < civ_count) {
            country_summary_cache[cities[i].owner].cities++;
            country_summary_cache[cities[i].owner].population += cities[i].population;
            if (cities[i].port) country_summary_cache[cities[i].owner].ports++;
        }
    }
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int owner = world[y][x].owner;
            TerrainStats stats;
            if (owner < 0 || owner >= civ_count) continue;
            stats = tile_stats(x, y);
            country_summary_cache[owner].food += stats.food;
            country_summary_cache[owner].livestock += stats.livestock;
            country_summary_cache[owner].wood += stats.wood;
            country_summary_cache[owner].stone += stats.stone;
            country_summary_cache[owner].minerals += stats.minerals;
            country_summary_cache[owner].water += stats.water;
            country_summary_cache[owner].pop_capacity += stats.pop_capacity;
            country_summary_cache[owner].money += stats.money;
            country_summary_cache[owner].habitability += stats.habitability;
        }
    }
    for (i = 0; i < civ_count; i++) {
        CountrySummary *summary = &country_summary_cache[i];
        if (summary->territory <= 0) continue;
        summary->food /= summary->territory;
        summary->livestock /= summary->territory;
        summary->wood /= summary->territory;
        summary->stone /= summary->territory;
        summary->minerals /= summary->territory;
        summary->water /= summary->territory;
        summary->pop_capacity /= summary->territory;
        summary->money /= summary->territory;
        summary->habitability /= summary->territory;
        summary->resource_score = summary->food + summary->livestock + summary->water +
                                  summary->pop_capacity + summary->money + summary->habitability;
    }
    country_summary_dirty = 0;
}

CountrySummary summarize_country(int civ_id) {
    CountrySummary summary;

    memset(&summary, 0, sizeof(summary));
    if (civ_id < 0 || civ_id >= civ_count) return summary;
    if (country_summary_dirty) rebuild_country_summary_cache();
    return country_summary_cache[civ_id];
}

int add_civilization_at(const char *name, char symbol, int military, int logistics,
                        int governance, int cohesion, int production, int commerce,
                        int innovation, int preferred_x, int preferred_y) {
    int x = preferred_x;
    int y = preferred_y;
    int city_id;
    int region_id;
    Civilization *civ;

    if (!world_generated) return 0;
    if (civ_count >= MAX_CIVS) return 0;
    if (!spawn_select_civilization_cradle(x, y, &x, &y)) return 0;
    region_id = x >= 0 && x < MAP_W && y >= 0 && y < MAP_H ? world[y][x].region_id : -1;

    civ = &civs[civ_count];
    memset(civ, 0, sizeof(*civ));
    strncpy(civ->name, name, NAME_LEN - 1);
    civ->symbol = symbol;
    civ->color = CIV_COLORS[civ_count % MAX_CIVS];
    civ->alive = 1;
    civ->population = starting_population_for_region(x, y, region_id);
    {
        TerrainStats birth = tile_stats(x, y);
        apply_civilization_core_metrics(civ, governance, cohesion, production, military,
                                        commerce, logistics, innovation, birth, 1);
    }
    civ->capital_city = -1;

    city_id = create_city(civ_count, x, y, civ->population, 1);
    if (city_id < 0) {
        memset(civ, 0, sizeof(*civ));
        return 0;
    }
    civ->capital_city = city_id;
    if (!regions_claim_as_province(region_id, civ_count, city_id)) {
        if (city_id == city_count - 1) city_count--;
        else cities[city_id].alive = 0;
        memset(civ, 0, sizeof(*civ));
        return 0;
    }

    civ_count++;
    recalculate_territory();
    population_sync_all();
    return 1;
}

void simulation_seed_default_civilizations(void) {
    const char *names[MAX_CIVS] = {
        "Red Dominion", "Blue League", "Green Pact", "Gold Union",
        "Violet Realm", "Orange Clans", "Teal Coast", "Rose March",
        "Ivory Compact", "Black Banner", "Amber League", "Azure Domain",
        "Crimson Isles", "Silver Order", "Jade Senate", "Copper March"
    };
    const char symbols[MAX_CIVS] = {
        'R', 'B', 'G', 'Y', 'V', 'O', 'T', 'M',
        'I', 'K', 'A', 'Z', 'C', 'S', 'J', 'P'
    };
    const int traits[MAX_CIVS][7] = {
        {8, 7, 4, 3, 6, 5, 4}, {3, 4, 8, 6, 5, 5, 6},
        {5, 6, 5, 7, 6, 6, 7}, {4, 6, 5, 8, 6, 8, 7},
        {6, 5, 6, 6, 6, 6, 6}, {7, 5, 5, 4, 6, 5, 4},
        {4, 7, 4, 7, 5, 7, 6}, {5, 5, 7, 5, 6, 5, 6},
        {2, 6, 8, 8, 6, 7, 8}, {9, 5, 5, 3, 6, 4, 4},
        {5, 8, 4, 6, 7, 7, 5}, {3, 7, 6, 7, 6, 7, 7},
        {8, 6, 4, 5, 6, 5, 5}, {4, 4, 9, 8, 6, 6, 8},
        {3, 6, 5, 9, 5, 7, 8}, {6, 7, 6, 4, 7, 6, 5}
    };
    int i;
    int count = clamp(initial_civ_count, 0, MAX_CIVS);

    for (i = 0; i < count; i++) {
        add_civilization_at(names[i], symbols[i], traits[i][0], traits[i][1], traits[i][2],
                            traits[i][3], traits[i][4], traits[i][5], traits[i][6], -1, -1);
    }
}

void simulation_reset_state(void) {
    year = 0;
    month = 1;
    civ_count = 0;
    city_count = 0;
    maritime_reset();
    plague_reset();
    territory_contact_hash = 0;
    world_invalidate_region_cache();
}

void simulation_apply_civilization_edit(int civ_id, const char *name, char symbol,
                                        int military, int logistics,
                                        int governance, int cohesion,
                                        int production, int commerce,
                                        int innovation) {
    Civilization *civ;
    TerrainStats birth;

    if (civ_id < 0 || civ_id >= civ_count) return;
    civ = &civs[civ_id];
    if (name && name[0] != '\0') snprintf(civ->name, NAME_LEN, "%s", name);
    if (symbol != '\0') civ->symbol = symbol;
    memset(&birth, 0, sizeof(birth));
    if (civ->capital_city >= 0 && civ->capital_city < city_count) {
        birth = tile_stats(cities[civ->capital_city].x, cities[civ->capital_city].y);
    }
    apply_civilization_core_metrics(civ, governance, cohesion, production, military,
                                    commerce, logistics, innovation, birth, 0);
    dirty_mark_labels();
}
