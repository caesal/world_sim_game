#include "game/game_plague_probe_internal.h"

#include "core/game_types.h"
#include "sim/maritime.h"
#include "sim/plague_adjacency.h"
#include "sim/plague_spread.h"
#include "sim/plague_state.h"
#include "sim/regions.h"
#include "sim/route_potential.h"
#include "sim/sea_lanes.h"

#include <stdlib.h>
#include <string.h>

static void reset_topology(int cities_in_fixture) {
    int i;
    memset(cities, 0, sizeof(cities));
    memset(civs, 0, sizeof(civs));
    memset(natural_regions, 0, sizeof(natural_regions));
    civ_count = 1;
    city_count = cities_in_fixture;
    region_count = cities_in_fixture;
    civs[0].alive = 1;
    civs[0].uid = 801;
    civs[0].tech_stage = 0;
    civs[0].population = cities_in_fixture * 1000;
    for (i = 0; i < cities_in_fixture; i++) {
        cities[i].alive = 1;
        cities[i].owner = 0;
        cities[i].population = 1000;
        cities[i].x = i;
        cities[i].y = 0;
        world[0][i].region_id = i;
        natural_regions[i].id = i;
        natural_regions[i].alive = 1;
        natural_regions[i].tile_count = 1;
        natural_regions[i].owner_civ = 0;
        natural_regions[i].city_id = i;
    }
    route_potential_reset();
    maritime_reset();
    sea_lanes_invalidate();
    plague_adjacency_reset();
    plague_spread_reset();
    plague_state_reset();
    srand(4400341u);
}

static void add_land_edge(int source_region, int target_region) {
    NaturalRegion *source = &natural_regions[source_region];
    if (source->neighbor_count >= MAX_REGION_NEIGHBORS) return;
    source->neighbors[source->neighbor_count++] = target_region;
}

static void begin_small_episode_with_severity(int spores, int severity) {
    PlagueEpisodeState episode;
    memset(&episode, 0, sizeof(episode));
    episode.active = 1;
    episode.size = PLAGUE_SIZE_SMALL;
    episode.severity = severity;
    episode.start_month = 0;
    episode.spores_initial = spores;
    episode.spores_remaining = spores;
    plague_state_begin_episode(&episode);
}

static void begin_small_episode(int spores) {
    begin_small_episode_with_severity(spores, 4);
}

static void gather_all(void) {
    while (!plague_spread_gather_step(1)) {
    }
}

static void check_cached_land_adjacency(PlagueProbeContext *context) {
    const PlagueAdjacencyContact *contacts;
    int count = 0;
    int first;
    int second;
    reset_topology(3);
    add_land_edge(0, 2);
    first = plague_adjacency_refresh();
    second = plague_adjacency_refresh();
    contacts = plague_adjacency_contacts(0, PLAGUE_ROUTE_LAND, &count);
    plague_probe_check(context, "adjacency", "cached_land_neighbors", first == 1 &&
        second == 0 && count == 1 && contacts && contacts[0].target_city == 2 &&
        plague_adjacency_revision() == 1 && plague_adjacency_rebuild_count() == 1,
        "first=%d second=%d count=%d revision=%d rebuilds=%d",
        first, second, count, plague_adjacency_revision(),
        plague_adjacency_rebuild_count());
}

static void check_pending_dedupe_commit(PlagueProbeContext *context) {
    PlagueSpreadStats stats;
    const PlagueModelState *model;
    int resolved;
    reset_topology(3);
    add_land_edge(0, 2);
    add_land_edge(1, 2);
    begin_small_episode(3);
    plague_state_infect_city(0, 0, 0, 42);
    plague_state_infect_city(1, 0, 0, 42);
    plague_spread_begin_month(6);
    gather_all();
    plague_spread_stats(&stats);
    plague_probe_check(context, "spread", "gather_is_pending_and_batched",
        stats.source_count == 2 && stats.due_pulses == 2 &&
        stats.pending_requests == 2 && stats.candidate_edges == 2 &&
        !plague_state_get()->cities[2].active,
        "sources=%d due=%d pending=%d edges=%d", stats.source_count,
        stats.due_pulses, stats.pending_requests, stats.candidate_edges);
    resolved = plague_spread_prepare_commits();
    plague_spread_stats(&stats);
    plague_probe_check(context, "spread", "same_target_requests_deduplicate",
        resolved == 1 && stats.deduplicated_requests == 1 &&
        !plague_state_get()->cities[2].active,
        "resolved=%d deduplicated=%d", resolved, stats.deduplicated_requests);
    plague_spread_commit_step(1);
    plague_spread_stats(&stats);
    model = plague_state_get();
    plague_probe_check(context, "spread", "commit_infects_same_month_without_cascade",
        stats.committed_infections == 1 && model->cities[2].active &&
        model->cities[2].generation == 1 &&
        model->cities[2].infection_start_month == 6 &&
        model->cities[2].next_pulse_month == 12 &&
        model->episode.spores_remaining == 2 && model->active_city_count == 3,
        "generation=%d start=%d next_pulse=%d spores=%d active=%d",
        model->cities[2].generation, model->cities[2].infection_start_month,
        model->cities[2].next_pulse_month, model->episode.spores_remaining,
        model->active_city_count);
}

static void check_spore_budget_caps_commits(PlagueProbeContext *context) {
    PlagueSpreadStats stats;
    const PlagueModelState *model;
    int infected_targets;
    reset_topology(4);
    add_land_edge(0, 2);
    add_land_edge(1, 3);
    begin_small_episode(1);
    plague_state_infect_city(0, 0, 0, 42);
    plague_state_infect_city(1, 0, 0, 42);
    plague_spread_begin_month(6);
    gather_all();
    plague_spread_prepare_commits();
    plague_spread_commit_step(10);
    plague_spread_stats(&stats);
    model = plague_state_get();
    infected_targets = model->cities[2].active + model->cities[3].active;
    plague_probe_check(context, "spread", "spores_cap_batched_commits",
        stats.pending_requests == 2 && stats.deduplicated_requests == 0 &&
        stats.committed_infections == 1 && infected_targets == 1 &&
        model->episode.spores_remaining == 0 && plague_spread_commit_done(),
        "pending=%d committed=%d infected_targets=%d spores=%d",
        stats.pending_requests, stats.committed_infections, infected_targets,
        model->episode.spores_remaining);
}

static void run_persistence_month(int absolute_month) {
    plague_spread_begin_month(absolute_month);
    gather_all();
    plague_spread_prepare_commits();
    plague_spread_commit_step(10);
}

static void check_persistence_and_cap(PlagueProbeContext *context) {
    PlagueSpreadStats stats;
    const PlagueModelState *model;
    int no_land_count = -1;
    reset_topology(1);
    plague_adjacency_contacts(0, PLAGUE_ROUTE_LAND, &no_land_count);
    begin_small_episode(3);
    plague_state_infect_city(0, 0, 0, 24);
    run_persistence_month(6);
    model = plague_state_get();
    plague_probe_check(context, "spread", "isolated_city_persists_twelve_months",
        no_land_count == 0 && model->cities[0].recovery_month == 36 &&
        model->episode.spores_remaining == 2,
        "land_neighbors=%d recovery=%d spores=%d", no_land_count,
        model->cities[0].recovery_month, model->episode.spores_remaining);
    run_persistence_month(12);
    run_persistence_month(18);
    plague_spread_stats(&stats);
    model = plague_state_get();
    plague_probe_check(context, "spread", "continuous_city_cap_stops_persistence",
        model->cities[0].recovery_month == 48 &&
        model->episode.spores_remaining == 1 && stats.due_pulses == 1 &&
        stats.pending_requests == 0 && stats.committed_persistence == 0,
        "recovery=%d cap=48 spores=%d final_pending=%d",
        model->cities[0].recovery_month, model->episode.spores_remaining,
        stats.pending_requests);
}

static void check_generation_cap(PlagueProbeContext *context) {
    PlagueSpreadStats stats;
    reset_topology(2);
    add_land_edge(0, 1);
    begin_small_episode(2);
    plague_state_infect_city(0, 4, 0, 42);
    plague_spread_begin_month(6);
    gather_all();
    plague_spread_prepare_commits();
    plague_spread_stats(&stats);
    plague_probe_check(context, "spread", "maximum_generation_blocks_spread",
        stats.due_pulses == 1 && stats.pending_requests == 0 &&
        !plague_state_get()->cities[1].active &&
        plague_state_get()->episode.spores_remaining == 2,
        "generation=4 small_max=4 pending=%d spores=%d",
        stats.pending_requests, plague_state_get()->episode.spores_remaining);

    reset_topology(2);
    add_land_edge(0, 1);
    begin_small_episode(2);
    plague_state_infect_city(0, 3, 0, 48);
    plague_spread_begin_month(6);
    gather_all();
    plague_spread_prepare_commits();
    plague_spread_commit_step(10);
    plague_spread_stats(&stats);
    plague_probe_check(context, "spread", "generation_below_cap_reaches_cap",
        stats.committed_infections == 1 &&
        plague_state_get()->cities[1].active &&
        plague_state_get()->cities[1].generation == 4 &&
        plague_state_get()->episode.spores_remaining == 1,
        "source_generation=3 target_generation=%d small_max=4 committed=%d spores=%d",
        plague_state_get()->cities[1].generation, stats.committed_infections,
        plague_state_get()->episode.spores_remaining);
}

typedef struct {
    int target_active;
    int target_generation;
    int target_recovery_month;
    int spores_remaining;
    int candidate_edges;
    int pending_requests;
    int committed_infections;
} SeveritySpreadOutcome;

static void run_severity_spread_case(int severity, SeveritySpreadOutcome *out) {
    PlagueSpreadStats stats;
    const PlagueModelState *model;
    reset_topology(2);
    add_land_edge(0, 1);
    begin_small_episode_with_severity(2, severity);
    plague_state_infect_city(0, 0, 0, 48);
    plague_spread_begin_month(6);
    gather_all();
    plague_spread_prepare_commits();
    plague_spread_commit_step(10);
    plague_spread_stats(&stats);
    model = plague_state_get();
    memset(out, 0, sizeof(*out));
    out->target_active = model->cities[1].active;
    out->target_generation = model->cities[1].generation;
    out->target_recovery_month = model->cities[1].recovery_month;
    out->spores_remaining = model->episode.spores_remaining;
    out->candidate_edges = stats.candidate_edges;
    out->pending_requests = stats.pending_requests;
    out->committed_infections = stats.committed_infections;
}

static void check_severity_independence(PlagueProbeContext *context) {
    SeveritySpreadOutcome low;
    SeveritySpreadOutcome high;
    run_severity_spread_case(1, &low);
    run_severity_spread_case(4, &high);
    plague_probe_check(context, "spread", "severity_independent_spread_timing_generation",
        low.target_active && high.target_active &&
        low.target_generation == high.target_generation &&
        low.target_recovery_month == high.target_recovery_month &&
        low.spores_remaining == high.spores_remaining &&
        low.candidate_edges == high.candidate_edges &&
        low.pending_requests == high.pending_requests &&
        low.committed_infections == high.committed_infections,
        "severity=1/4 active=%d/%d generation=%d/%d recovery=%d/%d spores=%d/%d",
        low.target_active, high.target_active, low.target_generation,
        high.target_generation, low.target_recovery_month,
        high.target_recovery_month, low.spores_remaining, high.spores_remaining);
}

void plague_probe_run_spread(PlagueProbeContext *context) {
    check_cached_land_adjacency(context);
    check_pending_dedupe_commit(context);
    check_spore_budget_caps_commits(context);
    check_persistence_and_cap(context);
    check_generation_cap(context);
    check_severity_independence(context);
}
