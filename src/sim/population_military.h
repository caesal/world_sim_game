#ifndef WORLD_SIM_POPULATION_MILITARY_H
#define WORLD_SIM_POPULATION_MILITARY_H

#include "core/sim_types.h"

int population_military_base_soldiers_from_summary(PopulationSummary summary);
int population_military_base_soldiers_for_civ(int civ_id);
int population_military_current_soldiers_for_civ(int civ_id,
                                                int active_war_casualties);
void population_military_split_current_soldiers(PopulationSummary summary,
                                                int current_soldiers,
                                                int *male, int *female);
int population_military_apply_casualties(int civ_id, int casualties);

#endif
