#include "sim/plague_adjacency.h"

#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "core/profiler.h"
#include "sim/maritime.h"
#include "sim/regions.h"
#include "sim/sea_lanes.h"
#include "sim/technology.h"

#include <stdlib.h>
#include <string.h>

#define MAX_PLAGUE_LAND_EDGES (MAX_CITIES * MAX_REGION_NEIGHBORS)
#define MAX_PLAGUE_SEA_EDGES (MAX_SEA_LANES * 2)

typedef struct {
    int city_count;
    int region_count;
    int ownership_revision;
    int city_revision;
    int route_revision;
    int maritime_ownership_revision;
    int coast_revision;
    int deep_revision;
} PlagueAdjacencyKey;

typedef struct {
    int source;
    int target;
    int type;
    int lane_id;
} SeaEdge;

static PlagueAdjacencyKey cached_key;
static int cache_valid;
static int cache_revision;
static int rebuilds;
static int last_rebuild_us;
static int land_offsets[MAX_CITIES + 1];
static int shallow_offsets[MAX_CITIES + 1];
static int deep_offsets[MAX_CITIES + 1];
static PlagueAdjacencyContact land_targets[MAX_PLAGUE_LAND_EDGES];
static PlagueAdjacencyContact shallow_targets[MAX_PLAGUE_SEA_EDGES];
static PlagueAdjacencyContact deep_targets[MAX_PLAGUE_SEA_EDGES];
static SeaEdge sea_edges[MAX_PLAGUE_SEA_EDGES];

static PlagueAdjacencyKey current_key(void) {
    PlagueAdjacencyKey key;
    key.city_count = city_count;
    key.region_count = region_count;
    key.ownership_revision = dirty_revision_ownership();
    key.city_revision = dirty_revision_city();
    key.route_revision = maritime_route_revision();
    key.maritime_ownership_revision = maritime_ownership_revision();
    key.coast_revision = dirty_revision_coast();
    key.deep_revision = technology_deep_sea_revision();
    return key;
}

static int same_key(const PlagueAdjacencyKey *a, const PlagueAdjacencyKey *b) {
    return memcmp(a, b, sizeof(*a)) == 0;
}

static int valid_city_index(int city_id) {
    return city_id >= 0 && city_id < city_count && city_id < MAX_CITIES;
}

static void build_land_adjacency(void) {
    int counts[MAX_CITIES];
    int write[MAX_CITIES];
    int source;
    int total = 0;
    memset(counts, 0, sizeof(counts));
    memset(land_offsets, 0, sizeof(land_offsets));
    for (source = 0; source < city_count && source < MAX_CITIES; source++) {
        int region_id = regions_region_for_city(source);
        const NaturalRegion *region = regions_get(region_id);
        int n;
        if (!region) continue;
        for (n = 0; n < region->neighbor_count; n++) {
            const NaturalRegion *neighbor = regions_get(region->neighbors[n]);
            int target = neighbor ? neighbor->city_id : -1;
            if (!valid_city_index(target) || target == source) continue;
            if (counts[source] < MAX_REGION_NEIGHBORS) counts[source]++;
        }
    }
    for (source = 0; source < MAX_CITIES; source++) {
        land_offsets[source] = total;
        total += counts[source];
        write[source] = land_offsets[source];
    }
    land_offsets[MAX_CITIES] = total;
    for (source = 0; source < city_count && source < MAX_CITIES; source++) {
        int region_id = regions_region_for_city(source);
        const NaturalRegion *region = regions_get(region_id);
        int n;
        if (!region) continue;
        for (n = 0; n < region->neighbor_count && write[source] < land_offsets[source + 1]; n++) {
            const NaturalRegion *neighbor = regions_get(region->neighbors[n]);
            int target = neighbor ? neighbor->city_id : -1;
            if (!valid_city_index(target) || target == source) continue;
            land_targets[write[source]++] = (PlagueAdjacencyContact){target, -1};
        }
    }
}

static int compare_sea_edge(const void *left, const void *right) {
    const SeaEdge *a = (const SeaEdge *)left;
    const SeaEdge *b = (const SeaEdge *)right;
    if (a->type != b->type) return a->type - b->type;
    if (a->source != b->source) return a->source - b->source;
    if (a->target != b->target) return a->target - b->target;
    return a->lane_id - b->lane_id;
}

static int collect_sea_edges(void) {
    const SeaLane *lanes;
    int lane_count;
    int edge_count = 0;
    int i;
    lanes = sea_lanes_get(&lane_count);
    for (i = 0; i < lane_count && edge_count + 1 < MAX_PLAGUE_SEA_EDGES; i++) {
        int type;
        if (!lanes[i].active || !valid_city_index(lanes[i].from_city) ||
            !valid_city_index(lanes[i].to_city) || lanes[i].from_city == lanes[i].to_city) continue;
        type = lanes[i].type == SEA_LANE_DEEP ? PLAGUE_ROUTE_DEEP : PLAGUE_ROUTE_SHALLOW;
        sea_edges[edge_count++] = (SeaEdge){lanes[i].from_city, lanes[i].to_city, type, i};
        sea_edges[edge_count++] = (SeaEdge){lanes[i].to_city, lanes[i].from_city, type, i};
    }
    qsort(sea_edges, (size_t)edge_count, sizeof(sea_edges[0]), compare_sea_edge);
    if (edge_count > 1) {
        int read;
        int write = 1;
        for (read = 1; read < edge_count; read++) {
            SeaEdge *previous = &sea_edges[write - 1];
            SeaEdge *current = &sea_edges[read];
            if (previous->type == current->type && previous->source == current->source &&
                previous->target == current->target) continue;
            sea_edges[write++] = *current;
        }
        edge_count = write;
    }
    return edge_count;
}

static void build_sea_type(int type, int edge_count, int offsets[MAX_CITIES + 1],
                           PlagueAdjacencyContact targets[MAX_PLAGUE_SEA_EDGES]) {
    int counts[MAX_CITIES];
    int write[MAX_CITIES];
    int total = 0;
    int city_id;
    int i;
    memset(counts, 0, sizeof(counts));
    memset(offsets, 0, sizeof(int) * (MAX_CITIES + 1));
    for (i = 0; i < edge_count; i++) {
        if (sea_edges[i].type == type && valid_city_index(sea_edges[i].source)) {
            counts[sea_edges[i].source]++;
        }
    }
    for (city_id = 0; city_id < MAX_CITIES; city_id++) {
        offsets[city_id] = total;
        total += counts[city_id];
        write[city_id] = offsets[city_id];
    }
    offsets[MAX_CITIES] = total;
    for (i = 0; i < edge_count; i++) {
        int source = sea_edges[i].source;
        if (sea_edges[i].type != type || !valid_city_index(source)) continue;
        targets[write[source]++] = (PlagueAdjacencyContact){sea_edges[i].target,
                                                            sea_edges[i].lane_id};
    }
}

void plague_adjacency_reset(void) {
    memset(&cached_key, 0, sizeof(cached_key));
    cache_valid = 0;
    cache_revision = 0;
    rebuilds = 0;
    last_rebuild_us = 0;
}

int plague_adjacency_refresh(void) {
    PlagueAdjacencyKey key = current_key();
    long long start;
    int sea_edge_count;
    if (cache_valid && same_key(&key, &cached_key)) return 0;
    start = profiler_now_us();
    build_land_adjacency();
    sea_edge_count = collect_sea_edges();
    build_sea_type(PLAGUE_ROUTE_SHALLOW, sea_edge_count, shallow_offsets, shallow_targets);
    build_sea_type(PLAGUE_ROUTE_DEEP, sea_edge_count, deep_offsets, deep_targets);
    cached_key = key;
    cache_valid = 1;
    cache_revision++;
    rebuilds++;
    last_rebuild_us = (int)(profiler_now_us() - start);
    return 1;
}

const PlagueAdjacencyContact *plague_adjacency_contacts(int source_city,
                                                        PlagueRouteType type,
                                                        int *out_count) {
    const PlagueAdjacencyContact *targets = NULL;
    const int *offsets = NULL;
    if (out_count) *out_count = 0;
    if (!valid_city_index(source_city)) return NULL;
    plague_adjacency_refresh();
    if (type == PLAGUE_ROUTE_LAND) { offsets = land_offsets; targets = land_targets; }
    else if (type == PLAGUE_ROUTE_SHALLOW) { offsets = shallow_offsets; targets = shallow_targets; }
    else if (type == PLAGUE_ROUTE_DEEP) { offsets = deep_offsets; targets = deep_targets; }
    if (!offsets || !targets) return NULL;
    if (out_count) *out_count = offsets[source_city + 1] - offsets[source_city];
    return &targets[offsets[source_city]];
}

int plague_adjacency_revision(void) { return cache_revision; }
int plague_adjacency_rebuild_count(void) { return rebuilds; }
int plague_adjacency_last_rebuild_us(void) { return last_rebuild_us; }
