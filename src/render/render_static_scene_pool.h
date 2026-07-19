#ifndef WORLD_SIM_RENDER_STATIC_SCENE_POOL_H
#define WORLD_SIM_RENDER_STATIC_SCENE_POOL_H

#include "render/render_layer_cache.h"

enum { RENDER_STATIC_SCENE_POOL_CAPACITY = 4 };

typedef struct {
    LayerCache *cache;
    unsigned int boundary_key;
    int current;
    int safe;
    int full;
} RenderStaticScenePoolView;

typedef struct {
    int hits;
    int misses;
    int publishes;
    int evictions;
    int persistent_bitmaps;
    int persistent_dcs;
    unsigned long long persistent_bitmap_bytes;
} RenderStaticScenePoolStats;

int render_static_scene_pool_find_exact(RECT client, MapLayout layout,
                                        unsigned int key, int display,
                                        RenderStaticScenePoolView *view);
int render_static_scene_pool_find_presentable(RECT client, int display,
                                              RenderStaticScenePoolView *view);
int render_static_scene_pool_publish(HDC hdc, RECT client, MapLayout layout,
                                     const LayerCache *source, unsigned int key,
                                     unsigned int boundary_key, int display,
                                     int current, int safe, int full,
                                     RenderStaticScenePoolView *view);
void render_static_scene_pool_invalidate(void);
void render_static_scene_pool_reset_debug(void);
const RenderStaticScenePoolStats *render_static_scene_pool_stats(void);

#endif
