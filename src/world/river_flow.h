#ifndef WORLD_SIM_RIVER_FLOW_H
#define WORLD_SIM_RIVER_FLOW_H

#include "world/river_state.h"

int river_flow_build(RiverGenerationState *state);
int river_flow_build_segments(RiverGenerationState *state);

#endif
