#ifndef WORLD_SIM_TERRAIN_QUERY_H
#define WORLD_SIM_TERRAIN_QUERY_H

#include "core/world_types.h"

TerrainStats tile_stats(int x, int y);
void terrain_stats_invalidate_cache(void);
void terrain_stats_rebuild_cache(void);
int is_land(Geography geography);
int world_tile_cost(int x, int y);
int world_terrain_resource_value(TerrainStats stats);
int world_is_coastal_land_tile(int x, int y);

#endif
