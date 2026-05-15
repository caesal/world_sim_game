#include "sim/sea_lanes.h"
#include "core/dirty_flags.h"
#include "core/game_state.h"
#include "core/profiler.h"
#include "sim/maritime.h"
#include "sim/ports.h"
#include "sim/regions.h"
#include "sim/route_potential.h"
#include "sim/technology.h"
#include <stdlib.h>
#include <string.h>
typedef struct {
    int node;
    int city;
    int owner;
    int region;
    int network;
    int status;
    MapPoint port;
    MapPoint sea;
} PortInfo;
typedef struct {
    int from_port;
    int to_port;
    int distance;
    int point_count;
    MapPoint points[MAX_SEA_LANE_POINTS];
} ShallowEdge;
static SeaLane lanes[MAX_SEA_LANES];
static PortInfo ports[MAX_CITIES];
static int port_count;
static int port_index_by_city[MAX_CITIES];
static int parent[MAX_CITIES];
static int network_size[MAX_CITIES];
static ShallowEdge shallow_edges[MAX_SHALLOW_CANDIDATE_EDGES];
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
static int union_ports(int a, int b) {
    int ra = find_root(a);
    int rb = find_root(b);
    if (ra == rb) return ra;
    parent[rb] = ra;
    network_size[ra] += network_size[rb];
    return ra;
}
static int shallow_max_direct_distance(void) {
    return clamp(min(MAP_W, MAP_H) * 9 / 100, 14, 72);
}
static int shallow_max_network_diameter(void) {
    return shallow_max_direct_distance() * 2;
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
    const RoutePortNode *nodes;
    int node_total;
    int i;
    memset(port_index_by_city, -1, sizeof(port_index_by_city));
    port_count = 0;
    nodes = route_potential_nodes(&node_total);
    for (i = 0; i < node_total && port_count < MAX_CITIES; i++) {
        const RoutePortNode *node = &nodes[i];
        NaturalRegion *region;
        int owner;
        int city_id;
        if (!node->has_port || node->region_id < 0 || node->region_id >= region_count) continue;
        region = &natural_regions[node->region_id];
        owner = region->owner_civ;
        city_id = region->city_id;
        if (owner < 0 || owner >= civ_count || !civs[owner].alive) continue;
        if (city_id < 0 || city_id >= city_count || !cities[city_id].alive || cities[city_id].owner != owner) {
            last_stats.missing_admin_city++;
            continue;
        }
        ports[port_count].node = i;
        ports[port_count].city = city_id;
        ports[port_count].owner = owner;
        ports[port_count].region = node->region_id;
        ports[port_count].network = port_count;
        ports[port_count].status = SEA_PORT_ISOLATED;
        ports[port_count].port.x = node->port_x;
        ports[port_count].port.y = node->port_y;
        ports[port_count].sea.x = node->sea_x;
        ports[port_count].sea.y = node->sea_y;
        if (port_index_by_city[city_id] < 0) port_index_by_city[city_id] = port_count;
        parent[port_count] = port_count;
        network_size[port_count] = 1;
        port_count++;
    }
    last_stats.active_port_nodes = port_count;
}
static int lane_exists(int node_a, int node_b, int type) {
    int i;
    for (i = 0; i < lane_count; i++) {
        if (lanes[i].type != type) continue;
        if ((lanes[i].from_node == node_a && lanes[i].to_node == node_b) ||
            (lanes[i].from_node == node_b && lanes[i].to_node == node_a)) return 1;
    }
    return 0;
}
static int add_lane(int type, int net_a, int net_b, int from_idx, int to_idx, const MapPoint *points, int count) {
    SeaLane *lane;
    if (lane_count >= MAX_SEA_LANES || count < 2 || lane_exists(ports[from_idx].node, ports[to_idx].node, type)) {
        last_stats.skipped_routes++;
        if (lane_count >= MAX_SEA_LANES) last_stats.max_lane_skips++;
        return 0;
    }
    lane = &lanes[lane_count++];
    memset(lane, 0, sizeof(*lane));
    lane->active = 1;
    lane->type = type;
    lane->network_a = net_a;
    lane->network_b = net_b;
    lane->from_node = ports[from_idx].node;
    lane->to_node = ports[to_idx].node;
    lane->from_region = ports[from_idx].region;
    lane->to_region = ports[to_idx].region;
    lane->from_city = ports[from_idx].city;
    lane->to_city = ports[to_idx].city;
    lane->from_port = ports[from_idx].port;
    lane->to_port = ports[to_idx].port;
    lane->from_sea_entry = points[0];
    lane->to_sea_entry = points[count - 1];
    lane->point_count = min(count, MAX_SEA_LANE_POINTS);
    memcpy(lane->points, points, (size_t)lane->point_count * sizeof(points[0]));
    return 1;
}
static int compare_shallow_edge(const void *a, const void *b) {
    const ShallowEdge *ea = (const ShallowEdge *)a;
    const ShallowEdge *eb = (const ShallowEdge *)b;
    return ea->distance - eb->distance;
}
static int find_port_by_region(int region_id) {
    int i;
    for (i = 0; i < port_count; i++) {
        if (ports[i].region == region_id) return i;
    }
    return -1;
}
static int network_diameter_after_merge(int root_a, int root_b) {
    int i, j;
    int best = 0;
    for (i = 0; i < port_count; i++) {
        int ri = find_root(i);
        if (ri != root_a && ri != root_b) continue;
        for (j = i + 1; j < port_count; j++) {
            int rj = find_root(j);
            if (rj != root_a && rj != root_b) continue;
            best = max(best, point_distance(ports[i].sea, ports[j].sea));
        }
    }
    return best;
}
static void remember_shallow_edge(ShallowEdge *edges, int *count, int a, int b,
                                  const MapPoint *points, int point_count, int distance) {
    ShallowEdge *edge;
    if (*count >= MAX_SHALLOW_CANDIDATE_EDGES) {
        last_stats.skipped_routes++;
        return;
    }
    edge = &edges[(*count)++];
    edge->from_port = a;
    edge->to_port = b;
    edge->distance = distance;
    edge->point_count = point_count;
    memcpy(edge->points, points, (size_t)point_count * sizeof(points[0]));
}
static int collect_shallow_edges(ShallowEdge *edges) {
    int edge_count = 0;
    const RoutePotentialEdge *potential;
    int potential_count;
    int i;
    potential = route_potential_edges(&potential_count);
    for (i = 0; i < potential_count; i++) {
        const RoutePotentialEdge *edge = &potential[i];
        int a;
        int b;
        if (!edge->active || edge->type != ROUTE_POTENTIAL_SHALLOW) continue;
        a = find_port_by_region(edge->from_region);
        b = find_port_by_region(edge->to_region);
        if (a < 0 || b < 0) continue;
        remember_shallow_edge(edges, &edge_count, a, b, edge->points, edge->point_count, edge->distance);
    }
    last_stats.shallow_candidate_edges = edge_count;
    qsort(edges, (size_t)edge_count, sizeof(edges[0]), compare_shallow_edge);
    return edge_count;
}
static void build_coastal_networks(void) {
    int edge_count = collect_shallow_edges(shallow_edges);
    int max_diameter = shallow_max_network_diameter();
    int i;
    for (i = 0; i < edge_count; i++) {
        ShallowEdge *edge = &shallow_edges[i];
        int a = edge->from_port;
        int b = edge->to_port;
        int ra = find_root(a);
        int rb = find_root(b);
        if (ra == rb) continue;
        if (network_size[ra] + network_size[rb] > SHALLOW_NETWORK_MAX_PORTS) {
            last_stats.shallow_rejected_full++;
            continue;
        }
        if (network_diameter_after_merge(ra, rb) > max_diameter) {
            last_stats.shallow_rejected_diameter++;
            continue;
        }
        if (add_lane(SEA_LANE_SHALLOW, ra, rb, a, b, edge->points, edge->point_count)) {
            union_ports(ra, rb);
            ports[a].status = SEA_PORT_SHALLOW;
            ports[b].status = SEA_PORT_SHALLOW;
            last_stats.shallow_accepted_edges++;
        }
    }
    for (i = 0; i < port_count; i++) {
        ports[i].network = find_root(i);
        if (network_size[ports[i].network] <= 1) last_stats.shallow_isolated_ports++;
    }
}
static int deep_pair_allowed(int a, int b) {
    int same_owner = ports[a].owner == ports[b].owner;
    if (same_owner) return technology_deep_sea_unlocked(ports[a].owner);
    return technology_deep_sea_unlocked(ports[a].owner) && technology_deep_sea_unlocked(ports[b].owner);
}
static int deep_find_root(int *deep_parent, int x) {
    while (deep_parent[x] != x) {
        deep_parent[x] = deep_parent[deep_parent[x]];
        x = deep_parent[x];
    }
    return x;
}
static void build_deep_networks(void) {
    const RoutePotentialEdge *potential;
    int deep_parent[MAX_CITIES];
    int potential_count;
    int i;
    for (i = 0; i < MAX_CITIES; i++) deep_parent[i] = i;
    potential = route_potential_edges(&potential_count);
    for (i = 0; i < potential_count && lane_count < MAX_SEA_LANES; i++) {
        const RoutePotentialEdge *edge = &potential[i];
        int a;
        int b;
        int net_a;
        int net_b;
        int root_a;
        int root_b;
        if (!edge->active || edge->type != ROUTE_POTENTIAL_DEEP) continue;
        a = find_port_by_region(edge->from_region);
        b = find_port_by_region(edge->to_region);
        if (a < 0 || b < 0 || ports[a].network == ports[b].network || !deep_pair_allowed(a, b)) continue;
        net_a = ports[a].network;
        net_b = ports[b].network;
        root_a = deep_find_root(deep_parent, net_a);
        root_b = deep_find_root(deep_parent, net_b);
        if (root_a == root_b) continue;
        if (add_lane(SEA_LANE_DEEP, net_a, net_b, a, b, edge->points, edge->point_count)) {
            ports[a].status = SEA_PORT_DEEP_GATEWAY;
            ports[b].status = SEA_PORT_DEEP_GATEWAY;
            deep_parent[root_b] = root_a;
            last_stats.deep_links++;
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
int sea_lanes_network_connected(int city_a, int city_b) {
    int idx_a, idx_b, net_a, net_b;
    unsigned char seen[MAX_CITIES];
    int queue[MAX_CITIES], head = 0, tail = 0;
    sea_lanes_get(NULL);
    if (city_a < 0 || city_a >= MAX_CITIES || city_b < 0 || city_b >= MAX_CITIES) return 0;
    idx_a = port_index_by_city[city_a];
    idx_b = port_index_by_city[city_b];
    if (idx_a < 0 || idx_b < 0) return 0;
    net_a = ports[idx_a].network;
    net_b = ports[idx_b].network;
    if (net_a == net_b) return 1;
    memset(seen, 0, sizeof(seen));
    seen[net_a] = 1;
    queue[tail++] = net_a;
    while (head < tail) {
        int net = queue[head++];
        int i;
        for (i = 0; i < lane_count; i++) {
            int next = -1;
            if (lanes[i].type != SEA_LANE_DEEP) continue;
            if (lanes[i].network_a == net) next = lanes[i].network_b;
            else if (lanes[i].network_b == net) next = lanes[i].network_a;
            if (next < 0 || next >= MAX_CITIES || seen[next]) continue;
            if (next == net_b) return 1;
            seen[next] = 1;
            queue[tail++] = next;
        }
    }
    return 0;
}
int sea_lanes_contact_kind(int civ_a, int civ_b) {
    unsigned char a_net[MAX_CITIES];
    unsigned char b_net[MAX_CITIES];
    unsigned char seen[MAX_CITIES];
    int queue[MAX_CITIES];
    int head = 0, tail = 0;
    int i;
    if (civ_a < 0 || civ_b < 0 || civ_a == civ_b) return 0;
    sea_lanes_get(NULL);
    memset(a_net, 0, sizeof(a_net));
    memset(b_net, 0, sizeof(b_net));
    memset(seen, 0, sizeof(seen));
    for (i = 0; i < port_count; i++) {
        int net = ports[i].network;
        if (net < 0 || net >= MAX_CITIES) continue;
        if (ports[i].owner == civ_a) a_net[net] = 1;
        if (ports[i].owner == civ_b) b_net[net] = 1;
    }
    for (i = 0; i < MAX_CITIES; i++) {
        if (a_net[i] && b_net[i]) return 1;
        if (a_net[i]) {
            seen[i] = 1;
            queue[tail++] = i;
        }
    }
    while (head < tail) {
        int net = queue[head++];
        for (i = 0; i < lane_count; i++) {
            int next = -1;
            if (lanes[i].type != SEA_LANE_DEEP) continue;
            if (lanes[i].network_a == net) next = lanes[i].network_b;
            else if (lanes[i].network_b == net) next = lanes[i].network_a;
            if (next < 0 || next >= MAX_CITIES || seen[next]) continue;
            if (b_net[next]) return 2;
            seen[next] = 1;
            queue[tail++] = next;
        }
    }
    return 0;
}
int sea_lanes_has_contact(int civ_a, int civ_b) {
    return sea_lanes_contact_kind(civ_a, civ_b) != 0;
}
int sea_lanes_trade_bonus(int civ_a, int civ_b) {
    int kind = sea_lanes_contact_kind(civ_a, civ_b);
    return kind == 2 ? 8 : kind == 1 ? 5 : 0;
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
