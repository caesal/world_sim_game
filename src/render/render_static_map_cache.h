#ifndef WORLD_SIM_RENDER_STATIC_MAP_CACHE_H
#define WORLD_SIM_RENDER_STATIC_MAP_CACHE_H

#include "render/render_common.h"

void draw_cached_static_map_nonblocking(HDC hdc, RECT client, MapLayout layout);
int render_static_map_cache_needs_work(void);
const char *render_static_map_cache_last_reason(void);
const char *render_static_map_cache_reason_summary(void);
int render_static_map_cache_presented_current(void);
int render_static_map_cache_snapshot_revision(void);
int render_static_map_cache_live_revision(void);

#endif
