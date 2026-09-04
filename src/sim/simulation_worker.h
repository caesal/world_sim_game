#ifndef WORLD_SIM_SIMULATION_WORKER_H
#define WORLD_SIM_SIMULATION_WORKER_H

void simulation_worker_start(void);
int simulation_worker_quiesce(void);
void simulation_worker_shutdown(void);
void simulation_worker_reset_scheduler(void);
void simulation_worker_request_pause(void);
void simulation_worker_request_resume(void);
int simulation_worker_pause_in_progress(void);
int simulation_worker_pause_settled(void);
int simulation_worker_actual_ms_per_month(void);
int simulation_worker_pending_months(void);
int simulation_worker_last_budget_ms(void);
int simulation_worker_last_used_ms(void);
int simulation_worker_overloaded(void);
int simulation_worker_snapshot_age_ms(void);
int simulation_worker_take_visual_month(int *out_year, int *out_month);
int simulation_worker_visual_backlog(void);
int simulation_worker_visual_coalesced_months(void);
int simulation_worker_visual_max_backlog(void);
int simulation_worker_visual_presented_total(void);
int simulation_worker_visual_dropped_months(void);
int simulation_worker_presentation_throttled(void);
const char *simulation_worker_status(void);
void simulation_worker_debug_reset_presentation_queue(void);
int simulation_worker_debug_enqueue_completed_month(int completed_year, int completed_month);

#endif
