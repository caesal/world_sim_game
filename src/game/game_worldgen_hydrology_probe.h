#ifndef WORLD_SIM_GAME_WORLDGEN_HYDROLOGY_PROBE_H
#define WORLD_SIM_GAME_WORLDGEN_HYDROLOGY_PROBE_H

#include <stdio.h>

#include "world/world_gen.h"

void game_worldgen_hydrology_probe_reset(void);
int game_worldgen_hydrology_probe_check_context(FILE *file, const char *label,
                                                const WorldGenContext *context,
                                                int check_decay);
int game_worldgen_hydrology_probe_finish(FILE *file);

#endif
