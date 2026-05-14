#include "sim/sea_nav.h"

#include "core/dirty_flags.h"
#include "core/game_state.h"
#include "core/profiler.h"
#include "world/ports.h"
#include "world/terrain_query.h"

#include <stdlib.h>
#include <string.h>

#define SEA_NAV_INF 0x3fffffff
#define SEA_NAV_MAX_CELLS (MAX_MAP_W * MAX_MAP_H)

typedef struct {
    int idx;
    int score;
} HeapNode;

static unsigned char water[SEA_NAV_MAX_CELLS];
static unsigned char land_dist[SEA_NAV_MAX_CELLS];
static int nav_cost[SEA_NAV_MAX_CELLS];
static int nav_prev[SEA_NAV_MAX_CELLS];
static int nav_seen[SEA_NAV_MAX_CELLS];
static int nav_closed[SEA_NAV_MAX_CELLS];
static unsigned char nav_dir[SEA_NAV_MAX_CELLS];
static HeapNode heap[SEA_NAV_MAX_CELLS];
static int heap_count;
static int nav_stamp = 1;
static int cached_revision = -1;
static int cached_w = 0;
static int cached_h = 0;

static int idx_xy(int x, int y) { return y * MAP_W + x; }
static int in_bounds(int x, int y) { return x >= 0 && x < MAP_W && y >= 0 && y < MAP_H; }
static void ensure_nav_field(void);

int sea_nav_is_water(int x, int y) {
    if (!in_bounds(x, y)) return 0;
    return world_water_depth_at(x, y) != WATER_DEPTH_NONE;
}

int sea_nav_is_shallow(int x, int y, int shallow_region) {
    if (!in_bounds(x, y) || shallow_region < 0) return 0;
    return ports_tile_shallow_region_near_land(x, y) == shallow_region;
}

int sea_nav_is_shallow_water_tile(int x, int y) {
    ensure_nav_field();
    if (!in_bounds(x, y) || !water[idx_xy(x, y)]) return 0;
    return world_is_shallow_water(x, y);
}

static int nav_tile_ok(int x, int y, SeaNavMode mode, int shallow_region) {
    if (!in_bounds(x, y)) return 0;
    if (mode == SEA_NAV_SHALLOW_ONLY) return sea_nav_is_shallow(x, y, shallow_region);
    if (mode == SEA_NAV_SHALLOW_ANY) return sea_nav_is_shallow_water_tile(x, y);
    return water[idx_xy(x, y)] != 0;
}

static void rebuild_nav_field(void) {
    int x, y, pass;
    int total = MAP_W * MAP_H;

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int idx = idx_xy(x, y);
            water[idx] = (unsigned char)sea_nav_is_water(x, y);
            land_dist[idx] = water[idx] ? 60 : 0;
        }
    }
    for (pass = 0; pass < 2; pass++) {
        for (y = 0; y < MAP_H; y++) {
            for (x = 0; x < MAP_W; x++) {
                int idx = idx_xy(x, y);
                if (!water[idx]) continue;
                if (x > 0) land_dist[idx] = min(land_dist[idx], (unsigned char)(land_dist[idx - 1] + 1));
                if (y > 0) land_dist[idx] = min(land_dist[idx], (unsigned char)(land_dist[idx - MAP_W] + 1));
                if (x > 0 && y > 0) land_dist[idx] = min(land_dist[idx], (unsigned char)(land_dist[idx - MAP_W - 1] + 1));
                if (x + 1 < MAP_W && y > 0) land_dist[idx] = min(land_dist[idx], (unsigned char)(land_dist[idx - MAP_W + 1] + 1));
            }
        }
        for (y = MAP_H - 1; y >= 0; y--) {
            for (x = MAP_W - 1; x >= 0; x--) {
                int idx = idx_xy(x, y);
                if (!water[idx]) continue;
                if (x + 1 < MAP_W) land_dist[idx] = min(land_dist[idx], (unsigned char)(land_dist[idx + 1] + 1));
                if (y + 1 < MAP_H) land_dist[idx] = min(land_dist[idx], (unsigned char)(land_dist[idx + MAP_W] + 1));
                if (x + 1 < MAP_W && y + 1 < MAP_H) land_dist[idx] = min(land_dist[idx], (unsigned char)(land_dist[idx + MAP_W + 1] + 1));
                if (x > 0 && y + 1 < MAP_H) land_dist[idx] = min(land_dist[idx], (unsigned char)(land_dist[idx + MAP_W - 1] + 1));
            }
        }
    }
    for (x = 0; x < total; x++) if (land_dist[x] > 30) land_dist[x] = 30;
    cached_revision = dirty_revision_coast();
    cached_w = MAP_W;
    cached_h = MAP_H;
}

static void ensure_nav_field(void) {
    if (cached_revision != dirty_revision_coast() || cached_w != MAP_W || cached_h != MAP_H) {
        rebuild_nav_field();
    }
}

int sea_nav_distance_to_land(int x, int y) {
    ensure_nav_field();
    if (!in_bounds(x, y)) return 0;
    return land_dist[idx_xy(x, y)];
}

int sea_nav_segment_water(MapPoint a, MapPoint b) {
    return sea_nav_segment_water_mode(a, b, SEA_NAV_DEEP_ALLOWED, -1);
}

int sea_nav_segment_water_mode(MapPoint a, MapPoint b, SeaNavMode mode, int shallow_region) {
    int dx = b.x - a.x;
    int dy = b.y - a.y;
    int steps = max(abs(dx), abs(dy));
    int i;
    ensure_nav_field();
    for (i = 0; i <= steps; i++) {
        int x = a.x + dx * i / max(1, steps);
        int y = a.y + dy * i / max(1, steps);
        if (!nav_tile_ok(x, y, mode, shallow_region)) return 0;
    }
    return 1;
}

static void heap_push(int idx, int score) {
    int pos;
    if (heap_count >= SEA_NAV_MAX_CELLS) return;
    pos = heap_count++;
    while (pos > 0) {
        int parent = (pos - 1) / 2;
        if (heap[parent].score <= score) break;
        heap[pos] = heap[parent];
        pos = parent;
    }
    heap[pos].idx = idx;
    heap[pos].score = score;
}

static int heap_pop(void) {
    HeapNode root;
    HeapNode tail;
    int pos = 0;
    if (heap_count <= 0) return -1;
    root = heap[0];
    tail = heap[--heap_count];
    while (1) {
        int child = pos * 2 + 1;
        if (child >= heap_count) break;
        if (child + 1 < heap_count && heap[child + 1].score < heap[child].score) child++;
        if (heap[child].score >= tail.score) break;
        heap[pos] = heap[child];
        pos = child;
    }
    if (heap_count > 0) heap[pos] = tail;
    return root.idx;
}

static int heuristic(int x, int y, int gx, int gy) {
    int dx = abs(gx - x);
    int dy = abs(gy - y);
    return max(dx, dy) * 10 + min(dx, dy) * 4;
}

static int shore_penalty(int idx) {
    int d = land_dist[idx];
    if (d <= 1) return 90;
    if (d == 2) return 45;
    if (d <= 4) return 18;
    if (d > 14) return 8;
    return 0;
}

static int reconstruct_path(int end_idx, MapPoint *out, int max_points) {
    MapPoint reverse[MAX_MARITIME_ROUTE_POINTS * 4];
    int count = 0;
    int idx = end_idx;
    int i;
    while (idx >= 0 && count < (int)(sizeof(reverse) / sizeof(reverse[0]))) {
        reverse[count].x = idx % MAP_W;
        reverse[count].y = idx / MAP_W;
        count++;
        idx = nav_prev[idx];
    }
    if (count <= 1) return 0;
    for (i = 0; i < count && i < max_points; i++) out[i] = reverse[count - 1 - i];
    return min(count, max_points);
}

int sea_nav_find_path_mode(MapPoint start, MapPoint goal, SeaNavMode mode, int shallow_region, MapPoint *out, int max_points) {
    static const int dirs[8][2] = {{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1},{0,-1},{1,-1}};
    int start_idx;
    int goal_idx;
    int visited = 0;

    ensure_nav_field();
    if (!out || max_points <= 1 || !in_bounds(start.x, start.y) || !in_bounds(goal.x, goal.y)) return 0;
    start_idx = idx_xy(start.x, start.y);
    goal_idx = idx_xy(goal.x, goal.y);
    if (!nav_tile_ok(start.x, start.y, mode, shallow_region) || !nav_tile_ok(goal.x, goal.y, mode, shallow_region)) return 0;
    if (++nav_stamp <= 0) { memset(nav_seen, 0, sizeof(nav_seen)); memset(nav_closed, 0, sizeof(nav_closed)); nav_stamp = 1; }
    heap_count = 0;
    nav_seen[start_idx] = nav_stamp;
    nav_cost[start_idx] = 0;
    nav_prev[start_idx] = -1;
    nav_dir[start_idx] = 255;
    heap_push(start_idx, heuristic(start.x, start.y, goal.x, goal.y));
    while (heap_count > 0) {
        int current = heap_pop();
        int cx = current % MAP_W;
        int cy = current / MAP_W;
        int d;
        if (nav_closed[current] == nav_stamp) continue;
        nav_closed[current] = nav_stamp;
        visited++;
        if (current == goal_idx) {
            profiler_add_maritime_path_search(visited);
            return reconstruct_path(goal_idx, out, max_points);
        }
        for (d = 0; d < 8; d++) {
            int nx = cx + dirs[d][0], ny = cy + dirs[d][1];
            int nidx, step, turn, next_cost;
            if (!in_bounds(nx, ny)) continue;
            nidx = idx_xy(nx, ny);
            if (!nav_tile_ok(nx, ny, mode, shallow_region) || nav_closed[nidx] == nav_stamp) continue;
            if (dirs[d][0] && dirs[d][1] &&
                (!nav_tile_ok(cx + dirs[d][0], cy, mode, shallow_region) ||
                 !nav_tile_ok(cx, cy + dirs[d][1], mode, shallow_region))) continue;
            step = dirs[d][0] && dirs[d][1] ? 14 : 10;
            turn = nav_dir[current] == 255 || nav_dir[current] == d ? 0 : 10;
            next_cost = nav_cost[current] + step + shore_penalty(nidx) + turn;
            if (nav_seen[nidx] != nav_stamp || next_cost < nav_cost[nidx]) {
                nav_seen[nidx] = nav_stamp;
                nav_cost[nidx] = next_cost;
                nav_prev[nidx] = current;
                nav_dir[nidx] = (unsigned char)d;
                heap_push(nidx, next_cost + heuristic(nx, ny, goal.x, goal.y));
            }
        }
    }
    profiler_add_maritime_path_search(visited);
    return 0;
}

int sea_nav_find_path(MapPoint start, MapPoint goal, MapPoint *out, int max_points) {
    return sea_nav_find_path_mode(start, goal, SEA_NAV_DEEP_ALLOWED, -1, out, max_points);
}
