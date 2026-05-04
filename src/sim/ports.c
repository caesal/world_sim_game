#include "ports.h"

#include "sim/maritime.h"
#include "sim/simulation.h"
#include "world/ports.h"

void ports_refresh_city_regions(void) {
    int i;

    for (i = 0; i < city_count; i++) {
        if (!cities[i].alive || !cities[i].port) continue;
        if (cities[i].port_x < 0 || cities[i].port_y < 0) {
            cities[i].port_x = cities[i].x;
            cities[i].port_y = cities[i].y;
        }
        cities[i].port_region = ports_shallow_region_near_land(cities[i].port_x, cities[i].port_y);
        if (cities[i].port_region < 0) cities[i].port = 0;
    }
}

void ports_maybe_make_city_port(int city_id) {
    City *city;
    int x;
    int y;
    int region;

    if (city_id < 0 || city_id >= city_count) return;
    city = &cities[city_id];
    if (!city->alive || city->port || !ports_province_has_coast(city_id)) return;
    if (rnd(100) >= 50) return;
    if (!world_select_city_site_in_province(city_id, 0, 1, &x, &y)) return;
    region = ports_shallow_region_near_land(x, y);
    if (region < 0) return;
    city->port = 1;
    city->port_x = x;
    city->port_y = y;
    city->port_region = region;
    maritime_mark_routes_dirty();
    world_invalidate_region_cache();
}

int ports_city_is_valid_port(int city_id) {
    if (city_id < 0 || city_id >= city_count) return 0;
    return cities[city_id].alive && cities[city_id].port && cities[city_id].port_region >= 0;
}

int ports_city_region(int city_id) {
    if (!ports_city_is_valid_port(city_id)) return -1;
    return cities[city_id].port_region;
}

int ports_same_region(int city_a, int city_b) {
    int region_a = ports_city_region(city_a);
    int region_b = ports_city_region(city_b);

    return region_a >= 0 && region_a == region_b;
}

void ports_update_migration(void) {
    maritime_update_migration();
}
