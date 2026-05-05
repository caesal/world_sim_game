#include "simulation_scheduler.h"

#include "sim/simulation_month.h"

static int pending_months = 0;
static SimulationMonthState active_month;

void sim_scheduler_reset(void) {
    pending_months = 0;
    active_month.active = 0;
}

void sim_scheduler_request_month(void) {
    if (pending_months < 1) pending_months++;
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
        work_units--;
    }
    return did_work;
}

void sim_scheduler_run_blocking_month(void) {
    simulation_month_run_blocking();
}
