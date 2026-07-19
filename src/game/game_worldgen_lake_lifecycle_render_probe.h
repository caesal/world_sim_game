#ifndef WORLD_SIM_GAME_WORLDGEN_LAKE_LIFECYCLE_RENDER_PROBE_H
#define WORLD_SIM_GAME_WORLDGEN_LAKE_LIFECYCLE_RENDER_PROBE_H

#include "core/world_types.h"
#include "world/world_gen_context.h"

int game_worldgen_lake_lifecycle_render_check(
    const WorldGenContext *context, const RiverPath *inlet_path,
    const RiverPath *outlet_path, int inlet_receiver, int outlet,
    int *semantic_anchors);

#endif
