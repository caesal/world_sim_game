#ifndef WORLD_SIM_PLAGUE_H
#define WORLD_SIM_PLAGUE_H

#include "sim/plague_engine.h"

void plague_reset(void);
void plague_after_restore(void);
void plague_update_month(void);
int plague_update_month_step(PlagueUpdateState *state, int batch_size);

int plague_city_active(int city_id);
int plague_city_severity(int city_id);
int plague_city_deaths_total(int city_id);
int plague_city_months_left(int city_id);
int plague_city_reinfection_cooldown_months(int city_id);
int plague_tile_severity(int x, int y);
int plague_civ_active_count(int civ_id);
int plague_active_for_civ(int civ_id);
int plague_civ_pressure(int civ_id);
int plague_civ_deaths_total(int civ_id);
int plague_civ_peak_severity(int civ_id);
int plague_civ_months_left(int civ_id);
int plague_random_immunity_months(int civ_id);
int plague_random_immunity_civ_count(void);
int plague_global_active_state(int *first_city_id);
int plague_route_exposure(int route_id);

#endif
