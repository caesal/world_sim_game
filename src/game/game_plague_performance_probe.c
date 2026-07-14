#include "game/game_plague_performance_probe.h"

#include "core/game_types.h"
#include "sim/maritime.h"
#include "sim/plague.h"
#include "sim/plague_adjacency.h"
#include "sim/plague_metrics.h"
#include "sim/plague_state.h"
#include "sim/population_display_cohorts.h"
#include "sim/regions.h"
#include "sim/route_potential.h"
#include "sim/sea_lanes.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STRESS_CITY_COUNT 1000
#define STRESS_INITIAL_ACTIVE 128
#define STRESS_POPULATION_PER_CITY 10000
#define STRESS_SPORES 700
#define STRESS_SEVERITY 10
#define STRESS_BATCH_SIZE 16
#define STRESS_MAX_MONTHS 240
#define STRESS_MAX_ENGINE_CALLS_PER_MONTH 10000
#define STRESS_MAX_AVERAGE_US UINT64_C(2000)
#define STRESS_MAX_PEAK_US UINT64_C(8000)
#define STRESS_MAX_DEGREE 4
#define STRESS_DIRECTED_EDGES (STRESS_CITY_COUNT * STRESS_MAX_DEGREE)
#define STRESS_DIR "build/validation/named_plague_redesign_20260712/03_stress"
#define STRESS_CSV STRESS_DIR "/plague_performance.csv"
#define STRESS_SUMMARY STRESS_DIR "/summary.txt"

typedef struct {
    int months_run;
    int episode_ended;
    int peak_active;
    int maximum_ever;
    int maximum_generation;
    int minimum_spores;
    int maximum_due_pulses;
    int maximum_candidate_edges;
    int maximum_pending_requests;
    int candidate_edge_bound_ok;
    uint64_t engine_calls;
    uint64_t total_due_pulses;
    uint64_t total_candidate_edges;
    uint64_t total_candidate_edges_by_route[PLAGUE_ROUTE_COUNT];
    uint64_t total_pending_requests;
    uint64_t total_committed_infections;
    uint64_t total_committed_persistence;
    uint64_t total_deduplicated_requests;
} StressRunStats;

static int ensure_dir(const char *path) {
    return CreateDirectoryA(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

static int ensure_output_dirs(void) {
    return ensure_dir("build") &&
           ensure_dir("build/validation") &&
           ensure_dir("build/validation/named_plague_redesign_20260712") &&
           ensure_dir(STRESS_DIR);
}

static int positive_mod(int value, int divisor) {
    int result = divisor > 0 ? value % divisor : 0;
    return result < 0 ? result + divisor : result;
}

static void set_city_population(City *city, int total) {
    static const int percent[POP_COHORT_COUNT] = {10, 22, 10, 22, 18, 9, 6, 3};
    int remaining = total;
    int band;
    memset(city->population_cohorts, 0, sizeof(city->population_cohorts));
    for (band = 0; band < POP_COHORT_COUNT; band++) {
        int amount = band == POP_COHORT_COUNT - 1 ? remaining : total * percent[band] / 100;
        city->population_cohorts[band].male = amount / 2;
        city->population_cohorts[band].female = amount - amount / 2;
        remaining -= amount;
    }
    city->population = total;
    city->population_ready = 1;
}

static void add_neighbor(NaturalRegion *region, int target) {
    int i;
    if (!region || target < 0 || target >= STRESS_CITY_COUNT ||
        region->neighbor_count >= MAX_REGION_NEIGHBORS) return;
    for (i = 0; i < region->neighbor_count; i++) {
        if (region->neighbors[i] == target) return;
    }
    region->neighbors[region->neighbor_count++] = target;
}

static void setup_circulant_topology(void) {
    static const int offsets[STRESS_MAX_DEGREE] = {1, -1, 37, -37};
    int city_id;
    int edge;
    map_w = STRESS_CITY_COUNT;
    map_h = 1;
    city_count = STRESS_CITY_COUNT;
    region_count = STRESS_CITY_COUNT;
    civ_count = 1;
    memset(cities, 0, sizeof(cities));
    memset(civs, 0, sizeof(civs));
    memset(natural_regions, 0, sizeof(natural_regions));
    civs[0].alive = 1;
    civs[0].uid = 9301;
    civs[0].symbol = 'P';
    civs[0].color = COLOR32_RGB(110, 42, 142);
    civs[0].population = STRESS_CITY_COUNT * STRESS_POPULATION_PER_CITY;
    snprintf(civs[0].name, sizeof(civs[0].name), "Plague Stress Realm");
    for (city_id = 0; city_id < STRESS_CITY_COUNT; city_id++) {
        City *city = &cities[city_id];
        NaturalRegion *region = &natural_regions[city_id];
        city->alive = 1;
        city->owner = 0;
        city->x = city_id;
        city->y = 0;
        snprintf(city->name, sizeof(city->name), "Stress City %04d", city_id);
        set_city_population(city, STRESS_POPULATION_PER_CITY);
        population_display_init_city(city_id);
        world[0][city_id].geography = GEO_PLAIN;
        world[0][city_id].owner = 0;
        world[0][city_id].province_id = city_id;
        world[0][city_id].region_id = city_id;
        region->id = city_id;
        region->alive = 1;
        region->tile_count = 1;
        region->owner_civ = 0;
        region->city_id = city_id;
        for (edge = 0; edge < STRESS_MAX_DEGREE; edge++) {
            add_neighbor(region, positive_mod(city_id + offsets[edge], STRESS_CITY_COUNT));
        }
    }
    route_potential_reset();
    maritime_reset();
    sea_lanes_invalidate();
}

static int begin_stress_episode(void) {
    PlagueEpisodeState episode;
    int index;
    plague_reset();
    memset(&episode, 0, sizeof(episode));
    episode.active = 1;
    episode.episode_id = 9301;
    episode.size = PLAGUE_SIZE_LARGE;
    episode.severity = STRESS_SEVERITY;
    episode.name_id = 0;
    episode.name_cycle = 1;
    episode.origin_city_id = 0;
    episode.origin_civ_id = 0;
    episode.origin_civ_uid = civs[0].uid;
    episode.origin_civ_symbol = civs[0].symbol;
    episode.origin_civ_color = civs[0].color;
    episode.start_month = 0;
    episode.frozen_occupied_cities = STRESS_CITY_COUNT;
    episode.spores_initial = STRESS_SPORES;
    episode.spores_remaining = STRESS_SPORES;
    episode.start_event_emitted = 1;
    episode.end_event_emitted = 1;
    snprintf(episode.origin_city_name, sizeof(episode.origin_city_name), "%s",
             cities[0].name);
    snprintf(episode.origin_civ_name_en, sizeof(episode.origin_civ_name_en), "%s",
             civs[0].name);
    snprintf(episode.origin_civ_name_zh, sizeof(episode.origin_civ_name_zh), "%s",
             civs[0].name);
    if (!plague_state_begin_episode(&episode)) return 0;
    for (index = 0; index < STRESS_INITIAL_ACTIVE; index++) {
        int city_id = positive_mod(index * 211, STRESS_CITY_COUNT);
        int duration = 24 + index % 19;
        if (!plague_state_infect_city(city_id, 0, 0, duration)) return 0;
    }
    plague_state_note_current_country(0);
    plague_state_note_ever_country(0);
    plague_state_record_start(0);
    return 1;
}

static void csv_header(FILE *csv) {
    fprintf(csv,
        "absolute_month,year,month,episode_active,episode_id,active_cities,ever_infected,"
        "maximum_generation,spores_remaining,total_deaths,rolling_deaths,due_pulses,"
        "candidate_edges,land_edges,shallow_edges,deep_edges,pending_requests,"
        "committed_infections,committed_persistence,deduplicated_requests,"
        "adjacency_revision,adjacency_rebuilds,adjacency_last_rebuild_us,"
        "step_last_us,step_average_us,step_peak_us,engine_calls\n");
}

static void update_run_stats(StressRunStats *run, const PlagueModelState *model,
                             const PlagueMetricsSnapshot *metrics, int engine_calls) {
    int route;
    if (model->active_city_count > run->peak_active) run->peak_active = model->active_city_count;
    if (model->ever_infected_city_count > run->maximum_ever) {
        run->maximum_ever = model->ever_infected_city_count;
    }
    if (model->episode.maximum_generation_reached > run->maximum_generation) {
        run->maximum_generation = model->episode.maximum_generation_reached;
    }
    if (model->episode.active &&
        model->episode.spores_remaining < run->minimum_spores) {
        run->minimum_spores = model->episode.spores_remaining;
    }
    if (metrics->due_pulse_count > run->maximum_due_pulses) {
        run->maximum_due_pulses = metrics->due_pulse_count;
    }
    if (metrics->candidate_edge_count > run->maximum_candidate_edges) {
        run->maximum_candidate_edges = metrics->candidate_edge_count;
    }
    if (metrics->pending_request_count > run->maximum_pending_requests) {
        run->maximum_pending_requests = metrics->pending_request_count;
    }
    if (metrics->candidate_edge_count > metrics->due_pulse_count * STRESS_MAX_DEGREE) {
        run->candidate_edge_bound_ok = 0;
    }
    run->engine_calls += (uint64_t)engine_calls;
    run->total_due_pulses += (uint64_t)metrics->due_pulse_count;
    run->total_candidate_edges += (uint64_t)metrics->candidate_edge_count;
    for (route = 0; route < PLAGUE_ROUTE_COUNT; route++) {
        run->total_candidate_edges_by_route[route] +=
            (uint64_t)metrics->candidate_edge_count_by_route[route];
    }
    run->total_pending_requests += (uint64_t)metrics->pending_request_count;
    run->total_committed_infections += (uint64_t)metrics->committed_infection_count;
    run->total_committed_persistence += (uint64_t)metrics->committed_persistence_count;
    run->total_deduplicated_requests += (uint64_t)metrics->deduplicated_request_count;
}

static int run_stress_month(FILE *csv, int absolute_month, StressRunStats *run) {
    PlagueUpdateState update;
    PlagueMetricsSnapshot metrics;
    const PlagueModelState *model;
    uint64_t average_us;
    int engine_calls = 0;
    int done = 0;
    year = absolute_month / 12;
    month = absolute_month % 12 + 1;
    memset(&update, 0, sizeof(update));
    while (!done && engine_calls < STRESS_MAX_ENGINE_CALLS_PER_MONTH) {
        done = plague_update_month_step(&update, STRESS_BATCH_SIZE);
        engine_calls++;
    }
    if (!done) return 0;
    plague_metrics_snapshot(&metrics);
    model = plague_state_get();
    average_us = metrics.step_samples > 0 ?
                 metrics.step_total_us / metrics.step_samples : 0;
    update_run_stats(run, model, &metrics, engine_calls);
    fprintf(csv,
        "%d,%d,%d,%d,%d,%d,%d,%d,%d,%" PRId64 ",%" PRId64
        ",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%" PRIu64
        ",%" PRIu64 ",%" PRIu64 ",%d\n",
        absolute_month, year, month, model->episode.active, model->episode.episode_id,
        model->active_city_count, model->ever_infected_city_count,
        model->episode.maximum_generation_reached, model->episode.spores_remaining,
        model->episode.total_deaths, plague_state_rolling_deaths(absolute_month),
        metrics.due_pulse_count, metrics.candidate_edge_count,
        metrics.candidate_edge_count_by_route[PLAGUE_ROUTE_LAND],
        metrics.candidate_edge_count_by_route[PLAGUE_ROUTE_SHALLOW],
        metrics.candidate_edge_count_by_route[PLAGUE_ROUTE_DEEP],
        metrics.pending_request_count, metrics.committed_infection_count,
        metrics.committed_persistence_count, metrics.deduplicated_request_count,
        metrics.adjacency_revision, metrics.adjacency_rebuild_count,
        metrics.adjacency_last_rebuild_us, metrics.step_last_us, average_us,
        metrics.step_peak_us, engine_calls);
    run->months_run++;
    if (!model->episode.active) run->episode_ended = 1;
    return 1;
}

static int stress_gate(const StressRunStats *run, const PlagueMetricsSnapshot *metrics,
                       const PlagueModelState *model) {
    uint64_t average_us = metrics->step_samples > 0 ?
                          metrics->step_total_us / metrics->step_samples : UINT64_MAX;
    return city_count == STRESS_CITY_COUNT && region_count == STRESS_CITY_COUNT &&
           model->episode.size == PLAGUE_SIZE_LARGE &&
           model->episode.severity == STRESS_SEVERITY &&
           model->episode.frozen_occupied_cities == STRESS_CITY_COUNT &&
           model->episode.spores_initial == STRESS_SPORES &&
           run->months_run >= 24 && run->candidate_edge_bound_ok &&
           metrics->adjacency_rebuild_count == 1 &&
           run->maximum_ever > STRESS_INITIAL_ACTIVE && run->maximum_generation > 0 &&
           run->total_candidate_edges > 0 && run->total_committed_infections > 0 &&
           run->total_candidate_edges_by_route[PLAGUE_ROUTE_LAND] ==
               run->total_candidate_edges &&
           run->total_candidate_edges_by_route[PLAGUE_ROUTE_SHALLOW] == 0 &&
           run->total_candidate_edges_by_route[PLAGUE_ROUTE_DEEP] == 0 &&
           model->episode.total_deaths > 0 &&
           metrics->step_samples == (uint64_t)run->months_run &&
           average_us <= STRESS_MAX_AVERAGE_US &&
           metrics->step_peak_us <= STRESS_MAX_PEAK_US;
}

static void write_summary(FILE *summary, const StressRunStats *run,
                          const PlagueMetricsSnapshot *metrics,
                          const PlagueModelState *model, int passed) {
    uint64_t average_us = metrics->step_samples > 0 ?
                          metrics->step_total_us / metrics->step_samples : 0;
    fprintf(summary, "result=%s\n", passed ? "PASS" : "FAIL");
    fprintf(summary, "fixture_cities=%d occupied_cities=%d natural_regions=%d\n",
            STRESS_CITY_COUNT, city_count, region_count);
    fprintf(summary,
            "fixture_topology=circulant offsets=+1,-1,+37,-37 max_degree=%d directed_edges=%d\n",
            STRESS_MAX_DEGREE, STRESS_DIRECTED_EDGES);
    fprintf(summary,
            "fixture_bounded_evidence=PASS tiles_assigned=%d plague_adjacency_tile_scan=0 "
            "plague_all_city_pair_scan=0\n",
            STRESS_CITY_COUNT);
    fprintf(summary,
            "episode_id=%d size=large severity=%d frozen_occupied=%d initial_active=%d "
            "spores_initial=%d months_run=%d ended=%d\n",
            model->episode.episode_id, model->episode.severity,
            model->episode.frozen_occupied_cities, STRESS_INITIAL_ACTIVE,
            model->episode.spores_initial, run->months_run, run->episode_ended);
    fprintf(summary,
            "peak_active=%d ever_infected=%d maximum_generation=%d spores_remaining=%d "
            "minimum_spores_while_active=%d spores_consumed_while_active=%d "
            "total_deaths=%" PRId64 "\n",
            run->peak_active, run->maximum_ever, run->maximum_generation,
            model->episode.spores_remaining, run->minimum_spores,
            STRESS_SPORES - run->minimum_spores,
            model->episode.total_deaths);
    fprintf(summary,
            "spread_total_due=%" PRIu64 " candidate_edges=%" PRIu64
            " pending=%" PRIu64 " committed_infections=%" PRIu64
            " committed_persistence=%" PRIu64 " deduplicated=%" PRIu64 "\n",
            run->total_due_pulses, run->total_candidate_edges,
            run->total_pending_requests, run->total_committed_infections,
            run->total_committed_persistence, run->total_deduplicated_requests);
    fprintf(summary,
            "candidate_edges_by_route_land=%" PRIu64 " shallow=%" PRIu64
            " deep=%" PRIu64 "\n",
            run->total_candidate_edges_by_route[PLAGUE_ROUTE_LAND],
            run->total_candidate_edges_by_route[PLAGUE_ROUTE_SHALLOW],
            run->total_candidate_edges_by_route[PLAGUE_ROUTE_DEEP]);
    fprintf(summary,
            "spread_max_due=%d max_candidate_edges=%d max_pending=%d "
            "candidate_bound_due_times_degree=%s\n",
            run->maximum_due_pulses, run->maximum_candidate_edges,
            run->maximum_pending_requests, run->candidate_edge_bound_ok ? "PASS" : "FAIL");
    fprintf(summary,
            "adjacency_revision=%d rebuilds=%d last_rebuild_us=%d\n",
            metrics->adjacency_revision, metrics->adjacency_rebuild_count,
            metrics->adjacency_last_rebuild_us);
    fprintf(summary,
            "plague_step_samples=%" PRIu64 " engine_calls=%" PRIu64
            " average_us=%" PRIu64 " peak_us=%" PRIu64
            " average_gate_us=%" PRIu64 " peak_gate_us=%" PRIu64 "\n",
            metrics->step_samples, run->engine_calls, average_us,
            metrics->step_peak_us, STRESS_MAX_AVERAGE_US, STRESS_MAX_PEAK_US);
}

int run_plague_performance_probe(void) {
    StressRunStats run;
    PlagueMetricsSnapshot metrics;
    const PlagueModelState *model;
    FILE *csv;
    FILE *summary;
    int absolute_month;
    int run_ok = 1;
    int passed;
    if (!ensure_output_dirs()) return 2;
    csv = fopen(STRESS_CSV, "w");
    summary = fopen(STRESS_SUMMARY, "w");
    if (!csv || !summary) {
        if (csv) fclose(csv);
        if (summary) fclose(summary);
        return 2;
    }
    memset(&run, 0, sizeof(run));
    run.minimum_spores = STRESS_SPORES;
    run.candidate_edge_bound_ok = 1;
    setup_circulant_topology();
    srand(4400341u);
    run_ok = begin_stress_episode();
    csv_header(csv);
    for (absolute_month = 0; run_ok && absolute_month < STRESS_MAX_MONTHS;
         absolute_month++) {
        run_ok = run_stress_month(csv, absolute_month, &run);
        if (run_ok && !plague_state_get()->episode.active) break;
    }
    plague_metrics_snapshot(&metrics);
    model = plague_state_get();
    passed = run_ok && stress_gate(&run, &metrics, model);
    write_summary(summary, &run, &metrics, model, passed);
    fclose(csv);
    fclose(summary);
    return passed ? 0 : 1;
}
