#ifndef WORLD_SIM_PROVINCE_H
#define WORLD_SIM_PROVINCE_H

#include "core/game_types.h"

void province_invalidate_region_cache(void);
void province_invalidate_region_summary(void);
int province_city_for_tile(int x, int y);
RegionSummary province_summarize_city_region(int city_id);
int province_city_too_close(int x, int y, int min_distance);
int province_city_site_has_room(int x, int y, int owner, int radius);
int province_nearby_enemy_border(int owner, int x, int y, int radius);
int province_city_radius_for_tile(int x, int y, int population);
int province_select_city_site(int city_id, int min_border_distance, int port_only,
                              int *out_x, int *out_y);
void province_claim_city_region(int city_id, int owner);

#endif
