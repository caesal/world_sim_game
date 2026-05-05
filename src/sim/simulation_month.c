#include "simulation_month.h"

#include "sim/diplomacy.h"
#include "sim/civilization_metrics.h"
#include "sim/expansion.h"
#include "sim/maritime.h"
#include "sim/plague.h"
#include "sim/population.h"
#include "sim/ports.h"
#include "sim/province.h"
#include "sim/simulation.h"
#include "sim/war.h"
#include "world/terrain_query.h"

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
    SIM_MONTH_DONE
};

#define RESOURCE_SCAN_ROWS_PER_STEP 24

static int step_owned_resource_scores(SimulationMonthState *state) {
    int x;
    int y;
    int end_y = min(state->resource_y + RESOURCE_SCAN_ROWS_PER_STEP, MAP_H);

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
    instability = civ->disorder + civ->disorder_plague / 2 + civ->disorder_migration / 2;
    score = social_capacity / 4 + hardship / 3 + resource_diversity / 2 +
            clamp(18 - resource_score, 0, 10) / 3 - instability / 2;
    return clamp(score, 0, 10);
}

static void update_civilization_pressures(const int resource_scores[MAX_CIVS]) {
    int i;

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
}

static void random_event(char *log, size_t log_size) {
    int id;
    Civilization *civ;

    if (civ_count == 0 || rnd(100) > 18) return;
    id = rnd(civ_count);
    civ = &civs[id];
    if (!civ->alive) return;
    switch (rnd(5)) {
        case 0: append_log(log, log_size, "Festival in %s. ", civ->name); break;
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

int simulation_month_begin(SimulationMonthState *state) {
    if (!world_generated || !state) return 0;
    memset(state, 0, sizeof(*state));
    state->active = 1;
    state->phase = SIM_MONTH_RESOURCES;
    population_sync_all();
    return 1;
}

int simulation_month_run_next(SimulationMonthState *state) {
    if (!state || !state->active) return 0;
    switch (state->phase) {
        case SIM_MONTH_RESOURCES:
            if (step_owned_resource_scores(state)) {
                finish_owned_resource_scores(state);
                state->phase = SIM_MONTH_CIVS;
            }
            break;
        case SIM_MONTH_CIVS:
            update_civilization_pressures(state->resource_scores);
            state->phase = SIM_MONTH_GROWTH_PORTS;
            break;
        case SIM_MONTH_GROWTH_PORTS:
            population_update_month();
            province_invalidate_region_cache();
            ports_update_migration();
            world_invalidate_population_cache();
            state->city_count_before_expansion = city_count;
            state->territory_revision_before_expansion = world_visual_revision;
            state->expansion_civ = 0;
            state->phase = SIM_MONTH_EXPANSION;
            break;
        case SIM_MONTH_EXPANSION:
            while (state->expansion_civ < civ_count) {
                int civ_id = state->expansion_civ++;
                if (civs[civ_id].alive) {
                    expansion_update_civilization(civ_id, state->resource_scores[civ_id],
                                                  state->log, sizeof(state->log));
                    return 1;
                }
            }
            if (world_visual_revision != state->territory_revision_before_expansion ||
                city_count != state->city_count_before_expansion) {
                ports_refresh_city_regions();
                maritime_mark_routes_dirty();
                maritime_ensure_routes();
                diplomacy_mark_contacts_dirty();
            }
            state->phase = SIM_MONTH_PLAGUE;
            break;
        case SIM_MONTH_PLAGUE:
            plague_update_month();
            state->phase = SIM_MONTH_RANDOM_EVENT;
            break;
        case SIM_MONTH_RANDOM_EVENT:
            random_event(state->log, sizeof(state->log));
            state->phase = SIM_MONTH_TERRITORY;
            break;
        case SIM_MONTH_TERRITORY:
            if (world_visual_revision != state->territory_revision_before_expansion ||
                city_count != state->city_count_before_expansion) {
                world_recalculate_territory();
            }
            state->phase = SIM_MONTH_DIPLOMACY;
            break;
        case SIM_MONTH_DIPLOMACY:
            diplomacy_update_contacts();
            state->phase = SIM_MONTH_CALENDAR;
            break;
        case SIM_MONTH_CALENDAR:
            month++;
            if (month > 12) {
                month = 1;
                year = clamp(year + 1, 0, 99999);
                diplomacy_update_year();
                war_update_year();
            }
            if (living_civilizations() <= 1) auto_run = 0;
            state->phase = SIM_MONTH_DONE;
            state->active = 0;
            break;
        default:
            state->active = 0;
            break;
    }
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
