#ifndef WORLD_SIM_RENDER_WATER_COVERAGE_H
#define WORLD_SIM_RENDER_WATER_COVERAGE_H

#include "core/render_snapshot.h"

#include <stdint.h>

typedef struct {
    uint64_t rebuilds;
    uint64_t source_scans;
    uint64_t raster_samples;
    uint64_t retained_bytes;
    uint64_t lake_diagonal_cells;
    uint64_t lake_land_pixels_rejected;
    uint64_t ocean_land_pixels_rejected;
    uint64_t ocean_overlap_pixels_removed;
    uint64_t coast_removed_ocean_tiles;
    uint64_t coast_filled_land_tiles;
    uint64_t coast_protected_land_tiles;
    uint64_t coast_protected_lake_neighbor_tiles;
    uint64_t coast_protected_marine_tiles;
    uint64_t coast_protected_marine_components;
    uint64_t coast_cleanup_tiles;
    uint64_t coast_presentation_hash;
    uint64_t coast_presentation_transient_bytes;
    int ocean_tiles;
    int lake_tiles;
} RenderWaterCoverageStats;

int render_water_coverage_prepare(const RenderSnapshot *snapshot);
unsigned char render_water_coverage_ocean_alpha(int x, int y);
unsigned char render_water_coverage_lake_alpha(int x, int y);
unsigned char render_water_coverage_total_alpha(int x, int y);
int render_water_coverage_width(void);
int render_water_coverage_height(void);
int render_water_coverage_scale(void);
const RenderWaterCoverageStats *render_water_coverage_stats(void);
void render_water_coverage_invalidate(void);
void render_water_coverage_reset_debug(void);

#endif
