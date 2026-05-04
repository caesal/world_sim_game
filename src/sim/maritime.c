#include "maritime.h"

#include "sim/population.h"
#include "sim/plague.h"
#include "sim/ports.h"
#include "sim/simulation.h"
#include "world/ports.h"
#include "world/terrain_query.h"

#include <stdlib.h>
#include <string.h>

#define MARITIME_LINKS_PER_PORT 3
#define MARITIME_MAX_ROUTE_DISTANCE 340
#define MARITIME_MAX_SEARCH_NODES 52000
#define MARITIME_TARGET_KEEP 8

static POINT sea_queue[MAP_W * MAP_H];
static POINT raw_path[MARITIME_MAX_ROUTE_DISTANCE + 2];
static unsigned short sea_dist[MAP_H][MAP_W];
static int sea_mark[MAP_H][MAP_W];
static signed char sea_prev_dir[MAP_H][MAP_W];
static int sea_mark_id = 1;
static int maritime_routes_dirty = 1;

typedef struct {
    int x;
    int y;
    int sea_x;
    int sea_y;
    int region;
    int score;
    int direct_distance;
} MaritimeTarget;

void maritime_reset(void) {
    memset(maritime_routes, 0, sizeof(maritime_routes));
    maritime_route_count = 0;
    maritime_routes_dirty = 1;
    world_visual_revision++;
}

void maritime_mark_routes_dirty(void) {
    maritime_routes_dirty = 1;
}

void maritime_ensure_routes(void) {
    if (maritime_routes_dirty) maritime_rebuild_routes();
}

static int city_sea_entry(int city_id, int *sea_x, int *sea_y, int *region) {
    City *city;

    if (!ports_city_is_valid_port(city_id)) return 0;
    city = &cities[city_id];
    if (!ports_find_nearby_sea_entry(city->port_x, city->port_y, sea_x, sea_y)) return 0;
    if (region) *region = ports_tile_shallow_region_near_land(*sea_x, *sea_y);
    return region == NULL || *region >= 0;
}

static int route_exists(int city_a, int city_b) {
    int i;

    for (i = 0; i < maritime_route_count; i++) {
        MaritimeRoute *route = &maritime_routes[i];
        if (!route->active) continue;
        if ((route->from_city == city_a && route->to_city == city_b) ||
            (route->from_city == city_b && route->to_city == city_a)) return 1;
    }
    return 0;
}

static int route_tile_ok(int x, int y, int region) {
    if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) return 0;
    return ports_tile_shallow_region_near_land(x, y) == region;
}

static void append_sampled_point(POINT *points, int *count, POINT point) {
    if (*count <= 0 || points[*count - 1].x != point.x || points[*count - 1].y != point.y) {
        if (*count < MAX_MARITIME_ROUTE_POINTS) points[(*count)++] = point;
    }
}

static int sample_route_points(int raw_count, POINT *points, int *point_count) {
    int i;
    int stride = raw_count / (MAX_MARITIME_ROUTE_POINTS - 2) + 1;
    int last_dx = 0;
    int last_dy = 0;

    *point_count = 0;
    if (raw_count < 2) return 0;
    append_sampled_point(points, point_count, raw_path[raw_count - 1]);
    for (i = raw_count - 2; i > 0; i--) {
        int dx = raw_path[i - 1].x - raw_path[i].x;
        int dy = raw_path[i - 1].y - raw_path[i].y;
        int bend = i < raw_count - 2 && (dx != last_dx || dy != last_dy);
        if (bend || (raw_count - 1 - i) % stride == 0) append_sampled_point(points, point_count, raw_path[i]);
        last_dx = dx;
        last_dy = dy;
    }
    append_sampled_point(points, point_count, raw_path[0]);
    return *point_count >= 2;
}

static int find_sea_path(int sx, int sy, int ex, int ey, int region,
                         POINT *points, int *point_count, int *distance) {
    static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int head = 0;
    int tail = 0;
    int found = 0;
    int raw_count = 0;
    int cx;
    int cy;

    if (!route_tile_ok(sx, sy, region) || !route_tile_ok(ex, ey, region)) return 0;
    if (++sea_mark_id == 0) {
        memset(sea_mark, 0, sizeof(sea_mark));
        sea_mark_id = 1;
    }
    sea_queue[tail].x = sx;
    sea_queue[tail].y = sy;
    sea_mark[sy][sx] = sea_mark_id;
    sea_dist[sy][sx] = 0;
    sea_prev_dir[sy][sx] = -1;
    tail++;

    while (head < tail && tail < MARITIME_MAX_SEARCH_NODES) {
        POINT node = sea_queue[head++];
        int i;
        if (node.x == ex && node.y == ey) {
            found = 1;
            break;
        }
        if (sea_dist[node.y][node.x] >= MARITIME_MAX_ROUTE_DISTANCE) continue;
        for (i = 0; i < 4; i++) {
            int nx = node.x + dirs[i][0];
            int ny = node.y + dirs[i][1];
            if (!route_tile_ok(nx, ny, region) || sea_mark[ny][nx] == sea_mark_id) continue;
            sea_mark[ny][nx] = sea_mark_id;
            sea_dist[ny][nx] = sea_dist[node.y][node.x] + 1;
            sea_prev_dir[ny][nx] = (signed char)i;
            sea_queue[tail].x = nx;
            sea_queue[tail].y = ny;
            tail++;
        }
    }
    if (!found) return 0;
    cx = ex;
    cy = ey;
    while (raw_count < MARITIME_MAX_ROUTE_DISTANCE + 2) {
        int dir = sea_prev_dir[cy][cx];
        raw_path[raw_count].x = cx;
        raw_path[raw_count].y = cy;
        raw_count++;
        if (cx == sx && cy == sy) break;
        if (dir < 0) return 0;
        cx -= dirs[dir][0];
        cy -= dirs[dir][1];
    }
    if (distance) *distance = sea_dist[ey][ex];
    return sample_route_points(raw_count, points, point_count);
}

static int add_route_if_path_exists(int city_a, int city_b) {
    MaritimeRoute *route;
    POINT points[MAX_MARITIME_ROUTE_POINTS];
    int point_count = 0;
    int distance = 0;
    int ax;
    int ay;
    int bx;
    int by;
    int region_a;
    int region_b;

    if (maritime_route_count >= MAX_MARITIME_ROUTES || route_exists(city_a, city_b)) return 0;
    if (!city_sea_entry(city_a, &ax, &ay, &region_a) || !city_sea_entry(city_b, &bx, &by, &region_b)) return 0;
    if (region_a < 0 || region_a != region_b) return 0;
    if (abs(ax - bx) + abs(ay - by) > MARITIME_MAX_ROUTE_DISTANCE) return 0;
    if (!find_sea_path(ax, ay, bx, by, region_a, points, &point_count, &distance)) return 0;
    route = &maritime_routes[maritime_route_count++];
    memset(route, 0, sizeof(*route));
    route->active = 1;
    route->from_city = city_a;
    route->to_city = city_b;
    route->sea_region = region_a;
    route->distance = distance;
    route->point_count = point_count;
    memcpy(route->points, points, (size_t)point_count * sizeof(points[0]));
    return 1;
}

static void select_nearest_ports(int city_id, int ports[MAX_CITIES], int port_count,
                                 int best_city[MARITIME_LINKS_PER_PORT]) {
    int best_dist[MARITIME_LINKS_PER_PORT];
    int i;
    int slot;

    for (slot = 0; slot < MARITIME_LINKS_PER_PORT; slot++) {
        best_city[slot] = -1;
        best_dist[slot] = 1000000;
    }
    for (i = 0; i < port_count; i++) {
        int other = ports[i];
        int dist;
        if (other == city_id || !ports_same_region(city_id, other)) continue;
        dist = abs(cities[city_id].port_x - cities[other].port_x) +
               abs(cities[city_id].port_y - cities[other].port_y);
        if (dist > MARITIME_MAX_ROUTE_DISTANCE) continue;
        for (slot = 0; slot < MARITIME_LINKS_PER_PORT; slot++) {
            if (dist < best_dist[slot]) {
                int move;
                for (move = MARITIME_LINKS_PER_PORT - 1; move > slot; move--) {
                    best_dist[move] = best_dist[move - 1];
                    best_city[move] = best_city[move - 1];
                }
                best_dist[slot] = dist;
                best_city[slot] = other;
                break;
            }
        }
    }
}

void maritime_rebuild_routes(void) {
    int ports[MAX_CITIES];
    int port_count = 0;
    int i;

    memset(maritime_routes, 0, sizeof(maritime_routes));
    maritime_route_count = 0;
    ports_refresh_city_regions();
    for (i = 0; i < city_count; i++) {
        if (ports_city_is_valid_port(i)) ports[port_count++] = i;
    }
    for (i = 0; i < port_count && maritime_route_count < MAX_MARITIME_ROUTES; i++) {
        int best_city[MARITIME_LINKS_PER_PORT];
        int slot;
        select_nearest_ports(ports[i], ports, port_count, best_city);
        for (slot = 0; slot < MARITIME_LINKS_PER_PORT; slot++) {
            if (best_city[slot] >= 0) add_route_if_path_exists(ports[i], best_city[slot]);
        }
    }
    maritime_routes_dirty = 0;
    world_visual_revision++;
}

int maritime_route_between_cities(int city_a, int city_b, int *distance) {
    int i;

    maritime_ensure_routes();
    for (i = 0; i < maritime_route_count; i++) {
        MaritimeRoute *route = &maritime_routes[i];
        if (!route->active) continue;
        if ((route->from_city == city_a && route->to_city == city_b) ||
            (route->from_city == city_b && route->to_city == city_a)) {
            if (distance) *distance = route->distance;
            return 1;
        }
    }
    return 0;
}

void maritime_update_migration(void) {
    int i;
    int changed = 0;

    maritime_ensure_routes();
    for (i = 0; i < maritime_route_count; i++) {
        MaritimeRoute *route = &maritime_routes[i];
        City *a;
        City *b;
        City *donor;
        City *receiver;
        int diff;
        int migrants;
        if (!route->active) continue;
        a = &cities[route->from_city];
        b = &cities[route->to_city];
        if (!a->alive || !b->alive || a->owner < 0 || a->owner != b->owner) continue;
        donor = a->population >= b->population ? a : b;
        receiver = donor == a ? b : a;
        diff = donor->population - receiver->population;
        if (diff < 40) continue;
        migrants = clamp(diff / (130 + route->distance * 2), 1, 5);
        {
            int from_city = (int)(donor - cities);
            int to_city = (int)(receiver - cities);
            int moved = population_migrate_between_cities(from_city, to_city, migrants);
            if (moved > 0) {
                plague_notify_migration(from_city, to_city, moved);
                changed = 1;
            }
        }
    }
    if (changed) world_invalidate_region_cache();
}

int maritime_has_contact(int civ_a, int civ_b) {
    int i;

    maritime_ensure_routes();
    for (i = 0; i < maritime_route_count; i++) {
        City *a;
        City *b;
        if (!maritime_routes[i].active) continue;
        a = &cities[maritime_routes[i].from_city];
        b = &cities[maritime_routes[i].to_city];
        if (!a->alive || !b->alive) continue;
        if ((a->owner == civ_a && b->owner == civ_b) || (a->owner == civ_b && b->owner == civ_a)) return 1;
    }
    return 0;
}

int maritime_trade_bonus(int civ_a, int civ_b) {
    int i;
    int best = 0;

    maritime_ensure_routes();
    for (i = 0; i < maritime_route_count; i++) {
        MaritimeRoute *route = &maritime_routes[i];
        City *a;
        City *b;
        int bonus;
        if (!route->active) continue;
        a = &cities[route->from_city];
        b = &cities[route->to_city];
        if (!a->alive || !b->alive) continue;
        if (!((a->owner == civ_a && b->owner == civ_b) || (a->owner == civ_b && b->owner == civ_a))) continue;
        bonus = clamp(18 - route->distance / 24, 3, 18);
        if (bonus > best) best = bonus;
    }
    return best;
}

static int civ_port_in_region(int civ_id, int region, int target_x, int target_y,
                              int *port_city, int *distance) {
    int i;
    int best_city = -1;
    int best_dist = 1000000;

    for (i = 0; i < city_count; i++) {
        int dist;
        if (!ports_city_is_valid_port(i) || cities[i].owner != civ_id || cities[i].port_region != region) continue;
        dist = abs(cities[i].port_x - target_x) + abs(cities[i].port_y - target_y);
        if (dist < best_dist) {
            best_dist = dist;
            best_city = i;
        }
    }
    if (best_city < 0) return 0;
    if (port_city) *port_city = best_city;
    if (distance) *distance = best_dist;
    return 1;
}

static void keep_maritime_target(MaritimeTarget targets[MARITIME_TARGET_KEEP], MaritimeTarget candidate) {
    int i;

    for (i = 0; i < MARITIME_TARGET_KEEP; i++) {
        if (candidate.score > targets[i].score) {
            int move;
            for (move = MARITIME_TARGET_KEEP - 1; move > i; move--) targets[move] = targets[move - 1];
            targets[i] = candidate;
            return;
        }
    }
}

static int target_base_score(int civ_id, int x, int y, int pressure) {
    TerrainStats stats = tile_stats(x, y);
    int score = world_terrain_resource_value(stats);

    score += stats.water * 5 + stats.food * 4 + stats.pop_capacity * 4 + stats.money * 4;
    score += stats.wood * 2 + stats.stone * 2 + stats.minerals * 3 + pressure * 8;
    score += civs[civ_id].logistics * 4 + civs[civ_id].commerce * 4 + civs[civ_id].expansion * 3;
    score -= world_tile_cost(x, y) * 8;
    if (world_nearby_enemy_border(civ_id, x, y, 3)) score -= 110;
    return score;
}

static int collect_overseas_targets(int civ_id, int pressure, MaritimeTarget targets[MARITIME_TARGET_KEEP]) {
    int tries;
    int found = 0;

    for (tries = 0; tries < MARITIME_TARGET_KEEP; tries++) targets[tries].score = -1000000;
    for (tries = 0; tries < 850; tries++) {
        MaritimeTarget candidate;
        int port_city;
        int direct;
        int x = rnd(MAP_W);
        int y = rnd(MAP_H);
        if (!is_land(world[y][x].geography) || world[y][x].owner != -1 || world[y][x].province_id != -1) continue;
        if (!world_is_coastal_land_tile(x, y) || city_at(x, y) >= 0) continue;
        if (!world_city_site_has_room(x, y, civ_id, 4)) continue;
        if (!ports_find_nearby_sea_entry(x, y, &candidate.sea_x, &candidate.sea_y)) continue;
        candidate.region = ports_tile_shallow_region_near_land(candidate.sea_x, candidate.sea_y);
        if (!civ_port_in_region(civ_id, candidate.region, candidate.sea_x, candidate.sea_y, &port_city, &direct)) continue;
        candidate.x = x;
        candidate.y = y;
        candidate.direct_distance = direct;
        candidate.score = target_base_score(civ_id, x, y, pressure) - direct / 3 + rnd(24);
        keep_maritime_target(targets, candidate);
        found = 1;
    }
    return found;
}

static int path_distance_to_target(int civ_id, MaritimeTarget target, int *distance) {
    POINT points[MAX_MARITIME_ROUTE_POINTS];
    int point_count;
    int i;
    int best = 1000000;

    for (i = 0; i < city_count; i++) {
        int sx;
        int sy;
        int region;
        int dist;
        if (!ports_city_is_valid_port(i) || cities[i].owner != civ_id) continue;
        if (!city_sea_entry(i, &sx, &sy, &region) || region != target.region) continue;
        if (!find_sea_path(sx, sy, target.sea_x, target.sea_y, region, points, &point_count, &dist)) continue;
        if (dist < best) best = dist;
    }
    if (best == 1000000) return 0;
    if (distance) *distance = best;
    return 1;
}

void maritime_try_overseas_expansion(int civ_id, int resource_score, char *log, size_t log_size) {
    MaritimeTarget targets[MARITIME_TARGET_KEEP];
    int pressure = clamp(18 - resource_score, 0, 14);
    int i;
    int chance;

    if (!civs[civ_id].alive || city_count >= MAX_CITIES) return;
    maritime_ensure_routes();
    chance = clamp(5 + pressure * 4 + civs[civ_id].logistics * 2 +
                   civs[civ_id].commerce * 2 + civs[civ_id].expansion * 2, 4, 62);
    if (rnd(100) >= chance) return;
    if (!collect_overseas_targets(civ_id, pressure, targets)) return;
    for (i = 0; i < MARITIME_TARGET_KEEP && targets[i].score > -1000000; i++) {
        TerrainStats stats;
        int distance;
        int threshold;
        int city_id;
        if (!path_distance_to_target(civ_id, targets[i], &distance)) continue;
        targets[i].score -= distance / 2;
        threshold = 152 - pressure * 4 - civs[civ_id].logistics * 3 -
                    civs[civ_id].commerce * 2 - civs[civ_id].expansion * 2;
        if (targets[i].score < threshold) continue;
        stats = tile_stats(targets[i].x, targets[i].y);
        city_id = world_create_city(civ_id, targets[i].x, targets[i].y,
                                    30 + stats.food * 3 + stats.water * 3 + stats.money * 2 + rnd(35), 0);
        if (city_id < 0) continue;
        world_claim_city_region(city_id, civ_id);
        append_log(log, log_size, "%s founded an overseas province. ", civs[civ_id].name);
        return;
    }
}
