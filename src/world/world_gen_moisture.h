#ifndef WORLD_SIM_WORLD_GEN_MOISTURE_H
#define WORLD_SIM_WORLD_GEN_MOISTURE_H

#include <stdint.h>

#include "world/world_gen_context.h"

typedef struct {
    int tile_count;
    uint32_t climate_seed;
    int solved_land_tiles;
    int cycle_tiles;
    int cycle_count;
    int cycle_iterations;
    int max_cycle_iterations;
    int unconverged_cycles;
    int ocean_reached_land_tiles;
    int ocean_reached_beyond_20;
    int max_ocean_chain_length;
    int diffusion_passes;
    int advection_rounds;
    int subtile_advection_samples;
    int subtile_lateral_samples;
    uint64_t advection_weight_total;
    uint64_t lateral_mix_weight_total;
    uint64_t orographic_precipitation_total;
    uint64_t lee_drying_total;
} WorldGenMoistureDiagnostics;

int world_gen_transport_moisture(WorldGenContext *context, int moisture_seed);
const WorldGenMoistureDiagnostics *world_gen_moisture_last_diagnostics(void);

#endif
