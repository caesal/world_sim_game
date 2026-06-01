#ifndef WORLD_SIM_RENDER_LAYER_CACHE_H
#define WORLD_SIM_RENDER_LAYER_CACHE_H

#include "ui/ui_types.h"
#include <windows.h>

typedef struct {
    HDC dc;
    HBITMAP bitmap;
    HBITMAP old_bitmap;
    int width;
    int height;
    int map_x;
    int map_y;
    int draw_w;
    int draw_h;
    int side_w;
    int display;
    int revision;
    unsigned int key;
    int valid;
} LayerCache;

unsigned int render_layer_mix_key(unsigned int key, int value);
unsigned int render_layer_layout_key(RECT client, MapLayout layout, int side_w, int display);
int render_layer_cache_ensure(HDC hdc, LayerCache *cache, RECT client, MapLayout layout,
                              int side_w, int display);
int render_layer_cache_matches(const LayerCache *cache, RECT client, MapLayout layout,
                               unsigned int key, int display);
int render_layer_cache_preview_presentable(const LayerCache *cache, RECT client, int display);
void render_layer_cache_blit_viewport(HDC hdc, RECT client, const LayerCache *cache);
void render_layer_cache_clear_transparent(LayerCache *cache);
void render_layer_cache_transparent_viewport(HDC hdc, RECT client, const LayerCache *cache);
void render_layer_cache_transparent_map(HDC hdc, RECT client, MapLayout layout,
                                        const LayerCache *cache);

#endif
