#ifndef WORLD_SIM_MOUNTAIN_GEN_H
#define WORLD_SIM_MOUNTAIN_GEN_H

#include "world/world_gen_context.h"

int world_gen_apply_mountains(WorldGenContext *context);
int world_mountain_chain_count(void);
int world_mountain_average_chain_length(void);

#endif
