#include "simulation_worker.h"

#include "core/game_types.h"
#include "core/profiler.h"
#include "core/render_snapshot.h"
#include "core/render_snapshot_keys.h"
#include "core/render_snapshot_plague_cache.h"
#include "core/state_lock.h"
#include "sim/simulation_scheduler.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static HANDLE worker_thread = NULL;
static volatile LONG worker_stop = 0;
static volatile LONG actual_ms_per_month = 0;
static volatile LONG pending_months_snapshot = 0;
static volatile LONG last_budget_ms = 0;
static volatile LONG last_used_ms = 0;
static volatile LONG overloaded_flag = 0;
static volatile LONG last_completed_tick = 0;
static CRITICAL_SECTION visual_month_lock;
static int visual_month_lock_ready = 0;
static SimCompletedMonthDate visual_months[64];
static int visual_month_head = 0;
static int visual_month_count = 0;
static volatile LONG visual_max_backlog = 0;
static volatile LONG visual_presented_total = 0;
static volatile LONG visual_dropped_months = 0;
static volatile LONG presentation_throttled = 0;
static volatile LONG publish_pending_flag = 0;
static volatile LONG pause_publish_required = 0;
static volatile LONG pause_settled_flag = 1;
static volatile LONG resume_after_pause_flag = 0;
static char worker_status[64] = "Idle";

#define VISUAL_MONTH_QUEUE_CAP 64
#define VISUAL_MONTH_THROTTLE_CAP 3
#define COMPLETED_MONTH_BATCH_CAP 16

static void request_pause_publication(void) {
    sim_scheduler_request_pause_drain();
    InterlockedExchange(&pause_settled_flag, 0);
    InterlockedExchange(&pause_publish_required, 1);
}

static void refresh_dirty_pause_plague_cache(void) {
    int lane_key = render_snapshot_lanes_revision_key();
    int plague_key = render_snapshot_plague_revision_key(lane_key);
    if (render_snapshot_plague_cache_is_dirty(plague_key))
        render_snapshot_plague_cache_update_if_dirty(plague_key);
}

static int budget_for_speed(int speed) {
    static const int budgets[SPEED_COUNT] = {3, 4, 5, 6, 8};
    return budgets[clamp(speed, 0, SPEED_COUNT - 1)];
}

static void set_status(const char *status) {
    lstrcpynA(worker_status, status ? status : "Idle", sizeof(worker_status));
}

static void ensure_visual_month_lock(void) {
    if (visual_month_lock_ready) return;
    InitializeCriticalSection(&visual_month_lock);
    visual_month_lock_ready = 1;
}

static void note_visual_backlog_peak(int backlog) {
    LONG peak = visual_max_backlog;
    while (backlog > peak) {
        LONG previous = InterlockedCompareExchange(&visual_max_backlog, backlog, peak);
        if (previous == peak) break;
        peak = previous;
    }
}

static int visual_backlog_count(void) {
    int backlog;
    ensure_visual_month_lock();
    EnterCriticalSection(&visual_month_lock);
    backlog = visual_month_count;
    LeaveCriticalSection(&visual_month_lock);
    return backlog;
}

static int append_visual_months(const SimCompletedMonthDate *dates, int completed) {
    int i;
    int backlog;
    ensure_visual_month_lock();
    EnterCriticalSection(&visual_month_lock);
    for (i = 0; i < completed; i++) {
        int index;
        if (visual_month_count >= VISUAL_MONTH_QUEUE_CAP) {
            InterlockedIncrement(&visual_dropped_months);
            continue;
        }
        index = (visual_month_head + visual_month_count) % VISUAL_MONTH_QUEUE_CAP;
        visual_months[index] = dates[i];
        visual_month_count++;
    }
    backlog = visual_month_count;
    LeaveCriticalSection(&visual_month_lock);
    note_visual_backlog_peak(backlog);
    InterlockedExchange(&presentation_throttled,
                        backlog >= VISUAL_MONTH_THROTTLE_CAP ? 1 : 0);
    return backlog;
}

static void record_completed_months(const SimCompletedMonthDate *dates, int completed, DWORD *last_tick) {
    DWORD now = GetTickCount();
    int elapsed;
    int sample;
    int current;

    if (completed <= 0) return;
    append_visual_months(dates, completed);
    elapsed = clamp((int)(now - *last_tick), 1, 60000);
    *last_tick = now;
    sample = elapsed / completed;
    current = (int)actual_ms_per_month;
    actual_ms_per_month = current <= 0 ? sample : (current * 3 + sample) / 4;
    last_completed_tick = (LONG)now;
}

static DWORD WINAPI worker_main(void *unused) {
    DWORD last_tick = GetTickCount();
    DWORD last_month_tick = last_tick;
    int accumulator_ms = 0;
    int was_active = 0;
    (void)unused;

    while (!worker_stop) {
        DWORD now = GetTickCount();
        int elapsed = clamp((int)(now - last_tick), 0, 250);
        int speed = clamp(speed_index, 0, SPEED_COUNT - 1);
        int target_ms = SPEED_MS[speed];
        int budget_ms = budget_for_speed(speed);
        int used_ms = 0;
        int completed;
        int pending;
        int performance_limited;
        int has_work;
        int pause_drain = sim_scheduler_pause_drain_requested();

        last_tick = now;
        if (!world_generated || (!auto_run && (!pause_drain || pause_settled_flag))) {
            accumulator_ms = 0;
            last_month_tick = now;
            was_active = 0;
            set_status("Idle");
            Sleep(4);
            continue;
        }
        if (pause_drain) InterlockedExchange(&pause_settled_flag, 0);
        if (!was_active) {
            last_month_tick = now;
            was_active = 1;
        }
        if (!pause_drain && visual_backlog_count() >= VISUAL_MONTH_THROTTLE_CAP) {
            if (publish_pending_flag && render_snapshot_publish_from_live_state_throttled(0)) {
                InterlockedExchange(&publish_pending_flag, 0);
            } else if (!publish_pending_flag && render_snapshot_age_ms() > 1000) {
                render_snapshot_publish_from_live_state_throttled(1);
            }
            accumulator_ms = 0;
            presentation_throttled = 1;
            overloaded_flag = 1;
            set_status("Draining month display");
            Sleep(1);
            continue;
        }

        state_write_lock();
        pause_drain = sim_scheduler_pause_drain_requested();
        if (pause_drain) {
            sim_scheduler_cancel_queued_months_preserve_active();
            accumulator_ms = 0;
        }
        pending = sim_scheduler_pending_months();
        performance_limited = 0;
        if (!pause_drain) {
            accumulator_ms = min(accumulator_ms + elapsed, max(target_ms * 4, 120));
        }
        if (!pause_drain && pending >= sim_scheduler_pending_month_cap()) {
            sim_scheduler_trim_pending_months(2);
            pending = sim_scheduler_pending_months();
            overloaded_flag = 1;
        }
        while (!pause_drain && !performance_limited && accumulator_ms >= target_ms) {
            if (!sim_scheduler_can_accept_month()) {
                overloaded_flag = 1;
                accumulator_ms = target_ms - 1;
                break;
            }
            sim_scheduler_request_month();
            accumulator_ms -= target_ms;
        }
        pending_months_snapshot = sim_scheduler_pending_months();
        has_work = sim_scheduler_has_pending_work();
        state_write_unlock();

        if (has_work) {
            DWORD start = GetTickCount();
            SimCompletedMonthDate completed_dates[COMPLETED_MONTH_BATCH_CAP];
            set_status(pause_drain ? "Finishing current month" :
                       (performance_limited ? "Performance limited; backlog prevented" :
                                              "Running simulation"));
            do {
                int remaining;
                state_write_lock();
                if (!sim_scheduler_has_pending_work()) {
                    state_write_unlock();
                    break;
                }
                remaining = max(1, budget_ms - used_ms);
                sim_scheduler_run_for_ms(remaining);
                if (!auto_run && !sim_scheduler_pause_drain_requested()) {
                    request_pause_publication();
                    pause_drain = 1;
                }
                completed = sim_scheduler_take_completed_month_dates(completed_dates, COMPLETED_MONTH_BATCH_CAP);
                if (completed > 0) InterlockedExchange(&publish_pending_flag, 1);
                pending_months_snapshot = sim_scheduler_pending_months();
                state_write_unlock();
                record_completed_months(completed_dates, completed, &last_month_tick);
                used_ms = (int)(GetTickCount() - start);
                if (completed > 0 || used_ms >= budget_ms || completed == 0) break;
            } while (used_ms < budget_ms);
            state_write_lock();
            has_work = sim_scheduler_has_pending_work();
            state_write_unlock();
            if (used_ms >= budget_ms && has_work) overloaded_flag = 1;
        } else {
            set_status("Waiting for next month");
        }
        pause_drain = sim_scheduler_pause_drain_requested();
        if (pause_drain && !has_work && pause_publish_required) {
            state_write_lock();
            refresh_dirty_pause_plague_cache();
            state_write_unlock();
        }
        if ((!pause_drain && publish_pending_flag) ||
            (pause_drain && !has_work &&
             (publish_pending_flag || pause_publish_required))) {
            int force = pause_drain ? 1 : 0;
            if (pause_drain) set_status("Publishing paused state");
            if (render_snapshot_publish_from_live_state_throttled(force)) {
                InterlockedExchange(&publish_pending_flag, 0);
                InterlockedExchange(&pause_publish_required, 0);
            }
        }
        if (pause_drain) {
            state_write_lock();
            has_work = sim_scheduler_has_pending_work();
            pending_months_snapshot = sim_scheduler_pending_months();
            state_write_unlock();
            if (!has_work && !publish_pending_flag && !pause_publish_required) {
                InterlockedExchange(&pause_settled_flag, 1);
                set_status("Idle");
                if (InterlockedExchange(&resume_after_pause_flag, 0)) {
                    auto_run = 1;
                    InterlockedExchange(&pause_settled_flag, 0);
                    sim_scheduler_resume_month_admission();
                    set_status("Waiting for next month");
                }
            }
        }
        last_budget_ms = budget_ms;
        last_used_ms = used_ms;
        if (used_ms > budget_ms || pending_months_snapshot >= sim_scheduler_pending_month_cap()) {
            overloaded_flag = 1;
        } else if (pending_months_snapshot == 0) {
            overloaded_flag = 0;
        }
        Sleep(speed >= 3 ? 0 : 1);
    }
    return 0;
}

void simulation_worker_start(void) {
    if (worker_thread) return;
    ensure_visual_month_lock();
    worker_stop = 0;
    worker_thread = CreateThread(NULL, 0, worker_main, NULL, 0, NULL);
}

int simulation_worker_quiesce(void) {
    DWORD wait_result;
    if (!worker_thread) return 1;
    InterlockedExchange(&worker_stop, 1);
    wait_result = WaitForSingleObject(worker_thread, INFINITE);
    if (wait_result != WAIT_OBJECT_0) return 0;
    CloseHandle(worker_thread);
    worker_thread = NULL;
    return 1;
}

void simulation_worker_shutdown(void) { (void)simulation_worker_quiesce(); }

void simulation_worker_request_pause(void) {
    InterlockedExchange(&resume_after_pause_flag, 0);
    if (sim_scheduler_pause_drain_requested() && pause_settled_flag) {
        auto_run = 0;
        return;
    }
    request_pause_publication();
    auto_run = 0;
    if (!world_generated) {
        InterlockedExchange(&pause_publish_required, 0);
        InterlockedExchange(&pause_settled_flag, 1);
        return;
    }
    simulation_worker_start();
}

void simulation_worker_request_resume(void) {
    InterlockedExchange(&resume_after_pause_flag, 1);
    if (simulation_worker_pause_in_progress()) {
        simulation_worker_start();
        return;
    }
    InterlockedExchange(&resume_after_pause_flag, 0);
    auto_run = 1;
    InterlockedExchange(&pause_publish_required, 0);
    InterlockedExchange(&pause_settled_flag, 0);
    sim_scheduler_resume_month_admission();
    simulation_worker_start();
}

int simulation_worker_pause_in_progress(void) {
    return sim_scheduler_pause_drain_requested() && !pause_settled_flag;
}

int simulation_worker_pause_settled(void) {
    return sim_scheduler_pause_drain_requested() && pause_settled_flag;
}

void simulation_worker_reset_scheduler(void) {
    ensure_visual_month_lock();
    state_write_lock();
    sim_scheduler_reset();
    state_write_unlock();
    actual_ms_per_month = 0;
    pending_months_snapshot = 0;
    last_budget_ms = 0;
    last_used_ms = 0;
    overloaded_flag = 0;
    last_completed_tick = 0;
    publish_pending_flag = 0;
    pause_publish_required = 0;
    pause_settled_flag = auto_run ? 0 : 1;
    resume_after_pause_flag = 0;
    EnterCriticalSection(&visual_month_lock);
    visual_month_head = 0;
    visual_month_count = 0;
    LeaveCriticalSection(&visual_month_lock);
    visual_max_backlog = 0;
    visual_presented_total = 0;
    visual_dropped_months = 0;
    presentation_throttled = 0;
    set_status("Idle");
}

int simulation_worker_actual_ms_per_month(void) { return (int)actual_ms_per_month; }
int simulation_worker_pending_months(void) { return (int)pending_months_snapshot; }
int simulation_worker_last_budget_ms(void) { return (int)last_budget_ms; }
int simulation_worker_last_used_ms(void) { return (int)last_used_ms; }
int simulation_worker_overloaded(void) { return (int)overloaded_flag; }
int simulation_worker_snapshot_age_ms(void) {
    LONG tick = last_completed_tick;
    if (tick <= 0) return 0;
    return clamp((int)(GetTickCount() - (DWORD)tick), 0, 600000);
}
int simulation_worker_take_visual_month(int *out_year, int *out_month) {
    SimCompletedMonthDate date;
    int remaining;
    ensure_visual_month_lock();
    EnterCriticalSection(&visual_month_lock);
    if (visual_month_count <= 0) {
        LeaveCriticalSection(&visual_month_lock);
        return 0;
    }
    date = visual_months[visual_month_head];
    visual_month_head = (visual_month_head + 1) % VISUAL_MONTH_QUEUE_CAP;
    visual_month_count--;
    remaining = visual_month_count;
    LeaveCriticalSection(&visual_month_lock);
    if (out_year) *out_year = date.year;
    if (out_month) *out_month = date.month;
    InterlockedIncrement(&visual_presented_total);
    if (remaining < VISUAL_MONTH_THROTTLE_CAP) InterlockedExchange(&presentation_throttled, 0);
    return 1;
}
int simulation_worker_visual_backlog(void) { return visual_backlog_count(); }
int simulation_worker_visual_coalesced_months(void) { return (int)visual_dropped_months; }
int simulation_worker_visual_max_backlog(void) { return (int)visual_max_backlog; }
int simulation_worker_visual_presented_total(void) { return (int)visual_presented_total; }
int simulation_worker_visual_dropped_months(void) { return (int)visual_dropped_months; }
int simulation_worker_presentation_throttled(void) {
    return (int)presentation_throttled || visual_backlog_count() >= VISUAL_MONTH_THROTTLE_CAP;
}
const char *simulation_worker_status(void) { return worker_status; }

void simulation_worker_debug_reset_presentation_queue(void) {
    ensure_visual_month_lock();
    EnterCriticalSection(&visual_month_lock);
    visual_month_head = 0;
    visual_month_count = 0;
    LeaveCriticalSection(&visual_month_lock);
    visual_max_backlog = 0;
    visual_presented_total = 0;
    visual_dropped_months = 0;
    presentation_throttled = 0;
}

int simulation_worker_debug_enqueue_completed_month(int completed_year, int completed_month) {
    SimCompletedMonthDate date;
    int dropped_before = (int)visual_dropped_months;
    date.year = completed_year;
    date.month = completed_month;
    append_visual_months(&date, 1);
    return (int)visual_dropped_months == dropped_before;
}
