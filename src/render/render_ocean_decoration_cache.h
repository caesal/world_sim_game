#ifndef WORLD_SIM_RENDER_OCEAN_DECORATION_CACHE_H
#define WORLD_SIM_RENDER_OCEAN_DECORATION_CACHE_H

#include <stdint.h>
#include <windows.h>

typedef enum {
    OCEAN_DECORATION_LAYER_EXTERIOR = 0,
    OCEAN_DECORATION_LAYER_INTERIOR = 1
} OceanDecorationLayerKind;

typedef struct {
    HDC dc;
    HBITMAP bitmap;
    HBITMAP old_bitmap;
    uint32_t *pixels;
    int width;
    int height;
    unsigned int key;
    unsigned int surface_identity;
    int valid;
} OceanDecorationLayer;

typedef struct {
    int hits;
    int misses;
    int evictions;
    int persistent_bitmaps;
    int persistent_dcs;
    uint64_t persistent_bitmap_bytes;
    uint64_t exterior_rebuilds;
    uint64_t interior_rebuilds;
    uint64_t exterior_presents;
    uint64_t interior_presents;
    uint64_t exterior_allocations;
    uint64_t interior_allocations;
    uint64_t exterior_clears;
    uint64_t interior_clears;
    uint64_t exterior_cleared_pixels;
    uint64_t interior_cleared_pixels;
    uint64_t exterior_retained_bytes;
    uint64_t interior_retained_bytes;
    unsigned int exterior_key;
    unsigned int interior_key;
    unsigned int exterior_surface_identity;
    unsigned int interior_surface_identity;
} OceanDecorationCacheStats;

OceanDecorationLayer *ocean_decoration_cache_find(
    OceanDecorationLayerKind kind, int width, int height, unsigned int key);
OceanDecorationLayer *ocean_decoration_cache_prepare(
    HDC hdc, OceanDecorationLayerKind kind, int width, int height);
int ocean_decoration_cache_clear(OceanDecorationLayerKind kind,
                                 OceanDecorationLayer *layer);
void ocean_decoration_cache_commit(OceanDecorationLayerKind kind,
                                   OceanDecorationLayer *layer,
                                   unsigned int key);
void ocean_decoration_cache_note_present(OceanDecorationLayerKind kind);
void ocean_decoration_cache_invalidate(void);
void ocean_decoration_cache_reset_debug(void);
const OceanDecorationCacheStats *ocean_decoration_cache_stats(void);

#endif
