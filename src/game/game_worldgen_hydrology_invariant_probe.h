#ifndef WORLD_SIM_GAME_WORLDGEN_HYDROLOGY_INVARIANT_PROBE_H
#define WORLD_SIM_GAME_WORLDGEN_HYDROLOGY_INVARIANT_PROBE_H

#include <stdio.h>

#include "world/river_types.h"
#include "world/world_gen_context.h"

int game_worldgen_hydrology_invariant_probe_check(
    FILE *file, const char *label, const WorldGenContext *context,
    const RiverNetworkView *view);

#endif
