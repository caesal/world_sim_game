#include "ports.h"

#include "core/dirty_flags.h"
#include "sim/maritime.h"
#include "sim/regions.h"
#include "sim/regions_port_policy.h"
#include "sim/simulation.h"
#include "world/ports.h"

static IslandPortStats last_island_port_stats;

void ports_ensure_island_ports(void) {
    RegionPortPolicyStats stats;

    regions_port_policy_apply_all();
    regions_port_policy_last_stats(&stats);
    last_island_port_stats.components_checked = stats.island_components_checked;
    last_island_port_stats.tiny_excluded = 0;
    last_island_port_stats.region_ports_added = stats.forced_island_ports;
    last_island_port_stats.city_ports_added = stats.port_city_count;
}

void ports_last_island_port_stats(IslandPortStats *out_stats) {
    if (out_stats) *out_stats = last_island_port_stats;
}

void ports_refresh_city_regions(void) {
    int i;

    for (i = 0; i < city_count; i++) {
        int old_port;
        int old_x;
        int old_y;
        int old_region;
        if (!cities[i].alive || !cities[i].port) continue;
        old_port = cities[i].port;
        old_x = cities[i].port_x;
        old_y = cities[i].port_y;
        old_region = cities[i].port_region;
        if (cities[i].port_x < 0 || cities[i].port_y < 0) {
            cities[i].port_x = cities[i].x;
            cities[i].port_y = cities[i].y;
        }
        cities[i].port_region = ports_shallow_region_near_land(cities[i].port_x, cities[i].port_y);
        if (cities[i].port_region < 0) cities[i].port = 0;
        if (cities[i].port != old_port || cities[i].port_x != old_x ||
            cities[i].port_y != old_y || cities[i].port_region != old_region) {
            dirty_mark_city();
        }
    }
}

void ports_maybe_make_city_port(int city_id) {
    (void)city_id;
}

int ports_activate_region_port_for_city(int region_id, int city_id, int owner) {
    NaturalRegion *region;
    int port_region;

    if (region_id < 0 || region_id >= region_count || city_id < 0 || city_id >= city_count) return 0;
    region = &natural_regions[region_id];
    if (!region->alive || !region->has_port_site || region->port_x < 0 || region->port_y < 0) return 0;
    if (!cities[city_id].alive || cities[city_id].owner != owner) return 0;
    port_region = ports_shallow_region_near_land(region->port_x, region->port_y);
    if (port_region < 0) return 0;
    if (cities[city_id].port && cities[city_id].port_x == region->port_x &&
        cities[city_id].port_y == region->port_y && cities[city_id].port_region == port_region) return 1;
    cities[city_id].port = 1;
    cities[city_id].port_x = region->port_x;
    cities[city_id].port_y = region->port_y;
    cities[city_id].port_region = port_region;
    dirty_mark_city();
    maritime_mark_routes_dirty();
    world_invalidate_region_cache();
    return 1;
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
