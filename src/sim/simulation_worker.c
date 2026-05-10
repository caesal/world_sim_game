#include "simulation_worker.h"

#include "core/game_types.h"
#include "core/profiler.h"
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
static volatile LONG visual_completed_months = 0;
static char worker_status[64] = "Idle";

static int budget_for_speed(int speed) {
    static const int budgets[SPEED_COUNT] = {4, 8, 12, 16, 22};
    return budgets[clamp(speed, 0, SPEED_COUNT - 1)];
}

static void set_status(const char *status) {
    lstrcpynA(worker_status, status ? status : "Idle", sizeof(worker_status));
}

static void record_completed_months(int completed, DWORD *last_tick) {
    DWORD now = GetTickCount();
    int elapsed;
    int sample;
    int current;

    if (completed <= 0) return;
    InterlockedAdd(&visual_completed_months, completed);
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
        int severe_overload;
        int has_work;

        last_tick = now;
        if (!auto_run || !world_generated) {
            accumulator_ms = 0;
            set_status("Idle");
            Sleep(4);
            continue;
        }

        state_write_lock();
        pending = sim_scheduler_pending_months();
        severe_overload = actual_ms_per_month > target_ms * 10 && pending > 0;
        accumulator_ms = severe_overload ? 0 : min(accumulator_ms + elapsed, max(target_ms * 8, 250));
        while (!severe_overload && accumulator_ms >= target_ms) {
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
            set_status("Running simulation");
            do {
                state_write_lock();
                if (!sim_scheduler_has_pending_work()) {
                    state_write_unlock();
                    break;
                }
                sim_scheduler_run_budget(1);
                completed = sim_scheduler_take_completed_months();
                pending_months_snapshot = sim_scheduler_pending_months();
                state_write_unlock();
                record_completed_months(completed, &last_month_tick);
                used_ms = (int)(GetTickCount() - start);
            } while (used_ms < budget_ms);
            state_write_lock();
            has_work = sim_scheduler_has_pending_work();
            state_write_unlock();
            if (used_ms >= budget_ms && has_work) overloaded_flag = 1;
        } else {
            set_status("Waiting for next month");
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
    worker_stop = 0;
    worker_thread = CreateThread(NULL, 0, worker_main, NULL, 0, NULL);
}

void simulation_worker_shutdown(void) {
    if (!worker_thread) return;
    worker_stop = 1;
    WaitForSingleObject(worker_thread, 2000);
    CloseHandle(worker_thread);
    worker_thread = NULL;
}

void simulation_worker_reset_scheduler(void) {
    state_write_lock();
    sim_scheduler_reset();
    state_write_unlock();
    actual_ms_per_month = 0;
    pending_months_snapshot = 0;
    last_budget_ms = 0;
    last_used_ms = 0;
    overloaded_flag = 0;
    last_completed_tick = 0;
    visual_completed_months = 0;
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
int simulation_worker_take_visual_tick(void) { return (int)InterlockedExchange(&visual_completed_months, 0); }
const char *simulation_worker_status(void) { return worker_status; }
