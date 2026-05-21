#ifndef WORLD_SIM_TERRAIN_QUERY_H
#define WORLD_SIM_TERRAIN_QUERY_H

#include "core/world_types.h"

typedef enum {
    WATER_DEPTH_NONE = 0,
    WATER_DEPTH_SHALLOW,
    WATER_DEPTH_DEEP
} WaterDepth;

#define WATER_DEPTH_SHALLOW_MAX_SCORE 55
#define WATER_DEPTH_DEEP_MIN_SCORE 56

TerrainStats tile_stats(int x, int y);
void terrain_stats_invalidate_cache(void);
void terrain_stats_rebuild_cache(void);
int is_land(Geography geography);
WaterDepth world_water_depth_at(int x, int y);
int world_is_shallow_water(int x, int y);
int world_is_deep_water(int x, int y);
int world_water_distance_to_land(int x, int y);
int world_water_visual_deep_percent(int x, int y);
int world_water_shelf_width_at(int x, int y);
int world_water_depth_rebuild_ms(void);
int world_water_shallow_tile_count(void);
int world_water_deep_tile_count(void);
int world_water_shelf_min(void);
int world_water_shelf_max(void);
int world_water_shelf_avg(void);
int world_tile_cost(int x, int y);
int world_terrain_resource_value(TerrainStats stats);
int world_is_coastal_land_tile(int x, int y);

#endif
