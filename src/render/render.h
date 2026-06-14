#ifndef WORLD_SIM_RENDER_H
#define WORLD_SIM_RENDER_H

#include "core/game_types.h"

void paint_window(HWND hwnd);
int render_scene_cache_hits(void);
int render_scene_cache_misses(void);
int render_scene_cache_last_build_ms(void);
int render_scene_cache_last_reason_code(void);
const char *render_scene_cache_last_reason(void);
const char *render_scene_cache_reason_summary(void);
int render_static_scene_presented_current(void);
int render_static_scene_presentable(void);
int render_static_scene_complete(void);
int render_static_scene_fully_current(void);
int render_static_scene_no_safe_frames(void);
int render_static_scene_stale_safe_age_ms(void);
int render_city_overlay_cache_hits(void);
int render_city_overlay_cache_misses(void);
int render_city_overlay_exact_rebuilds(void);
int render_city_overlay_preview_reuses(void);
int render_city_overlay_last_rebuild_ms(void);
int render_city_overlay_peak_rebuild_ms(void);
int render_city_overlay_last_blit_ms(void);
int render_city_overlay_peak_blit_ms(void);
const char *render_overlay_cache_last_reason(void);
void render_scene_cache_reset_debug(void);
int render_city_icons_drawn_last_frame(void);
int render_port_icons_drawn_last_frame(void);
int render_neutral_city_icons_drawn_last_frame(void);

#endif
