#ifndef WORLD_SIM_WORLD_GEN_CLASSIFY_H
#define WORLD_SIM_WORLD_GEN_CLASSIFY_H

#include "core/world_types.h"
#include "world/world_gen_context.h"

Geography world_gen_classify_underlying_land(const WorldGenContext *context,
                                              int index, int x, int y,
                                              Climate climate);
int world_gen_classify_macro_climate(WorldGenContext *context);
int world_gen_classify_final(WorldGenContext *context);

#endif
