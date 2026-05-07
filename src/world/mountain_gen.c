#include "mountain_gen.h"

#include "core/game_types.h"
#include "world/terrain_query.h"

#include <stdio.h>
#include <stdlib.h>

static int last_chain_count = 0;
static int last_average_length = 0;

static int find_land_seed(int *out_x, int *out_y) {
    int attempt;

    for (attempt = 0; attempt < 400; attempt++) {
        int x = rnd(MAP_W);
        int y = rnd(MAP_H);
        if (is_land(world[y][x].geography) && world[y][x].geography != GEO_COAST &&
            world[y][x].geography != GEO_LAKE && world[y][x].geography != GEO_BAY) {
            *out_x = x;
            *out_y = y;
            return 1;
        }
    }
    return 0;
}

static void apply_mountain_point(int cx, int cy, int strength) {
    int radius = clamp(strength / 18, 2, 5);
    int y;
    int x;

    for (y = cy - radius; y <= cy + radius; y++) {
        for (x = cx - radius; x <= cx + radius; x++) {
            int dx = abs(x - cx);
            int dy = abs(y - cy);
            int dist = dx + dy;
            int lift;
            if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H || dist > radius) continue;
            if (!is_land(world[y][x].geography)) continue;
            lift = strength * (radius + 1 - dist) / (radius + 1);
            world[y][x].elevation = clamp(world[y][x].elevation + lift, 0, 100);
            if (dist <= 1 && world[y][x].elevation >= 80) {
                world[y][x].geography = GEO_MOUNTAIN;
                if (world[y][x].resource == RESOURCE_FEATURE_NONE) world[y][x].resource = RESOURCE_FEATURE_MINE;
            } else if (dist <= 2 && world[y][x].geography != GEO_MOUNTAIN) {
                world[y][x].geography = GEO_HILL;
            }
        }
    }
}

static int trace_chain(int x, int y, int length, int dir_x, int dir_y, int strength) {
    int step;
    int carved = 0;
    int drift = rnd(3) - 1;

    for (step = 0; step < length; step++) {
        if (x < 2 || x >= MAP_W - 2 || y < 2 || y >= MAP_H - 2) break;
        if (is_land(world[y][x].geography)) {
            apply_mountain_point(x, y, strength);
            carved++;
            if (step % 9 == 0 && rnd(100) < 34) {
                int branch_x = dir_y + (rnd(3) - 1);
                int branch_y = -dir_x + (rnd(3) - 1);
                int branch_len = clamp(length / 4 + rnd(max(4, length / 5)), 8, 55);
                trace_chain(x, y, branch_len, branch_x, branch_y, strength * 2 / 3);
            }
        }
        if (step % 7 == 0) drift = rnd(3) - 1;
        x += dir_x + (dir_y == 0 ? 0 : drift);
        y += dir_y + (dir_x == 0 ? 0 : drift);
        if (rnd(100) < 18) {
            if (abs(dir_x) > abs(dir_y)) dir_y = clamp(dir_y + rnd(3) - 1, -1, 1);
            else dir_x = clamp(dir_x + rnd(3) - 1, -1, 1);
            if (dir_x == 0 && dir_y == 0) dir_x = rnd(2) ? 1 : -1;
        }
    }
    return carved;
}

void world_apply_mountain_chains(int relief_value, int mountain_bias) {
    int chains = clamp(MAP_W * MAP_H / 90000 + relief_value / 18 + mountain_bias / 32, 2, 12);
    int total_length = 0;
    int i;
    char buffer[160];

    last_chain_count = 0;
    last_average_length = 0;
    for (i = 0; i < chains; i++) {
        static const int dirs[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, -1}, {1, -1}, {-1, 1}};
        int x;
        int y;
        int dir = rnd(8);
        int length = clamp((MAP_W + MAP_H) / 9 + rnd((MAP_W + MAP_H) / 7), 45, 170);
        int strength = clamp(16 + relief_value / 4 + mountain_bias / 5 + rnd(18), 18, 58);
        int carved;
        if (!find_land_seed(&x, &y)) continue;
        carved = trace_chain(x, y, length, dirs[dir][0], dirs[dir][1], strength);
        if (carved <= 0) continue;
        last_chain_count++;
        total_length += carved;
    }
    last_average_length = last_chain_count > 0 ? total_length / last_chain_count : 0;
    snprintf(buffer, sizeof(buffer), "World Sim: mountain_chain_count=%d average_chain_length=%d\n",
             last_chain_count, last_average_length);
    OutputDebugStringA(buffer);
}

int world_mountain_chain_count(void) {
    return last_chain_count;
}

int world_mountain_average_chain_length(void) {
    return last_average_length;
}
