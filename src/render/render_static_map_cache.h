#ifndef WORLD_SIM_RENDER_STATIC_MAP_CACHE_H
#define WORLD_SIM_RENDER_STATIC_MAP_CACHE_H

#include "render/render_common.h"
#include "render/render_layer_cache.h"

void draw_cached_static_map_nonblocking(HDC hdc, RECT client, MapLayout layout);
int render_static_map_cache_needs_work(void);
int render_static_map_cache_snapshot_fallback_draws(void);
int render_static_map_cache_compositions(void);
const char *render_static_map_cache_last_reason(void);
const char *render_static_map_cache_reason_summary(void);
int render_static_map_cache_presented_current(void);
int render_static_map_cache_presented_complete(void);
int render_static_map_cache_presented_boundary_safe(void);
int render_static_map_cache_presented_fully_current(void);
int render_static_map_cache_snapshot_revision(void);
int render_static_map_cache_live_revision(void);
int render_static_map_cache_snapshot_fill_revision(void);
int render_static_map_cache_snapshot_border_revision(void);
int render_static_map_cache_live_fill_revision(void);
int render_static_map_cache_live_border_revision(void);
int render_static_map_cache_published_fill_revision(void);
int render_static_map_cache_published_border_revision(void);
int render_static_map_cache_ownership_current(void);
int render_static_map_cache_political_pending_ms(void);
int render_static_map_cache_political_last_latency_ms(void);
RenderLayerCacheMemory render_static_map_cache_memory(void);
void render_static_map_cache_invalidate_all(void);
void render_static_map_cache_reset_debug(void);

#endif
