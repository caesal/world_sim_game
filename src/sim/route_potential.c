#include "sim/route_potential.h"

#include "sim/regions.h"
#include "sim/sea_lanes.h"
#include "sim/sea_nav.h"
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
static int deep_degree[MAX_ROUTE_PORT_NODES];
static int node_count;
static int edge_count;
static RoutePotentialStats stats;

static int rp_min(int a, int b) { return a < b ? a : b; }
static int rp_max(int a, int b) { return a > b ? a : b; }
static int rp_clamp(int value, int lo, int hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

static int shallow_direct_limit(void) {
    int base = rp_min(MAP_W, MAP_H) * 9 / 100;
    return rp_clamp(base, 14, 72);
}

static int shallow_diameter_limit(void) {
    return shallow_direct_limit() * 2;
}

static int deep_direct_limit(void) {
    return rp_clamp(rp_max(MAP_W, MAP_H) * 45 / 100, 120, 430);
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

static void add_edge(RoutePotentialType type, int a, int b, const MapPoint *points, int point_count) {
    RoutePotentialEdge *edge;
    if (edge_count >= MAX_ROUTE_POTENTIAL_EDGES) return;
    edge = &edges[edge_count++];
    memset(edge, 0, sizeof(*edge));
    edge->active = 1;
    edge->type = type;
    edge->from_region = nodes[a].region_id;
    edge->to_region = nodes[b].region_id;
    edge->from_port_index = a;
    edge->to_port_index = b;
    edge->required_tech_stage = type == ROUTE_POTENTIAL_DEEP ? 6 : 0;
    edge->distance = point_count;
    edge->point_count = rp_min(point_count, MAX_ROUTE_POTENTIAL_POINTS);
    if (points && edge->point_count > 0) memcpy(edge->points, points, (size_t)edge->point_count * sizeof(points[0]));
    edge->valid = 1;
    if (type == ROUTE_POTENTIAL_SHALLOW) stats.shallow_edges++;
    else stats.deep_edges++;
    stats.edge_count = edge_count;
}

static void build_shallow_edges(void) {
    int candidate_count = 0;
    int limit = shallow_direct_limit();
    for (int i = 0; i < node_count; i++) {
        for (int j = i + 1; j < node_count; j++) {
            RouteCandidate *candidate;
            int direct = port_distance(i, j);
            int count;
            if (direct > limit) { stats.rejected_too_far++; continue; }
            candidate = &candidates[candidate_count];
            count = sea_nav_find_path_mode((MapPoint){nodes[i].sea_x, nodes[i].sea_y},
                                           (MapPoint){nodes[j].sea_x, nodes[j].sea_y},
                                           SEA_NAV_SHALLOW_ANY, -1,
                                           candidate->points, MAX_ROUTE_POTENTIAL_POINTS);
            if (count < 2 || count > limit * 2) { stats.rejected_no_path++; continue; }
            candidate->a = i;
            candidate->b = j;
            candidate->distance = count;
            candidate->point_count = count;
            candidate_count++;
            if (candidate_count >= MAX_ROUTE_POTENTIAL_EDGES) goto done_collecting;
        }
    }
done_collecting:
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
                 candidates[i].points, candidates[i].point_count);
    }
    for (int i = 0; i < node_count; i++) nodes[i].network = root(i);
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

static void build_deep_edges(void) {
    int limit = deep_direct_limit();
    int candidate_count = 0;
    memset(deep_degree, 0, sizeof(deep_degree));
    for (int i = 0; i < node_count; i++) {
        for (int j = i + 1; j < node_count; j++) {
            RouteCandidate *candidate;
            int direct;
            if (nodes[i].network == nodes[j].network) continue;
            direct = port_distance(i, j);
            if (direct > limit) continue;
            candidate = &candidates[candidate_count++];
            candidate->a = i;
            candidate->b = j;
            candidate->distance = direct;
            candidate->point_count = 0;
            if (candidate_count >= MAX_ROUTE_POTENTIAL_EDGES) goto done_collecting;
        }
    }
done_collecting:
    qsort(candidates, (size_t)candidate_count, sizeof(candidates[0]), cmp_candidate);
    for (int i = 0; i < candidate_count && edge_count < MAX_ROUTE_POTENTIAL_EDGES; i++) {
        int na = nodes[candidates[i].a].network;
        int nb = nodes[candidates[i].b].network;
        int count;
        if (deep_degree[na] >= 2 || deep_degree[nb] >= 2 || deep_pair_exists(na, nb)) continue;
        count = sea_nav_find_path_mode((MapPoint){nodes[candidates[i].a].sea_x, nodes[candidates[i].a].sea_y},
                                       (MapPoint){nodes[candidates[i].b].sea_x, nodes[candidates[i].b].sea_y},
                                       SEA_NAV_DEEP_ALLOWED, -1,
                                       candidates[i].points, MAX_ROUTE_POTENTIAL_POINTS);
        if (count < 2) continue;
        candidates[i].point_count = count;
        add_edge(ROUTE_POTENTIAL_DEEP, candidates[i].a, candidates[i].b, candidates[i].points, count);
        deep_degree[na]++;
        deep_degree[nb]++;
    }
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
