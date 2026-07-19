#ifndef WORLD_SIM_RENDER_WATER_COVERAGE_RASTER_H
#define WORLD_SIM_RENDER_WATER_COVERAGE_RASTER_H

#include <stdint.h>

enum {
    RENDER_WATER_CATEGORY_LAND,
    RENDER_WATER_CATEGORY_OCEAN,
    RENDER_WATER_CATEGORY_LAKE
};

typedef struct {
    uint64_t raster_samples;
    uint64_t lake_diagonal_cells;
    uint64_t lake_land_pixels_rejected;
    uint64_t ocean_land_pixels_rejected;
    uint64_t ocean_overlap_pixels_removed;
} RenderWaterCoverageRasterMetrics;

int render_water_coverage_rasterize(
    const unsigned char *source, int map_w, int map_h,
    int scale, int subsamples, unsigned char *ocean_alpha,
    unsigned char *lake_alpha, unsigned char *scratch,
    RenderWaterCoverageRasterMetrics *metrics);

#endif
