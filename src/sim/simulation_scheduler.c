#include "simulation_scheduler.h"

#include "core/game_types.h"
#include "core/profiler.h"
#include "sim/simulation_month.h"

#define SIM_PENDING_MONTH_CAP 4

static int pending_months = 0;
static int completed_months = 0;
static int last_step_ms = 0;
static SimulationMonthState active_month;

void sim_scheduler_reset(void) {
    pending_months = 0;
    completed_months = 0;
    last_step_ms = 0;
    active_month.active = 0;
}

int sim_scheduler_can_accept_month(void) {
    return pending_months < SIM_PENDING_MONTH_CAP;
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
            pending_months--;
            if (!simulation_month_begin(&active_month)) continue;
        }
        did_work |= simulation_month_run_next(&active_month);
        if (simulation_month_is_done(&active_month)) completed_months++;
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
            event_log_push("[Performance] Simulation step exceeded frame budget.");
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

int sim_scheduler_take_completed_months(void) {
    int result = completed_months;
    completed_months = 0;
    return result;
}

void sim_scheduler_run_blocking_month(void) {
    simulation_month_run_blocking();
}
