#ifndef WORLD_SIM_GAME_WORLDGEN_PHYSICAL_PROBE_H
#define WORLD_SIM_GAME_WORLDGEN_PHYSICAL_PROBE_H

#include <stdio.h>

#include "world/world_gen.h"

int game_worldgen_physical_probe_run_matrix(FILE *file, const WorldGenConfig *base_config);
int game_worldgen_physical_probe_check_context(FILE *file, const char *label,
                                               const WorldGenContext *context,
                                               int require_mountain_contrast,
                                               int check_phy20);

#endif
