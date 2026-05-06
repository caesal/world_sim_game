#include "simulation_scheduler.h"

#include "sim/simulation_month.h"

#define SIM_PENDING_MONTH_CAP 12

static int pending_months = 0;
static int completed_months = 0;
static SimulationMonthState active_month;

void sim_scheduler_reset(void) {
    pending_months = 0;
    completed_months = 0;
    active_month.active = 0;
}

void sim_scheduler_request_month(void) {
    if (pending_months < SIM_PENDING_MONTH_CAP) pending_months++;
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

int sim_scheduler_pending_months(void) {
    return pending_months + (simulation_month_is_done(&active_month) ? 0 : 1);
}

int sim_scheduler_take_completed_months(void) {
    int result = completed_months;
    completed_months = 0;
    return result;
}

void sim_scheduler_run_blocking_month(void) {
    simulation_month_run_blocking();
}
