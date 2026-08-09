#ifndef WORLD_SIM_RENDER_OCEAN_DECORATION_WATER_H
#define WORLD_SIM_RENDER_OCEAN_DECORATION_WATER_H

#include "core/render_snapshot.h"
#include <stdint.h>

typedef struct {
    uint64_t motif_clearance_calls;
    uint64_t motif_clearance_tile_visits;
} OceanDecorationWaterDebugStats;

extern OceanDecorationWaterDebugStats ocean_decoration_water_debug_stats;

int ocean_decoration_water_tile(const RenderSnapshot *snapshot, int x, int y);
int ocean_decoration_deep_ocean_tile(const RenderSnapshot *snapshot, int x, int y);
int ocean_decoration_deep_clearance(const RenderSnapshot *snapshot, int x, int y,
                                    int max_radius);
int ocean_decoration_motif_clearance(const RenderSnapshot *snapshot, int x, int y,
                                     unsigned char type, int max_radius);
void ocean_decoration_water_reset_debug(void);

#endif
