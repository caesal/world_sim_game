#ifndef WORLD_SIM_RENDER_STATIC_MAP_CACHE_STATUS_H
#define WORLD_SIM_RENDER_STATIC_MAP_CACHE_STATUS_H

typedef enum {
    RENDER_STATIC_MAP_REASON_NONE = 0,
    RENDER_STATIC_MAP_REASON_PHYSICAL,
    RENDER_STATIC_MAP_REASON_FILL,
    RENDER_STATIC_MAP_REASON_COAST,
    RENDER_STATIC_MAP_REASON_HYDRO,
    RENDER_STATIC_MAP_REASON_BORDER,
    RENDER_STATIC_MAP_REASON_COUNT
} RenderStaticMapReason;

void render_static_map_cache_status_note_reason(RenderStaticMapReason reason);
void render_static_map_cache_status_set_keys(int snapshot_static_key, int live_static_key,
                                             int snapshot_fill_key, int snapshot_border_key,
                                             int live_fill_key, int live_border_key);
void render_static_map_cache_status_set_presented(int current, int boundary_safe,
                                                  int fully_current, int ownership_current);
void render_static_map_cache_status_note_published(int fill_key, int border_key,
                                                   int ownership_current);
void render_static_map_cache_status_reset_keys(void);

#endif
