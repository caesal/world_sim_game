#ifndef WORLD_SIM_SIM_PORTS_H
#define WORLD_SIM_SIM_PORTS_H

void ports_maybe_make_city_port(int city_id);
void ports_refresh_city_regions(void);
void ports_update_migration(void);
int ports_city_is_valid_port(int city_id);
int ports_city_region(int city_id);
int ports_same_region(int city_a, int city_b);

#endif
