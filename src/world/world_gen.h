#ifndef WORLD_SIM_WORLD_GEN_H
#define WORLD_SIM_WORLD_GEN_H

#include "core/constants.h"

enum {
    WORLD_GEN_DEFAULT_OCEAN = 50,
    WORLD_GEN_DEFAULT_CONTINENT = 50,
    WORLD_GEN_DEFAULT_RELIEF = 50,
    WORLD_GEN_DEFAULT_MOISTURE = 50,
    WORLD_GEN_DEFAULT_DROUGHT = 50,
    WORLD_GEN_DEFAULT_VEGETATION = 50,
    WORLD_GEN_DEFAULT_BIAS_FOREST = 50,
    WORLD_GEN_DEFAULT_BIAS_DESERT = 50,
    WORLD_GEN_DEFAULT_BIAS_MOUNTAIN = 50,
    WORLD_GEN_DEFAULT_BIAS_WETLAND = 50
};

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
    unsigned int seed;
    int random_seed;
} WorldGenConfig;

extern const WorldGenConfig DEFAULT_WORLD_GEN_CONFIG;

void generate_world_with_config(const WorldGenConfig *config);

#endif
