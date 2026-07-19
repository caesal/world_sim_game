#ifndef WORLD_SIM_WORLD_GEN_LAND_MASK_H
#define WORLD_SIM_WORLD_GEN_LAND_MASK_H

#include "world/world_gen.h"

typedef enum {
    WORLD_GEN_LAND_MASK_OK = 0,
    WORLD_GEN_LAND_MASK_INVALID_CONTEXT,
    WORLD_GEN_LAND_MASK_INVALID_TARGET,
    WORLD_GEN_LAND_MASK_GROWTH_STALLED,
    WORLD_GEN_LAND_MASK_SHRINK_STALLED,
    WORLD_GEN_LAND_MASK_TARGET_DRIFT,
    WORLD_GEN_LAND_MASK_SEMANTIC_ARTIFACT
} WorldGenLandMaskFailure;

typedef struct {
    int target_land_tiles;
    int initial_land_tiles;
    int final_land_tiles;
    int threshold_elevation;
    int threshold_candidates;
    int threshold_selected;
    int frontier_added;
    int frontier_removed;
    int frontier_resolutions;
    int forced_frontier_seeds;
    int coastal_lowland_tiles;
    int land_components;
    int water_components;
    int lattice_cells;
    int comb_cells;
    int mesh_cells;
    int tendril_cells;
    int threshold_lattice_cells;
    int threshold_comb_cells;
    int threshold_mesh_cells;
    int threshold_tendril_cells;
    int topology_errors;
    int target_drift;
    uint64_t mask_hash;
    WorldGenLandMaskFailure failure;
} WorldGenLandMaskDiagnostics;

int world_gen_land_mask_build(WorldGenContext *context);
int world_gen_land_mask_target_tiles(int ocean_amount, int tile_count);
void world_gen_land_mask_refresh_ocean_distance(WorldGenContext *context);
void world_gen_land_mask_measure(WorldGenContext *context);
const WorldGenLandMaskDiagnostics *world_gen_land_mask_diagnostics(
    const WorldGenContext *context);

#endif
