#include "ports.h"

#include "sim/maritime.h"
#include "sim/regions.h"
#include "sim/simulation.h"
#include "world/ports.h"
#include "world/terrain_query.h"

#include <string.h>

#define ISLAND_PORT_MIN_TILES 18

typedef struct {
    int tiles;
    int has_region_port;
    int has_city_port;
    int best_x;
    int best_y;
    int best_region;
    int best_score;
} PortComponent;

static IslandPortStats last_island_port_stats;
static int component_id[MAX_MAP_H][MAX_MAP_W];
static int queue_x[MAX_MAP_W * MAX_MAP_H];
static int queue_y[MAX_MAP_W * MAX_MAP_H];

static int water_neighbor_score(int x, int y) {
    static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int i;
    int score = 0;

    for (i = 0; i < 4; i++) {
        int nx = x + dirs[i][0];
        int ny = y + dirs[i][1];
        Geography geo;
        if (nx < 0 || ny < 0 || nx >= MAP_W || ny >= MAP_H) continue;
        geo = world[ny][nx].geography;
        if (is_land(geo)) continue;
        if (world_is_shallow_water(nx, ny)) score += 84;
        else if (world_is_deep_water(nx, ny)) score += 26;
        else score += 12;
    }
    return score;
}

static int port_candidate_score(int x, int y, int region_id) {
    TerrainStats stats;
    int sea_x;
    int sea_y;
    int score;

    if (region_id < 0 || region_id >= region_count) return -100000;
    if (!world_is_coastal_land_tile(x, y)) return -100000;
    if (!ports_find_nearby_sea_entry(x, y, &sea_x, &sea_y)) return -100000;
    stats = tile_stats(x, y);
    score = water_neighbor_score(x, y) + stats.habitability / 2;
    score += world_terrain_resource_value(stats) / 3;
    score -= world_tile_cost(x, y) * 3;
    if (world[y][x].geography == GEO_COAST || world[y][x].geography == GEO_DELTA) score += 20;
    return score;
}

static void scan_component(int sx, int sy, int id, PortComponent *component) {
    static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int head = 0;
    int tail = 0;

    component->tiles = 0;
    component->has_region_port = 0;
    component->has_city_port = 0;
    component->best_x = -1;
    component->best_y = -1;
    component->best_region = -1;
    component->best_score = -100000;
    component_id[sy][sx] = id;
    queue_x[tail] = sx;
    queue_y[tail++] = sy;
    while (head < tail) {
        int x = queue_x[head];
        int y = queue_y[head++];
        int region_id = world[y][x].region_id;
        int score = port_candidate_score(x, y, region_id);
        int i;

        component->tiles++;
        if (region_id >= 0 && region_id < region_count && natural_regions[region_id].has_port_site) {
            component->has_region_port = 1;
        }
        if (score > component->best_score) {
            component->best_score = score;
            component->best_x = x;
            component->best_y = y;
            component->best_region = region_id;
        }
        for (i = 0; i < 4; i++) {
            int nx = x + dirs[i][0];
            int ny = y + dirs[i][1];
            if (nx < 0 || ny < 0 || nx >= MAP_W || ny >= MAP_H) continue;
            if (component_id[ny][nx] >= 0 || !is_land(world[ny][nx].geography)) continue;
            component_id[ny][nx] = id;
            queue_x[tail] = nx;
            queue_y[tail++] = ny;
        }
    }
}

static int best_city_for_component(int id, const PortComponent *component, int *has_port) {
    int i;
    int best_city = -1;
    int best_score = -100000;

    *has_port = 0;
    for (i = 0; i < city_count; i++) {
        int dx;
        int dy;
        int score;
        if (!cities[i].alive) continue;
        if (cities[i].x < 0 || cities[i].y < 0 || cities[i].x >= MAP_W || cities[i].y >= MAP_H) continue;
        if (component_id[cities[i].y][cities[i].x] != id) continue;
        if (ports_city_is_valid_port(i)) *has_port = 1;
        dx = cities[i].x - component->best_x;
        dy = cities[i].y - component->best_y;
        score = cities[i].population / 1000 - (dx * dx + dy * dy);
        if (cities[i].capital) score += 5000;
        if (score > best_score) {
            best_score = score;
            best_city = i;
        }
    }
    return best_city;
}

static int assign_city_port(int city_id, const PortComponent *component) {
    int shallow_region;

    if (city_id < 0 || component->best_x < 0 || component->best_region < 0) return 0;
    shallow_region = ports_shallow_region_near_land(component->best_x, component->best_y);
    if (shallow_region < 0) return 0;
    cities[city_id].port = 1;
    cities[city_id].port_x = component->best_x;
    cities[city_id].port_y = component->best_y;
    cities[city_id].port_region = shallow_region;
    return 1;
}

void ports_ensure_island_ports(void) {
    int x;
    int y;
    int id = 0;
    int changed = 0;

    memset(component_id, 0xff, sizeof(component_id));
    memset(&last_island_port_stats, 0, sizeof(last_island_port_stats));
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            PortComponent component;
            int has_city_port;
            int best_city;

            if (component_id[y][x] >= 0 || !is_land(world[y][x].geography)) continue;
            scan_component(x, y, id, &component);
            best_city = best_city_for_component(id, &component, &has_city_port);
            component.has_city_port = has_city_port;
            if (component.tiles < ISLAND_PORT_MIN_TILES && best_city < 0) {
                last_island_port_stats.tiny_excluded++;
                id++;
                continue;
            }
            last_island_port_stats.components_checked++;
            if (!component.has_region_port && component.best_region >= 0) {
                natural_regions[component.best_region].has_port_site = 1;
                natural_regions[component.best_region].port_x = component.best_x;
                natural_regions[component.best_region].port_y = component.best_y;
                last_island_port_stats.region_ports_added++;
                changed = 1;
            }
            if (!component.has_city_port && assign_city_port(best_city, &component)) {
                last_island_port_stats.city_ports_added++;
                changed = 1;
            }
            id++;
        }
    }
    if (changed) {
        maritime_mark_routes_dirty();
        world_invalidate_region_cache();
    }
}

void ports_last_island_port_stats(IslandPortStats *out_stats) {
    if (out_stats) *out_stats = last_island_port_stats;
}

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
