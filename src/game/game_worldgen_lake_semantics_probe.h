#ifndef WORLD_SIM_GAME_WORLDGEN_LAKE_SEMANTICS_PROBE_H
#define WORLD_SIM_GAME_WORLDGEN_LAKE_SEMANTICS_PROBE_H

#include <stdio.h>

#include "world/world_gen.h"

int game_worldgen_lake_semantics_probe_check(FILE *file, const char *label,
                                             const WorldGenContext *context);

#endif
