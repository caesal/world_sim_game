#ifndef WORLD_SIM_RENDER_OCEAN_STATIC_MASK_H
#define WORLD_SIM_RENDER_OCEAN_STATIC_MASK_H

#include "core/render_snapshot.h"

#include <stdint.h>

typedef struct {
    uint64_t applications;
    uint64_t pixel_scans;
    uint64_t coverage_samples;
    uint64_t transparent_ocean_pixels;
    uint64_t partial_ocean_pixels;
    uint64_t opaque_pixels;
    uint64_t lake_opaque_pixels;
    uint64_t failures;
    int last_width;
    int last_height;
} RenderOceanStaticMaskStats;

extern RenderOceanStaticMaskStats ocean_static_mask_debug_stats;

int render_ocean_static_mask_apply(uint32_t *pixels, int width, int height,
                                   const RenderSnapshot *snapshot);
const RenderOceanStaticMaskStats *render_ocean_static_mask_stats(void);
void render_ocean_static_mask_reset_debug(void);

#endif
