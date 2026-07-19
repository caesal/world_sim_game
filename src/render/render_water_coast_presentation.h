#ifndef WORLD_SIM_RENDER_WATER_COAST_PRESENTATION_H
#define WORLD_SIM_RENDER_WATER_COAST_PRESENTATION_H

#include "core/render_snapshot.h"
#include "render/render_water_coast_smoothing.h"

#include <stdint.h>

typedef enum {
    WATER_COAST_PRESENTATION_LAND = 0,
    WATER_COAST_PRESENTATION_OCEAN,
    WATER_COAST_PRESENTATION_LAKE
} WaterCoastPresentationCategory;

enum {
    WATER_COAST_PRESENTATION_OCEAN_CLEANUP_PASSES = 4,
    WATER_COAST_PRESENTATION_MARINE_ANCHOR_RADIUS = 0
};

typedef struct {
    RenderWaterCoastSmoothingMetrics ocean_fringe;
    RenderWaterCoastSmoothingMetrics land_fringe;
    RenderWaterCoastSmoothingMetrics ocean_cleanup;
    uint64_t semantic_ocean_tiles;
    uint64_t semantic_land_tiles;
    uint64_t semantic_lake_tiles;
    uint64_t removed_ocean_tiles;
    uint64_t filled_land_tiles;
    uint64_t protected_land_tiles;
    uint64_t protected_lake_neighbor_tiles;
    uint64_t protected_marine_tiles;
    uint64_t initial_protected_marine_components;
    uint64_t initial_protected_marine_tiles;
    uint64_t ocean_cleanup_passes;
    uint64_t ocean_cleanup_tiles;
    uint64_t ocean_cleanup_semantic_candidates;
    uint64_t ocean_cleanup_filled_candidates;
    uint64_t ocean_cleanup_protected_components;
    uint64_t ocean_cleanup_protected_tiles;
    uint64_t presentation_hash;
    uint64_t transient_bytes;
} RenderWaterCoastPresentationMetrics;

int render_water_coast_presentation_build(
    const RenderSnapshot *snapshot, unsigned char *categories,
    RenderWaterCoastPresentationMetrics *metrics);

int render_water_coast_presentation_build_marine_protection(
    const RenderSnapshot *snapshot, unsigned char *protected_ocean,
    uint64_t *protected_tiles);

int render_water_coast_presentation_find_shallow_entry(
    const RenderSnapshot *snapshot, int land_x, int land_y,
    int *out_x, int *out_y);

int render_water_coast_presentation_preserve_anchored_components(
    const unsigned char *ocean_mask, const unsigned char *protected_ocean,
    int width, int height,
    unsigned char *suppressed_tiles, uint64_t *protected_components,
    uint64_t *protected_tiles, uint64_t *transient_bytes);

#endif
