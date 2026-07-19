#ifndef WORLD_SIM_RENDER_DYNAMIC_OVERLAY_CACHE_H
#define WORLD_SIM_RENDER_DYNAMIC_OVERLAY_CACHE_H

#include "core/render_snapshot.h"
#include "ui/ui_types.h"

#include <windows.h>

int render_dynamic_route_overlay_draw(HDC hdc, RECT client, MapLayout layout,
                                      const RenderSnapshot *snapshot, int display,
                                      int selected, int side_w,
                                      int interaction_preview);
int render_dynamic_city_overlay_draw(HDC hdc, RECT client, MapLayout layout,
                                     const RenderSnapshot *snapshot, int display,
                                     int side_w, int interaction_preview);

int render_city_overlay_cache_hits(void);
int render_city_overlay_cache_misses(void);
int render_city_overlay_exact_rebuilds(void);
int render_city_overlay_preview_reuses(void);
int render_city_overlay_last_rebuild_ms(void);
int render_city_overlay_peak_rebuild_ms(void);
int render_city_overlay_last_blit_ms(void);
int render_city_overlay_peak_blit_ms(void);
int render_dynamic_route_overlay_exact_rebuilds(void);
int render_dynamic_route_overlay_camera_draws(void);
int render_dynamic_city_overlay_camera_draws(void);
const char *render_overlay_cache_last_reason(void);
const char *render_dynamic_overlay_status_summary(void);
void render_dynamic_overlay_reset_debug(void);

#endif
