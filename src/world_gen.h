#ifndef WORLD_SIM_WORLD_GEN_H
#define WORLD_SIM_WORLD_GEN_H

#include "game_types.h"

void generate_world(void);
TerrainStats tile_stats(int x, int y);
const char *geography_name(Geography geography);
const char *climate_name(Climate climate);
const char *ecology_name(Ecology ecology);
const char *resource_name(ResourceFeature resource);
COLORREF geography_color(Geography geography);
COLORREF climate_color(Climate climate);
COLORREF overview_color(int x, int y);
int is_land(Geography geography);
int world_tile_cost(int x, int y);
int world_terrain_resource_value(TerrainStats stats);
int world_is_coastal_land_tile(int x, int y);

#endif
