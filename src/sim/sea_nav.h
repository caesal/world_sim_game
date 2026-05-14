#ifndef WORLD_SIM_SEA_NAV_H
#define WORLD_SIM_SEA_NAV_H

#include "core/value_types.h"

typedef enum {
    SEA_NAV_DEEP_ALLOWED = 0,
    SEA_NAV_SHALLOW_ONLY = 1,
    SEA_NAV_SHALLOW_ANY = 2
} SeaNavMode;

int sea_nav_is_water(int x, int y);
int sea_nav_is_shallow(int x, int y, int shallow_region);
int sea_nav_is_shallow_water_tile(int x, int y);
int sea_nav_distance_to_land(int x, int y);
int sea_nav_segment_water(MapPoint a, MapPoint b);
int sea_nav_segment_water_mode(MapPoint a, MapPoint b, SeaNavMode mode, int shallow_region);
int sea_nav_find_path(MapPoint start, MapPoint goal, MapPoint *out, int max_points);
int sea_nav_find_path_mode(MapPoint start, MapPoint goal, SeaNavMode mode, int shallow_region, MapPoint *out, int max_points);

#endif
