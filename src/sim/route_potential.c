#include "sim/route_potential.h"

#include "core/worldgen_progress.h"
#include "sim/regions.h"
#include "sim/sea_lanes.h"
#include "sim/sea_nav.h"
#include "world/terrain_query.h"
#include "world/ports.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    int a;
    int b;
    int distance;
    int point_count;
    MapPoint points[MAX_ROUTE_POTENTIAL_POINTS];
} RouteCandidate;

static RoutePortNode nodes[MAX_ROUTE_PORT_NODES];
static RoutePotentialEdge edges[MAX_ROUTE_POTENTIAL_EDGES];
static RouteCandidate candidates[MAX_ROUTE_POTENTIAL_EDGES];
static int region_to_node[MAX_NATURAL_REGIONS];
static int parent[MAX_ROUTE_PORT_NODES];
static int network_size[MAX_ROUTE_PORT_NODES];
static int deep_parent[MAX_ROUTE_PORT_NODES];
static unsigned char deep_blocked[MAX_ROUTE_PORT_NODES][MAX_ROUTE_PORT_NODES];
static int node_count;
static int edge_count;
static RoutePotentialStats stats;

static int shallow_network_count(void);

static int rp_min(int a, int b) { return a < b ? a : b; }
static int rp_max(int a, int b) { return a > b ? a : b; }
static int rp_clamp(int value, int lo, int hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

static void report_route_progress(WorldGenStage stage, int current, int total) {
    if (worldgen_progress_active()) worldgen_progress_update_stage(stage, current, total);
}

static int shallow_direct_limit(void) {
    int base = rp_min(MAP_W, MAP_H) * 9 / 100;
    return rp_clamp(base, 14, 72);
}

static int shallow_diameter_limit(void) {
    return shallow_direct_limit() * 2;
}

static int deep_direct_limit(void) {
    return rp_clamp(MAP_W + MAP_H, 120, MAX_MAP_W + MAX_MAP_H);
}

static int port_distance(int a, int b) {
    return abs(nodes[a].sea_x - nodes[b].sea_x) + abs(nodes[a].sea_y - nodes[b].sea_y);
}

static int root(int x) {
    while (parent[x] != x) {
        parent[x] = parent[parent[x]];
        x = parent[x];
    }
    return x;
}

static int cmp_candidate(const void *pa, const void *pb) {
    const RouteCandidate *a = (const RouteCandidate *)pa;
    const RouteCandidate *b = (const RouteCandidate *)pb;
    return a->distance - b->distance;
}

static int same_point(MapPoint a, MapPoint b) {
    return a.x == b.x && a.y == b.y;
}

static int route_endpoint_ok(const MapPoint *points, int count, MapPoint start, MapPoint goal) {
    return points && count >= 2 && same_point(points[0], start) && same_point(points[count - 1], goal);
}

static int route_segments_valid(const MapPoint *points, int count, SeaNavMode mode) {
    for (int i = 1; i < count; i++) {
        if (!sea_nav_segment_water_mode(points[i - 1], points[i], mode, -1)) return 0;
    }
    return 1;
}

static void sample_segment_water_metrics(MapPoint a, MapPoint b, int *deep_tiles, int *near_shore, int *total_tiles) {
    int dx = abs(b.x - a.x);
    int dy = abs(b.y - a.y);
    int steps = rp_max(dx, dy);
    if (steps <= 0) steps = 1;
    for (int s = 0; s <= steps; s++) {
        int x = a.x + (b.x - a.x) * s / steps;
        int y = a.y + (b.y - a.y) * s / steps;
        if (world_is_deep_water(x, y)) (*deep_tiles)++;
        if (sea_nav_distance_to_land(x, y) <= 4) (*near_shore)++;
        (*total_tiles)++;
    }
}

static int route_has_required_deep_water(const MapPoint *points, int count) {
    int deep_tiles = 0;
    int near_shore = 0;
    int total_tiles = 0;
    for (int i = 1; i < count; i++) sample_segment_water_metrics(points[i - 1], points[i], &deep_tiles, &near_shore, &total_tiles);
    if (deep_tiles < 16 && (total_tiles <= 0 || deep_tiles * 100 / total_tiles < 35)) return 0;
    if (total_tiles > 0 && near_shore * 100 / total_tiles > 45) {
        stats.rejected_near_shore++;
        return 0;
    }
    return 1;
}

static int find_route_path(int a, int b, SeaNavMode mode, int require_deep,
                           MapPoint *points, int *out_points, int *out_distance) {
    MapPoint start = {nodes[a].sea_x, nodes[a].sea_y};
    MapPoint goal = {nodes[b].sea_x, nodes[b].sea_y};
    int full_count = 0;
    int truncated = 0;
    int count = sea_nav_find_path_mode_checked(start, goal, mode, -1,
                                               points, MAX_ROUTE_POTENTIAL_POINTS,
                                               &full_count, &truncated);
    if (truncated) { stats.rejected_truncated++; return 0; }
    if (count < 2) { stats.rejected_no_path++; return 0; }
    if (!route_endpoint_ok(points, count, start, goal)) { stats.rejected_endpoint++; return 0; }
    if (!route_segments_valid(points, count, mode)) { stats.rejected_no_path++; return 0; }
    if (require_deep && !route_has_required_deep_water(points, count)) {
        stats.rejected_no_deep_water++;
        return 0;
    }
    if (out_points) *out_points = count;
    if (out_distance) *out_distance = full_count > 0 ? full_count : count;
    return 1;
}

static void reset_storage(void) {
    memset(nodes, 0, sizeof(nodes));
    memset(edges, 0, sizeof(edges));
    for (int i = 0; i < MAX_NATURAL_REGIONS; i++) region_to_node[i] = -1;
    memset(&stats, 0, sizeof(stats));
    node_count = 0;
    edge_count = 0;
}

void route_potential_reset(void) {
    reset_storage();
}

static void collect_nodes(void) {
    for (int i = 0; i < region_count && node_count < MAX_ROUTE_PORT_NODES; i++) {
        const NaturalRegion *region = regions_get(i);
        int sea_x;
        int sea_y;
        RoutePortNode *node;
        if (!region || !region->alive || !region->has_port_site) continue;
        if (region->port_x < 0 || region->port_y < 0) continue;
        if (!ports_find_nearby_sea_entry(region->port_x, region->port_y, &sea_x, &sea_y)) continue;
        node = &nodes[node_count];
        node->region_id = i;
        node->port_x = region->port_x;
        node->port_y = region->port_y;
        node->sea_x = sea_x;
        node->sea_y = sea_y;
        node->has_port = 1;
        node->network = node_count;
        region_to_node[i] = node_count;
        parent[node_count] = node_count;
        network_size[node_count] = 1;
        node_count++;
    }
    stats.node_count = node_count;
}

static int merged_diameter_ok(int ra, int rb) {
    int limit = shallow_diameter_limit();
    for (int i = 0; i < node_count; i++) {
        int ri = root(i);
        if (ri != ra && ri != rb) continue;
        for (int j = i + 1; j < node_count; j++) {
            int rj = root(j);
            if (rj != ra && rj != rb) continue;
            if (port_distance(i, j) > limit) return 0;
        }
    }
    return 1;
}

static int add_edge(RoutePotentialType type, int a, int b, const MapPoint *points, int point_count, int distance) {
    RoutePotentialEdge *edge;
    if (edge_count >= MAX_ROUTE_POTENTIAL_EDGES) return 0;
    edge = &edges[edge_count++];
    memset(edge, 0, sizeof(*edge));
    edge->active = 1;
    edge->type = type;
    edge->from_region = nodes[a].region_id;
    edge->to_region = nodes[b].region_id;
    edge->from_port_index = a;
    edge->to_port_index = b;
    edge->required_tech_stage = type == ROUTE_POTENTIAL_DEEP ? 6 : 0;
    edge->distance = distance;
    edge->point_count = rp_min(point_count, MAX_ROUTE_POTENTIAL_POINTS);
    if (points && edge->point_count > 0) memcpy(edge->points, points, (size_t)edge->point_count * sizeof(points[0]));
    edge->valid = 1;
    if (type == ROUTE_POTENTIAL_SHALLOW) stats.shallow_edges++;
    else stats.deep_edges++;
    stats.edge_count = edge_count;
    return 1;
}

static void merge_small_shallow_networks(RouteCandidate *list, int count) {
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < count; i++) {
            int ra = root(list[i].a);
            int rb = root(list[i].b);
            if (ra == rb) continue;
            if (network_size[ra] >= SHALLOW_NETWORK_MIN_PORTS &&
                network_size[rb] >= SHALLOW_NETWORK_MIN_PORTS) continue;
            if (network_size[ra] + network_size[rb] > SHALLOW_NETWORK_MAX_PORTS) continue;
            if (!merged_diameter_ok(ra, rb)) continue;
            parent[rb] = ra;
            network_size[ra] += network_size[rb];
            add_edge(ROUTE_POTENTIAL_SHALLOW, list[i].a, list[i].b,
                     list[i].points, list[i].point_count, list[i].distance);
        }
    }
}

static void build_shallow_edges(void) {
    int candidate_count = 0;
    int limit = shallow_direct_limit();
    int pair_total = rp_max(1, node_count * (node_count - 1) / 2);
    int pair_done = 0;
    report_route_progress(WORLDGEN_ROUTE_POTENTIAL_SHALLOW, 0, pair_total);
    for (int i = 0; i < node_count; i++) {
        for (int j = i + 1; j < node_count; j++) {
            RouteCandidate *candidate;
            int direct = port_distance(i, j);
            int count;
            int distance;
            pair_done++;
            if ((pair_done & 63) == 0) report_route_progress(WORLDGEN_ROUTE_POTENTIAL_SHALLOW, pair_done, pair_total);
            if (direct > limit) { stats.rejected_too_far++; continue; }
            candidate = &candidates[candidate_count];
            if (!find_route_path(i, j, SEA_NAV_SHALLOW_ANY, 0, candidate->points, &count, &distance)) continue;
            if (distance > limit * 2) { stats.rejected_too_far++; continue; }
            candidate->a = i;
            candidate->b = j;
            candidate->distance = distance;
            candidate->point_count = count;
            candidate_count++;
            if (candidate_count >= MAX_ROUTE_POTENTIAL_EDGES) goto done_collecting;
        }
    }
done_collecting:
    report_route_progress(WORLDGEN_ROUTE_POTENTIAL_SHALLOW, pair_done, pair_total);
    qsort(candidates, (size_t)candidate_count, sizeof(candidates[0]), cmp_candidate);
    for (int i = 0; i < candidate_count; i++) {
        int ra = root(candidates[i].a);
        int rb = root(candidates[i].b);
        if (ra == rb) continue;
        if (network_size[ra] + network_size[rb] > SHALLOW_NETWORK_MAX_PORTS) {
            stats.rejected_network_full++;
            continue;
        }
        if (!merged_diameter_ok(ra, rb)) {
            stats.rejected_diameter++;
            continue;
        }
        parent[rb] = ra;
        network_size[ra] += network_size[rb];
        add_edge(ROUTE_POTENTIAL_SHALLOW, candidates[i].a, candidates[i].b,
                 candidates[i].points, candidates[i].point_count, candidates[i].distance);
    }
    merge_small_shallow_networks(candidates, candidate_count);
    for (int i = 0; i < node_count; i++) nodes[i].network = root(i);
    shallow_network_count();
    report_route_progress(WORLDGEN_ROUTE_POTENTIAL_SHALLOW, pair_total, pair_total);
}

static int deep_pair_exists(int na, int nb) {
    int a = rp_min(na, nb);
    int b = rp_max(na, nb);
    for (int i = 0; i < edge_count; i++) {
        if (edges[i].type != ROUTE_POTENTIAL_DEEP) continue;
        if (rp_min(nodes[edges[i].from_port_index].network, nodes[edges[i].to_port_index].network) == a &&
            rp_max(nodes[edges[i].from_port_index].network, nodes[edges[i].to_port_index].network) == b) return 1;
    }
    return 0;
}

static int deep_root(int x) {
    while (deep_parent[x] != x) {
        deep_parent[x] = deep_parent[deep_parent[x]];
        x = deep_parent[x];
    }
    return x;
}

static void remember_deep_candidate(int *candidate_count, int a, int b, int distance) {
    RouteCandidate *candidate;
    if (*candidate_count < MAX_ROUTE_POTENTIAL_EDGES) {
        candidate = &candidates[(*candidate_count)++];
    } else {
        int worst = 0;
        for (int i = 1; i < *candidate_count; i++) {
            if (candidates[i].distance > candidates[worst].distance) worst = i;
        }
        if (distance >= candidates[worst].distance) return;
        candidate = &candidates[worst];
    }
    candidate->a = a;
    candidate->b = b;
    candidate->distance = distance;
    candidate->point_count = 0;
}

static int try_add_deep_bridge(int a, int b, int require_new_component) {
    int na = nodes[a].network;
    int nb = nodes[b].network;
    int ra = deep_root(na);
    int rb = deep_root(nb);
    int count = 0;
    int distance = 0;
    MapPoint points[MAX_ROUTE_POTENTIAL_POINTS];
    if ((require_new_component && ra == rb) || deep_pair_exists(na, nb)) return 0;
    if (deep_blocked[a][b] || deep_blocked[b][a]) return 0;
    if (!find_route_path(a, b, SEA_NAV_DEEP_ALLOWED, 1, points, &count, &distance)) {
        deep_blocked[a][b] = 1;
        deep_blocked[b][a] = 1;
        return 0;
    }
    if (!add_edge(ROUTE_POTENTIAL_DEEP, a, b, points, count, distance)) return 0;
    if (ra != rb) deep_parent[rb] = ra;
    stats.deep_bridge_count++;
    return 1;
}

static int deep_component_count(int *largest) {
    static int sizes[MAX_ROUTE_PORT_NODES];
    int components = 0;
    int max_size = 0;
    memset(sizes, 0, sizeof(sizes));
    for (int i = 0; i < node_count; i++) {
        int n = nodes[i].network;
        if (n != i) continue;
        sizes[deep_root(n)]++;
    }
    for (int i = 0; i < node_count; i++) {
        if (sizes[i] <= 0) continue;
        components++;
        if (sizes[i] > max_size) max_size = sizes[i];
    }
    if (largest) *largest = max_size;
    return components;
}

static int shallow_network_count(void) {
    static int sizes[MAX_ROUTE_PORT_NODES];
    int count = 0;
    int total = 0;
    int small = 0;
    memset(sizes, 0, sizeof(sizes));
    for (int i = 0; i < node_count; i++) {
        int network = nodes[i].network >= 0 ? nodes[i].network : root(i);
        if (network >= 0 && network < MAX_ROUTE_PORT_NODES) sizes[network]++;
    }
    for (int i = 0; i < node_count; i++) {
        if (sizes[i] <= 0) continue;
        count++;
        total += sizes[i];
        if (sizes[i] < SHALLOW_NETWORK_MIN_PORTS) small++;
    }
    stats.shallow_network_count = count;
    stats.small_shallow_network_count = small;
    stats.isolated_small_network_count = small;
    stats.average_shallow_network_size = count > 0 ? total / count : 0;
    return count;
}

static void connect_remaining_deep_components(void) {
    int guard = 0;
    while (deep_component_count(NULL) > 1 && guard++ < node_count * 4) {
        int best_a = -1;
        int best_b = -1;
        int best_d = 0x3fffffff;
        for (int i = 0; i < node_count; i++) {
            for (int j = i + 1; j < node_count; j++) {
                int d;
                if (nodes[i].network == nodes[j].network) continue;
                if (deep_root(nodes[i].network) == deep_root(nodes[j].network)) continue;
                if (deep_blocked[i][j] || deep_blocked[j][i]) continue;
                d = port_distance(i, j);
                if (d < best_d) {
                    best_d = d;
                    best_a = i;
                    best_b = j;
                }
            }
        }
        if (best_a < 0 || !try_add_deep_bridge(best_a, best_b, 1)) {
            if (best_a < 0) break;
        }
    }
}

static void build_deep_edges(void) {
    int limit = deep_direct_limit();
    int candidate_count = 0;
    int largest = 0;
    int pair_total = rp_max(1, node_count * (node_count - 1) / 2);
    int pair_done = 0;
    int work_total;
    memset(deep_blocked, 0, sizeof(deep_blocked));
    for (int i = 0; i < node_count; i++) deep_parent[i] = i;
    report_route_progress(WORLDGEN_ROUTE_POTENTIAL_DEEP, 0, pair_total * 2);
    for (int i = 0; i < node_count; i++) {
        for (int j = i + 1; j < node_count; j++) {
            int direct;
            pair_done++;
            if ((pair_done & 127) == 0) report_route_progress(WORLDGEN_ROUTE_POTENTIAL_DEEP, pair_done, pair_total * 2);
            if (nodes[i].network == nodes[j].network) continue;
            direct = port_distance(i, j);
            if (direct > limit) continue;
            remember_deep_candidate(&candidate_count, i, j, direct);
        }
    }
    stats.deep_bridge_candidates = candidate_count;
    qsort(candidates, (size_t)candidate_count, sizeof(candidates[0]), cmp_candidate);
    work_total = rp_max(1, pair_total + candidate_count);
    for (int i = 0; i < candidate_count && edge_count < MAX_ROUTE_POTENTIAL_EDGES; i++) {
        try_add_deep_bridge(candidates[i].a, candidates[i].b, 1);
        if ((i & 7) == 0) report_route_progress(WORLDGEN_ROUTE_POTENTIAL_DEEP, pair_total + i, work_total);
    }
    connect_remaining_deep_components();
    report_route_progress(WORLDGEN_ROUTE_POTENTIAL_DEEP, work_total, work_total);
    stats.connected_networks = deep_component_count(&largest) <= 0 ? 0 : largest;
    stats.disconnected_networks = node_count > 0 ? shallow_network_count() - largest : 0;
}

void route_potential_rebuild(void) {
    reset_storage();
    collect_nodes();
    build_shallow_edges();
    build_deep_edges();
}

const RoutePortNode *route_potential_nodes(int *out_count) {
    if (out_count) *out_count = node_count;
    return nodes;
}

const RoutePortNode *route_potential_node_at(int node_index) {
    if (node_index < 0 || node_index >= node_count) return NULL;
    return &nodes[node_index];
}

int route_potential_node_count(void) {
    return node_count;
}

const RoutePotentialEdge *route_potential_edges(int *out_count) {
    if (out_count) *out_count = edge_count;
    return edges;
}

void route_potential_stats(RoutePotentialStats *out) {
    if (out) *out = stats;
}

int route_potential_region_node(int region_id) {
    if (region_id < 0 || region_id >= MAX_NATURAL_REGIONS) return -1;
    return region_to_node[region_id];
}

int route_potential_civ_port_count(int civ_id) {
    int count = 0;
    for (int i = 0; i < node_count; i++) {
        int region = nodes[i].region_id;
        if (region >= 0 && region < region_count && natural_regions[region].owner_civ == civ_id) count++;
    }
    return count;
}

int route_potential_region_reachable(int civ_id, int region_id, int allow_deep,
                                     int *out_distance, int *out_type) {
    int target = route_potential_region_node(region_id);
    static unsigned char seen[MAX_ROUTE_PORT_NODES];
    static int dist[MAX_ROUTE_PORT_NODES];
    static unsigned char path_type[MAX_ROUTE_PORT_NODES];
    int queue[MAX_ROUTE_PORT_NODES];
    int head = 0, tail = 0;
    if (target < 0 || node_count <= 0) return 0;
    memset(seen, 0, (size_t)node_count);
    for (int i = 0; i < node_count; i++) {
        int region = nodes[i].region_id;
        if (region >= 0 && region < region_count && natural_regions[region].owner_civ == civ_id) {
            seen[i] = 1;
            dist[i] = 0;
            path_type[i] = ROUTE_POTENTIAL_SHALLOW;
            queue[tail++] = i;
        }
    }
    while (head < tail) {
        int current = queue[head++];
        if (current == target) {
            if (out_distance) *out_distance = dist[current];
            if (out_type) *out_type = path_type[current];
            return 1;
        }
        for (int e = 0; e < edge_count; e++) {
            int next = -1;
            if (edges[e].type == ROUTE_POTENTIAL_DEEP && !allow_deep) continue;
            if (edges[e].from_port_index == current) next = edges[e].to_port_index;
            else if (edges[e].to_port_index == current) next = edges[e].from_port_index;
            if (next < 0 || seen[next]) continue;
            seen[next] = 1;
            dist[next] = dist[current] + rp_max(1, edges[e].distance);
            path_type[next] = (unsigned char)((edges[e].type == ROUTE_POTENTIAL_DEEP ||
                                               path_type[current] == ROUTE_POTENTIAL_DEEP)
                                              ? ROUTE_POTENTIAL_DEEP : ROUTE_POTENTIAL_SHALLOW);
            queue[tail++] = next;
        }
    }
    return 0;
}
