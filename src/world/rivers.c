#include "rivers.h"

#include "terrain_query.h"

#include <string.h>

static const int RIVER_DIRS[8][2] = {
    {1, 0}, {1, 1}, {0, 1}, {-1, 1},
    {-1, 0}, {-1, -1}, {0, -1}, {1, -1}
};

static int water_mouth_tile(int x, int y) {
    if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) return 0;
    return world[y][x].geography == GEO_OCEAN || world[y][x].geography == GEO_BAY ||
           world[y][x].geography == GEO_LAKE;
}

static int nearby_water_count(int x, int y, int radius) {
    int count = 0;
    int dy;
    int dx;

    for (dy = -radius; dy <= radius; dy++) {
        for (dx = -radius; dx <= radius; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
            if (water_mouth_tile(nx, ny)) count++;
        }
    }
    return count;
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

static int path_contains(RiverPoint *points, int count, int x, int y) {
    int i;

    for (i = count - 1; i >= 0; i--) {
        if (points[i].x == x && points[i].y == y) return 1;
    }
    return 0;
}

static int find_mouth_near(int x, int y, int radius, RiverPoint *mouth) {
    int best_distance = 9999;
    int found = 0;
    int dy;
    int dx;

    for (dy = -radius; dy <= radius; dy++) {
        for (dx = -radius; dx <= radius; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            int distance = dx * dx + dy * dy;
            if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
            if (!water_mouth_tile(nx, ny) || distance >= best_distance) continue;
            best_distance = distance;
            mouth->x = nx;
            mouth->y = ny;
            found = 1;
        }
    }
    return found;
}

static int source_is_valid(int x, int y, int tributary_source) {
    Geography geography;
    int near_rivers;

    if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) return 0;
    geography = world[y][x].geography;
    if (!is_land(geography) || nearby_water_count(x, y, 5) > 0) return 0;
    near_rivers = nearby_river_count(x, y, tributary_source ? 24 : 11);
    if (!tributary_source && near_rivers > 0) return 0;
    if (tributary_source && (near_rivers == 0 || nearby_river_count(x, y, 5) > 0)) return 0;
    if (world[y][x].elevation < (tributary_source ? 50 : 56) + rnd(18)) return 0;
    if (world[y][x].moisture < 28 + rnd(34)) return 0;
    return geography == GEO_MOUNTAIN || geography == GEO_HILL ||
           geography == GEO_PLATEAU || geography == GEO_BASIN;
}

static int turn_penalty(int prev_dx, int prev_dy, int dx, int dy) {
    int dot;

    if (prev_dx == 0 && prev_dy == 0) return 0;
    dot = prev_dx * dx + prev_dy * dy;
    if (dot < 0) return 34;
    if (dot == 0) return 7;
    return 0;
}

static int next_step_score(int x, int y, int nx, int ny, int prev_dx, int prev_dy, int prefer_join) {
    int dx = nx - x;
    int dy = ny - y;
    int climb = world[ny][nx].elevation - world[y][x].elevation;
    int diagonal = dx != 0 && dy != 0;
    int water_pull = nearby_water_count(nx, ny, 3) * 7;
    int score = world[ny][nx].elevation * 5 - world[ny][nx].moisture / 2;

    if (climb > 0) score += climb * 20;
    else score += climb * 3;
    score += diagonal ? 3 : 0;
    score += turn_penalty(prev_dx, prev_dy, dx, dy);
    score -= water_pull;
    if (prefer_join) score -= nearby_river_count(nx, ny, 5) * 18;
    score += rnd(12);
    return score;
}

static int choose_next_step(RiverPoint *points, int count, int *out_x, int *out_y,
                            int *joined_river, int *reached_mouth, int prefer_join) {
    int x = points[count - 1].x;
    int y = points[count - 1].y;
    int prev_dx = count > 1 ? x - points[count - 2].x : 0;
    int prev_dy = count > 1 ? y - points[count - 2].y : 0;
    int best_score = 1000000;
    int best_x = x;
    int best_y = y;
    int found = 0;
    int start = rnd(8);
    int i;

    *joined_river = 0;
    *reached_mouth = 0;
    for (i = 0; i < 8; i++) {
        int dir = (start + i) % 8;
        int nx = x + RIVER_DIRS[dir][0];
        int ny = y + RIVER_DIRS[dir][1];
        int score;

        if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
        if (water_mouth_tile(nx, ny) && count >= 18) {
            *out_x = nx;
            *out_y = ny;
            *reached_mouth = 1;
            return 1;
        }
        if (!is_land(world[ny][nx].geography)) continue;
        if (path_contains(points, count, nx, ny)) continue;
        if (world[ny][nx].river && count >= (prefer_join ? 8 : 14)) {
            *out_x = nx;
            *out_y = ny;
            *joined_river = 1;
            return 1;
        }
        if (world[ny][nx].elevation > world[y][x].elevation + 5 && count > 10) continue;
        if (nearby_river_count(nx, ny, 3) > 2 && count < 18) continue;
        score = next_step_score(x, y, nx, ny, prev_dx, prev_dy, prefer_join);
        if (score < best_score) {
            best_score = score;
            best_x = nx;
            best_y = ny;
            found = 1;
        }
    }

    if (!found) return 0;
    *out_x = best_x;
    *out_y = best_y;
    return 1;
}

static void mark_river_tiles(const RiverPath *river) {
    int i;

    for (i = 0; i < river->point_count; i++) {
        int x = river->points[i].x;
        int y = river->points[i].y;
        if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) continue;
        if (is_land(world[y][x].geography)) world[y][x].river = 1;
    }
}

static void accept_river_path(RiverPoint *points, int count, int joined_river, int reached_mouth, int tributary_hint) {
    RiverPath *river;
    int i;

    if (river_path_count >= MAX_RIVER_PATHS || count < 22) return;
    if (!reached_mouth && !joined_river) return;
    if (tributary_hint && !joined_river && !reached_mouth) return;
    river = &river_paths[river_path_count];
    memset(river, 0, sizeof(*river));
    river->active = 1;
    river->point_count = clamp(count, 0, MAX_RIVER_POINTS);
    river->flow = river->point_count + (reached_mouth ? 32 : 0);
    river->order = joined_river || tributary_hint ? 1 : (river->point_count > 95 ? 3 : 2);
    river->width = clamp(river->order + river->flow / 130, 1, 3);
    for (i = 0; i < river->point_count; i++) river->points[i] = points[i];
    mark_river_tiles(river);
    river_path_count++;
}

static void trace_river_from(int start_x, int start_y, int tributary_hint) {
    RiverPoint points[MAX_RIVER_POINTS];
    RiverPoint mouth;
    int count = 1;
    int joined_river = 0;
    int reached_mouth = 0;
    int steps;

    points[0].x = start_x;
    points[0].y = start_y;
    for (steps = 0; steps < MAX_RIVER_POINTS - 1; steps++) {
        int nx;
        int ny;
        int joined = 0;
        int mouth_reached = 0;

        if (!choose_next_step(points, count, &nx, &ny, &joined, &mouth_reached, tributary_hint)) break;
        points[count].x = nx;
        points[count].y = ny;
        count++;
        if (joined) {
            joined_river = 1;
            break;
        }
        if (mouth_reached) {
            reached_mouth = 1;
            break;
        }
    }

    if (!reached_mouth && !joined_river && count >= 24 &&
        find_mouth_near(points[count - 1].x, points[count - 1].y, 5, &mouth)) {
        points[count++] = mouth;
        reached_mouth = 1;
    }
    if ((reached_mouth || joined_river) && count >= 22) {
        accept_river_path(points, count, joined_river, reached_mouth, tributary_hint);
    }
}

static void reset_rivers(void) {
    int x;
    int y;

    river_path_count = 0;
    memset(river_paths, 0, sizeof(river_paths));
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) world[y][x].river = 0;
    }
}

void generate_rivers(int moisture, int bias_wetland) {
    int target = clamp(18 + (moisture + bias_wetland) / 5, 16, MAX_RIVER_PATHS);
    int main_target = clamp(target * 2 / 3, 10, target);
    int pass;

    reset_rivers();
    for (pass = 0; pass < target; pass++) {
        int tries;
        for (tries = 0; tries < 900; tries++) {
            int rx = rnd(MAP_W);
            int ry = rnd(MAP_H);
            if (source_is_valid(rx, ry, pass >= main_target)) {
                trace_river_from(rx, ry, pass >= main_target);
                break;
            }
        }
    }
}
