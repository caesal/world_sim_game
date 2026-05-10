#include "sim/sea_lanes.h"

#include "core/dirty_flags.h"
#include "core/game_state.h"
#include "core/profiler.h"
#include "sim/maritime.h"
#include "sim/ports.h"
#include "sim/sea_nav.h"
#include "sim/technology.h"
#include "world/ports.h"

#include <stdlib.h>
#include <string.h>

#define COASTAL_MAX_PATH 150
#define DEEP_MAX_DIRECT 520
#define MAX_DEEP_CANDIDATES 256

typedef struct {
    int city;
    int owner;
    int region;
    int network;
    int status;
    MapPoint sea;
} PortInfo;

typedef struct {
    int net_a;
    int net_b;
    int from_port;
    int to_port;
    int score;
} DeepCandidate;

static SeaLane lanes[MAX_SEA_LANES];
static PortInfo ports[MAX_CITIES];
static int port_count;
static int port_index_by_city[MAX_CITIES];
static int parent[MAX_CITIES];
static int lane_count;
static int cached_key = -1;
static SeaLaneStats last_stats;

static int point_distance(MapPoint a, MapPoint b) {
    return max(abs(a.x - b.x), abs(a.y - b.y));
}

static int find_root(int x) {
    while (parent[x] != x) {
        parent[x] = parent[parent[x]];
        x = parent[x];
    }
    return x;
}

static void union_ports(int a, int b) {
    int ra = find_root(a);
    int rb = find_root(b);
    if (ra != rb) parent[rb] = ra;
}

static int valid_lane_port(int city_id) {
    City *city;
    if (!ports_city_is_valid_port(city_id)) return 0;
    city = &cities[city_id];
    return city->owner >= 0 && city->owner < civ_count && civs[city->owner].alive;
}

static int cache_key(void) {
    int key = maritime_route_revision() * 31 + maritime_ownership_revision() * 17;
    int i;
    key += dirty_revision_coast() * 13 + city_count * 7 + civ_count;
    for (i = 0; i < civ_count; i++) {
        key = key * 33 + (civs[i].alive ? 1 : 0) + clamp(civs[i].tech_stage, 0, 10) * 3;
    }
    for (i = 0; i < city_count; i++) {
        if (!cities[i].alive) continue;
        key = key * 33 + cities[i].owner * 5 + cities[i].port_region + (cities[i].port ? 11 : 0);
    }
    return key;
}

static void collect_ports(void) {
    int i;
    memset(port_index_by_city, -1, sizeof(port_index_by_city));
    port_count = 0;
    for (i = 0; i < city_count && port_count < MAX_CITIES; i++) {
        City *city = &cities[i];
        int sx, sy;
        if (!valid_lane_port(i)) continue;
        if (!ports_find_nearby_sea_entry(city->port_x, city->port_y, &sx, &sy)) continue;
        ports[port_count].city = i;
        ports[port_count].owner = city->owner;
        ports[port_count].region = city->port_region;
        ports[port_count].network = port_count;
        ports[port_count].status = SEA_PORT_ISOLATED;
        ports[port_count].sea.x = sx;
        ports[port_count].sea.y = sy;
        port_index_by_city[i] = port_count;
        parent[port_count] = port_count;
        port_count++;
    }
}

static int lane_exists(int city_a, int city_b, int type) {
    int i;
    for (i = 0; i < lane_count; i++) {
        if (lanes[i].type != type) continue;
        if ((lanes[i].from_city == city_a && lanes[i].to_city == city_b) ||
            (lanes[i].from_city == city_b && lanes[i].to_city == city_a)) return 1;
    }
    return 0;
}

static int add_lane(int type, int net_a, int net_b, int from_idx, int to_idx, const MapPoint *points, int count) {
    SeaLane *lane;
    if (lane_count >= MAX_SEA_LANES || count < 2 || lane_exists(ports[from_idx].city, ports[to_idx].city, type)) {
        last_stats.skipped_routes++;
        return 0;
    }
    lane = &lanes[lane_count++];
    memset(lane, 0, sizeof(*lane));
    lane->active = 1;
    lane->type = type;
    lane->network_a = net_a;
    lane->network_b = net_b;
    lane->from_city = ports[from_idx].city;
    lane->to_city = ports[to_idx].city;
    lane->from_sea_entry = points[0];
    lane->to_sea_entry = points[count - 1];
    lane->point_count = min(count, MAX_SEA_LANE_POINTS);
    memcpy(lane->points, points, (size_t)lane->point_count * sizeof(points[0]));
    return 1;
}

static void try_add_shallow_lane(int a, int b) {
    MapPoint path[MAX_SEA_LANE_POINTS];
    int count;
    if (ports[a].region < 0 || ports[a].region != ports[b].region) return;
    if (lane_exists(ports[a].city, ports[b].city, SEA_LANE_SHALLOW)) return;
    count = sea_nav_find_path_mode(ports[a].sea, ports[b].sea, SEA_NAV_SHALLOW_ONLY,
                                   ports[a].region, path, MAX_SEA_LANE_POINTS);
    if (count < 2 || count > COASTAL_MAX_PATH) {
        last_stats.land_rejections++;
        return;
    }
    if (add_lane(SEA_LANE_SHALLOW, ports[a].network, ports[b].network, a, b, path, count)) {
        union_ports(a, b);
        ports[a].status = SEA_PORT_SHALLOW;
        ports[b].status = SEA_PORT_SHALLOW;
    }
}

static void build_coastal_networks(void) {
    int i;
    for (i = 0; i < port_count; i++) {
        int best[2] = {-1, -1};
        int best_dist[2] = {999999, 999999};
        int j;
        for (j = 0; j < port_count; j++) {
            int dist;
            if (i == j || ports[i].region < 0 || ports[i].region != ports[j].region) continue;
            dist = point_distance(ports[i].sea, ports[j].sea);
            if (dist < best_dist[0]) {
                best[1] = best[0]; best_dist[1] = best_dist[0];
                best[0] = j; best_dist[0] = dist;
            } else if (dist < best_dist[1]) {
                best[1] = j; best_dist[1] = dist;
            }
        }
        for (j = 0; j < 2; j++) if (best[j] >= 0) try_add_shallow_lane(i, best[j]);
    }
    for (i = 0; i < port_count; i++) ports[i].network = find_root(i);
}

static int deep_pair_exists(int net_a, int net_b) {
    int a = min(net_a, net_b);
    int b = max(net_a, net_b);
    int i;
    for (i = 0; i < lane_count; i++) {
        if (lanes[i].type == SEA_LANE_DEEP &&
            min(lanes[i].network_a, lanes[i].network_b) == a &&
            max(lanes[i].network_a, lanes[i].network_b) == b) return 1;
    }
    return 0;
}

static int deep_pair_allowed(int a, int b) {
    int same_owner = ports[a].owner == ports[b].owner;
    if (same_owner) return technology_deep_sea_unlocked(ports[a].owner);
    return technology_deep_sea_unlocked(ports[a].owner) && technology_deep_sea_unlocked(ports[b].owner);
}

static void remember_deep_candidate(DeepCandidate *cands, int *count, int a, int b, int score) {
    int net_a = min(ports[a].network, ports[b].network);
    int net_b = max(ports[a].network, ports[b].network);
    int i;
    if (net_a == net_b || deep_pair_exists(net_a, net_b)) return;
    for (i = 0; i < *count; i++) {
        if (cands[i].net_a == net_a && cands[i].net_b == net_b) {
            if (score < cands[i].score) {
                cands[i].from_port = a; cands[i].to_port = b; cands[i].score = score;
            }
            return;
        }
    }
    if (*count >= MAX_DEEP_CANDIDATES) return;
    cands[*count].net_a = net_a;
    cands[*count].net_b = net_b;
    cands[*count].from_port = a;
    cands[*count].to_port = b;
    cands[*count].score = score;
    (*count)++;
}

static int compare_deep_candidate(const void *a, const void *b) {
    const DeepCandidate *pa = (const DeepCandidate *)a;
    const DeepCandidate *pb = (const DeepCandidate *)b;
    return pa->score - pb->score;
}

static void build_deep_networks(void) {
    DeepCandidate cands[MAX_DEEP_CANDIDATES];
    int deep_degree[MAX_CITIES];
    int cand_count = 0;
    int i, j;
    memset(deep_degree, 0, sizeof(deep_degree));
    for (i = 0; i < port_count; i++) {
        for (j = i + 1; j < port_count; j++) {
            int direct;
            if (ports[i].network == ports[j].network || !deep_pair_allowed(i, j)) continue;
            direct = point_distance(ports[i].sea, ports[j].sea);
            if (direct > DEEP_MAX_DIRECT) continue;
            remember_deep_candidate(cands, &cand_count, i, j, direct);
        }
    }
    qsort(cands, (size_t)cand_count, sizeof(cands[0]), compare_deep_candidate);
    for (i = 0; i < cand_count && lane_count < MAX_SEA_LANES; i++) {
        MapPoint path[MAX_SEA_LANE_POINTS];
        int a = cands[i].from_port;
        int b = cands[i].to_port;
        int count = sea_nav_find_path_mode(ports[a].sea, ports[b].sea, SEA_NAV_DEEP_ALLOWED, -1,
                                           path, MAX_SEA_LANE_POINTS);
        if (deep_degree[cands[i].net_a] >= 2 || deep_degree[cands[i].net_b] >= 2) continue;
        if (count < 2) {
            last_stats.land_rejections++;
            continue;
        }
        if (add_lane(SEA_LANE_DEEP, cands[i].net_a, cands[i].net_b, a, b, path, count)) {
            ports[a].status = SEA_PORT_DEEP_GATEWAY;
            ports[b].status = SEA_PORT_DEEP_GATEWAY;
            deep_degree[cands[i].net_a]++;
            deep_degree[cands[i].net_b]++;
        }
    }
}

static void rebuild_lanes(void) {
    ProfilerCallTrace trace = profiler_call_begin();
    memset(lanes, 0, sizeof(lanes));
    memset(&last_stats, 0, sizeof(last_stats));
    lane_count = 0;
    collect_ports();
    last_stats.logical_routes = port_count;
    build_coastal_networks();
    build_deep_networks();
    last_stats.visual_lanes = lane_count;
    cached_key = cache_key();
    last_stats.rebuild_ms = profiler_call_end_quiet("sea_lane_rebuild", -1, -1, trace);
    profiler_record_sea_lanes(last_stats.rebuild_ms, last_stats.visual_lanes,
                              last_stats.merged_routes, last_stats.skipped_routes,
                              last_stats.land_rejections);
}

const SeaLane *sea_lanes_get(int *out_count) {
    int key = cache_key();
    if (cached_key != key) rebuild_lanes();
    if (out_count) *out_count = lane_count;
    return lanes;
}

void sea_lanes_invalidate(void) {
    cached_key = -1;
}

void sea_lanes_last_stats(SeaLaneStats *out) {
    if (out) *out = last_stats;
}

int sea_lanes_city_status(int city_id) {
    int idx;
    sea_lanes_get(NULL);
    if (city_id < 0 || city_id >= MAX_CITIES) return SEA_PORT_ISOLATED;
    idx = port_index_by_city[city_id];
    return idx >= 0 ? ports[idx].status : SEA_PORT_ISOLATED;
}

int sea_lanes_city_network(int city_id) {
    int idx;
    sea_lanes_get(NULL);
    if (city_id < 0 || city_id >= MAX_CITIES) return -1;
    idx = port_index_by_city[city_id];
    return idx >= 0 ? ports[idx].network : -1;
}

int sea_lanes_connected(int city_a, int city_b, int *out_distance) {
    int i;
    sea_lanes_get(NULL);
    for (i = 0; i < lane_count; i++) {
        SeaLane *lane = &lanes[i];
        if ((lane->from_city == city_a && lane->to_city == city_b) ||
            (lane->from_city == city_b && lane->to_city == city_a)) {
            if (out_distance) *out_distance = lane->point_count;
            return 1;
        }
    }
    return 0;
}

int sea_lanes_has_contact(int civ_a, int civ_b) {
    int i;
    sea_lanes_get(NULL);
    for (i = 0; i < lane_count; i++) {
        int owner_a = cities[lanes[i].from_city].owner;
        int owner_b = cities[lanes[i].to_city].owner;
        if ((owner_a == civ_a && owner_b == civ_b) || (owner_a == civ_b && owner_b == civ_a)) return 1;
    }
    return 0;
}

int sea_lanes_trade_bonus(int civ_a, int civ_b) {
    int i;
    int best = 0;
    sea_lanes_get(NULL);
    for (i = 0; i < lane_count; i++) {
        int owner_a = cities[lanes[i].from_city].owner;
        int owner_b = cities[lanes[i].to_city].owner;
        if ((owner_a == civ_a && owner_b == civ_b) || (owner_a == civ_b && owner_b == civ_a)) {
            int bonus = lanes[i].type == SEA_LANE_DEEP ? 8 : 5;
            best = max(best, bonus);
        }
    }
    return best;
}

int sea_lanes_plague_contacts_from_city(int source_city, SeaLanePlagueContact *out, int max_contacts) {
    int i, count = 0;
    sea_lanes_get(NULL);
    if (!out || max_contacts <= 0) return 0;
    for (i = 0; i < lane_count && count < max_contacts; i++) {
        SeaLane *lane = &lanes[i];
        int target = -1;
        int owner;
        if (lane->from_city == source_city) target = lane->to_city;
        else if (lane->to_city == source_city) target = lane->from_city;
        if (target < 0) continue;
        owner = cities[source_city].owner;
        out[count].lane_id = i;
        out[count].target_city = target;
        out[count].type = lane->type;
        out[count].distance = lane->point_count;
        out[count].deep_plague_percent = lane->type == SEA_LANE_DEEP ? technology_deep_sea_plague_percent(owner) : 100;
        count++;
    }
    return count;
}

void sea_lanes_add_exposure(int lane_id, int amount) {
    sea_lanes_get(NULL);
    if (lane_id < 0 || lane_id >= lane_count || amount <= 0) return;
    lanes[lane_id].exposure = clamp(lanes[lane_id].exposure + amount, 0, 60);
}

int sea_lanes_decay_exposure(void) {
    int i, changed = 0;
    sea_lanes_get(NULL);
    for (i = 0; i < lane_count; i++) {
        if (lanes[i].exposure > 0) {
            lanes[i].exposure--;
            changed = 1;
        }
    }
    return changed;
}

int sea_lanes_exposure(int lane_id) {
    sea_lanes_get(NULL);
    if (lane_id < 0 || lane_id >= lane_count) return 0;
    return lanes[lane_id].exposure;
}
