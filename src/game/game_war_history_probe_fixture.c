#include "game/game_war_history_probe_fixture.h"

#include "core/game_state.h"
#include "game/game_worldgen.h"
#include "sim/civilization_slots.h"
#include "sim/diplomacy.h"
#include "sim/population.h"
#include "sim/regions.h"
#include "sim/simulation.h"
#include "sim/war_history.h"
#include "sim/war_internal.h"
#include "world/terrain_query.h"

#include <stdio.h>
#include <string.h>

static void fill_civilization(int civ_id) {
    Civilization *civ = &civs[civ_id];
    snprintf(civ->name, sizeof(civ->name), "War History Probe %d", civ_id);
    civ->custom_name = 1;
    civ->alive = 1;
    civ->heritage = CIV_HERITAGE_WESTERN;
    civ->capital_city = -1;
    civ->color = COLOR32_RGB(70 + civ_id * 35, 105 + civ_id * 23,
                             145 + civ_id * 17);
    civ->population = 100000;
    civ->cohesion = 8;
    civ->governance = 8;
    civ->military = 7;
    civ->treasury = 0;
    civ->treasury_cap = 1000;
}

void war_history_probe_fixture_reset(int civilization_count) {
    int civ_id;
    set_active_map_size(MAP_SIZE_SMALL);
    war_reset();
    simulation_reset_state();
    diplomacy_reset();
    game_clear_world_tiles();
    memset(civs, 0, sizeof(civs));
    memset(cities, 0, sizeof(cities));
    memset(natural_regions, 0, sizeof(natural_regions));
    civ_count = clamp(civilization_count, 0, MAX_CIVS);
    city_count = 0;
    region_count = 0;
    year = 20;
    month = 1;
    world_generated = 1;
    for (civ_id = 0; civ_id < civ_count; civ_id++) {
        civilization_reset_slot_state(civ_id);
        fill_civilization(civ_id);
    }
    war_history_rebind_current_slots();
}

void war_history_probe_fixture_set_calendar(int current_year, int current_month) {
    year = max(0, current_year);
    month = clamp(current_month, 1, 12);
}

ActiveWar *war_history_probe_fixture_active(int attacker, int defender,
                                            int start_absolute_month) {
    ActiveWar *war = &active_wars[0];
    memset(war, 0, sizeof(*war));
    war->active = 1;
    war->attacker = attacker;
    war->defender = defender;
    war->war_serial = war_history_allocate_serial();
    war->attacker_uid = civs[attacker].uid;
    war->defender_uid = civs[defender].uid;
    war->start_absolute_month = max(0, start_absolute_month);
    war->initial_national_a = 1000;
    war->initial_national_b = 1000;
    war->initial_soldiers_a = 100;
    war->initial_soldiers_b = 100;
    war->soldiers_a = 100;
    war->soldiers_b = 100;
    war->casualties_a = 17;
    war->casualties_b = 29;
    war->support_casualties_a = 31;
    war->support_casualties_b = 37;
    return war;
}

WarHistoryRecord war_history_probe_fixture_record(int local_civ, int opponent_civ,
                                                  uint64_t serial, int result,
                                                  int winner_uid, int loser_uid) {
    WarHistoryRecord record;
    memset(&record, 0, sizeof(record));
    record.war_serial = serial;
    record.local.uid = civs[local_civ].uid;
    snprintf(record.local.name_en, sizeof(record.local.name_en), "%s", civs[local_civ].name);
    snprintf(record.local.name_zh, sizeof(record.local.name_zh), "%s", civs[local_civ].name);
    record.local.color = civs[local_civ].color;
    record.opponent.uid = civs[opponent_civ].uid;
    snprintf(record.opponent.name_en, sizeof(record.opponent.name_en), "%s",
             civs[opponent_civ].name);
    snprintf(record.opponent.name_zh, sizeof(record.opponent.name_zh), "%s",
             civs[opponent_civ].name);
    record.opponent.color = civs[opponent_civ].color;
    record.result = result;
    record.winner_uid = winner_uid;
    record.loser_uid = loser_uid;
    record.local_casualties = 10 + local_civ;
    record.opponent_casualties = 10 + opponent_civ;
    record.end_year = year;
    record.end_month = month;
    record.duration_months = 1;
    return record;
}

static void add_neighbor(int a, int b) {
    NaturalRegion *left = &natural_regions[a];
    NaturalRegion *right = &natural_regions[b];
    left->neighbors[left->neighbor_count++] = b;
    right->neighbors[right->neighbor_count++] = a;
}

static void fill_region_city(int region_id, int owner, int x, int y) {
    NaturalRegion *region = &natural_regions[region_id];
    City *city = &cities[region_id];
    memset(region, 0, sizeof(*region));
    memset(city, 0, sizeof(*city));
    region->id = region_id;
    region->alive = 1;
    region->owner_civ = owner;
    region->city_id = region_id;
    region->tile_count = 1;
    region->center_x = region->capital_x = x;
    region->center_y = region->capital_y = y;
    region->habitability = 6;
    region->development_score = 30;
    region->average_stats.food = 6;
    region->average_stats.water = 6;
    region->average_stats.pop_capacity = 6;
    region->average_stats.habitability = 6;
    region->average_stats.money = 4;
    region->total_stats = region->average_stats;
    city->alive = 1;
    city->owner = owner;
    city->x = x;
    city->y = y;
    city->population = 1000;
    city->radius = 1;
    city->capital = civs[owner].capital_city == region_id;
    population_init_city(region_id, city->population);
    world[y][x].geography = GEO_PLAIN;
    world[y][x].climate = CLIMATE_CONTINENTAL;
    world[y][x].owner = owner;
    world[y][x].province_id = region_id;
    world[y][x].region_id = region_id;
}

ActiveWar *war_history_probe_fixture_settlement(WarHistoryProbeSettlement settlement) {
    int i;
    war_history_probe_fixture_reset(2);
    city_count = 20;
    region_count = 20;
    civs[0].capital_city = 0;
    civs[1].capital_city = 10;
    civs[1].treasury = settlement == WAR_HISTORY_PROBE_SETTLEMENT_INDEMNITY ||
                       settlement == WAR_HISTORY_PROBE_SETTLEMENT_BOTH ? 1000 : 0;
    for (i = 0; i < 10; i++) {
        fill_region_city(i, 0, 2 + i, 2);
        fill_region_city(10 + i, 1, 2 + i, 4);
    }
    if (settlement == WAR_HISTORY_PROBE_SETTLEMENT_CESSION ||
        settlement == WAR_HISTORY_PROBE_SETTLEMENT_BOTH) {
        add_neighbor(0, 10);
        add_neighbor(1, 11);
    }
    terrain_stats_invalidate_cache();
    regions_claim_cache_reset();
    world_recalculate_territory();
    population_sync_all();
    war_history_probe_fixture_set_calendar(20, 6);
    return war_history_probe_fixture_active(0, 1, 20 * 12);
}

int war_history_probe_owned_regions(int civ_id) {
    int count = 0;
    int i;
    for (i = 0; i < region_count; i++) {
        if (natural_regions[i].alive && natural_regions[i].owner_civ == civ_id) count++;
    }
    return count;
}
