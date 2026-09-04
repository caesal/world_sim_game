#include "game/game_pause_snapshot_coherence_probe_internal.h"

#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "core/plague_perf.h"
#include "core/render_snapshot.h"
#include "core/render_snapshot_cache.h"
#include "core/render_snapshot_keys.h"
#include "core/render_snapshot_plague_cache.h"
#include "core/state_lock.h"
#include "game/game_decision_cache_probe_fixture.h"
#include "sim/decision_snapshot_cache.h"
#include "sim/plague.h"
#include "sim/plague_rules.h"
#include "sim/plague_state.h"
#include "sim/simulation.h"
#include "sim/simulation_scheduler.h"
#include "sim/simulation_worker.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdlib.h>
#include <string.h>

static int same_history(const PlagueEpisodeHistory *left,
                        const PlagueEpisodeHistory *right) {
    return left->episode_id == right->episode_id &&
           left->start_month == right->start_month &&
           left->end_month == right->end_month &&
           left->duration_months == right->duration_months;
}

int pause_probe_cleanup(void) {
    DecisionSnapshotCacheDiagnostics decision;
    simulation_worker_shutdown();
    auto_run = 0;
    simulation_worker_reset_scheduler();
    render_snapshot_shutdown();
    simulation_reset_state();
    world_generated = 0;
    plague_perf_set_system_enabled(1);
    decision_snapshot_cache_get_diagnostics(&decision);
    return sim_scheduler_pending_months() == 0 && !sim_scheduler_active_month() &&
           sim_scheduler_queued_months() == 0 && !sim_scheduler_pause_drain_requested() &&
           !decision.building_active;
}

int pause_probe_setup_fixture(void) {
    PauseProbeObservation observation;
    if (!pause_probe_cleanup()) return 0;
    decision_cache_probe_setup_fixture(2, 370000);
    memset(&cities[0], 0, sizeof(cities[0]));
    cities[0].alive = 1;
    cities[0].owner = 0;
    cities[0].population = 50000;
    cities[0].x = 1;
    cities[0].y = 1;
    snprintf(cities[0].name, sizeof(cities[0].name), "Pause Probe City");
    city_count = 1;
    civs[0].capital_city = 0;
    civs[0].population = cities[0].population;
    civs[0].territory = 1;
    civs[0].collapse_grace_months = 120;
    civs[1].territory = 1;
    civs[1].collapse_grace_months = 120;
    year = 31;
    month = 5;
    speed_index = 0;
    auto_run = 1;
    world_generated = 1;
    plague_perf_set_system_enabled(1);
    srand(370001);
    dirty_mark_world();
    decision_snapshot_cache_reset();
    decision_snapshot_cache_seed_complete();
    render_snapshot_init();
    render_snapshot_cache_update_all();
    render_snapshot_publish_from_live_state();
    pause_probe_observe(&observation);
    return observation.front_valid && observation.front_year == year &&
           observation.front_month == month && observation.front_history_count == 0 &&
           observation.cache_valid && !observation.cache_dirty &&
           observation.decision.published_year == year &&
           observation.decision.published_month == month;
}

int pause_probe_seed_episode(int episode_id, int duration_months,
                             int completes_this_month,
                             PauseProbeExpectedEpisode *expected) {
    PlagueEpisodeState episode;
    int absolute_month = plague_rules_absolute_month(year, month);
    int infection_start;
    int infection_duration;
    if (!expected || duration_months < 1) return 0;
    memset(&episode, 0, sizeof(episode));
    episode.active = 1;
    episode.episode_id = episode_id;
    episode.size = PLAGUE_SIZE_MEDIUM;
    episode.severity = 7;
    episode.name_id = 0;
    episode.name_cycle = 1;
    episode.origin_city_id = 0;
    episode.origin_civ_id = 0;
    episode.origin_civ_uid = civs[0].uid;
    episode.origin_civ_symbol = civs[0].symbol;
    episode.origin_civ_color = civs[0].color;
    episode.start_month = absolute_month - duration_months;
    episode.frozen_occupied_cities = 1;
    episode.spores_initial = 1;
    episode.spores_remaining = 1;
    snprintf(episode.origin_city_name, sizeof(episode.origin_city_name),
             "%s", cities[0].name);
    snprintf(episode.origin_civ_name_en, sizeof(episode.origin_civ_name_en),
             "%s", civs[0].name);
    snprintf(episode.origin_civ_name_zh, sizeof(episode.origin_civ_name_zh),
             "%s", civs[0].name);
    if (!plague_state_begin_episode(&episode)) return 0;
    infection_start = absolute_month - 1;
    infection_duration = completes_this_month ? 1 : 13;
    if (!plague_state_infect_city(0, 0, infection_start, infection_duration) ||
        !plague_state_note_current_country(0) ||
        !plague_state_note_ever_country(0)) return 0;
    plague_state_record_start(episode.start_month);
    expected->episode_id = episode_id;
    expected->start_month = episode.start_month;
    expected->end_month = completes_this_month ? absolute_month : 0;
    expected->duration_months = completes_this_month ? duration_months : 0;
    return 1;
}

int pause_probe_queue_months(int count) {
    int i;
    auto_run = 1;
    for (i = 0; i < count; i++) {
        if (!sim_scheduler_request_month()) return 0;
    }
    return 1;
}

int pause_probe_run_until_history(int history_count, int max_steps) {
    int step;
    for (step = 0; step < max_steps; step++) {
        if (plague_state_get()->history_count >= history_count) return 1;
        if (!sim_scheduler_has_pending_work()) return 0;
        sim_scheduler_run_budget(1);
    }
    return plague_state_get()->history_count >= history_count;
}

int pause_probe_run_until_plague_revision(int initial_revision, int require_active,
                                          int max_steps) {
    int step;
    for (step = 0; step < max_steps; step++) {
        if (dirty_revision_plague() != initial_revision &&
            (!require_active || plague_state_get()->episode.active)) return 1;
        if (!sim_scheduler_has_pending_work()) return 0;
        sim_scheduler_run_budget(1);
    }
    return dirty_revision_plague() != initial_revision &&
           (!require_active || plague_state_get()->episode.active);
}

int pause_probe_wait_settled(int timeout_ms) {
    DWORD start = GetTickCount();
    while ((int)(GetTickCount() - start) <= timeout_ms) {
        if (simulation_worker_pause_settled()) return 1;
        Sleep(2);
    }
    return 0;
}

int pause_probe_wait_calendar_front(int target_year, int target_month, int timeout_ms) {
    DWORD start = GetTickCount();
    while ((int)(GetTickCount() - start) <= timeout_ms) {
        const RenderSnapshot *snapshot;
        DecisionSnapshotCacheDiagnostics decision;
        int live_year;
        int live_month;
        state_read_lock();
        live_year = year;
        live_month = month;
        decision_snapshot_cache_get_diagnostics(&decision);
        state_read_unlock();
        snapshot = render_snapshot_acquire();
        if (live_year == target_year && live_month == target_month && snapshot &&
            snapshot->year == target_year && snapshot->month == target_month &&
            !decision.building_active && decision.published_year == target_year &&
            decision.published_month == target_month) {
            render_snapshot_release(snapshot);
            return 1;
        }
        if (snapshot) render_snapshot_release(snapshot);
        Sleep(2);
    }
    return 0;
}

void pause_probe_observe(PauseProbeObservation *out) {
    PlagueStateView model;
    PlagueStateView cached;
    const RenderSnapshot *front;
    int lane_key;
    if (!out) return;
    memset(out, 0, sizeof(*out));
    memset(&model, 0, sizeof(model));
    memset(&cached, 0, sizeof(cached));
    state_read_lock();
    out->year = year;
    out->month = month;
    plague_state_build_view(plague_rules_absolute_month(year, month), &model);
    out->model_history_count = model.recent_history_count;
    out->model_active = model.episode.active;
    out->model_episode_id = model.episode.episode_id;
    out->model_episode_start = model.episode.start_month;
    if (model.recent_history_count > 0) out->model_history = model.recent_history[0];
    lane_key = render_snapshot_lanes_revision_key();
    out->plague_key = render_snapshot_plague_revision_key(lane_key);
    out->cache_dirty = render_snapshot_plague_cache_is_dirty(out->plague_key);
    out->cache_valid = render_snapshot_cache_plague_summary(
        out->plague_key, &cached, NULL, NULL);
    if (out->cache_valid) {
        out->cache_history_count = cached.recent_history_count;
        out->cache_active = cached.episode.active;
        out->cache_episode_id = cached.episode.episode_id;
        if (cached.recent_history_count > 0) out->cache_history = cached.recent_history[0];
    }
    out->scheduler_pending = sim_scheduler_pending_months();
    out->scheduler_active = sim_scheduler_active_month();
    out->scheduler_queued = sim_scheduler_queued_months();
    decision_snapshot_cache_get_diagnostics(&out->decision);
    state_read_unlock();
    out->worker_pending = simulation_worker_pending_months();
    out->pause_in_progress = simulation_worker_pause_in_progress();
    out->pause_settled = simulation_worker_pause_settled();
    front = render_snapshot_acquire();
    if (!front) return;
    out->front_valid = 1;
    out->front_year = front->year;
    out->front_month = front->month;
    out->front_history_count = front->plague_state.recent_history_count;
    out->front_active = front->plague_state.episode.active;
    out->front_episode_id = front->plague_state.episode.episode_id;
    out->front_plague_revision = front->plague_revision;
    out->front_revision = front->revision;
    if (front->plague_state.recent_history_count > 0) {
        out->front_history = front->plague_state.recent_history[0];
    }
    render_snapshot_release(front);
}

int pause_probe_history_coherent(const PauseProbeObservation *observation,
                                 const PauseProbeExpectedEpisode *expected) {
    PlagueEpisodeHistory expected_history;
    if (!observation || !expected) return 0;
    memset(&expected_history, 0, sizeof(expected_history));
    expected_history.episode_id = expected->episode_id;
    expected_history.start_month = expected->start_month;
    expected_history.end_month = expected->end_month;
    expected_history.duration_months = expected->duration_months;
    return observation->pause_settled && !observation->pause_in_progress &&
           observation->scheduler_pending == 0 && !observation->scheduler_active &&
           observation->scheduler_queued == 0 && observation->worker_pending == 0 &&
           observation->cache_valid && !observation->cache_dirty &&
           observation->front_valid && observation->front_year == observation->year &&
           observation->front_month == observation->month &&
           observation->front_plague_revision == observation->plague_key &&
           observation->model_history_count == 1 &&
           observation->cache_history_count == 1 &&
           observation->front_history_count == 1 &&
           same_history(&observation->model_history, &expected_history) &&
           same_history(&observation->cache_history, &expected_history) &&
           same_history(&observation->front_history, &expected_history) &&
           !observation->model_active && !observation->cache_active &&
           !observation->front_active && !observation->decision.building_active &&
           observation->decision.published_year == observation->year &&
           observation->decision.published_month == observation->month;
}

int pause_probe_active_coherent(const PauseProbeObservation *observation,
                                const PauseProbeExpectedEpisode *expected) {
    if (!observation || !expected) return 0;
    return observation->pause_settled && !observation->pause_in_progress &&
           observation->scheduler_pending == 0 && !observation->scheduler_active &&
           observation->scheduler_queued == 0 && observation->worker_pending == 0 &&
           observation->cache_valid && !observation->cache_dirty &&
           observation->front_valid && observation->front_year == observation->year &&
           observation->front_month == observation->month &&
           observation->front_plague_revision == observation->plague_key &&
           observation->model_history_count == 0 &&
           observation->cache_history_count == 0 &&
           observation->front_history_count == 0 && observation->model_active &&
           observation->cache_active && observation->front_active &&
           observation->model_episode_id == expected->episode_id &&
           observation->cache_episode_id == expected->episode_id &&
           observation->front_episode_id == expected->episode_id &&
           observation->model_episode_start == expected->start_month &&
           !observation->decision.building_active &&
           observation->decision.published_year == observation->year &&
           observation->decision.published_month == observation->month;
}
