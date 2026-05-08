#ifndef WORLD_SIM_SIMULATION_SCHEDULER_H
#define WORLD_SIM_SIMULATION_SCHEDULER_H

void sim_scheduler_reset(void);
int sim_scheduler_can_accept_month(void);
int sim_scheduler_request_month(void);
int sim_scheduler_has_pending_work(void);
int sim_scheduler_run_budget(int work_units);
int sim_scheduler_run_for_ms(int budget_ms);
int sim_scheduler_pending_months(void);
int sim_scheduler_pending_month_cap(void);
int sim_scheduler_last_step_ms(void);
void sim_scheduler_trim_pending_months(int max_total_pending);
int sim_scheduler_take_completed_months(void);
void sim_scheduler_run_blocking_month(void);

#endif
