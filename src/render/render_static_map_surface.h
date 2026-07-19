#ifndef WORLD_SIM_RENDER_STATIC_MAP_SURFACE_H
#define WORLD_SIM_RENDER_STATIC_MAP_SURFACE_H

#include "render/render_static_map_cache_internal.h"
#include "ui/ui_layout.h"

int render_static_map_surface_ensure(HDC hdc, MapLayerCache *cache,
                                     int width, int height);
void render_static_map_surface_release(MapLayerCache *cache);
int render_static_map_surface_matches(const MapLayerCache *cache, int width, int height,
                                      int revision, int display_key);
int render_static_map_surface_presentable(const MapLayerCache *cache, int width, int height,
                                          int display_key);
void render_static_map_surface_mark_valid(MapLayerCache *cache, int revision,
                                          int display_key, int complete);
void render_static_map_surface_alpha(HDC destination, const MapLayerCache *source);
int render_static_map_surface_categorical_stretch_mode(void);
void render_static_map_surface_present(HDC destination, RECT client, MapLayout layout,
                                       const MapLayerCache *cache, int stretch_mode);
void render_static_map_surface_present_alpha(HDC destination, RECT client,
                                             MapLayout layout,
                                             const MapLayerCache *cache,
                                             int stretch_mode);
void render_static_map_surface_present_transparent(HDC destination, RECT client,
                                                   MapLayout layout,
                                                   const MapLayerCache *cache,
                                                   int stretch_mode, COLORREF key);

#endif
