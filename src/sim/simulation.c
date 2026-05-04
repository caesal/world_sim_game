#include "sim/simulation.h"

#include "sim/diplomacy.h"
#include "sim/civilization_metrics.h"
#include "sim/expansion.h"
#include "sim/maritime.h"
#include "sim/plague.h"
#include "sim/population.h"
#include "sim/ports.h"
#include "sim/province.h"
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
            if (owner >= 0 && (owner >= civ_count || !civs[owner].alive)) world[y][x].owner = -1;
            next_hash = (next_hash ^ (unsigned int)(world[y][x].owner + 2)) * 16777619u;
            next_hash = (next_hash ^ (unsigned int)(world[y][x].province_id + 2)) * 16777619u;
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
    if (next_hash != territory_contact_hash) {
        territory_contact_hash = next_hash;
        diplomacy_mark_contacts_dirty();
    }
    world_invalidate_region_cache();
}

static int find_empty_land(int *out_x, int *out_y) {
    int tries;

    for (tries = 0; tries < 12000; tries++) {
        int x = rnd(MAP_W);
        int y = rnd(MAP_H);
        TerrainStats stats = tile_stats(x, y);
        if (is_land(world[y][x].geography) && world[y][x].owner == -1 && world[y][x].province_id == -1 &&
            stats.habitability > 2 && !province_city_too_close(x, y, CITY_MIN_DISTANCE) &&
            province_city_site_has_room(x, y, -1, 3)) {
            *out_x = x;
            *out_y = y;
            return 1;
        }
    }
    return 0;
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
    province_claim_city_region(city_id, owner);
}

void world_recalculate_territory(void) {
    recalculate_territory();
}

void world_invalidate_region_cache(void) {
    province_invalidate_region_cache();
    country_summary_dirty = 1;
    population_mark_dirty();
    world_visual_revision++;
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

int add_civilization_at(const char *name, char symbol, int aggression, int expansion,
                        int defense, int culture, int preferred_x, int preferred_y) {
    int x = preferred_x;
    int y = preferred_y;
    int city_id;
    Civilization *civ;

    if (civ_count >= MAX_CIVS) return 0;
    if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H || !is_land(world[y][x].geography) ||
        world[y][x].owner != -1 || world[y][x].province_id != -1 ||
        province_city_too_close(x, y, CITY_MIN_DISTANCE)) {
        if (!find_empty_land(&x, &y)) return 0;
    }

    civ = &civs[civ_count];
    memset(civ, 0, sizeof(*civ));
    strncpy(civ->name, name, NAME_LEN - 1);
    civ->symbol = symbol;
    civ->color = CIV_COLORS[civ_count % MAX_CIVS];
    civ->alive = 1;
    civ->population = 120 + tile_stats(x, y).food * 10 + rnd(80);
    civ->aggression = clamp(aggression, 0, 10);
    civ->expansion = clamp(expansion, 0, 10);
    civ->defense = clamp(defense, 0, 10);
    civ->culture = clamp(culture, 0, 10);
    {
        TerrainStats birth = tile_stats(x, y);
        derive_civilization_metrics_from_traits_and_birthplace(civ, aggression, expansion, defense, culture, birth);
    }
    civ->capital_city = -1;

    world[y][x].owner = civ_count;
    city_id = create_city(civ_count, x, y, civ->population / 2, 1);
    civ->capital_city = city_id;
    if (city_id >= 0) province_claim_city_region(city_id, civ_count);

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
    const int traits[MAX_CIVS][4] = {
        {8, 7, 4, 3}, {3, 4, 8, 6}, {5, 6, 5, 7}, {4, 6, 5, 8},
        {6, 5, 6, 6}, {7, 5, 5, 4}, {4, 7, 4, 7}, {5, 5, 7, 5},
        {2, 6, 8, 8}, {9, 5, 5, 3}, {5, 8, 4, 6}, {3, 7, 6, 7},
        {8, 6, 4, 5}, {4, 4, 9, 8}, {3, 6, 5, 9}, {6, 7, 6, 4}
    };
    int i;
    int count = clamp(initial_civ_count, 0, MAX_CIVS);

    for (i = 0; i < count; i++) {
        add_civilization_at(names[i], symbols[i], traits[i][0], traits[i][1], traits[i][2], traits[i][3], -1, -1);
    }
}

static void update_city_growth_and_regions(void) {
    population_update_month();
    province_invalidate_region_cache();
}

static void random_event(char *log, size_t log_size) {
    int id;
    Civilization *civ;

    if (civ_count == 0 || rnd(100) > 18) return;
    id = rnd(civ_count);
    civ = &civs[id];
    if (!civ->alive) return;

    switch (rnd(5)) {
        case 0:
            append_log(log, log_size, "Festival in %s. ", civ->name);
            break;
        case 1:
            if (plague_seed_random_outbreak()) append_log(log, log_size, "Plague outbreak reported. ");
            break;
        case 2:
            civ->defense = clamp(civ->defense + 1, 0, 10);
            civ->disorder_stability = clamp(civ->disorder_stability + 1, 0, 10);
            civ->disorder = clamp(civ->disorder - 1, 0, 10);
            append_log(log, log_size, "%s fortified borders. ", civ->name);
            break;
        case 3:
            civ->aggression = clamp(civ->aggression + 1, 0, 10);
            append_log(log, log_size, "%s became ambitious. ", civ->name);
            break;
        default:
            civ->expansion = clamp(civ->expansion + 1, 0, 10);
            civ->disorder_migration = clamp(civ->disorder_migration + 1, 0, 10);
            civ->disorder = clamp(civ->disorder + 1, 0, 10);
            append_log(log, log_size, "%s started migrating. ", civ->name);
            break;
    }
}

static int living_civilizations(void) {
    int i;
    int living = 0;
    for (i = 0; i < civ_count; i++) {
        if (civs[i].alive) living++;
    }
    return living;
}

static void compute_owned_resource_scores(int scores[MAX_CIVS]) {
    int totals[MAX_CIVS] = {0};
    int counts[MAX_CIVS] = {0};
    int i;
    int x;
    int y;

    for (i = 0; i < MAX_CIVS; i++) scores[i] = 0;
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int owner = world[y][x].owner;
            if (owner >= 0 && owner < civ_count && civs[owner].alive) {
                TerrainStats stats = tile_stats(x, y);
                totals[owner] += stats.food + stats.livestock + stats.wood + stats.stone +
                                 stats.minerals + stats.water + stats.pop_capacity + stats.money +
                                 stats.habitability;
                counts[owner]++;
            }
        }
    }
    for (i = 0; i < civ_count; i++) {
        scores[i] = counts[i] > 0 ? totals[i] / (counts[i] * 2) : 0;
    }
}

static int compute_dynamic_adaptation(int civ_id, int resource_score) {
    CountrySummary summary = summarize_country(civ_id);
    Civilization *civ = &civs[civ_id];
    int resource_diversity = 0;
    int hardship;
    int social_capacity;
    int instability;
    int score;

    if (summary.food >= 5) resource_diversity++;
    if (summary.livestock >= 5) resource_diversity++;
    if (summary.wood >= 5) resource_diversity++;
    if (summary.stone >= 5) resource_diversity++;
    if (summary.minerals >= 5) resource_diversity++;
    if (summary.water >= 5) resource_diversity++;
    if (summary.money >= 5) resource_diversity++;

    hardship = clamp(10 - summary.habitability, 0, 10) +
               clamp(5 - summary.food, 0, 5) +
               clamp(5 - summary.water, 0, 5);
    social_capacity = civ->culture + civ->cohesion + civ->logistics + civ->innovation;
    instability = civ->disorder + civ->disorder_plague / 2 + civ->disorder_migration / 2;
    score = social_capacity / 4 + hardship / 3 + resource_diversity / 2 +
            clamp(18 - resource_score, 0, 10) / 3 - instability / 2;
    return clamp(score, 0, 10);
}

void simulate_one_month(void) {
    int i;
    int city_count_before_expansion;
    int resource_scores[MAX_CIVS];
    char log[512];

    log[0] = '\0';
    population_sync_all();
    compute_owned_resource_scores(resource_scores);
    for (i = 0; i < civ_count; i++) {
        Civilization *civ = &civs[i];
        int resources;
        int pressure;
        int pressure_disorder;
        int scarcity_disorder;
        if (!civ->alive) continue;
        resources = resource_scores[i];
        pressure = population_pressure_for_civ(i);
        pressure_disorder = clamp((pressure - 95) / 12, 0, 10);
        scarcity_disorder = clamp(12 - resources, 0, 10) / 2;
        civ->disorder_resource = clamp(pressure_disorder + scarcity_disorder -
                                       civ->governance / 5 - civ->logistics / 6, 0, 10);
        civ->disorder_plague = clamp(civ->disorder_plague - 1, 0, 10);
        civ->disorder_migration = clamp(civ->disorder_migration - 1, 0, 10);
        civ->disorder_stability = clamp(civ->disorder_stability - 1, 0, 10);
        civ->disorder = clamp(civ->disorder + (civ->disorder_resource > 5 ? 1 : -1) +
                              civ->disorder_plague / 4 + civ->disorder_migration / 5 -
                              civ->governance / 8 - civ->cohesion / 8, 0, 10);
        civ->adaptation = compute_dynamic_adaptation(i, resources);
    }

    update_city_growth_and_regions();
    ports_update_migration();
    country_summary_dirty = 1;
    city_count_before_expansion = city_count;
    for (i = 0; i < civ_count; i++) {
        if (!civs[i].alive) continue;
        expansion_update_civilization(i, resource_scores[i], log, sizeof(log));
    }
    if (city_count != city_count_before_expansion) {
        ports_refresh_city_regions();
        maritime_mark_routes_dirty();
        maritime_ensure_routes();
        diplomacy_mark_contacts_dirty();
    }
    plague_update_month();

    random_event(log, sizeof(log));
    recalculate_territory();
    diplomacy_update_contacts();
    month++;
    if (month > 12) {
        month = 1;
        year = clamp(year + 1, 0, 99999);
        diplomacy_update_year();
        war_update_year();
    }
    if (living_civilizations() <= 1) auto_run = 0;
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
                                        int aggression, int expansion, int defense, int culture) {
    Civilization *civ;

    if (civ_id < 0 || civ_id >= civ_count) return;
    civ = &civs[civ_id];
    if (name && name[0] != '\0') snprintf(civ->name, NAME_LEN, "%s", name);
    if (symbol != '\0') civ->symbol = symbol;
    civ->aggression = clamp(aggression, 0, 10);
    civ->expansion = clamp(expansion, 0, 10);
    civ->defense = clamp(defense, 0, 10);
    civ->culture = clamp(culture, 0, 10);
    world_visual_revision++;
}
