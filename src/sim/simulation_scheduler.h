#ifndef WORLD_SIM_SIMULATION_SCHEDULER_H
#define WORLD_SIM_SIMULATION_SCHEDULER_H

void sim_scheduler_reset(void);
void sim_scheduler_request_month(void);
int sim_scheduler_has_pending_work(void);
int sim_scheduler_run_budget(int work_units);
int sim_scheduler_pending_months(void);
int sim_scheduler_take_completed_months(void);
void sim_scheduler_run_blocking_month(void);

#endif
