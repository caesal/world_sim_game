#ifndef WORLD_SIM_SEA_LANE_DASH_CACHE_H
#define WORLD_SIM_SEA_LANE_DASH_CACHE_H

#include <windows.h>

#include "core/value_types.h"
#include "sim/route_potential.h"
#include "sim/sea_lanes.h"

#define SEA_LANE_DASH_CACHE_POTENTIAL_BASE MAX_SEA_LANES
#define SEA_LANE_DASH_CACHE_ENTRY_COUNT (MAX_SEA_LANES + MAX_ROUTE_POTENTIAL_EDGES)

void sea_lane_dash_cache_begin_frame(void);
int sea_lane_dash_cache_draw(HDC hdc, int cache_id, unsigned int route_key,
                             const MapPoint *map_points, const POINT *screen_points,
                             int point_count, int dash_units, int gap_units);
int sea_lane_dash_cache_hits(void);
int sea_lane_dash_cache_misses(void);
int sea_lane_dash_cache_last_rebuild_ms(void);
int sea_lane_dash_cache_segments_drawn(void);
const char *sea_lane_dash_cache_last_reason(void);
const char *sea_lane_dash_cache_reason_summary(void);

#endif
