#ifndef WORLD_SIM_RENDER_H
#define WORLD_SIM_RENDER_H

#include "core/game_types.h"

void paint_window(HWND hwnd);
void render_paint_side_panel_now(HWND hwnd);
int render_scene_cache_hits(void);
int render_scene_cache_misses(void);
int render_scene_cache_last_build_ms(void);
const char *render_scene_cache_last_reason(void);
const char *render_scene_cache_reason_summary(void);
int render_city_overlay_cache_hits(void);
int render_city_overlay_cache_misses(void);
int render_city_overlay_exact_rebuilds(void);
int render_city_overlay_preview_reuses(void);
const char *render_overlay_cache_last_reason(void);
int render_city_icons_drawn_last_frame(void);
int render_port_icons_drawn_last_frame(void);
int render_neutral_city_icons_drawn_last_frame(void);

#endif
