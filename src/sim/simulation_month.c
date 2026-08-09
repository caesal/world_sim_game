#include "simulation_month.h"

#include "sim/alliance.h"
#include "sim/civ_colors.h"
#include "sim/diplomacy.h"
#include "sim/civilization_metrics.h"
#include "sim/collapse.h"
#include "sim/decision_snapshot_cache.h"
#include "sim/disorder.h"
#include "sim/economy.h"
#include "sim/expansion.h"
#include "sim/maritime.h"
#include "sim/plague.h"
#include "sim/population.h"
#include "sim/ports.h"
#include "sim/stability_decision.h"
#include "sim/province.h"
#include "sim/simulation.h"
#include "sim/technology.h"
#include "sim/territory_integrity.h"
#include "sim/vassal.h"
#include "sim/war.h"
#include "sim/world_announcement.h"
#include "world/terrain_query.h"
#include "core/dirty_flags.h"
#include "core/plague_perf.h"
#include "core/profiler.h"
#include "core/render_snapshot_cache.h"

#include <string.h>

enum {
    SIM_MONTH_RESOURCES,
    SIM_MONTH_CIVS,
    SIM_MONTH_GROWTH_PORTS,
    SIM_MONTH_EXPANSION,
    SIM_MONTH_PLAGUE,
    SIM_MONTH_RANDOM_EVENT,
    SIM_MONTH_TERRITORY,
    SIM_MONTH_DIPLOMACY,
    SIM_MONTH_CALENDAR,
    SIM_MONTH_SNAPSHOT_CACHE,
    SIM_MONTH_DECISION_CACHE,
    SIM_MONTH_DONE
};

#define RESOURCE_SCAN_ROWS_PER_STEP 24
#define CIV_PRESSURES_PER_STEP 8
#define EXPANSION_CIV_CHECKS_PER_STEP 1
#define POPULATION_CITY_STEP 16
#define PLAGUE_CITY_STEP 16

static int cached_resource_scores[MAX_CIVS];
static int resource_scores_ready = 0;

static int simulation_month_index(void) {
    return year * 12 + month - 1;
}

static int resource_pressure_due_this_month(void) {
    return !resource_scores_ready || simulation_month_index() % 3 == 0;
}

static const char *simulation_phase_name(int phase) {
    switch (phase) {
        case SIM_MONTH_RESOURCES: return "Resources";
        case SIM_MONTH_CIVS: return "Civ Pressures";
        case SIM_MONTH_GROWTH_PORTS: return "Growth/Ports";
        case SIM_MONTH_EXPANSION: return "Expansion";
        case SIM_MONTH_PLAGUE: return "Plague";
        case SIM_MONTH_RANDOM_EVENT: return "Random Event";
        case SIM_MONTH_TERRITORY: return "Territory";
        case SIM_MONTH_DIPLOMACY: return "Diplomacy";
        case SIM_MONTH_CALENDAR: return "Calendar";
        case SIM_MONTH_SNAPSHOT_CACHE: return "Snapshot Cache";
        case SIM_MONTH_DECISION_CACHE: return "Decision Cache";
        default: return "Month";
    }
}

static int step_owned_resource_scores(SimulationMonthState *state) {
    int x;
    int y;
    int start_y = state->resource_y;
    int end_y = min(state->resource_y + RESOURCE_SCAN_ROWS_PER_STEP, MAP_H);

    profiler_add_scanned_tiles((end_y - start_y) * MAP_W);
    for (y = state->resource_y; y < end_y; y++) {
        for (x = 0; x < MAP_W; x++) {
            int owner = world[y][x].owner;
            if (owner >= 0 && owner < civ_count && civs[owner].alive) {
                TerrainStats stats = tile_stats(x, y);
                state->resource_totals[owner] += stats.food + stats.livestock + stats.wood + stats.stone +
                                                 stats.minerals + stats.water + stats.pop_capacity + stats.money +
                                                 stats.habitability;
                state->resource_counts[owner]++;
            }
        }
    }
    state->resource_y = end_y;
    return state->resource_y >= MAP_H;
}

static void finish_owned_resource_scores(SimulationMonthState *state) {
    int i;

    for (i = 0; i < civ_count; i++) {
        state->resource_scores[i] = state->resource_counts[i] > 0 ?
                                    state->resource_totals[i] / (state->resource_counts[i] * 2) : 0;
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
    hardship = clamp(10 - summary.habitability, 0, 10) + clamp(5 - summary.food, 0, 5) +
               clamp(5 - summary.water, 0, 5);
    social_capacity = civ->culture + civ->cohesion + civ->logistics + civ->innovation;
    instability = civ->disorder / 10 +
                  (plague_perf_system_enabled() ? civ->disorder_plague / 20 : 0) +
                  civ->disorder_migration / 20;
    score = social_capacity / 4 + hardship / 3 + resource_diversity / 2 +
            clamp(18 - resource_score, 0, 10) / 3 - instability / 2;
    return clamp(score, 0, 10);
}

static int step_civilization_pressures(SimulationMonthState *state) {
    int processed = 0;

    while (state->civ_pressure_cursor < civ_count && processed < CIV_PRESSURES_PER_STEP) {
        int i = state->civ_pressure_cursor++;
        Civilization *civ;
        int resources;

        processed++;
        if (!civs[i].alive) continue;
        civ = &civs[i];
        resources = state->resource_scores[i];
        disorder_update_month(i, resources);
        stability_decision_update_month(i);
        civ->adaptation = compute_dynamic_adaptation(i, resources);
    }
    return state->civ_pressure_cursor >= civ_count;
}

static void random_event(char *log, size_t log_size) {
    int alive_ids[MAX_CIVS];
    int alive_count = 0;
    int id;
    int i;
    Civilization *civ;

    for (i = 0; i < civ_count; i++) {
        if (civs[i].alive) alive_ids[alive_count++] = i;
    }
    if (alive_count == 0 || rnd(100) > 18) return;
    id = alive_ids[rnd(alive_count)];
    civ = &civs[id];
    switch (rnd(4)) {
        case 0: append_log(log, log_size, "Festival in %s. ",
                           civilization_display_name_for_language(id, 0));
            disorder_relieve(id, 12);
            break;
        case 1:
            civ->defense = clamp(civ->defense + 1, 0, 10);
            disorder_relieve(id, 5);
            append_log(log, log_size, "%s fortified borders. ",
                       civilization_display_name_for_language(id, 0));
            break;
        case 2:
            civ->aggression = clamp(civ->aggression + 1, 0, 10);
            append_log(log, log_size, "%s became ambitious. ",
                       civilization_display_name_for_language(id, 0));
            break;
        default:
            civ->expansion = clamp(civ->expansion + 1, 0, 10);
            disorder_add_migration_pressure(id, 8);
            append_log(log, log_size, "%s started migrating. ",
                       civilization_display_name_for_language(id, 0));
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

int simulation_month_should_stop_auto_run_for_living(int living) {
    return living <= 0;
}

int simulation_month_begin(SimulationMonthState *state) {
    ProfilerCallTrace trace;

    if (!world_generated || !state) return 0;
    memset(state, 0, sizeof(*state));
    profiler_begin_month();
    plague_perf_note_sim_skipped(0);
    decision_snapshot_cache_cancel_building();
    state->active = 1;
    state->phase = SIM_MONTH_RESOURCES;
    state->run_quarterly = resource_pressure_due_this_month();
    if (!state->run_quarterly) {
        memcpy(state->resource_scores, cached_resource_scores, sizeof(state->resource_scores));
    }
    trace = profiler_call_begin();
    population_sync_all();
    profiler_call_end("population_sync_all", -1, -1, trace);
    return 1;
}

int simulation_month_run_next(SimulationMonthState *state) {
    DWORD phase_start;
    int phase_before;
    const char *phase_profile_name;
    ProfilerCallTrace phase_trace;

    if (!state || !state->active) return 0;
    phase_before = state->phase;
    phase_profile_name = simulation_phase_name(phase_before);
    phase_start = GetTickCount();
    phase_trace = profiler_call_begin();
    profiler_set_current_job(phase_profile_name);
    switch (state->phase) {
        case SIM_MONTH_RESOURCES:
            if (!state->run_quarterly) {
                state->phase = SIM_MONTH_CIVS;
            } else if (step_owned_resource_scores(state)) {
                finish_owned_resource_scores(state);
                memcpy(cached_resource_scores, state->resource_scores, sizeof(cached_resource_scores));
                resource_scores_ready = 1;
                state->phase = SIM_MONTH_CIVS;
            }
            break;
        case SIM_MONTH_CIVS:
            if (state->run_quarterly && !step_civilization_pressures(state)) break;
            {
                ProfilerCallTrace trace = profiler_call_begin();
            technology_update_month();
                profiler_call_end("technology_update_month", -1, -1, trace);
            }
            state->phase = SIM_MONTH_GROWTH_PORTS;
            break;
        case SIM_MONTH_GROWTH_PORTS:
            {
                ProfilerCallTrace trace = profiler_call_begin();
                int done = population_update_month_step(&state->population_cursor, POPULATION_CITY_STEP);
                profiler_call_end("population_update_month_step", -1, -1, trace);
                if (!done) break;
            }
            province_invalidate_region_cache();
            {
                ProfilerCallTrace trace = profiler_call_begin();
            ports_update_migration();
                profiler_call_end("ports_update_migration", -1, -1, trace);
            }
            world_invalidate_population_cache();
            state->city_count_before_expansion = city_count;
            state->territory_revision_before_expansion = dirty_revision_ownership();
            state->expansion_civ = 0;
            state->phase = SIM_MONTH_EXPANSION;
            break;
        case SIM_MONTH_EXPANSION:
            {
            int checked = 0;
            while (state->expansion_civ < civ_count && checked < EXPANSION_CIV_CHECKS_PER_STEP) {
                int civ_id = state->expansion_civ;
                checked++;
                profiler_add_expansion_civs_checked(1);

                if (!civs[civ_id].alive || expansion_civ_months_until_claim(civ_id) > 0) {
                    state->expansion_civ++;
                    continue;
                }
                if (expansion_work_done(&state->expansion_work)) {
                    ProfilerCallTrace trace = profiler_call_begin();
                    expansion_work_begin(&state->expansion_work, civ_id, state->resource_scores[civ_id]);
                    profiler_call_end("expansion_work_begin", civ_id, -1, trace);
                }
                {
                    ProfilerCallTrace trace = profiler_call_begin();
                expansion_work_step(&state->expansion_work, state->log, sizeof(state->log));
                    profiler_call_end("expansion_work_step", civ_id, -1, trace);
                }
                if (!expansion_work_done(&state->expansion_work)) {
                    profiler_record_phase(phase_profile_name, (int)(GetTickCount() - phase_start));
                    profiler_call_end(phase_profile_name, -1, -1, phase_trace);
                    return 1;
                }
                state->expansion_civ++;
                profiler_record_phase(phase_profile_name, (int)(GetTickCount() - phase_start));
                profiler_call_end(phase_profile_name, -1, -1, phase_trace);
                return 1;
            }
            if (state->expansion_civ < civ_count) {
                profiler_record_phase(phase_profile_name, (int)(GetTickCount() - phase_start));
                profiler_call_end(phase_profile_name, -1, -1, phase_trace);
                return 1;
            }
            if (dirty_revision_ownership() != state->territory_revision_before_expansion ||
                city_count != state->city_count_before_expansion) {
                ports_refresh_city_regions();
                maritime_mark_routes_dirty();
                diplomacy_mark_contacts_dirty();
                civilization_repair_queued_color_conflicts(4);
            }
            state->phase = SIM_MONTH_PLAGUE;
            }
            break;
        case SIM_MONTH_PLAGUE:
            if (!plague_perf_system_enabled()) {
                plague_perf_note_sim_skipped(1);
                state->phase = SIM_MONTH_RANDOM_EVENT;
                break;
            }
            {
                ProfilerCallTrace trace = profiler_call_begin();
                int done = plague_update_month_step(&state->plague_work, PLAGUE_CITY_STEP);
                profiler_call_end("plague_update_month_step", -1, -1, trace);
                if (done) {
                state->phase = SIM_MONTH_RANDOM_EVENT;
                }
            }
            break;
        case SIM_MONTH_RANDOM_EVENT:
            random_event(state->log, sizeof(state->log));
            state->phase = SIM_MONTH_TERRITORY;
            break;
        case SIM_MONTH_TERRITORY:
            if (dirty_revision_ownership() != state->territory_revision_before_expansion ||
                city_count != state->city_count_before_expansion) {
                world_invalidate_population_cache();
                territory_integrity_repair_capitals();
            }
            state->phase = SIM_MONTH_DIPLOMACY;
            break;
        case SIM_MONTH_DIPLOMACY:
            {
                ProfilerCallTrace trace = profiler_call_begin();
            diplomacy_update_contacts();
                profiler_call_end("diplomacy_update_contacts", -1, -1, trace);
            }
            state->phase = SIM_MONTH_CALENDAR;
            break;
        case SIM_MONTH_CALENDAR:
            if (!state->calendar_started) {
                month++;
                if (month > 12) {
                    month = 1;
                    year = clamp(year + 1, 0, 99999);
                    state->calendar_year_rollover = 1;
                }
                state->calendar_started = 1;
                break;
            }
            if (state->calendar_year_rollover && state->calendar_step < 6) {
                switch (state->calendar_step) {
                    case 0:
                        phase_profile_name = "Economy Year";
                        profiler_set_current_job("economy-calendar");
                        if (!economy_update_year_step(&state->economy_year_cursor, 4)) break;
                        state->calendar_step++;
                        break;
                    case 1:
                        if (!state->diplomacy_alliance_year_started) {
                            diplomacy_year_work_begin(&state->diplomacy_year_work);
                            alliance_year_work_begin(&state->alliance_year_work);
                            state->diplomacy_alliance_year_started = 1;
                        }
                        phase_profile_name = "Diplomacy Year";
                        profiler_set_current_job("diplomacy-year");
                        if (!diplomacy_update_year_step(&state->diplomacy_year_work, 12)) break;
                        phase_profile_name = "Alliance Year";
                        profiler_set_current_job("alliance-year");
                        if (!alliance_update_year_step(&state->alliance_year_work, 96)) break;
                        state->calendar_step++;
                        break;
                    case 2: phase_profile_name = "Vassal Year"; profiler_set_current_job("vassal-calendar"); vassal_update_year(); state->calendar_step++; break;
                    case 3:
                        phase_profile_name = "War Year";
                        profiler_set_current_job("war-calendar");
                        if (!war_update_year_step(&state->war_year_cursor, 1,
                                                  &state->war_tail_cut_done,
                                                  &state->war_year_changed)) break;
                        state->calendar_step++;
                        break;
                    case 4: phase_profile_name = "Territory Year"; profiler_set_current_job("territory-integrity"); territory_integrity_update_year(); state->calendar_step++; break;
                    case 5:
                        phase_profile_name = year % 25 == 0 ? "Collapse Check" : "Calendar Year";
                        if (year % 25 == 0) { profiler_set_current_job("collapse-check"); collapse_update_decade(); }
                        state->calendar_step++;
                        break;
                }
                break;
            }
            economy_update_month_all();
            collapse_update_immediate();
            if (state->log[0]) {
                event_log_push_structured(EVENT_TYPE_DEBUG_NOTICE, EVENT_SEVERITY_INFO,
                                          -1, -1, -1, -1, 0, 0, state->log);
            }
            if (simulation_month_should_stop_auto_run_for_living(living_civilizations())) auto_run = 0;
            state->phase = SIM_MONTH_SNAPSHOT_CACHE;
            break;
        case SIM_MONTH_SNAPSHOT_CACHE:
            render_snapshot_cache_update_budgeted(16, 256, 1, 1);
            decision_snapshot_cache_begin_generation();
            state->phase = SIM_MONTH_DECISION_CACHE;
            break;
        case SIM_MONTH_DECISION_CACHE:
            if (!decision_snapshot_cache_service_slice()) break;
            state->phase = SIM_MONTH_DONE;
            state->active = 0;
            break;
        default:
            state->active = 0;
            break;
    }
    profiler_record_phase(phase_profile_name, (int)(GetTickCount() - phase_start));
    profiler_call_end(phase_profile_name, -1, -1, phase_trace);
    return 1;
}

int simulation_month_is_done(const SimulationMonthState *state) {
    return !state || !state->active || state->phase >= SIM_MONTH_DONE;
}

void simulation_month_run_blocking(void) {
    SimulationMonthState state;

    if (!simulation_month_begin(&state)) return;
    while (!simulation_month_is_done(&state)) simulation_month_run_next(&state);
}

void simulate_one_month(void) {
    simulation_month_run_blocking();
}
