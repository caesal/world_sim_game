#ifndef WORLD_SIM_RENDER_WATER_COAST_SMOOTHING_H
#define WORLD_SIM_RENDER_WATER_COAST_SMOOTHING_H

#include <stdint.h>

enum {
    WATER_COAST_SMOOTH_RADIUS = 1,
    WATER_COAST_SMOOTH_MAX_RADIUS = 4,
    WATER_COAST_SMOOTH_MAX_PASSES = 4,
    WATER_COAST_SMOOTH_CLUSTER_RADIUS = 20,
    WATER_COAST_SMOOTH_MIN_CLUSTER = 3,
    WATER_COAST_SMOOTH_COMB_MIN_AREA = 12,
    WATER_COAST_SMOOTH_COMB_MIN_CONTACT_GROUPS = 3,
    WATER_COAST_SMOOTH_COMB_MAX_FILL_PERCENT = 75
};

typedef struct {
    uint64_t thin_components;
    uint64_t one_ended_components;
    uint64_t preserved_components;
    uint64_t sparse_mesh_components;
    uint64_t sparse_mesh_tiles;
    uint64_t regularized_components;
    uint64_t regularized_tiles;
    uint64_t candidate_comparisons;
    uint64_t transient_bytes;
    int pass_count;
    int converged;
} RenderWaterCoastSmoothingMetrics;

int render_water_coast_smoothing_build(
    const unsigned char *ocean_mask, int width, int height,
    unsigned char *suppressed_tiles,
    RenderWaterCoastSmoothingMetrics *metrics);

#endif
