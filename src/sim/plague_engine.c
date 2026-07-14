#include "sim/plague_engine.h"

#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "sim/plague_adjacency.h"
#include "sim/plague_disorder.h"
#include "sim/plague_diagnostics.h"
#include "sim/plague_episode.h"
#include "sim/plague_metrics.h"
#include "sim/plague_mortality.h"
#include "sim/plague_rules.h"
#include "sim/plague_spread.h"
#include "sim/plague_state.h"
#include "sim/population.h"
#include "sim/sea_lanes.h"
#include "sim/simulation.h"
#include "sim/world_announcement_plague.h"

#include <string.h>
#include <windows.h>

enum {
    PLAGUE_ENGINE_INIT = 0,
    PLAGUE_ENGINE_GATHER,
    PLAGUE_ENGINE_PREPARE,
    PLAGUE_ENGINE_COMMIT,
    PLAGUE_ENGINE_MORTALITY,
    PLAGUE_ENGINE_FINISH
};

static uint64_t counter_now(void) {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (uint64_t)counter.QuadPart;
}

static uint64_t elapsed_us(uint64_t start) {
    LARGE_INTEGER frequency;
    uint64_t now = counter_now();
    if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0 || now < start) return 0;
    return (now - start) * UINT64_C(1000000) / (uint64_t)frequency.QuadPart;
}

static void initialize_month(PlagueUpdateState *state) {
    PlagueEpisodeHistory ended;
    int was_active = plague_state_get()->episode.active;
    memset(&ended, 0, sizeof(ended));
    state->absolute_month = plague_rules_absolute_month(year, month);
    plague_state_prepare_death_month(state->absolute_month);
    sea_lanes_decay_exposure();
    if (plague_episode_rebuild_active_state(state->absolute_month, &ended)) {
        world_announcement_plague_emit_end(&plague_state_mutable()->episode, &ended);
    }
    if (!was_active) plague_episode_try_scheduled_start(state->absolute_month);
    if (plague_state_get()->episode.active) {
        world_announcement_plague_emit_start(&plague_state_mutable()->episode);
    }
    if (plague_state_get()->episode.active) {
        plague_spread_begin_month(state->absolute_month);
        state->phase = PLAGUE_ENGINE_GATHER;
    } else {
        plague_spread_reset();
        state->phase = PLAGUE_ENGINE_FINISH;
    }
}

static void record_finished_month(uint64_t month_elapsed_us) {
    const PlagueModelState *model = plague_state_get();
    PlagueSpreadStats spread;
    PlagueMetricsSnapshot sample;
    int route;
    memset(&sample, 0, sizeof(sample));
    plague_spread_stats(&spread);
    sample.active_city_count = model->active_city_count;
    sample.due_pulse_count = spread.due_pulses;
    sample.candidate_edge_count = spread.candidate_edges;
    for (route = 0; route < PLAGUE_ROUTE_COUNT; route++) {
        sample.candidate_edge_count_by_route[route] = spread.candidate_edges_by_route[route];
    }
    sample.pending_request_count = spread.pending_requests;
    sample.committed_infection_count = spread.committed_infections;
    sample.committed_persistence_count = spread.committed_persistence;
    sample.deduplicated_request_count = spread.deduplicated_requests;
    sample.adjacency_revision = plague_adjacency_revision();
    sample.adjacency_rebuild_count = plague_adjacency_rebuild_count();
    sample.adjacency_last_rebuild_us = plague_adjacency_last_rebuild_us();
    sample.spores_remaining = model->episode.active ? model->episode.spores_remaining : 0;
    sample.episode_id = model->episode.episode_id;
    plague_metrics_record_month(month_elapsed_us, &sample);
}

static void finish_month(PlagueUpdateState *state) {
    PlagueEpisodeHistory ended;
    int episode_ended;
    memset(&ended, 0, sizeof(ended));
    episode_ended = plague_episode_rebuild_active_state(state->absolute_month, &ended);
    if (episode_ended) {
        world_announcement_plague_emit_end(&plague_state_mutable()->episode, &ended);
    }
    if (state->any_deaths) {
        world_invalidate_population_cache();
    }
    plague_disorder_refresh_targets(state->absolute_month);
    plague_diagnostics_refresh(state->absolute_month);
    dirty_mark_plague();
}

int plague_engine_update_month_step(PlagueUpdateState *state, int batch_size) {
    const PlagueModelState *model;
    uint64_t call_start = counter_now();
    int processed = 0;
    if (!state) return 1;
    if (batch_size < 1) batch_size = 1;
    if (!state->initialized) {
        memset(state, 0, sizeof(*state));
        state->initialized = 1;
        initialize_month(state);
    }
    switch (state->phase) {
        case PLAGUE_ENGINE_GATHER:
            if (plague_spread_gather_step(batch_size)) state->phase = PLAGUE_ENGINE_PREPARE;
            break;
        case PLAGUE_ENGINE_PREPARE:
            plague_spread_prepare_commits();
            state->phase = PLAGUE_ENGINE_COMMIT;
            break;
        case PLAGUE_ENGINE_COMMIT:
            if (plague_spread_commit_step(batch_size)) {
                state->mortality_cursor = 0;
                state->phase = PLAGUE_ENGINE_MORTALITY;
            }
            break;
        case PLAGUE_ENGINE_MORTALITY:
            model = plague_state_get();
            while (state->mortality_cursor < model->active_city_count && processed < batch_size) {
                int city_id = model->active_city_ids[state->mortality_cursor++];
                if (plague_mortality_apply_city_month(city_id, state->absolute_month) > 0) {
                    state->any_deaths = 1;
                }
                processed++;
            }
            if (state->mortality_cursor >= model->active_city_count) state->phase = PLAGUE_ENGINE_FINISH;
            break;
        case PLAGUE_ENGINE_FINISH:
            finish_month(state);
            state->accumulated_us += elapsed_us(call_start);
            record_finished_month(state->accumulated_us);
            state->initialized = 0;
            return 1;
        default:
            state->phase = PLAGUE_ENGINE_FINISH;
            break;
    }
    state->accumulated_us += elapsed_us(call_start);
    return 0;
}
