#ifndef WORLD_SIM_RENDER_STATIC_MAP_CACHE_INTERNAL_H
#define WORLD_SIM_RENDER_STATIC_MAP_CACHE_INTERNAL_H

#include "core/render_snapshot.h"
#include "render/render_common.h"

#define MAP_LAYER_CACHE_SCALE 2
#define MAP_TRANSPARENT_KEY RGB(255, 0, 255)

typedef struct {
    HDC dc;
    HBITMAP bitmap;
    HBITMAP old_bitmap;
    unsigned int *pixels;
    int width;
    int height;
    int display;
    int revision;
    int valid;
    int complete;
} MapLayerCache;

void render_static_map_cache_build_border_pixels(MapLayerCache *cache,
                                                 const RenderSnapshot *snapshot);
void render_static_map_cache_build_fill_pixels(MapLayerCache *cache,
                                               const RenderSnapshot *snapshot,
                                               int live);

#endif
