#ifndef WORLD_SIM_PORTS_H
#define WORLD_SIM_PORTS_H

void ports_reset_regions(void);
int ports_shallow_region_near_land(int x, int y);
int ports_tile_shallow_region_near_land(int x, int y);
int ports_find_nearby_sea_entry(int land_x, int land_y, int *out_x, int *out_y);
int ports_province_has_coast(int city_id);

#endif
