#ifndef WORLD_SIM_TERRAIN_QUERY_H
#define WORLD_SIM_TERRAIN_QUERY_H

#include "core/world_types.h"

typedef enum {
    WATER_DEPTH_NONE = 0,
    WATER_DEPTH_SHALLOW,
    WATER_DEPTH_DEEP
} WaterDepth;

#define SHALLOW_WATER_LOGIC_MAX_DIST 7
#define WATER_VISUAL_BLEND_START_DIST 5
#define WATER_VISUAL_BLEND_END_DIST 9

TerrainStats tile_stats(int x, int y);
void terrain_stats_invalidate_cache(void);
void terrain_stats_rebuild_cache(void);
int is_land(Geography geography);
WaterDepth world_water_depth_at(int x, int y);
int world_is_shallow_water(int x, int y);
int world_is_deep_water(int x, int y);
int world_water_distance_to_land(int x, int y);
int world_water_visual_deep_percent(int x, int y);
int world_tile_cost(int x, int y);
int world_terrain_resource_value(TerrainStats stats);
int world_is_coastal_land_tile(int x, int y);

#endif
