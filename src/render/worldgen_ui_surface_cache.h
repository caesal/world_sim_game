#ifndef WORLD_SIM_WORLDGEN_UI_SURFACE_CACHE_H
#define WORLD_SIM_WORLDGEN_UI_SURFACE_CACHE_H

#include <windows.h>

typedef int (*WorldgenUiSurfaceRender)(HDC hdc, int width, int height,
                                       void *context);

typedef struct {
    unsigned int builds;
    unsigned int hits;
    unsigned int allocations;
    unsigned int releases;
} WorldgenUiSurfaceStats;

int worldgen_ui_surface_cache_draw(HDC target, int asset_key, int variant_key,
                                   RECT destination,
                                   WorldgenUiSurfaceRender render,
                                   void *context);
void worldgen_ui_surface_cache_get_stats(int asset_key,
                                         WorldgenUiSurfaceStats *out);
void worldgen_ui_surface_cache_release(void);
void worldgen_ui_surface_cache_reset_for_tests(void);

#endif
