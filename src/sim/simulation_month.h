#ifndef WORLD_SIM_SIMULATION_MONTH_H
#define WORLD_SIM_SIMULATION_MONTH_H

#include "core/game_types.h"

typedef struct {
    int active;
    int phase;
    int resource_y;
    int expansion_civ;
    int city_count_before_expansion;
    int territory_revision_before_expansion;
    int resource_scores[MAX_CIVS];
    int resource_totals[MAX_CIVS];
    int resource_counts[MAX_CIVS];
    char log[512];
} SimulationMonthState;

int simulation_month_begin(SimulationMonthState *state);
int simulation_month_run_next(SimulationMonthState *state);
int simulation_month_is_done(const SimulationMonthState *state);
void simulation_month_run_blocking(void);

#endif
