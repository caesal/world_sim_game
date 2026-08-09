#ifndef WORLD_SIM_RENDER_OCEAN_TEXTURE_H
#define WORLD_SIM_RENDER_OCEAN_TEXTURE_H

#include "render/render_layer_cache.h"
#include <stdint.h>
#include <windows.h>

typedef struct {
    uint64_t ensure_calls;
    uint64_t cache_hits;
    uint64_t rebuilds;
    uint64_t allocation_rebuilds;
    uint64_t copy_calls;
    uint64_t copy_failures;
    uint64_t copied_pixels;
    uint64_t resample_calls;
    uint64_t stretchblt_calls;
    uint64_t generation;
    uint64_t persistent_bytes;
    uintptr_t bitmap_identity;
    uintptr_t dc_identity;
    unsigned int identity;
    unsigned int source_identity;
    int persistent_bitmaps;
    int persistent_dcs;
    int valid;
    int width;
    int height;
    int tile_px;
    int phase_x;
    int phase_y;
    int texture_loaded;
} OceanTextureDebugStats;

extern OceanTextureDebugStats ocean_texture_debug_stats;

int render_ocean_texture_ensure(HDC hdc, RECT client);
int render_ocean_texture_copy(HDC hdc, RECT rect);
int render_ocean_texture_score(void);
RenderLayerCacheMemory render_ocean_texture_memory(void);
OceanTextureDebugStats render_ocean_texture_debug_stats(void);
uint64_t render_ocean_texture_generation(void);
unsigned int render_ocean_texture_identity(void);
/* Resets counters only; the cached pixels and monotonic generation remain. */
void render_ocean_texture_reset_debug(void);
/* Invalidates content without releasing its reusable client-sized surface. */
void render_ocean_texture_invalidate(void);

#endif
