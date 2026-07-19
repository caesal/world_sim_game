#ifndef WORLD_SIM_RENDER_OCEAN_DECORATION_CACHE_H
#define WORLD_SIM_RENDER_OCEAN_DECORATION_CACHE_H

#include "render/render_layer_cache.h"

typedef struct {
    int hits;
    int misses;
    int evictions;
    int persistent_bitmaps;
    int persistent_dcs;
    unsigned long long persistent_bitmap_bytes;
} OceanDecorationCacheStats;

LayerCache *ocean_decoration_cache_find(RECT client, MapLayout layout,
                                        unsigned int key);
LayerCache *ocean_decoration_cache_prepare(HDC hdc, RECT client,
                                           MapLayout layout);
void ocean_decoration_cache_invalidate(void);
void ocean_decoration_cache_reset_debug(void);
const OceanDecorationCacheStats *ocean_decoration_cache_stats(void);

#endif
