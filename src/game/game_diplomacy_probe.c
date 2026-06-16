#include "game/game.h"

#include "core/render_snapshot.h"
#include "core/game_state.h"
#include "game/game_diplomacy_relation_probe.h"
#include "game/game_diplomacy_tooltip_probe.h"
#include "game/game_diplomacy_visual_probe.h"
#include "game/game_player_actions.h"
#include "game/game_worldgen.h"
#include "render/map_highlight.h"
#include "render/panel_country_diplomacy_cards.h"
#include "render/render_context.h"
#include "sim/civilization_slots.h"
#include "sim/diplomacy.h"
#include "sim/diplomacy_borders.h"
#include "sim/diplomacy_relation_score.h"
#include "sim/diplomacy_stability.h"
#include "sim/maritime.h"
#include "sim/population.h"
#include "sim/regions.h"
#include "sim/simulation.h"
#include "sim/stability_decision.h"
#include "sim/vassal.h"
#include "sim/war.h"
#include "sim/war_desire.h"
#include "world/terrain_query.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

#define DIPLOMACY_PROBE_DIR "build/validation/diplomacy_alliance_probe_20260613"

static void ensure_probe_dirs(void) {
    CreateDirectoryA("build", NULL);
    CreateDirectoryA("build/validation", NULL);
    CreateDirectoryA(DIPLOMACY_PROBE_DIR, NULL);
}

static void init_probe_civ(int id, const char *name, int city_id, int aggression, int military) {
    civilization_reset_slot_state(id);
    snprintf(civs[id].name, sizeof(civs[id].name), "%s", name);
    civs[id].custom_name = 1;
    civs[id].alive = 1;
    civs[id].heritage = CIV_HERITAGE_WESTERN;
    civs[id].aggression = aggression;
    civs[id].expansion = 5;
    civs[id].governance = 7;
    civs[id].cohesion = 7;
    civs[id].production = 6;
    civs[id].military = military;
    civs[id].commerce = 6;
    civs[id].logistics = 6;
    civs[id].innovation = 5;
    civs[id].capital_city = city_id;
    civs[id].treasury = 800;
    civs[id].treasury_cap = 1200;
}

static void init_probe_region(int id, int owner, int x, int y, int city_id, int rich) {
    NaturalRegion *region = &natural_regions[id];
    memset(region, 0, sizeof(*region));
    region->id = id;
    region->alive = 1;
    region->tile_count = 6;
    region->owner_civ = owner;
    region->city_id = city_id;
    region->center_x = x;
    region->center_y = y;
    region->capital_x = x;
    region->capital_y = y;
    region->habitability = rich ? 7 : 4;
    region->development_score = rich ? 55 : 25;
    region->cradle_score = 10;
    region->viable_direction_count = 2;
    region->average_stats.food = rich ? 7 : 4;
    region->average_stats.livestock = rich ? 7 : 3;
    region->average_stats.wood = rich ? 7 : 3;
    region->average_stats.stone = rich ? 7 : 3;
    region->average_stats.minerals = rich ? 7 : 3;
    region->average_stats.water = rich ? 7 : 4;
    region->average_stats.pop_capacity = rich ? 7 : 3;
    region->average_stats.money = rich ? 7 : 2;
    region->average_stats.habitability = rich ? 7 : 4;
    region->total_stats = region->average_stats;
    world[y][x].geography = GEO_PLAIN;
    world[y][x].climate = CLIMATE_CONTINENTAL;
    world[y][x].ecology = ECO_GRASSLAND;
    world[y][x].resource = RESOURCE_FEATURE_NONE;
    world[y][x].owner = owner;
    world[y][x].province_id = id;
    world[y][x].region_id = id;
}

static void init_probe_city(int id, int owner, const char *name, int x, int y, int pop) {
    City *city = &cities[id];
    memset(city, 0, sizeof(*city));
    city->alive = 1;
    city->owner = owner;
    snprintf(city->name, sizeof(city->name), "%s", name);
    city->x = x;
    city->y = y;
    city->radius = 1;
    city->capital = civs[owner].capital_city == id;
    population_init_city(id, pop);
}

static void add_probe_neighbor(int a, int b) {
    NaturalRegion *ra = &natural_regions[a];
    NaturalRegion *rb = &natural_regions[b];
    if (ra->neighbor_count < MAX_REGION_NEIGHBORS) ra->neighbors[ra->neighbor_count++] = b;
    if (rb->neighbor_count < MAX_REGION_NEIGHBORS) rb->neighbors[rb->neighbor_count++] = a;
}

static void reset_probe_fixture(int peaceful) {
    pending_map_size = MAP_SIZE_SMALL;
    initial_civ_count = 2;
    region_size_slider = 34;
    set_active_map_size(pending_map_size);
    diplomacy_reset();
    war_reset();
    war_desire_reset_all();
    stability_decision_reset();
    vassal_normalize_all();
    simulation_reset_state();
    game_clear_world_tiles();
    memset(natural_regions, 0, sizeof(natural_regions));
    memset(cities, 0, sizeof(cities));
    civ_count = 2;
    city_count = 2;
    region_count = 2;
    world_generated = 1;
    init_probe_civ(0, "Alliance Probe A", 0, peaceful ? 2 : 10, peaceful ? 6 : 10);
    init_probe_civ(1, "Alliance Probe B", 1, peaceful ? 2 : 4, peaceful ? 6 : 3);
    init_probe_region(0, 0, 2, 2, 0, peaceful);
    init_probe_region(1, 1, 3, 2, 1, peaceful);
    add_probe_neighbor(0, 1);
    init_probe_city(0, 0, "Probe Capital A", 2, 2, peaceful ? 4000 : 9000);
    init_probe_city(1, 1, "Probe Capital B", 3, 2, peaceful ? 4000 : 1000);
    civs[0].resource_pressure = peaceful ? 5 : 95;
    civs[1].resource_pressure = peaceful ? 5 : 20;
    terrain_stats_invalidate_cache();
    world_recalculate_territory();
    population_sync_all();
    maritime_rebuild_routes();
    diplomacy_borders_mark_dirty();
    diplomacy_mark_contacts_dirty();
    diplomacy_update_contacts();
}

static DiplomacyRelation probe_relation(DiplomacyStatus state, int score, int tension, int conflict) {
    DiplomacyRelation relation;
    memset(&relation, 0, sizeof(relation));
    relation.state = state;
    relation.relation_score = score;
    relation.border_tension = tension;
    relation.trade_fit = 0;
    relation.resource_conflict = conflict;
    relation.contact_kind = DIP_CONTACT_LAND_BORDER;
    relation.overlord = -1;
    relation.vassal = -1;
    relation.last_war_winner = -1;
    relation.last_war_loser = -1;
    relation.last_war_result = DIP_LAST_WAR_NONE;
    return relation;
}

static void restore_probe_relation(DiplomacyRelation relation) {
    diplomacy_restore_relation(0, 1, relation);
    diplomacy_restore_relation(1, 0, relation);
}

static int case_alliance_blocks(FILE *summary) {
    DiplomacyRelation relation;
    WarDesireBreakdown desire;
    GamePlayerActionResult player_result;
    int started;
    reset_probe_fixture(1);
    relation = probe_relation(DIPLOMACY_ALLIANCE, 90, 10, 5);
    restore_probe_relation(relation);
    desire = war_desire_calculate(0, 1, diplomacy_relation(0, 1));
    started = war_start(0, 1);
    player_result = game_player_declare_war(0, 1);
    fprintf(summary, "case=alliance_blocks final=%d result=%d player=%d started=%d reason=\"%s\"\n",
            desire.final_desire, desire.result, player_result, started, desire.reason);
    return desire.final_desire == 0 && player_result == GAME_PLAYER_ACTION_OK_BROKE_ALLIANCE &&
           !started && diplomacy_status(0, 1) == DIPLOMACY_WAR;
}

static int case_player_alliance_dissolve(FILE *summary) {
    GamePlayerActionResult alliance_result;
    GamePlayerActionResult dissolve_result;
    int alliance_state;
    int final_state;
    reset_probe_fixture(1);
    alliance_result = game_player_form_alliance(0, 1);
    alliance_state = diplomacy_status(0, 1);
    dissolve_result = game_player_dissolve_alliances(0);
    final_state = diplomacy_status(0, 1);
    fprintf(summary,
            "case=player_alliance_dissolve alliance_result=%d alliance_state=%d dissolve_result=%d final_state=%d\n",
            alliance_result, alliance_state, dissolve_result, final_state);
    return alliance_result == GAME_PLAYER_ACTION_OK && alliance_state == DIPLOMACY_ALLIANCE &&
           dissolve_result == GAME_PLAYER_ACTION_OK && final_state == DIPLOMACY_PEACE;
}

static int case_post_war_penalty(FILE *summary) {
    DiplomacyRelation relation;
    WarDesireBreakdown base;
    WarDesireBreakdown cooled;
    reset_probe_fixture(1);
    civs[0].aggression = 5;
    civs[1].aggression = 2;
    civs[0].resource_pressure = 5;
    relation = probe_relation(DIPLOMACY_TENSE, 45, 60, 10);
    base = war_desire_calculate(0, 1, relation);
    relation.last_war_result = DIP_LAST_WAR_NEGOTIATED_TRUCE;
    cooled = war_desire_calculate(0, 1, relation);
    fprintf(summary,
            "case=post_war_penalty base_pre=%d cooled_pre=%d truce=%d cooldown=%d result=%d label=\"Post-war cooldown/战后冷却\" reason=\"%s\"\n",
            base.pre_stability_desire, cooled.pre_stability_desire, cooled.truce_penalty,
            cooled.post_war_cooldown_penalty, cooled.result, cooled.reason);
    return cooled.truce_penalty == 0 && cooled.post_war_cooldown_penalty == 25 &&
           cooled.result == WAR_DESIRE_RESULT_POST_WAR_COOLDOWN &&
           cooled.pre_stability_desire < base.pre_stability_desire;
}

static int case_active_truce_penalty(FILE *summary) {
    DiplomacyRelation relation;
    WarDesireBreakdown truce;
    reset_probe_fixture(1);
    relation = probe_relation(DIPLOMACY_TRUCE, 55, 60, 10);
    relation.truce_years_left = 2;
    relation.last_war_result = DIP_LAST_WAR_NEGOTIATED_TRUCE;
    truce = war_desire_calculate(0, 1, relation);
    fprintf(summary,
            "case=active_truce_penalty truce=%d cooldown=%d result=%d label=\"Truce/停战中\" reason=\"%s\"\n",
            truce.truce_penalty, truce.post_war_cooldown_penalty, truce.result, truce.reason);
    return truce.truce_penalty == 50 && truce.post_war_cooldown_penalty == 0 &&
           truce.result == WAR_DESIRE_RESULT_TRUCE;
}

static int case_truce_expiry_and_memory_clear(FILE *summary) {
    DiplomacyRelation relation;
    DiplomacyRelation after;
    int i;
    reset_probe_fixture(1);
    relation = probe_relation(DIPLOMACY_TRUCE, 70, 60, 20);
    relation.truce_years_left = 1;
    relation.truce_initial_years = 1;
    relation.last_war_result = DIP_LAST_WAR_NEGOTIATED_TRUCE;
    restore_probe_relation(relation);
    diplomacy_update_year();
    after = diplomacy_relation(0, 1);
    for (i = 0; i < 16; i++) diplomacy_update_year();
    relation = diplomacy_relation(0, 1);
    fprintf(summary,
            "case=truce_expiry after_state=%d after_truce=%d final_state=%d last_war=%d easing=%d state_years=%d candidate=%d/%d score=%d trade=%d tension=%d conflict=%d\n",
            after.state, after.truce_years_left, relation.state, relation.last_war_result,
            relation.easing_years, diplomacy_stability_state_years(0, 1),
            diplomacy_stability_candidate_state(0, 1), diplomacy_stability_candidate_years(0, 1),
            relation.relation_score, relation.trade_fit,
            relation.border_tension, relation.resource_conflict);
    return after.state == DIPLOMACY_TENSE && after.truce_years_left == 0 &&
           relation.state == DIPLOMACY_PEACE &&
           relation.last_war_result == DIP_LAST_WAR_NONE;
}

static int case_high_pressure_can_war(FILE *summary) {
    DiplomacyRelation relation;
    WarDesireBreakdown desire;
    int started;
    reset_probe_fixture(0);
    relation = probe_relation(DIPLOMACY_TENSE, 20, 98, 95);
    restore_probe_relation(relation);
    desire = war_desire_calculate(0, 1, diplomacy_relation(0, 1));
    started = war_start(0, 1);
    fprintf(summary, "case=high_pressure_can_war final=%d result=%d started=%d state=%d reason=\"%s\"\n",
            desire.final_desire, desire.result, started, diplomacy_status(0, 1), desire.reason);
    return desire.final_desire >= desire.threshold && started && diplomacy_status(0, 1) == DIPLOMACY_WAR;
}

int run_diplomacy_probe(void) {
    FILE *summary;
    int ok_all = 1;
    ensure_probe_dirs();
    summary = fopen(DIPLOMACY_PROBE_DIR "/summary.txt", "w");
    if (!summary) return 2;
    ok_all &= case_alliance_blocks(summary);
    ok_all &= case_player_alliance_dissolve(summary);
    ok_all &= case_active_truce_penalty(summary);
    ok_all &= case_post_war_penalty(summary);
    ok_all &= case_truce_expiry_and_memory_clear(summary);
    ok_all &= case_high_pressure_can_war(summary);
    ok_all &= run_diplomacy_relation_probe_cases(summary);
    ok_all &= run_diplomacy_tooltip_probe_cases(summary);
    ok_all &= run_diplomacy_visual_probe_cases(summary);
    fprintf(summary, "overall_ok=%d\n", ok_all);
    fclose(summary);
    printf("diplomacy probe summary: %s\\summary.txt\n", DIPLOMACY_PROBE_DIR);
    return ok_all ? 0 : 1;
}
