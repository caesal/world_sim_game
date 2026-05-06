#include "rivers.h"

#include "terrain_query.h"

#include <stdlib.h>
#include <string.h>

static const int RIVER_DIRS[8][2] = {
    {1, 0}, {1, 1}, {0, 1}, {-1, 1},
    {-1, 0}, {-1, -1}, {0, -1}, {1, -1}
};

static signed short down_x[MAX_MAP_H][MAX_MAP_W];
static signed short down_y[MAX_MAP_H][MAX_MAP_W];
static int flow_accum[MAX_MAP_H][MAX_MAP_W];
static int sorted_tiles[MAX_MAP_W * MAX_MAP_H];
static int river_candidates[MAX_MAP_W * MAX_MAP_H];

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
            if (water_mouth_tile(x + dx, y + dy)) count++;
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

static int flow_rank(int x, int y) {
    return world[y][x].elevation * MAX_MAP_W * MAX_MAP_H + y * MAX_MAP_W + x;
}

static int receiver_score(int x, int y, int nx, int ny) {
    int diagonal = x != nx && y != ny;
    return world[ny][nx].elevation * 32 + world_tile_cost(nx, ny) * 5 -
           world[ny][nx].moisture / 3 + (diagonal ? 5 : 0);
}

static void choose_downhill_receiver(int x, int y) {
    int best_score = 1000000;
    int best_x = -1;
    int best_y = -1;
    int i;

    down_x[y][x] = -1;
    down_y[y][x] = -1;
    if (!is_land(world[y][x].geography)) return;
    for (i = 0; i < 8; i++) {
        int nx = x + RIVER_DIRS[i][0];
        int ny = y + RIVER_DIRS[i][1];
        int score;
        if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
        if (water_mouth_tile(nx, ny)) {
            down_x[y][x] = (signed short)nx;
            down_y[y][x] = (signed short)ny;
            return;
        }
        if (!is_land(world[ny][nx].geography)) continue;
        if (flow_rank(nx, ny) >= flow_rank(x, y)) continue;
        score = receiver_score(x, y, nx, ny);
        if (score < best_score) {
            best_score = score;
            best_x = nx;
            best_y = ny;
        }
    }
    down_x[y][x] = (signed short)best_x;
    down_y[y][x] = (signed short)best_y;
}

static int compare_tile_flow_desc(const void *a, const void *b) {
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    int ax = ia % MAX_MAP_W;
    int ay = ia / MAX_MAP_W;
    int bx = ib % MAX_MAP_W;
    int by = ib / MAX_MAP_W;
    return flow_accum[by][bx] - flow_accum[ay][ax];
}

static int collect_sorted_land_tiles(void) {
    int counts[101] = {0};
    int offsets[101];
    int next[101];
    int x;
    int y;
    int e;
    int total = 0;

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            if (!is_land(world[y][x].geography)) continue;
            counts[clamp(world[y][x].elevation, 0, 100)]++;
            total++;
        }
    }
    offsets[100] = 0;
    for (e = 99; e >= 0; e--) offsets[e] = offsets[e + 1] + counts[e + 1];
    memcpy(next, offsets, sizeof(next));
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int elev;
            if (!is_land(world[y][x].geography)) continue;
            elev = clamp(world[y][x].elevation, 0, 100);
            sorted_tiles[next[elev]++] = y * MAX_MAP_W + x;
        }
    }
    return total;
}

static void compute_flow_field(void) {
    int land_count = collect_sorted_land_tiles();
    int i;
    int x;
    int y;

    memset(flow_accum, 0, sizeof(flow_accum));
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            choose_downhill_receiver(x, y);
            if (is_land(world[y][x].geography)) {
                flow_accum[y][x] = 1 + world[y][x].moisture / 18 +
                                   (world[y][x].geography == GEO_MOUNTAIN ? 1 : 0);
            }
        }
    }
    for (i = 0; i < land_count; i++) {
        int idx = sorted_tiles[i];
        int cx = idx % MAX_MAP_W;
        int cy = idx / MAX_MAP_W;
        int nx = down_x[cy][cx];
        int ny = down_y[cy][cx];
        if (nx >= 0 && ny >= 0 && is_land(world[ny][nx].geography)) {
            flow_accum[ny][nx] += flow_accum[cy][cx];
        }
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

static int source_candidate_ok(int x, int y, int threshold, int tributary) {
    if (!is_land(world[y][x].geography)) return 0;
    if (flow_accum[y][x] < threshold) return 0;
    if (world[y][x].elevation < (tributary ? 42 : 48)) return 0;
    if (nearby_water_count(x, y, 4) > 0) return 0;
    if (nearby_river_count(x, y, tributary ? 5 : 9) > 0) return 0;
    return world[y][x].moisture >= 24 || flow_accum[y][x] >= threshold + 60;
}

static int collect_river_candidates(int threshold, int tributary) {
    int count = 0;
    int x;
    int y;

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            if (!source_candidate_ok(x, y, threshold, tributary)) continue;
            river_candidates[count++] = y * MAX_MAP_W + x;
        }
    }
    qsort(river_candidates, (size_t)count, sizeof(river_candidates[0]), compare_tile_flow_desc);
    return count;
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

static void accept_river_path(RiverPoint *points, int count, int max_flow,
                              int reached_mouth, int joined_river) {
    RiverPath *river;
    int i;

    if (river_path_count >= MAX_RIVER_PATHS || count < 14) return;
    if (!reached_mouth && !joined_river) return;
    river = &river_paths[river_path_count];
    memset(river, 0, sizeof(*river));
    river->active = 1;
    river->point_count = clamp(count, 0, MAX_RIVER_POINTS);
    river->flow = max_flow;
    river->order = clamp(1 + max_flow / 180, 1, 5);
    river->width = clamp(1 + max_flow / 360, 1, 4);
    for (i = 0; i < river->point_count; i++) river->points[i] = points[i];
    mark_river_tiles(river);
    river_path_count++;
}

static void trace_flow_path(int start_x, int start_y, int tributary) {
    RiverPoint points[MAX_RIVER_POINTS];
    int count = 1;
    int x = start_x;
    int y = start_y;
    int max_flow = flow_accum[y][x];
    int reached_mouth = 0;
    int joined_river = 0;

    points[0].x = x;
    points[0].y = y;
    while (count < MAX_RIVER_POINTS) {
        int nx = down_x[y][x];
        int ny = down_y[y][x];
        if (nx < 0 || ny < 0) break;
        points[count].x = nx;
        points[count].y = ny;
        count++;
        if (water_mouth_tile(nx, ny)) {
            reached_mouth = 1;
            break;
        }
        if (!is_land(world[ny][nx].geography)) break;
        if (world[ny][nx].river && count >= (tributary ? 7 : 12)) {
            joined_river = 1;
            break;
        }
        max_flow = max(max_flow, flow_accum[ny][nx]);
        x = nx;
        y = ny;
    }
    if (!tributary && !reached_mouth) return;
    if (tributary && !reached_mouth && !joined_river) return;
    accept_river_path(points, count, max_flow, reached_mouth, joined_river);
}

static void trace_candidate_pass(int threshold, int target, int tributary) {
    int count = collect_river_candidates(threshold, tributary);
    int i;

    for (i = 0; i < count && river_path_count < target; i++) {
        int idx = river_candidates[i];
        int x = idx % MAX_MAP_W;
        int y = idx / MAX_MAP_W;
        if (!source_candidate_ok(x, y, threshold, tributary)) continue;
        trace_flow_path(x, y, tributary);
    }
}

void generate_rivers(int moisture, int bias_wetland) {
    int target = clamp(14 + (moisture + bias_wetland) / 7, 12, MAX_RIVER_PATHS);
    int main_target = clamp(target * 2 / 3, 8, target);
    int threshold = clamp(180 - moisture - bias_wetland / 2, 55, 170);

    reset_rivers();
    compute_flow_field();
    trace_candidate_pass(threshold, main_target, 0);
    if (river_path_count < main_target) trace_candidate_pass(threshold * 3 / 4, main_target, 0);
    trace_candidate_pass(max(35, threshold / 2), target, 1);
}
