#ifndef WORLD_SIM_POPULATION_H
#define WORLD_SIM_POPULATION_H

#include "core/game_types.h"

void population_init_city(int city_id, int total_population);
int population_city_total(int city_id);
PopulationSummary population_city_summary(int city_id);
PopulationSummary population_country_summary(int civ_id);
int population_recruitable_for_civ(int civ_id);
int population_pressure_for_civ(int civ_id);
void population_sync_city(int city_id);
void population_sync_all(void);
void population_mark_dirty(void);
void population_update_month(void);
int population_migrate_between_cities(int from_city, int to_city, int amount);
int population_apply_casualties(int civ_id, int casualties);
int population_apply_city_plague(int city_id, int severity);
int population_apply_plague(int civ_id, int severity);

#endif
