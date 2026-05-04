#ifndef WORLD_SIM_WORLD_GEN_H
#define WORLD_SIM_WORLD_GEN_H

#include "core/game_types.h"

typedef struct {
    int ocean;
    int continent;
    int relief;
    int moisture;
    int drought;
    int vegetation;
    int bias_forest;
    int bias_desert;
    int bias_mountain;
    int bias_wetland;
} WorldGenConfig;

void generate_world_with_config(const WorldGenConfig *config);

#endif
