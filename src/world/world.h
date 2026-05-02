#ifndef WORLD_SIM_WORLD_H
#define WORLD_SIM_WORLD_H

#include "../core/game_state.h"

TerrainStats tile_stats(int x, int y);
const char *geography_name(Geography geography);
const char *climate_name(Climate climate);
COLORREF geography_color(Geography geography);
COLORREF climate_color(Climate climate);
COLORREF overview_color(int x, int y);
int is_land(Geography geography);
int city_at(int x, int y);
int city_for_tile(int x, int y);
RegionSummary summarize_city_region(int city_id);
int add_civilization_at(const char *name, char symbol, int aggression, int expansion,
                        int defense, int culture, int preferred_x, int preferred_y);
void simulate_one_month(void);
void reset_simulation(void);

#endif
