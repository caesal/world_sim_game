#ifndef WORLD_SIM_SIM_PORTS_H
#define WORLD_SIM_SIM_PORTS_H

typedef struct {
    int components_checked;
    int tiny_excluded;
    int region_ports_added;
    int city_ports_added;
} IslandPortStats;

void ports_ensure_island_ports(void);
void ports_last_island_port_stats(IslandPortStats *out_stats);
void ports_maybe_make_city_port(int city_id);
int ports_activate_region_port_for_city(int region_id, int city_id, int owner);
void ports_refresh_city_regions(void);
void ports_update_migration(void);
int ports_city_is_valid_port(int city_id);
int ports_city_region(int city_id);
int ports_same_region(int city_a, int city_b);

#endif
