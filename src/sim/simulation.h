#ifndef WORLD_SIM_SIMULATION_H
#define WORLD_SIM_SIMULATION_H

#include "core/game_types.h"

int city_at(int x, int y);
int city_for_tile(int x, int y);
RegionSummary summarize_city_region(int city_id);
CountrySummary summarize_country(int civ_id);
int world_city_site_has_room(int x, int y, int owner, int radius);
int world_nearby_enemy_border(int owner, int x, int y, int radius);
int world_city_radius_for_tile(int x, int y, int population);
int world_select_city_site_in_province(int city_id, int min_border_distance, int port_only,
                                       int *out_x, int *out_y);
int world_create_city(int owner, int x, int y, int population, int capital);
void world_claim_city_region(int city_id, int owner);
void world_recalculate_territory(void);
void world_invalidate_region_cache(void);
int add_civilization_at(const char *name, char symbol, int aggression, int expansion,
                        int defense, int culture, int preferred_x, int preferred_y);
void simulate_one_month(void);
void reset_simulation(void);

#endif
