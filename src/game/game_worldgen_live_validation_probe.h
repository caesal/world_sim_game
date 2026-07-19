#ifndef WORLD_SIM_GAME_WORLDGEN_LIVE_VALIDATION_PROBE_H
#define WORLD_SIM_GAME_WORLDGEN_LIVE_VALIDATION_PROBE_H

#include <stdio.h>

#include "world/world_gen.h"

int game_worldgen_live_validation_probe_run(FILE *file,
                                            const WorldGenConfig *expected_config);

#endif
