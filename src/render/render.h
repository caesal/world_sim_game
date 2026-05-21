#ifndef WORLD_SIM_RENDER_H
#define WORLD_SIM_RENDER_H

#include "core/game_types.h"

void paint_window(HWND hwnd);
int render_scene_cache_hits(void);
int render_scene_cache_misses(void);
int render_scene_cache_last_build_ms(void);

#endif
