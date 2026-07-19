#ifndef WORLD_SIM_GAME_WORLDGEN_HYDROLOGY_DIRECTION_PROBE_H
#define WORLD_SIM_GAME_WORLDGEN_HYDROLOGY_DIRECTION_PROBE_H

#include <stdio.h>

#include "world/river_types.h"
#include "world/world_gen_context.h"

void game_worldgen_hydrology_direction_probe_reset(void);
int game_worldgen_hydrology_direction_probe_check(
    FILE *file, const char *label, const WorldGenContext *context,
    const RiverNetworkView *view);
int game_worldgen_hydrology_direction_probe_finish(FILE *file);

#endif
