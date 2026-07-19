#ifndef WORLD_SIM_RENDER_OCEAN_COVERAGE_H
#define WORLD_SIM_RENDER_OCEAN_COVERAGE_H

#include "core/render_snapshot.h"
#include "ui/ui_types.h"

#include <stdint.h>

typedef struct {
    int field_rebuilds;
    uint64_t field_tile_scans;
    uint64_t field_raster_samples;
    uint64_t field_retained_bytes;
    int ocean_tiles;
    int lake_tiles;
    int scale;
} RenderOceanCoverageStats;

int render_ocean_coverage_prepare_field(const RenderSnapshot *snapshot);
int render_ocean_coverage_pixel_is_ocean(const RenderSnapshot *snapshot,
                                         MapLayout layout, int px, int py);
const RenderOceanCoverageStats *render_ocean_coverage_stats(void);
void render_ocean_coverage_invalidate(void);
void render_ocean_coverage_reset_debug(void);

#endif
