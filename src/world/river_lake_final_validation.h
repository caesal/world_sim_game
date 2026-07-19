#ifndef WORLD_SIM_RIVER_LAKE_FINAL_VALIDATION_H
#define WORLD_SIM_RIVER_LAKE_FINAL_VALIDATION_H

#include "world/river_state.h"

typedef struct {
    int components;
    int open_components;
    int closed_components;
    int invalid_components;
    int model_errors;
    int topology_errors;
    int flag_errors;
} RiverLakeFinalValidationReport;

int river_lake_final_validation_run(
    RiverGenerationState *state, RiverLakeFinalValidationReport *report);

#endif
