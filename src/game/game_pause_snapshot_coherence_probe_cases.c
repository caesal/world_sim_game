#include "game/game_pause_snapshot_coherence_probe_internal.h"

#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "core/render_snapshot_keys.h"
#include "core/render_snapshot_plague_cache.h"
#include "core/state_lock.h"
#include "game/game.h"
#include "sim/decision_snapshot_cache.h"
#include "sim/plague.h"
#include "sim/plague_state.h"
#include "sim/simulation_scheduler.h"
#include "sim/simulation_worker.h"
#include "sim/war_history.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int pause_probe_report(PauseProbeReport *report, const char *name, int ok,
                       const char *format, ...) {
    va_list args;
    if (!report || !report->summary || !name) return 0;
    report->case_count++;
    if (ok) report->pass_count++;
    fprintf(report->summary, "case=%s ok=%d", name, ok);
    if (format && format[0]) {
        fputc(' ', report->summary);
        va_start(args, format);
        vfprintf(report->summary, format, args);
        va_end(args);
    }
    fputc('\n', report->summary);
    return ok;
}

static void next_month(int source_year, int source_month,
                       int *target_year, int *target_month) {
    *target_year = source_year;
    *target_month = source_month + 1;
    if (*target_month > 12) {
        *target_month = 1;
        (*target_year)++;
    }
}

static int take_only_visual_month(int expected_year, int expected_month) {
    int actual_year = 0;
    int actual_month = 0;
    int extra_year = 0;
    int extra_month = 0;
    return simulation_worker_take_visual_month(&actual_year, &actual_month) &&
           actual_year == expected_year && actual_month == expected_month &&
           !simulation_worker_take_visual_month(&extra_year, &extra_month);
}

static int history_payload_matches(
    const PauseProbeObservation *observation,
    const PauseProbeExpectedEpisode *expected) {
    return observation->model_history_count == 1 &&
           observation->cache_history_count == 1 &&
           observation->front_history_count == 1 &&
           observation->model_history.episode_id == expected->episode_id &&
           observation->cache_history.episode_id == expected->episode_id &&
           observation->front_history.episode_id == expected->episode_id &&
           observation->model_history.start_month == expected->start_month &&
           observation->cache_history.start_month == expected->start_month &&
           observation->front_history.start_month == expected->start_month &&
           observation->model_history.end_month == expected->end_month &&
           observation->cache_history.end_month == expected->end_month &&
           observation->front_history.end_month == expected->end_month &&
           observation->model_history.duration_months == expected->duration_months &&
           observation->cache_history.duration_months == expected->duration_months &&
           observation->front_history.duration_months == expected->duration_months;
}

static int wait_deferred_resume(
    int target_year, int target_month,
    const PauseProbeExpectedEpisode *expected,
    PauseProbeObservation *observation, int timeout_ms) {
    DWORD start = GetTickCount();
    while ((int)(GetTickCount() - start) <= timeout_ms) {
        pause_probe_observe(observation);
        if (auto_run && !observation->pause_in_progress &&
            !sim_scheduler_pause_drain_requested() &&
            observation->year == target_year && observation->month == target_month &&
            observation->front_year == target_year &&
            observation->front_month == target_month &&
            observation->scheduler_pending == 0 && !observation->scheduler_active &&
            observation->scheduler_queued == 0 && observation->cache_valid &&
            !observation->cache_dirty &&
            observation->front_plague_revision == observation->plague_key &&
            !observation->decision.building_active &&
            history_payload_matches(observation, expected)) return 1;
        Sleep(2);
    }
    return 0;
}

static int case_resume_waits_for_pause_publication(PauseProbeReport *report) {
    PauseProbeExpectedEpisode expected;
    PauseProbeObservation boundary;
    PauseProbeObservation resumed;
    int target_year = 0;
    int target_month = 0;
    int admission_blocked = 0;
    int auto_run_after_request = -1;
    int pause_after_request = -1;
    int admission_after_request = -1;
    int resumed_ok = 0;
    int setup_ok = pause_probe_setup_fixture();

    memset(&expected, 0, sizeof(expected));
    memset(&boundary, 0, sizeof(boundary));
    memset(&resumed, 0, sizeof(resumed));
    if (setup_ok && pause_probe_seed_episode(904, 136, 1, &expected) &&
        pause_probe_queue_months(2) && pause_probe_run_until_history(1, 20000)) {
        pause_probe_observe(&boundary);
        next_month(boundary.year, boundary.month, &target_year, &target_month);
        state_write_lock();
        game_request_pause();
        game_request_resume();
        admission_blocked = !auto_run && game_pause_in_progress() &&
            !sim_scheduler_can_accept_month() && !sim_scheduler_request_month();
        auto_run_after_request = auto_run;
        pause_after_request = game_pause_in_progress();
        admission_after_request = sim_scheduler_can_accept_month();
        state_write_unlock();
        resumed_ok = admission_blocked &&
            wait_deferred_resume(target_year, target_month, &expected,
                                 &resumed, 15000) &&
            take_only_visual_month(target_year, target_month);
        game_request_pause();
        resumed_ok = resumed_ok && pause_probe_wait_settled(15000);
    }
    pause_probe_report(report, "resume_deferred_while_pause_publication_pending",
        setup_ok && admission_blocked,
        "auto_run=%d pause_in_progress=%d admission_open=%d pending=%d active=%d queued=%d",
        auto_run_after_request, pause_after_request, admission_after_request,
        boundary.scheduler_pending, boundary.scheduler_active,
        boundary.scheduler_queued);
    pause_probe_report(report, "deferred_resume_after_coherent_front_publication",
        setup_ok && resumed_ok,
        "target=%d/%d actual=%d/%d history=%d/%d/%d episode=%d",
        target_year, target_month, resumed.year, resumed.month,
        resumed.model_history_count, resumed.cache_history_count,
        resumed.front_history_count, resumed.front_history.episode_id);
    setup_ok = pause_probe_cleanup();
    pause_probe_report(report, "deferred_resume_restoration", setup_ok,
        "pending=%d active=%d queued=%d",
        sim_scheduler_pending_months(), sim_scheduler_active_month(),
        sim_scheduler_queued_months());
    return admission_blocked && resumed_ok && setup_ok;
}

static int case_history_boundary_queue_resume(PauseProbeReport *report) {
    PauseProbeExpectedEpisode expected;
    PauseProbeObservation boundary;
    PauseProbeObservation paused;
    PauseProbeObservation resumed;
    int initial_year;
    int initial_month;
    int paused_year;
    int paused_month;
    int resume_year;
    int resume_month;
    int boundary_ok = 0;
    int queue_ok = 0;
    int identity_ok = 0;
    int resume_ok = 0;
    int setup_ok = pause_probe_setup_fixture();

    memset(&boundary, 0, sizeof(boundary));
    memset(&paused, 0, sizeof(paused));
    memset(&resumed, 0, sizeof(resumed));
    memset(&expected, 0, sizeof(expected));
    initial_year = year;
    initial_month = month;
    if (setup_ok && pause_probe_seed_episode(901, 136, 1, &expected) &&
        pause_probe_queue_months(4) &&
        pause_probe_run_until_history(1, 20000)) {
        pause_probe_observe(&boundary);
        boundary_ok = boundary.model_history_count == 1 &&
            boundary.front_history_count == 0 && boundary.scheduler_active == 1 &&
            boundary.scheduler_queued == 3 && boundary.year == initial_year &&
            boundary.month == initial_month;
        game_request_pause();
        if (pause_probe_wait_settled(15000)) {
            pause_probe_observe(&paused);
            next_month(initial_year, initial_month, &paused_year, &paused_month);
            identity_ok = pause_probe_history_coherent(&paused, &expected) &&
                paused.year == paused_year && paused.month == paused_month;
            queue_ok = paused.scheduler_queued == 0 &&
                take_only_visual_month(paused_year, paused_month);
            if (identity_ok && queue_ok) {
                next_month(paused_year, paused_month, &resume_year, &resume_month);
                game_request_resume();
                state_write_lock();
                resume_ok = sim_scheduler_request_month();
                state_write_unlock();
                resume_ok = resume_ok &&
                    pause_probe_wait_calendar_front(resume_year, resume_month, 15000);
                game_request_pause();
                resume_ok = resume_ok && pause_probe_wait_settled(15000);
                pause_probe_observe(&resumed);
                resume_ok = resume_ok &&
                    pause_probe_history_coherent(&resumed, &expected) &&
                    resumed.year == resume_year && resumed.month == resume_month &&
                    take_only_visual_month(resume_year, resume_month) &&
                    resumed.model_history_count == 1;
            }
        }
    }
    pause_probe_report(report, "history_written_before_snapshot_pause_boundary",
        setup_ok && boundary_ok,
        "history=%d front_history=%d active=%d queued=%d date=%d/%d",
        boundary.model_history_count, boundary.front_history_count,
        boundary.scheduler_active, boundary.scheduler_queued,
        boundary.year, boundary.month);
    pause_probe_report(report, "queued_months_cancel_started_month_finishes",
        setup_ok && boundary_ok && queue_ok,
        "queued_before=%d queued_after=%d pending_after=%d date=%d/%d",
        boundary.scheduler_queued, paused.scheduler_queued, paused.scheduler_pending,
        paused.year, paused.month);
    pause_probe_report(report, "completed_episode_model_cache_front_identity",
        setup_ok && identity_ok,
        "episode=%d start=%d end=%d duration=%d plague_key=%d front_key=%d",
        expected.episode_id, expected.start_month, expected.end_month,
        expected.duration_months, paused.plague_key, paused.front_plague_revision);
    pause_probe_report(report, "pause_resume_month_and_episode_continuity",
        setup_ok && resume_ok,
        "paused=%d/%d resumed=%d/%d history=%d episode=%d",
        paused.year, paused.month, resumed.year, resumed.month,
        resumed.model_history_count, resumed.model_history.episode_id);
    setup_ok = pause_probe_cleanup();
    pause_probe_report(report, "history_boundary_restoration", setup_ok,
                       "pending=%d active=%d queued=%d",
                       sim_scheduler_pending_months(), sim_scheduler_active_month(),
                       sim_scheduler_queued_months());
    return boundary_ok && queue_ok && identity_ok && resume_ok && setup_ok;
}

static int case_no_active_dirty_publish(PauseProbeReport *report) {
    PauseProbeExpectedEpisode expected;
    PauseProbeObservation dirty;
    PauseProbeObservation paused;
    int initial_year;
    int initial_month;
    int ok = pause_probe_setup_fixture();
    initial_year = year;
    initial_month = month;
    if (ok) ok = pause_probe_seed_episode(902, 136, 1, &expected);
    if (ok) {
        plague_update_month();
        pause_probe_observe(&dirty);
        ok = dirty.model_history_count == 1 && dirty.front_history_count == 0 &&
             dirty.scheduler_pending == 0 && !dirty.scheduler_active;
    }
    if (ok) {
        game_request_pause();
        ok = pause_probe_wait_settled(15000);
    }
    pause_probe_observe(&paused);
    ok = ok && paused.year == initial_year && paused.month == initial_month &&
         pause_probe_history_coherent(&paused, &expected);
    pause_probe_report(report, "no_active_month_dirty_cache_publishes_on_pause", ok,
        "date=%d/%d history=%d/%d/%d cache_dirty=%d front_key=%d live_key=%d",
        paused.year, paused.month, paused.model_history_count,
        paused.cache_history_count, paused.front_history_count,
        paused.cache_dirty, paused.front_plague_revision, paused.plague_key);
    {
        int restored = pause_probe_cleanup();
        pause_probe_report(report, "no_active_dirty_restoration", restored,
                           "pending=%d active=%d",
                           sim_scheduler_pending_months(), sim_scheduler_active_month());
        return ok && restored;
    }
}

static int case_active_episode_pause(PauseProbeReport *report) {
    PauseProbeExpectedEpisode expected;
    PauseProbeObservation paused;
    int initial_year;
    int initial_month;
    int target_year;
    int target_month;
    int initial_plague_revision;
    int ok = pause_probe_setup_fixture();
    initial_year = year;
    initial_month = month;
    initial_plague_revision = dirty_revision_plague();
    if (ok) ok = pause_probe_seed_episode(903, 24, 0, &expected) &&
                 pause_probe_queue_months(1) &&
                 pause_probe_run_until_plague_revision(initial_plague_revision, 1, 20000);
    if (ok) {
        game_request_pause();
        ok = pause_probe_wait_settled(15000);
    }
    pause_probe_observe(&paused);
    next_month(initial_year, initial_month, &target_year, &target_month);
    ok = ok && paused.year == target_year && paused.month == target_month &&
         pause_probe_active_coherent(&paused, &expected) &&
         take_only_visual_month(target_year, target_month);
    pause_probe_report(report, "active_episode_pause_preserves_episode", ok,
        "date=%d/%d active=%d/%d/%d episode=%d/%d/%d history=%d/%d/%d",
        paused.year, paused.month, paused.model_active, paused.cache_active,
        paused.front_active, paused.model_episode_id, paused.cache_episode_id,
        paused.front_episode_id, paused.model_history_count,
        paused.cache_history_count, paused.front_history_count);
    {
        int restored = pause_probe_cleanup();
        pause_probe_report(report, "active_episode_restoration", restored,
                           "pending=%d active=%d",
                           sim_scheduler_pending_months(), sim_scheduler_active_month());
        return ok && restored;
    }
}

static int finish_decision_generation(void) {
    int guard = 0;
    while (!decision_snapshot_cache_service_slice() && guard++ < MAX_CIVS + 4) {}
    return guard <= MAX_CIVS + 4;
}

static int case_decision_war_key_isolation(PauseProbeReport *report) {
    DecisionSnapshotCacheDiagnostics before;
    DecisionSnapshotCacheDiagnostics after;
    int lane_key;
    int civ_before;
    int civ_after;
    int plague_before;
    int plague_after;
    int decision_civ_changed = 0;
    int decision_plague_stable = 0;
    int war_civ_changed;
    int war_plague_stable;
    int ok = pause_probe_setup_fixture();
    if (ok) {
        lane_key = render_snapshot_lanes_revision_key();
        civ_before = render_snapshot_civs_revision_key();
        plague_before = render_snapshot_plague_revision_key(lane_key);
        decision_snapshot_cache_get_diagnostics(&before);
        decision_snapshot_cache_mark_all_dirty();
        decision_snapshot_cache_begin_generation();
        ok = finish_decision_generation();
        decision_snapshot_cache_get_diagnostics(&after);
        civ_after = render_snapshot_civs_revision_key();
        plague_after = render_snapshot_plague_revision_key(lane_key);
        decision_civ_changed = civ_after != civ_before;
        decision_plague_stable = plague_after == plague_before;
        ok = ok && after.published_revision == before.published_revision + 1 &&
             decision_civ_changed && decision_plague_stable &&
             !render_snapshot_plague_cache_is_dirty(plague_after);
        civ_before = civ_after;
        plague_before = plague_after;
        war_history_reset();
        war_civ_changed = render_snapshot_civs_revision_key() != civ_before;
        war_plague_stable =
            render_snapshot_plague_revision_key(lane_key) == plague_before;
        ok = ok && war_civ_changed && war_plague_stable &&
             !render_snapshot_plague_cache_is_dirty(plague_before);
    } else {
        before.published_revision = 0;
        after.published_revision = 0;
        civ_before = civ_after = plague_before = plague_after = 0;
        decision_civ_changed = decision_plague_stable = 0;
        war_civ_changed = war_plague_stable = 0;
    }
    pause_probe_report(report, "decision_and_war_revision_isolation", ok,
        "decision_revision=%llu/%llu civ_changed=%d plague_stable=%d war_keys=%d/%d",
        (unsigned long long)before.published_revision,
        (unsigned long long)after.published_revision,
        decision_civ_changed, decision_plague_stable,
        war_civ_changed, war_plague_stable);
    {
        int restored = pause_probe_cleanup();
        pause_probe_report(report, "key_isolation_restoration", restored,
                           "pending=%d active=%d",
                           sim_scheduler_pending_months(), sim_scheduler_active_month());
        return ok && restored;
    }
}

int pause_probe_run_cases(PauseProbeReport *report) {
    int ok = 1;
    ok &= case_history_boundary_queue_resume(report);
    ok &= case_resume_waits_for_pause_publication(report);
    ok &= case_no_active_dirty_publish(report);
    ok &= case_active_episode_pause(report);
    ok &= case_decision_war_key_isolation(report);
    return ok;
}
