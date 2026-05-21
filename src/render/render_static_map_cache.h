#ifndef WORLD_SIM_RENDER_STATIC_MAP_CACHE_H
#define WORLD_SIM_RENDER_STATIC_MAP_CACHE_H

#include "render/render_common.h"

void draw_cached_static_map_nonblocking(HDC hdc, RECT client, MapLayout layout);
int render_static_map_cache_needs_work(void);

#endif
