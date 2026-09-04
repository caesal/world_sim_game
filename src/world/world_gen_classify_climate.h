#ifndef WORLD_SIM_WORLD_GEN_CLASSIFY_CLIMATE_H
#define WORLD_SIM_WORLD_GEN_CLASSIFY_CLIMATE_H

#include "core/world_types.h"
#include "world/world_gen_context.h"

Climate world_gen_classify_climate(const WorldGenContext *context, int index);
int world_gen_classify_climate_arid_eligible(int elevation, int temperature);
int world_gen_classify_climate_refreshes_after_hydrology(
    const WorldGenContext *context, int index);

#endif
