#include "province_partition.h"

#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "sim/province.h"
#include "sim/regions.h"
#include "world/terrain_query.h"

#include <limits.h>
#include <stdlib.h>

typedef struct {
    int x;
    int y;
    int city_id;
    int cost;
} ProvincePartitionNode;

static int partition_dirty[MAX_CIVS];
static int any_partition_dirty;
static int partition_cost[MAX_MAP_H][MAX_MAP_W];
static int partition_assign[MAX_MAP_H][MAX_MAP_W];
static ProvincePartitionNode heap_nodes[MAX_MAP_W * MAX_MAP_H * 2];
static int heap_count;

static void heap_push(ProvincePartitionNode node) {
    int i;

    if (heap_count >= (int)(sizeof(heap_nodes) / sizeof(heap_nodes[0]))) return;
    i = heap_count++;
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (heap_nodes[parent].cost <= node.cost) break;
        heap_nodes[i] = heap_nodes[parent];
        i = parent;
    }
    heap_nodes[i] = node;
}

static int heap_pop(ProvincePartitionNode *out) {
    ProvincePartitionNode last;
    int i = 0;

    if (heap_count <= 0) return 0;
    *out = heap_nodes[0];
    last = heap_nodes[--heap_count];
    while (1) {
        int child = i * 2 + 1;
        if (child >= heap_count) break;
        if (child + 1 < heap_count && heap_nodes[child + 1].cost < heap_nodes[child].cost) child++;
        if (last.cost <= heap_nodes[child].cost) break;
        heap_nodes[i] = heap_nodes[child];
        i = child;
    }
    if (heap_count > 0) heap_nodes[i] = last;
    return 1;
}

void province_mark_partition_dirty(int owner) {
    if (owner < 0 || owner >= MAX_CIVS) return;
    partition_dirty[owner] = 1;
    any_partition_dirty = 1;
}

void province_mark_all_partitions_dirty(void) {
    int i;

    for (i = 0; i < MAX_CIVS; i++) partition_dirty[i] = 1;
    any_partition_dirty = 1;
}

static int owner_city_count(int owner) {
    int i;
    int count = 0;

    for (i = 0; i < city_count; i++) {
        if (cities[i].alive && cities[i].owner == owner) count++;
    }
    return count;
}

static int partition_edge_cost(int city_id, int x, int y, int nx, int ny) {
    Tile *from = &world[y][x];
    Tile *to = &world[ny][nx];
    City *city = &cities[city_id];
    TerrainStats stats = tile_stats(nx, ny);
    int dx = abs(nx - city->x);
    int dy = abs(ny - city->y);
    int cost = 8 + world_tile_cost(nx, ny) * 3;

    cost += abs(to->elevation - from->elevation) / 6;
    cost += (dx + dy) / 24;
    if (to->geography != from->geography) cost += 7;
    if (to->climate != from->climate) cost += 4;
    if (to->ecology != from->ecology) cost += 2;
    if (to->geography == GEO_MOUNTAIN || from->geography == GEO_MOUNTAIN) cost += 18;
    if (to->geography == GEO_CANYON || from->geography == GEO_CANYON) cost += 16;
    if (to->river != from->river) cost += world[city->y][city->x].river ? 5 : 14;
    else if (to->river && from->river) cost -= world[city->y][city->x].river ? 5 : 2;
    if (city->port && world_is_coastal_land_tile(nx, ny)) cost -= 3;
    if (stats.habitability >= 7) cost -= 2;
    if (stats.water >= 7) cost -= 1;
    return clamp(cost, 2, 90);
}

static void initialize_owner_partition(int owner) {
    int x;
    int y;

    heap_count = 0;
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            partition_cost[y][x] = INT_MAX;
            partition_assign[y][x] = -1;
            if (world[y][x].owner == owner && is_land(world[y][x].geography)) world[y][x].province_id = -1;
        }
    }
}

static void seed_owner_cities(int owner) {
    int i;

    for (i = 0; i < city_count; i++) {
        City *city = &cities[i];
        ProvincePartitionNode node;
        if (!city->alive || city->owner != owner) continue;
        if (city->x < 0 || city->x >= MAP_W || city->y < 0 || city->y >= MAP_H) continue;
        if (!is_land(world[city->y][city->x].geography)) continue;
        world[city->y][city->x].owner = owner;
        partition_cost[city->y][city->x] = 0;
        partition_assign[city->y][city->x] = i;
        node.x = city->x;
        node.y = city->y;
        node.city_id = i;
        node.cost = 0;
        heap_push(node);
    }
}

static int nearest_owner_city(int owner, int x, int y) {
    int best = -1;
    int best_dist = MAP_W * MAP_W + MAP_H * MAP_H;
    int i;

    for (i = 0; i < city_count; i++) {
        int dx;
        int dy;
        int dist;
        if (!cities[i].alive || cities[i].owner != owner) continue;
        dx = x - cities[i].x;
        dy = y - cities[i].y;
        dist = dx * dx + dy * dy;
        if (dist < best_dist) {
            best_dist = dist;
            best = i;
        }
    }
    return best;
}

static int best_neighbor_province(int owner, int x, int y, int current, int *same_count) {
    static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int counts[4] = {-1, -1, -1, -1};
    int ids[4] = {-1, -1, -1, -1};
    int best_id = -1;
    int best_count = 0;
    int i;

    *same_count = 0;
    for (i = 0; i < 4; i++) {
        int nx = x + dirs[i][0];
        int ny = y + dirs[i][1];
        int id;
        int slot;
        if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
        if (world[ny][nx].owner != owner) continue;
        id = world[ny][nx].province_id;
        if (id == current) {
            (*same_count)++;
            continue;
        }
        if (id < 0) continue;
        for (slot = 0; slot < 4 && ids[slot] != id && ids[slot] != -1; slot++) {}
        if (slot >= 4) continue;
        if (ids[slot] == -1) {
            ids[slot] = id;
            counts[slot] = 0;
        }
        counts[slot]++;
        if (counts[slot] > best_count) {
            best_count = counts[slot];
            best_id = id;
        }
    }
    return best_id;
}

static void smooth_single_tile_corridors(int owner) {
    int x;
    int y;

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int current;
            int same = 0;
            int best;
            if (world[y][x].owner != owner || !is_land(world[y][x].geography)) continue;
            current = world[y][x].province_id;
            if (current >= 0 && current < city_count && cities[current].x == x && cities[current].y == y) continue;
            best = best_neighbor_province(owner, x, y, current, &same);
            if (best >= 0 && same <= 1) world[y][x].province_id = best;
        }
    }
}

static int region_admin_city(int region_id, int owner) {
    const NaturalRegion *region = regions_get(region_id);
    int admin;

    if (!region || region->owner_civ != owner) return -1;
    admin = region->city_id;
    if (admin >= 0 && admin < city_count && cities[admin].alive && cities[admin].owner == owner) return admin;
    return -1;
}

static int repartition_owner_from_regions(int owner) {
    int x;
    int y;
    int handled_any = 0;

    if (region_count <= 0) return 0;
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int admin;
            if (world[y][x].owner != owner || !is_land(world[y][x].geography)) continue;
            admin = region_admin_city(world[y][x].region_id, owner);
            if (admin < 0) admin = nearest_owner_city(owner, x, y);
            world[y][x].province_id = admin;
            handled_any = 1;
        }
    }
    return handled_any;
}

void province_repartition_owner(int owner) {
    static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    ProvincePartitionNode node;
    int x;
    int y;

    if (owner < 0 || owner >= civ_count || owner_city_count(owner) <= 0) return;
    if (repartition_owner_from_regions(owner)) {
        partition_dirty[owner] = 0;
        province_invalidate_region_cache();
        dirty_mark_province();
        return;
    }
    initialize_owner_partition(owner);
    seed_owner_cities(owner);
    while (heap_pop(&node)) {
        int i;
        if (node.cost != partition_cost[node.y][node.x] ||
            node.city_id != partition_assign[node.y][node.x]) continue;
        for (i = 0; i < 4; i++) {
            int nx = node.x + dirs[i][0];
            int ny = node.y + dirs[i][1];
            int next_cost;
            if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
            if (world[ny][nx].owner != owner || !is_land(world[ny][nx].geography)) continue;
            next_cost = node.cost + partition_edge_cost(node.city_id, node.x, node.y, nx, ny);
            if (next_cost >= partition_cost[ny][nx]) continue;
            partition_cost[ny][nx] = next_cost;
            partition_assign[ny][nx] = node.city_id;
            {
                ProvincePartitionNode next = {nx, ny, node.city_id, next_cost};
                heap_push(next);
            }
        }
    }
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            if (world[y][x].owner != owner || !is_land(world[y][x].geography)) continue;
            world[y][x].province_id = partition_assign[y][x] >= 0 ?
                                      partition_assign[y][x] : nearest_owner_city(owner, x, y);
        }
    }
    smooth_single_tile_corridors(owner);
    partition_dirty[owner] = 0;
    province_invalidate_region_cache();
    dirty_mark_province();
}

void province_repartition_dirty(void) {
    int i;
    int any = 0;

    if (!any_partition_dirty) return;
    for (i = 0; i < civ_count; i++) {
        if (!partition_dirty[i]) continue;
        province_repartition_owner(i);
    }
    for (i = 0; i < MAX_CIVS; i++) {
        if (partition_dirty[i]) {
            any = 1;
            break;
        }
    }
    any_partition_dirty = any;
}
