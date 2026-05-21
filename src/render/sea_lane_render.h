#ifndef WORLD_SIM_SEA_LANE_RENDER_H
#define WORLD_SIM_SEA_LANE_RENDER_H

#include <windows.h>

#include "ui/ui_types.h"

void draw_sea_lanes(HDC hdc, RECT client, MapLayout layout);
int sea_lane_render_cache_hits(void);
int sea_lane_render_cache_misses(void);
int sea_lane_render_last_ms(void);
int sea_lane_render_dash_segments(void);

#endif
