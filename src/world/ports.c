#include "ports.h"

#include "core/game_types.h"
#include "terrain_query.h"

#include <stdlib.h>

typedef struct {
    int x;
    int y;
} SeaFrontierNode;

static int shallow_sea_region[MAX_MAP_H][MAX_MAP_W];
static int shallow_sea_dirty = 1;
static SeaFrontierNode sea_frontier[MAX_MAP_W * MAX_MAP_H];

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

int ports_shallow_region_near_land(int x, int y) {
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

int ports_tile_shallow_region_near_land(int x, int y) {
    if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) return -1;
    if (shallow_sea_dirty) rebuild_shallow_sea_regions();
    return shallow_sea_region[y][x];
}

int ports_find_nearby_sea_entry(int land_x, int land_y, int *out_x, int *out_y) {
    int radius;
    int best_score = 1000000;
    int found = 0;

    if (!out_x || !out_y) return 0;
    if (shallow_sea_dirty) rebuild_shallow_sea_regions();
    for (radius = 1; radius <= 4; radius++) {
        int dy;
        int dx;
        for (dy = -radius; dy <= radius; dy++) {
            for (dx = -radius; dx <= radius; dx++) {
                int nx = land_x + dx;
                int ny = land_y + dy;
                int score;
                if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
                if (shallow_sea_region[ny][nx] < 0) continue;
                score = abs(dx) + abs(dy);
                if (score < best_score) {
                    best_score = score;
                    *out_x = nx;
                    *out_y = ny;
                    found = 1;
                }
            }
        }
        if (found) return 1;
    }
    return 0;
}

int ports_province_has_coast(int city_id) {
    int x;
    int y;

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            if (world[y][x].province_id == city_id && world_is_coastal_land_tile(x, y)) return 1;
        }
    }
    return 0;
}

void ports_reset_regions(void) {
    shallow_sea_dirty = 1;
}
