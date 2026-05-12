#include "sim/simulation.h"

#include "core/dirty_flags.h"
#include "sim/diplomacy.h"
#include "sim/civilization_slots.h"
#include "sim/civilization_metrics.h"
#include "sim/civ_colors.h"
#include "sim/disorder.h"
#include "sim/expansion.h"
#include "sim/maritime.h"
#include "sim/plague.h"
#include "sim/population.h"
#include "sim/ports.h"
#include "sim/regions.h"
#include "sim/province.h"
#include "sim/province_partition.h"
#include "sim/spawn.h"
#include "sim/territory_integrity.h"
#include "sim/civilization_uid.h"
#include "sim/technology.h"
#include "sim/vassal.h"
#include "sim/war.h"
#include "data/country_names.h"
#include "core/profiler.h"
#include "world/terrain_query.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static CountrySummary country_summary_cache[MAX_CIVS];
static int country_summary_dirty = 1;
static unsigned int territory_contact_hash = 0;
static int last_created_civ_id = -1;

static int is_default_manual_name(const char *name) {
    return !name || !name[0] || strcmp(name, "New Realm") == 0;
}

int civilization_pick_unused_name_id(void) {
    int used[COUNTRY_NAME_COUNT];
    int i;

    memset(used, 0, sizeof(used));
    for (i = 0; i < civ_count; i++) {
        if (!civs[i].custom_name && civs[i].name_id >= 0 && civs[i].name_id < COUNTRY_NAME_COUNT) {
            used[civs[i].name_id] = 1;
        }
    }
    for (i = 0; i < COUNTRY_NAME_COUNT; i++) {
        int candidate = rnd(COUNTRY_NAME_COUNT);
        if (!used[candidate]) return candidate;
    }
    for (i = 0; i < COUNTRY_NAME_COUNT; i++) {
        if (!used[i]) return i;
    }
    return -1;
}

void civilization_assign_generated_name(Civilization *civ, int name_id) {
    if (!civ) return;
    civ->custom_name = 0;
    civ->name_id = name_id;
    if (name_id >= 0 && name_id < COUNTRY_NAME_COUNT) {
        snprintf(civ->name, NAME_LEN, "%s", country_name_localized(name_id, 0));
    } else {
        snprintf(civ->name, NAME_LEN, "Country %d", civ_count + 1);
    }
}

void civilization_set_custom_name(Civilization *civ, const char *name) {
    if (!civ) return;
    civ->custom_name = 1;
    civ->name_id = -1;
    snprintf(civ->name, NAME_LEN, "%s", name && name[0] ? name : "Custom Country");
}

static void civilization_apply_input_name(Civilization *civ, const char *name, int pick_default) {
    int name_id;

    if (pick_default && is_default_manual_name(name)) {
        civilization_assign_generated_name(civ, civilization_pick_unused_name_id());
        return;
    }
    name_id = country_name_find_by_text(name);
    if (name_id >= 0) civilization_assign_generated_name(civ, name_id);
    else civilization_set_custom_name(civ, name);
}

const char *civilization_display_name_for_language(int civ_id, int language) {
    static char fallback[4][NAME_LEN];
    static int fallback_index = 0;
    Civilization *civ;

    if (civ_id < 0 || civ_id >= civ_count) return "";
    civ = &civs[civ_id];
    if (!civ->custom_name && civ->name_id >= 0 && civ->name_id < COUNTRY_NAME_COUNT) {
        return country_name_localized(civ->name_id, language);
    }
    if (!civ->custom_name && civ->name_id < 0) {
        char *buffer = fallback[fallback_index++ % 4];
        snprintf(buffer, NAME_LEN, language == 1 ? "国家 %d" : "Country %d", civ_id + 1);
        return buffer;
    }
    return civ->name;
}

const char *civilization_display_name(int civ_id) {
    return civilization_display_name_for_language(civ_id, ui_language);
}

void civilization_migrate_loaded_names(void) {
    int i;

    for (i = 0; i < civ_count; i++) {
        if (civs[i].name_id >= 0 && civs[i].name_id < COUNTRY_NAME_COUNT) {
            civs[i].custom_name = 0;
            continue;
        }
        civs[i].name_id = country_name_find_by_text(civs[i].name);
        civs[i].custom_name = civs[i].name_id < 0;
    }
}

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

    if (capital && owner >= 0 && owner < civ_count) {
        snprintf(buffer, buffer_size, "%s Capital", civilization_display_name_for_language(owner, 0));
    }
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
    ProfilerCallTrace trace = profiler_call_begin();
    recalculate_territory();
    profiler_call_end("world_recalculate_territory", -1, -1, trace);
}

void world_invalidate_region_cache(void) {
    province_invalidate_region_cache();
    country_summary_dirty = 1;
    population_mark_dirty();
    dirty_mark_territory();
    world_visual_revision++;
}

void world_invalidate_country_summary_cache(void) {
    country_summary_dirty = 1;
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
        int resource_percent = technology_resource_percent(i);
        int disorder_percent = disorder_productivity_percent(civs[i].disorder);
        if (summary->territory <= 0) continue;
        resource_percent = resource_percent * disorder_percent / 100;
        summary->food /= summary->territory;
        summary->livestock /= summary->territory;
        summary->wood /= summary->territory;
        summary->stone /= summary->territory;
        summary->minerals /= summary->territory;
        summary->water /= summary->territory;
        summary->pop_capacity /= summary->territory;
        summary->money /= summary->territory;
        summary->habitability /= summary->territory;
        summary->food = summary->food * resource_percent / 100;
        summary->livestock = summary->livestock * resource_percent / 100;
        summary->wood = summary->wood * resource_percent / 100;
        summary->stone = summary->stone * resource_percent / 100;
        summary->minerals = summary->minerals * resource_percent / 100;
        summary->water = summary->water * resource_percent / 100;
        summary->pop_capacity = summary->pop_capacity * resource_percent / 100;
        summary->money = summary->money * resource_percent / 100;
        summary->resource_score = summary->food + summary->livestock + summary->water +
                                  summary->pop_capacity + summary->money + summary->habitability;
    }
    for (i = 0; i < civ_count; i++) {
        int over = vassal_overlord(i);
        CountrySummary *from;
        CountrySummary *to;
        if (over < 0 || over >= civ_count) continue;
        from = &country_summary_cache[i];
        to = &country_summary_cache[over];
#define MOVE_TRIBUTE(field) do { int t = from->field * 40 / 100; from->field -= t; to->field += t; } while (0)
        MOVE_TRIBUTE(food);
        MOVE_TRIBUTE(livestock);
        MOVE_TRIBUTE(wood);
        MOVE_TRIBUTE(stone);
        MOVE_TRIBUTE(minerals);
        MOVE_TRIBUTE(water);
#undef MOVE_TRIBUTE
    }
    for (i = 0; i < civ_count; i++) {
        CountrySummary *summary = &country_summary_cache[i];
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
    int civ_id;
    Civilization *civ;

    if (!world_generated) return 0;
    last_created_civ_id = -1;
    if (!spawn_select_civilization_cradle(x, y, &x, &y)) return 0;
    region_id = x >= 0 && x < MAP_W && y >= 0 && y < MAP_H ? world[y][x].region_id : -1;

    civ_id = civilization_allocate_slot(1);
    if (civ_id < 0) return 0;
    civ = &civs[civ_id];
    civilization_apply_input_name(civ, name, 1);
    civ->symbol = symbol;
    civ->color = civilization_pick_auto_color(civ_id, region_id);
    civ->alive = 1;
    civ->population = starting_population_for_region(x, y, region_id);
    {
        TerrainStats birth = tile_stats(x, y);
        apply_civilization_core_metrics(civ, governance, cohesion, production, military,
                                        commerce, logistics, innovation, birth, 1);
    }
    civ->capital_city = -1;
    technology_initialize_civ(civ_id);

    city_id = create_city(civ_id, x, y, civ->population, 1);
    if (city_id < 0) {
        civilization_reset_slot_state(civ_id);
        return 0;
    }
    civ->capital_city = city_id;
    if (!regions_claim_as_province(region_id, civ_id, city_id)) {
        if (city_id == city_count - 1) city_count--;
        else cities[city_id].alive = 0;
        civilization_reset_slot_state(civ_id);
        return 0;
    }

    recalculate_territory();
    population_sync_all();
    ports_refresh_city_regions();
    maritime_mark_ownership_dirty();
    maritime_mark_routes_dirty();
    diplomacy_mark_contacts_dirty();
    territory_integrity_repair_capitals();
    last_created_civ_id = civ_id;
    return 1;
}

int simulation_last_created_civ_id(void) { return last_created_civ_id; }

void simulation_reset_state(void) {
    year = 0;
    month = 1;
    civ_count = 0;
    city_count = 0;
    civilization_uid_reset();
    event_log_clear();
    expansion_reset();
    maritime_reset();
    plague_reset();
    territory_integrity_reset();
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
    if (name && name[0] != '\0') civilization_apply_input_name(civ, name, 0);
    if (symbol != '\0') civ->symbol = symbol;
    memset(&birth, 0, sizeof(birth));
    if (civ->capital_city >= 0 && civ->capital_city < city_count) {
        birth = tile_stats(cities[civ->capital_city].x, cities[civ->capital_city].y);
    }
    apply_civilization_core_metrics(civ, governance, cohesion, production, military,
                                    commerce, logistics, innovation, birth, 0);
    dirty_mark_labels();
}
