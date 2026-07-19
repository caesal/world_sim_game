#ifndef WORLD_SIM_WORLD_GEN_ELEVATION_H
#define WORLD_SIM_WORLD_GEN_ELEVATION_H

#include "world/world_gen_context.h"

int world_gen_build_elevation_and_mask(WorldGenContext *context);
void world_gen_finalize_elevation(WorldGenContext *context);

#endif
