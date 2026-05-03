#include "rivers.h"

#include "terrain_query.h"

static int adjacent_water_tile(int x, int y) {
    static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int i;

    for (i = 0; i < 4; i++) {
        int nx = x + dirs[i][0];
        int ny = y + dirs[i][1];
        if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
        if (!is_land(world[ny][nx].geography) || world[ny][nx].geography == GEO_LAKE) return 1;
    }
    return 0;
}

static int nearby_river_count(int x, int y, int radius) {
    int count = 0;
    int dy;
    int dx;

    for (dy = -radius; dy <= radius; dy++) {
        for (dx = -radius; dx <= radius; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
            if (world[ny][nx].river) count++;
        }
    }
    return count;
}

static void mark_river_from(int start_x, int start_y) {
    static const int dirs[8][2] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1}
    };
    int x = start_x;
    int y = start_y;
    int path_x[240];
    int path_y[240];
    int length = 0;
    int reached_water = 0;
    int steps;

    for (steps = 0; steps < 240; steps++) {
        int best_x = x;
        int best_y = y;
        int best_score = 9999;
        int i;

        if (!is_land(world[y][x].geography)) break;
        path_x[length] = x;
        path_y[length] = y;
        length++;
        if (length > 12 && adjacent_water_tile(x, y)) {
            reached_water = 1;
            break;
        }

        for (i = 0; i < 8; i++) {
            int nx = x + dirs[i][0];
            int ny = y + dirs[i][1];
            int diagonal = dirs[i][0] != 0 && dirs[i][1] != 0;
            int score;

            if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
            if (!is_land(world[ny][nx].geography)) {
                if (length > 14) reached_water = 1;
                continue;
            }
            score = world[ny][nx].elevation * 5 - world[ny][nx].moisture / 3 +
                    diagonal * 3 + rnd(4);
            if (world[ny][nx].river && length > 14) {
                reached_water = 1;
                best_x = nx;
                best_y = ny;
                break;
            }
            if (score < best_score) {
                best_score = score;
                best_x = nx;
                best_y = ny;
            }
        }

        if (reached_water || (best_x == x && best_y == y)) break;
        if (world[best_y][best_x].elevation > world[y][x].elevation + 4 && length > 18) break;
        x = best_x;
        y = best_y;
    }

    if ((reached_water || length >= 34) && length >= 16) {
        int i;
        for (i = 0; i < length; i++) {
            world[path_y[i]][path_x[i]].river = 1;
        }
    }
}

void generate_rivers(int moisture, int bias_wetland) {
    int pass;

    for (pass = 0; pass < 18 + (moisture + bias_wetland) / 5; pass++) {
        int tries;
        for (tries = 0; tries < 500; tries++) {
            int rx = rnd(MAP_W);
            int ry = rnd(MAP_H);
            if ((world[ry][rx].geography == GEO_MOUNTAIN || world[ry][rx].geography == GEO_HILL ||
                 world[ry][rx].geography == GEO_PLATEAU) &&
                world[ry][rx].elevation > 58 && world[ry][rx].moisture > 34 + rnd(30) &&
                nearby_river_count(rx, ry, 12) == 0) {
                mark_river_from(rx, ry);
                break;
            }
        }
    }
}
