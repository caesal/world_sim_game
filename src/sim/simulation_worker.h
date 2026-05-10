#ifndef WORLD_SIM_SIMULATION_WORKER_H
#define WORLD_SIM_SIMULATION_WORKER_H

void simulation_worker_start(void);
void simulation_worker_shutdown(void);
void simulation_worker_reset_scheduler(void);
int simulation_worker_actual_ms_per_month(void);
int simulation_worker_pending_months(void);
int simulation_worker_last_budget_ms(void);
int simulation_worker_last_used_ms(void);
int simulation_worker_overloaded(void);
int simulation_worker_snapshot_age_ms(void);
int simulation_worker_take_visual_tick(void);
const char *simulation_worker_status(void);

#endif
