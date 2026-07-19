#ifndef WORLD_SIM_MAP_SHORE_COLOR_CACHE_H
#define WORLD_SIM_MAP_SHORE_COLOR_CACHE_H

#include "core/render_snapshot.h"

#include <stdint.h>

typedef struct {
    uint64_t rebuilds;
    uint64_t tile_scans;
    uint64_t retained_bytes;
} MapShoreColorCacheStats;

int map_shore_color_cache_prepare(const RenderSnapshot *snapshot);
const SnapshotTile *map_shore_color_cache_island_donor(
    const RenderSnapshot *snapshot, const SnapshotTile *tile);
const MapShoreColorCacheStats *map_shore_color_cache_stats(void);
void map_shore_color_cache_invalidate(void);

#endif
