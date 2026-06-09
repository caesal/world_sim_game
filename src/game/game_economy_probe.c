#include "game/game.h"

#include "core/game_types.h"
#include "core/dirty_flags.h"
#include "core/profiler.h"
#include "core/render_snapshot_keys.h"
#include "game/game_worldgen.h"
#include "sim/civilization_slots.h"
#include "sim/decision_snapshot.h"
#include "sim/diplomacy.h"
#include "sim/economy.h"
#include "sim/maritime.h"
#include "sim/population.h"
#include "sim/population_diagnostics.h"
#include "sim/ports.h"
#include "sim/regions.h"
#include "sim/route_potential.h"
#include "sim/simulation.h"
#include "sim/simulation_month.h"
#include "sim/vassal.h"
#include "sim/war.h"
#include "world/world_gen.h"
#include "world/ports.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

#define ECONOMY_PROBE_DIR "build/validation/economy_perf_treasury_split_20260606"

static void ensure_probe_dirs(void) {
    CreateDirectoryA("build", NULL);
    CreateDirectoryA("build/validation", NULL);
    CreateDirectoryA(ECONOMY_PROBE_DIR, NULL);
}

static void reset_probe_world(void) {
    pending_map_size = MAP_SIZE_LARGE;
    initial_civ_count = 26;
    region_size_slider = 34;
    set_active_map_size(pending_map_size);
    diplomacy_reset();
    war_reset();
    simulation_reset_state();
    game_clear_world_tiles();
    selected_x = -1;
    selected_y = -1;
    selected_civ = -1;
    auto_run = 0;
    world_generated = 0;
    event_log_clear();
}

static WorldGenConfig probe_config(unsigned int seed, int round) {
    WorldGenConfig config = DEFAULT_WORLD_GEN_CONFIG;
    config.seed = seed;
    config.random_seed = 0;
    config.ocean = 42 + (int)(seed % 19);
    config.continent = 44 + (int)((seed / 7) % 21);
    config.relief = 45 + (int)((seed / 11) % 28);
    config.moisture = 38 + (int)((seed / 13) % 31);
    config.drought = 35 + (int)((seed / 17) % 34);
    config.vegetation = 42 + (int)((seed / 19) % 29);
    config.bias_forest = 35 + (int)((seed / 23 + round * 7) % 45);
    config.bias_desert = 30 + (int)((seed / 29 + round * 11) % 45);
    config.bias_mountain = 30 + (int)((seed / 31 + round * 13) % 50);
    config.bias_wetland = 30 + (int)((seed / 37 + round * 17) % 45);
    return config;
}

static int alive_war_count(int civ_id) {
    int other, count = 0;
    for (other = 0; other < civ_count; other++) {
        if (other != civ_id && war_active_between(civ_id, other)) count++;
    }
    return count;
}

static int split_fixture_asset_for_region(int index) {
    static const int assets[10] = {20, 15, 15, 8, 7, 7, 7, 7, 7, 7};
    return index >= 0 && index < 10 ? assets[index] : 0;
}

static void setup_split_fixture(int child_pop) {
    int i;
    reset_probe_world();
    memset(world, 0, sizeof(world));
    memset(natural_regions, 0, sizeof(natural_regions));
    memset(cities, 0, sizeof(cities));
    civ_count = 2;
    region_count = 10;
    city_count = 10;
    for (i = 0; i < 2; i++) {
        civilization_reset_slot_state(i);
        snprintf(civs[i].name, sizeof(civs[i].name), "%s", i == 0 ? "Parent" : "Child");
        civs[i].custom_name = 1;
        civs[i].alive = 1;
        civs[i].heritage = CIV_HERITAGE_WESTERN;
        civs[i].governance = civs[i].cohesion = civs[i].commerce = 8;
        civs[i].military = civs[i].production = civs[i].logistics = 6;
        civs[i].culture = civs[i].innovation = civs[i].adaptation = 6;
        civs[i].capital_city = i == 0 ? 3 : 0;
    }
    for (i = 0; i < 10; i++) {
        int owner = i < 3 ? 1 : 0;
        int pop = owner == 1 ? child_pop : 20000;
        natural_regions[i].id = i;
        natural_regions[i].alive = 1;
        natural_regions[i].tile_count = 1;
        natural_regions[i].owner_civ = owner;
        natural_regions[i].city_id = i;
        natural_regions[i].center_x = natural_regions[i].capital_x = i;
        natural_regions[i].center_y = natural_regions[i].capital_y = 0;
        natural_regions[i].total_stats.food = split_fixture_asset_for_region(i);
        natural_regions[i].average_stats.food = 5;
        natural_regions[i].average_stats.water = 5;
        natural_regions[i].average_stats.pop_capacity = 5;
        natural_regions[i].average_stats.habitability = 5;
        cities[i].alive = 1;
        cities[i].owner = owner;
        cities[i].x = i;
        cities[i].y = 0;
        cities[i].capital = i == civs[owner].capital_city;
        world[0][i].geography = GEO_PLAIN;
        world[0][i].owner = owner;
        world[0][i].region_id = i;
        world[0][i].province_id = i;
        population_init_city(i, pop);
        civs[owner].territory += 1;
    }
    world_generated = 1;
    world_invalidate_region_cache();
    population_sync_all();
    economy_initialize_civ(0);
    economy_initialize_civ(1);
    civs[0].treasury = 1000;
    civs[1].treasury = 0;
    economy_normalize_civ(0);
    economy_normalize_civ(1);
}

static void setup_multi_split_fixture(int parent_pop, int child_b_pop, int parent_over_cap) {
    int i;
    reset_probe_world();
    memset(world, 0, sizeof(world));
    memset(natural_regions, 0, sizeof(natural_regions));
    memset(cities, 0, sizeof(cities));
    civ_count = 3;
    region_count = 10;
    city_count = 10;
    for (i = 0; i < 3; i++) {
        civilization_reset_slot_state(i);
        snprintf(civs[i].name, sizeof(civs[i].name), "%s",
                 i == 0 ? "Parent" : (i == 1 ? "ChildA" : "ChildB"));
        civs[i].custom_name = 1;
        civs[i].alive = 1;
        civs[i].heritage = CIV_HERITAGE_WESTERN;
        civs[i].governance = civs[i].cohesion = civs[i].commerce = 8;
        civs[i].military = civs[i].production = civs[i].logistics = 6;
        civs[i].culture = civs[i].innovation = civs[i].adaptation = 6;
        civs[i].capital_city = i == 0 ? 5 : (i == 1 ? 0 : 3);
    }
    for (i = 0; i < 10; i++) {
        int owner = i < 3 ? 1 : (i < 5 ? 2 : 0);
        int pop = owner == 0 ? parent_pop : (owner == 2 ? child_b_pop : 20000);
        natural_regions[i].id = i;
        natural_regions[i].alive = 1;
        natural_regions[i].tile_count = 1;
        natural_regions[i].owner_civ = owner;
        natural_regions[i].city_id = i;
        natural_regions[i].center_x = natural_regions[i].capital_x = i;
        natural_regions[i].center_y = natural_regions[i].capital_y = 0;
        natural_regions[i].total_stats.food = 10;
        natural_regions[i].average_stats.food = 5;
        natural_regions[i].average_stats.water = 5;
        natural_regions[i].average_stats.pop_capacity = 5;
        natural_regions[i].average_stats.habitability = 5;
        cities[i].alive = 1;
        cities[i].owner = owner;
        cities[i].x = i;
        cities[i].y = 0;
        cities[i].capital = i == civs[owner].capital_city;
        world[0][i].geography = GEO_PLAIN;
        world[0][i].owner = owner;
        world[0][i].region_id = i;
        world[0][i].province_id = i;
        population_init_city(i, pop);
        civs[owner].territory += 1;
    }
    world_generated = 1;
    world_invalidate_region_cache();
    population_sync_all();
    for (i = 0; i < 3; i++) economy_initialize_civ(i);
    civs[0].treasury = 1000;
    civs[1].treasury = 0;
    civs[2].treasury = 0;
    if (!parent_over_cap) economy_normalize_civ(0);
    economy_normalize_civ(1);
    economy_normalize_civ(2);
}

static void write_treasury_split_fixtures(FILE *summary) {
    int parent_asset = 100;
    int child_asset = 50;
    int transfer;
    int transfer_a;
    int transfer_b;
    int civ_key_before, civ_key_after, visual_before, visual_after;
    setup_split_fixture(20000);
    transfer = economy_split_treasury_to_child(0, 1, child_asset, parent_asset);
    fprintf(summary, "split_exact transfer=%d parent=%d child=%d expected_parent=500 expected_child=500\n",
            transfer, civs[0].treasury, civs[1].treasury);
    setup_split_fixture(1000);
    civs[0].treasury = 2000;
    economy_normalize_civ(0);
    transfer = economy_split_treasury_to_child(0, 1, child_asset, parent_asset);
    fprintf(summary, "split_cap transfer=%d parent=%d child=%d child_cap=%d raw=1000\n",
            transfer, civs[0].treasury, civs[1].treasury, civs[1].treasury_cap);
    setup_split_fixture(20000);
    transfer = economy_split_treasury_to_child(0, 1, child_asset, parent_asset);
    fprintf(summary, "collapse_successor_formula transfer=%d parent=%d child=%d\n",
            transfer, civs[0].treasury, civs[1].treasury);
    setup_split_fixture(20000);
    transfer = economy_split_treasury_to_child(0, 1, child_asset, parent_asset);
    fprintf(summary, "enclave_independent_formula transfer=%d parent=%d child=%d\n",
            transfer, civs[0].treasury, civs[1].treasury);
    setup_split_fixture(20000);
    diplomacy_start_vassal(0, 1, 70);
    transfer = economy_split_treasury_to_child(0, 1, child_asset, parent_asset);
    fprintf(summary, "original_vassal_formula transfer=%d parent=%d child=%d overlord=%d\n",
            transfer, civs[0].treasury, civs[1].treasury, vassal_overlord(1));
    setup_split_fixture(20000);
    civilization_reset_slot_state(1);
    fprintf(summary, "claim_failure_no_transfer parent=%d child_alive=%d\n", civs[0].treasury, civs[1].alive);
    setup_multi_split_fixture(20000, 20000, 0);
    transfer_a = economy_split_treasury_snapshot_to_child(0, 1, 1000, 30, parent_asset);
    transfer_b = economy_split_treasury_snapshot_to_child(0, 2, 1000, 20, parent_asset);
    fprintf(summary, "multi_successor_snapshot transfer_a=%d transfer_b=%d parent=%d child_a=%d child_b=%d "
            "expected_a=300 expected_b=200 expected_parent=500\n",
            transfer_a, transfer_b, civs[0].treasury, civs[1].treasury, civs[2].treasury);
    setup_multi_split_fixture(20000, 100, 0);
    civs[2].governance = 1;
    civs[2].commerce = 1;
    economy_normalize_civ(2);
    transfer_b = economy_split_treasury_snapshot_to_child(0, 2, 2000, 20, parent_asset);
    fprintf(summary, "multi_successor_cap transfer_b=%d parent=%d child_b=%d child_b_cap=%d raw=400\n",
            transfer_b, civs[0].treasury, civs[2].treasury, civs[2].treasury_cap);
    setup_multi_split_fixture(20000, 20000, 0);
    civilization_reset_slot_state(2);
    transfer_b = economy_split_treasury_snapshot_to_child(0, 2, 1000, 20, parent_asset);
    fprintf(summary, "multi_successor_claim_failure transfer_b=%d parent=%d child_b_alive=%d\n",
            transfer_b, civs[0].treasury, civs[2].alive);
    setup_multi_split_fixture(100, 20000, 1);
    transfer_a = economy_split_treasury_snapshot_to_child(0, 1, 1000, 30, parent_asset);
    fprintf(summary, "parent_cap_preserve transfer=%d parent=%d parent_cap=%d expected_parent=700 "
            "extra_cap_loss=%d\n",
            transfer_a, civs[0].treasury, civs[0].treasury_cap, 700 - civs[0].treasury);
    civ_key_before = render_snapshot_civs_revision_key();
    visual_before = render_snapshot_civ_visual_revision_key();
    dirty_mark_civ_stats();
    civ_key_after = render_snapshot_civs_revision_key();
    visual_after = render_snapshot_civ_visual_revision_key();
    fprintf(summary, "civ_stats_revision_probe civs_changed=%d visual_changed=%d\n",
            civ_key_before != civ_key_after, visual_before != visual_after);
}

static void write_probe_header(FILE *file) {
    fprintf(file, "round,seed,year,month,civ_id,name,population,carrying_capacity,resource_pressure,");
    fprintf(file, "actual_population_pressure,national_resource_pressure,capacity_overload_pressure,birth_multiplier,births_month,pressure_deaths_month,natural_age_deaths_month,child_stress_deaths_month,total_deaths_month,net_month,");
    fprintf(file, "treasury,treasury_cap,pending_surplus,annual_balance,deficit_years,");
    fprintf(file, "raw_disorder,effective_disorder,disorder_resource,disorder_plague,disorder_stability,");
    fprintf(file, "stability_project_months,stability_cooldown,mercenary_cooldown,");
    fprintf(file, "tech_stage,cities,ports,wars,vassals,main_intent,expansion_weight,war_weight,stability_weight,pop_delta_period\n");
}

static void write_probe_rows(FILE *file, int round, unsigned int seed, int *last_population) {
    int i;
    for (i = 0; i < civ_count; i++) {
        const Civilization *civ = &civs[i];
        CountrySummary country;
        PopulationSummary pop;
        PopulationDiagnostics diag;
        DecisionSnapshot decision;
        int delta;
        if (!civ->alive) continue;
        country = summarize_country(i);
        pop = population_country_summary(i);
        diag = population_diagnostics_for_country(i, pop, country);
        decision_snapshot_for_civ(i, &decision);
        delta = last_population[i] > 0 ? pop.total - last_population[i] : 0;
        last_population[i] = pop.total;
        fprintf(file, "%d,%u,%d,%d,%d,\"%s\",%d,%d,%d,",
                round, seed, year, month, i, civilization_display_name_for_language(i, 0),
                pop.total, pop.carrying_capacity, civ->resource_pressure);
        fprintf(file, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,",
                diag.effective_pressure, diag.national_resource_pressure,
                diag.local_overcapacity_pressure, diag.birth_multiplier_percent,
                diag.estimated_monthly_births, diag.estimated_pressure_deaths,
                diag.estimated_natural_age_deaths, diag.estimated_child_stress_deaths,
                diag.estimated_total_deaths, diag.estimated_net_monthly_change);
        fprintf(file, "%d,%d,%d,%d,%d,",
                civ->treasury, civ->treasury_cap, civ->treasury_pending_surplus,
                civ->treasury_last_annual_balance, civ->treasury_deficit_years);
        fprintf(file, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,",
                civ->disorder, economy_effective_disorder_for_civ(i), civ->disorder_resource,
                civ->disorder_plague, civ->disorder_stability,
                civ->treasury_stability_months_left, civ->treasury_stability_cooldown_months,
                civ->mercenary_cooldown_months, civ->tech_stage, country.cities, country.ports,
                alive_war_count(i), vassal_direct_count(i));
        fprintf(file, "\"%s\",%d,%d,%d,%d\n",
                decision.main_intent ? decision.main_intent : "",
                decision.expansion_weight, decision.war_weight, decision.stability_weight, delta);
    }
}

static void write_event_example(FILE *summary, const char *label, const EventLogEntry *entry) {
    char en[256];
    char zh[256];
    if (!entry) return;
    event_log_format_entry_data(entry, 0, en, sizeof(en));
    event_log_format_entry_data(entry, 1, zh, sizeof(zh));
    fprintf(summary, "%s_en=%s\n%s_zh=%s\n", label, en, label, zh);
}

static void write_event_summary(FILE *summary) {
    EventLogEntry entry;
    EventLogEntry first_stability;
    EventLogEntry first_mercenary;
    EventLogEntry first_indemnity;
    int has_stability = 0;
    int has_mercenary = 0;
    int has_indemnity = 0;
    int stability_count = 0;
    int mercenary_count = 0;
    int indemnity_count = 0;
    int war_count = 0;
    int truce_count = 0;
    int i;

    memset(&first_stability, 0, sizeof(first_stability));
    memset(&first_mercenary, 0, sizeof(first_mercenary));
    memset(&first_indemnity, 0, sizeof(first_indemnity));
    for (i = 0; i < event_log_count; i++) {
        if (!event_log_get_entry(i, &entry)) continue;
        if (entry.type == EVENT_TYPE_WAR_STARTED) war_count++;
        else if (entry.type == EVENT_TYPE_TRUCE_SIGNED) truce_count++;
        else if (entry.type == EVENT_TYPE_STABILITY_PROJECT) {
            stability_count++;
            if (!has_stability) {
                first_stability = entry;
                has_stability = 1;
            }
        } else if (entry.type == EVENT_TYPE_MERCENARIES_HIRED) {
            mercenary_count++;
            if (!has_mercenary) {
                first_mercenary = entry;
                has_mercenary = 1;
            }
        } else if (entry.type == EVENT_TYPE_TREASURY_INDEMNITY) {
            indemnity_count++;
            if (!has_indemnity) {
                first_indemnity = entry;
                has_indemnity = 1;
            }
        }
    }
    fprintf(summary, "events_total=%d wars=%d truces=%d stability_projects=%d mercenaries=%d indemnities=%d\n",
            event_log_count, war_count, truce_count, stability_count, mercenary_count, indemnity_count);
    if (has_stability) write_event_example(summary, "stability_example", &first_stability);
    if (has_mercenary) write_event_example(summary, "mercenary_example", &first_mercenary);
    if (has_indemnity) write_event_example(summary, "indemnity_example", &first_indemnity);
}

static int run_one_probe(FILE *summary, int round, unsigned int seed) {
    char path[160];
    FILE *csv;
    WorldGenConfig config = probe_config(seed, round);
    int last_population[MAX_CIVS] = {0};
    int month_index;
    RuntimeProfilerSnapshot perf;
    char max_phase[32] = "none";
    int max_phase_ms = 0;
    int max_terrain_rebuilds = 0;
    int max_political_rebuilds = 0;
    int max_border_rebuilds = 0;
    int max_label_rebuilds = 0;
    snprintf(path, sizeof(path), "%s/round_%d_seed_%u.csv", ECONOMY_PROBE_DIR, round, seed);
    csv = fopen(path, "w");
    if (!csv) return 0;
    reset_probe_world();
    generate_world_with_config(&config);
    world_generated = 1;
    ports_reset_regions();
    regions_generate(region_size_slider);
    ports_ensure_island_ports();
    world_invalidate_region_cache();
    simulation_seed_default_civilizations();
    world_recalculate_territory();
    ports_ensure_island_ports();
    ports_refresh_city_regions();
    route_potential_rebuild();
    maritime_rebuild_routes();
    diplomacy_update_contacts();
    write_probe_header(csv);
    write_probe_rows(csv, round, seed, last_population);
    for (month_index = 0; month_index < 240 * 12; month_index++) {
        simulation_month_run_blocking();
        profiler_snapshot(&perf);
        if (perf.slowest_phase_ms > max_phase_ms) {
            snprintf(max_phase, sizeof(max_phase), "%s",
                     perf.slowest_phase[0] ? perf.slowest_phase : "none");
            max_phase_ms = perf.slowest_phase_ms;
            max_terrain_rebuilds = perf.terrain_rebuild_count;
            max_political_rebuilds = perf.political_rebuild_count;
            max_border_rebuilds = perf.border_rebuild_count;
            max_label_rebuilds = perf.label_rebuild_count;
        }
        if (year % 10 == 0 && month == 1) write_probe_rows(csv, round, seed, last_population);
    }
    profiler_snapshot(&perf);
    fprintf(summary, "round=%d seed=%u final_year=%d month=%d civs=%d regions=%d csv=%s\n",
            round, seed, year, month, civ_count, region_count, path);
    fprintf(summary, "perf latest_slowest_phase=%s latest_slowest_ms=%d terrain_rebuilds=%d political_rebuilds=%d border_rebuilds=%d label_rebuilds=%d\n",
            perf.slowest_phase[0] ? perf.slowest_phase : "none", perf.slowest_phase_ms,
            perf.terrain_rebuild_count, perf.political_rebuild_count,
            perf.border_rebuild_count, perf.label_rebuild_count);
    fprintf(summary, "perf_max phase=%s ms=%d terrain_rebuilds=%d political_rebuilds=%d border_rebuilds=%d label_rebuilds=%d\n",
            max_phase, max_phase_ms, max_terrain_rebuilds, max_political_rebuilds,
            max_border_rebuilds, max_label_rebuilds);
    write_event_summary(summary);
    fclose(csv);
    return 1;
}

int run_economy_probe(void) {
    static const unsigned int seeds[] = {160661u, 271883u, 390307u};
    FILE *summary;
    int round;
    int i;
    ensure_probe_dirs();
    summary = fopen(ECONOMY_PROBE_DIR "/summary.txt", "w");
    if (!summary) return 1;
    write_treasury_split_fixtures(summary);
    for (round = 1; round <= 1; round++) {
        for (i = 0; i < (int)(sizeof(seeds) / sizeof(seeds[0])); i++) {
            if (!run_one_probe(summary, round, seeds[i] + (unsigned int)round * 1009u)) {
                fclose(summary);
                return 1;
            }
        }
    }
    fclose(summary);
    return 0;
}
