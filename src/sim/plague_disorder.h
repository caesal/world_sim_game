#ifndef WORLD_SIM_PLAGUE_DISORDER_H
#define WORLD_SIM_PLAGUE_DISORDER_H

void plague_disorder_reset(void);
void plague_disorder_refresh_targets(int absolute_month);
int plague_disorder_target(int civ_id);
int plague_disorder_step(int civ_id, int current, int *out_decay);
void plague_disorder_max_current_target(int *out_current, int *out_target);

#endif
