#include "ports.h"

#include "world.h"

typedef struct {
    int x;
    int y;
} SeaFrontierNode;

static int shallow_sea_region[MAP_H][MAP_W];
static int shallow_sea_dirty = 1;
static SeaFrontierNode sea_frontier[MAP_W * MAP_H];

static int is_sea_water(Geography geography) {
    return geography == GEO_OCEAN || geography == GEO_BAY;
}

static int is_shallow_sea_tile(int x, int y) {
    int dy;
    int dx;

    if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) return 0;
    if (!is_sea_water(world[y][x].geography)) return 0;
    for (dy = -2; dy <= 2; dy++) {
        for (dx = -2; dx <= 2; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
            if (is_land(world[ny][nx].geography)) return 1;
        }
    }
    return 0;
}

static void rebuild_shallow_sea_regions(void) {
    static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int x;
    int y;
    int region = 0;

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) shallow_sea_region[y][x] = -1;
    }

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int head = 0;
            int tail = 0;
            if (!is_shallow_sea_tile(x, y) || shallow_sea_region[y][x] >= 0) continue;
            shallow_sea_region[y][x] = region;
            sea_frontier[tail].x = x;
            sea_frontier[tail].y = y;
            tail++;
            while (head < tail) {
                SeaFrontierNode node = sea_frontier[head++];
                int i;
                for (i = 0; i < 4; i++) {
                    int nx = node.x + dirs[i][0];
                    int ny = node.y + dirs[i][1];
                    if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
                    if (!is_shallow_sea_tile(nx, ny) || shallow_sea_region[ny][nx] >= 0) continue;
                    shallow_sea_region[ny][nx] = region;
                    if (tail < MAP_W * MAP_H) {
                        sea_frontier[tail].x = nx;
                        sea_frontier[tail].y = ny;
                        tail++;
                    }
                }
            }
            region++;
        }
    }
    shallow_sea_dirty = 0;
}

static int shallow_region_near_land(int x, int y) {
    int dy;
    int dx;

    if (shallow_sea_dirty) rebuild_shallow_sea_regions();
    for (dy = -2; dy <= 2; dy++) {
        for (dx = -2; dx <= 2; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
            if (shallow_sea_region[ny][nx] >= 0) return shallow_sea_region[ny][nx];
        }
    }
    return -1;
}

static int province_has_coast(int city_id) {
    int x;
    int y;

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            if (world[y][x].province_id == city_id && world_is_coastal_land_tile(x, y)) return 1;
        }
    }
    return 0;
}

static void refresh_city_port_regions(void) {
    int i;

    if (shallow_sea_dirty) rebuild_shallow_sea_regions();
    for (i = 0; i < city_count; i++) {
        if (!cities[i].alive || !cities[i].port) continue;
        cities[i].port_region = shallow_region_near_land(cities[i].x, cities[i].y);
        if (cities[i].port_region < 0) cities[i].port = 0;
    }
}

void ports_reset_regions(void) {
    shallow_sea_dirty = 1;
}

void ports_maybe_make_city_port(int city_id) {
    City *city;
    int x;
    int y;
    int region;

    if (city_id < 0 || city_id >= city_count) return;
    city = &cities[city_id];
    if (!city->alive || city->port || !province_has_coast(city_id)) return;
    if (rnd(100) >= 50) return;
    if (!world_select_city_site_in_province(city_id, 0, 1, &x, &y)) return;
    region = shallow_region_near_land(x, y);
    if (region < 0) return;
    city->x = x;
    city->y = y;
    city->port = 1;
    city->port_region = region;
    city->radius = world_city_radius_for_tile(x, y, city->population);
    world_invalidate_region_cache();
}

void ports_update_migration(void) {
    int i;
    int j;

    refresh_city_port_regions();
    for (i = 0; i < city_count; i++) {
        for (j = i + 1; j < city_count; j++) {
            City *a = &cities[i];
            City *b = &cities[j];
            City *donor;
            City *receiver;
            int diff;
            int migrants;

            if (!a->alive || !b->alive || !a->port || !b->port) continue;
            if (a->owner != b->owner || a->owner < 0) continue;
            if (a->port_region < 0 || a->port_region != b->port_region) continue;
            donor = a->population >= b->population ? a : b;
            receiver = donor == a ? b : a;
            diff = donor->population - receiver->population;
            if (diff < 40) continue;
            migrants = clamp(diff / 120, 1, 6);
            donor->population = clamp(donor->population - migrants, 1, MAX_POPULATION);
            receiver->population = clamp(receiver->population + migrants, 1, MAX_POPULATION);
            world_invalidate_region_cache();
        }
    }
}
