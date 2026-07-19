#ifndef WORLD_SIM_WORLD_GEN_CONTEXT_H
#define WORLD_SIM_WORLD_GEN_CONTEXT_H

#include <stddef.h>
#include <stdint.h>

#include "core/constants.h"
#include "world/world_gen.h"
#include "world/world_gen_land_mask.h"
#include "world/river_types.h"

typedef enum {
    WORLD_GEN_PHASE_ELEVATION = 0,
    WORLD_GEN_PHASE_MOUNTAIN,
    WORLD_GEN_PHASE_CLIMATE,
    WORLD_GEN_PHASE_HYDROLOGY,
    WORLD_GEN_PHASE_CLASSIFY,
    WORLD_GEN_PHASE_DOWNSTREAM,
    WORLD_GEN_PHASE_COUNT
} WorldGenPhaseId;

enum {
    WORLD_GEN_RIVER_CHANNEL = 1,
    WORLD_GEN_RIVER_LAKE = 2,
    WORLD_GEN_RIVER_MOUTH = 4,
    WORLD_GEN_RIVER_DELTA = 8,
    WORLD_GEN_RIVER_CONFLUENCE = 16,
    WORLD_GEN_RIVER_CLOSED_BASIN = 32,
    WORLD_GEN_RIVER_SOURCE = 64,
    WORLD_GEN_RIVER_DISTRIBUTARY = 128,
    WORLD_GEN_RIVER_SALT_LAKE = 256
};

struct WorldGenContext {
    int width;
    int height;
    int tile_count;
    int sea_level;
    int topological_count;
    WorldGenConfig config;
    uint32_t master_seed;
    uint32_t phase_seed[WORLD_GEN_PHASE_COUNT];
    uint64_t hydrology_token;
    uint64_t staged_river_token;
    void *staged_river_paths;
    int staged_river_path_count;
    int staged_river_paths_required;
    RiverGenerationDiagnostics staged_river_diagnostics;
    int staged_river_diagnostics_valid;
    size_t allocated_bytes;
    WorldGenLandMaskDiagnostics land_mask_diagnostics;

    int16_t *base_elevation;
    int16_t *elevation;
    int16_t *relative_altitude;
    int16_t *slope;
    int16_t *curvature;
    int16_t *mountain_uplift;
    int16_t *ocean_distance;
    int16_t *moisture;
    int16_t *temperature;
    int16_t *precipitation;

    uint8_t *land_mask;
    uint8_t *coastal_lowland_hint;
    uint8_t *wind_direction16;
    uint8_t *wind_speed;
    uint8_t *geography;
    uint8_t *climate;
    uint8_t *ecology;
    uint8_t *resource;
    uint8_t *resource_variation;
    uint8_t *river_order;
    uint16_t *river_flags;
    uint8_t *soil_fertility;

    uint16_t *river_width;
    int32_t *drainage_receiver;
    int32_t *drainage_basin;
    int32_t *topological_order;
    uint32_t *runoff;
    uint32_t *river_flow;
    uint32_t *upstream_count;

    int32_t *scratch_a;
    int32_t *scratch_b;
};

int world_gen_context_create(WorldGenContext *context, const WorldGenConfig *config,
                             int width, int height, uint32_t master_seed);
void world_gen_context_destroy(WorldGenContext *context);
int world_gen_context_index(const WorldGenContext *context, int x, int y);
int world_gen_context_in_bounds(const WorldGenContext *context, int x, int y);

#endif
