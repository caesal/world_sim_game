#include "simulation_scheduler.h"

#include "core/game_types.h"
#include "core/profiler.h"
#include "sim/simulation_month.h"

#include <stdio.h>

#define SIM_PENDING_MONTH_CAP 4
#define SIM_COMPLETED_MONTH_CAP 16

static int pending_months = 0;
static SimCompletedMonthDate completed_months[SIM_COMPLETED_MONTH_CAP];
static int completed_month_head = 0;
static int completed_month_count = 0;
static int last_step_ms = 0;
static SimulationMonthState active_month;
static DWORD last_budget_event_tick = 0;
static int budget_event_repeat = 0;
static volatile LONG month_admission_state = 1;

enum {
    MONTH_ADMISSION_PAUSED = 0,
    MONTH_ADMISSION_OPEN = 1,
    MONTH_ADMISSION_START_CLAIMED = 2
};

static int claim_month_start(void) {
    if (!auto_run) return 0;
    return InterlockedCompareExchange(&month_admission_state,
                                      MONTH_ADMISSION_START_CLAIMED,
                                      MONTH_ADMISSION_OPEN) == MONTH_ADMISSION_OPEN;
}

static void release_month_start_claim(void) {
    InterlockedCompareExchange(&month_admission_state,
                               MONTH_ADMISSION_OPEN,
                               MONTH_ADMISSION_START_CLAIMED);
}

static void log_budget_yield(int step_ms, int budget_ms) {
    DWORD now = GetTickCount();
    char text[160];

    budget_event_repeat++;
    if (now - last_budget_event_tick < 2000) return;
    if (budget_event_repeat > 1) {
        snprintf(text, sizeof(text), "scheduler over-budget yield");
    } else {
        snprintf(text, sizeof(text), "scheduler over-budget yield");
    }
    event_log_push_structured(EVENT_TYPE_SCHEDULER_YIELD, EVENT_SEVERITY_WARNING,
                              -1, -1, budget_event_repeat, -1, step_ms, budget_ms, text);
    budget_event_repeat = 0;
    last_budget_event_tick = now;
}

static void enqueue_completed_month_date(void) {
    int index;
    if (completed_month_count >= SIM_COMPLETED_MONTH_CAP) return;
    index = (completed_month_head + completed_month_count) % SIM_COMPLETED_MONTH_CAP;
    completed_months[index].year = year;
    completed_months[index].month = month;
    completed_month_count++;
}

void sim_scheduler_reset(void) {
    pending_months = 0;
    completed_month_head = 0;
    completed_month_count = 0;
    last_step_ms = 0;
    active_month.active = 0;
    simulation_month_reset_runtime();
    InterlockedExchange(&month_admission_state, MONTH_ADMISSION_OPEN);
}

void sim_scheduler_request_pause_drain(void) {
    InterlockedExchange(&month_admission_state, MONTH_ADMISSION_PAUSED);
}

void sim_scheduler_resume_month_admission(void) {
    InterlockedExchange(&month_admission_state, MONTH_ADMISSION_OPEN);
}

int sim_scheduler_pause_drain_requested(void) {
    return InterlockedCompareExchange(&month_admission_state, 0, 0) ==
           MONTH_ADMISSION_PAUSED;
}

int sim_scheduler_cancel_queued_months_preserve_active(void) {
    int canceled = pending_months;
    pending_months = 0;
    return canceled;
}

int sim_scheduler_active_month(void) {
    return simulation_month_is_done(&active_month) ? 0 : 1;
}

int sim_scheduler_queued_months(void) { return pending_months; }

int sim_scheduler_can_accept_month(void) {
    return InterlockedCompareExchange(&month_admission_state, 0, 0) ==
               MONTH_ADMISSION_OPEN &&
           pending_months < SIM_PENDING_MONTH_CAP;
}

int sim_scheduler_request_month(void) {
    if (!sim_scheduler_can_accept_month()) return 0;
    pending_months++;
    return 1;
}

int sim_scheduler_has_pending_work(void) {
    return pending_months > 0 || !simulation_month_is_done(&active_month);
}

int sim_scheduler_run_budget(int work_units) {
    int did_work = 0;

    while (work_units > 0) {
        if (simulation_month_is_done(&active_month)) {
            if (pending_months <= 0) break;
            if (!claim_month_start()) {
                pending_months = 0;
                break;
            }
            pending_months--;
            if (!simulation_month_begin(&active_month)) {
                release_month_start_claim();
                continue;
            }
            release_month_start_claim();
        }
        did_work |= simulation_month_run_next(&active_month);
        if (simulation_month_is_done(&active_month)) enqueue_completed_month_date();
        work_units--;
    }
    return did_work;
}

int sim_scheduler_run_for_ms(int budget_ms) {
    DWORD start = GetTickCount();
    int did_work = 0;

    if (budget_ms < 1) budget_ms = 1;
    do {
        DWORD step_start = GetTickCount();
        did_work |= sim_scheduler_run_budget(1);
        last_step_ms = (int)(GetTickCount() - step_start);
        profiler_record_scheduler_step(last_step_ms, last_step_ms > budget_ms);
        if (last_step_ms > budget_ms) {
            log_budget_yield(last_step_ms, budget_ms);
            break;
        }
    } while (sim_scheduler_has_pending_work() && (int)(GetTickCount() - start) < budget_ms);
    return did_work;
}

int sim_scheduler_pending_months(void) {
    return pending_months + (simulation_month_is_done(&active_month) ? 0 : 1);
}

int sim_scheduler_pending_month_cap(void) {
    return SIM_PENDING_MONTH_CAP;
}

int sim_scheduler_last_step_ms(void) {
    return last_step_ms;
}

void sim_scheduler_trim_pending_months(int max_total_pending) {
    int active = simulation_month_is_done(&active_month) ? 0 : 1;
    int allowed_pending = max_total_pending - active;

    if (allowed_pending < 0) allowed_pending = 0;
    if (pending_months > allowed_pending) pending_months = allowed_pending;
}

int sim_scheduler_take_completed_month_dates(SimCompletedMonthDate *out_dates, int max_dates) {
    int i;
    int count = min(completed_month_count, max_dates);
    if (!out_dates || max_dates <= 0) return 0;
    for (i = 0; i < count; i++) {
        out_dates[i] = completed_months[(completed_month_head + i) % SIM_COMPLETED_MONTH_CAP];
    }
    completed_month_head = (completed_month_head + count) % SIM_COMPLETED_MONTH_CAP;
    completed_month_count -= count;
    return count;
}

void sim_scheduler_run_blocking_month(void) {
    simulation_month_run_blocking();
}
