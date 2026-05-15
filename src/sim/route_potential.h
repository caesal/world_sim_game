#ifndef WORLD_SIM_ROUTE_POTENTIAL_H
#define WORLD_SIM_ROUTE_POTENTIAL_H

#include "core/constants.h"
#include "core/value_types.h"

#define MAX_ROUTE_PORT_NODES MAX_NATURAL_REGIONS
#define MAX_ROUTE_POTENTIAL_EDGES 4096
#define MAX_ROUTE_POTENTIAL_POINTS 192

typedef enum {
    ROUTE_POTENTIAL_SHALLOW = 0,
    ROUTE_POTENTIAL_DEEP = 1
} RoutePotentialType;

typedef enum {
    ROUTE_POTENTIAL_VALID = 0,
    ROUTE_POTENTIAL_INVALID_TOO_FAR,
    ROUTE_POTENTIAL_INVALID_NO_PATH,
    ROUTE_POTENTIAL_INVALID_NETWORK_LIMIT,
    ROUTE_POTENTIAL_INVALID_TRUNCATED,
    ROUTE_POTENTIAL_INVALID_NO_DEEP_WATER,
    ROUTE_POTENTIAL_INVALID_ENDPOINT
} RoutePotentialInvalidReason;

typedef struct {
    int region_id;
    int port_x;
    int port_y;
    int sea_x;
    int sea_y;
    int has_port;
    int network;
} RoutePortNode;

typedef struct {
    int active;
    RoutePotentialType type;
    int from_region;
    int to_region;
    int from_port_index;
    int to_port_index;
    int required_tech_stage;
    int distance;
    int point_count;
    MapPoint points[MAX_ROUTE_POTENTIAL_POINTS];
    int valid;
    int invalid_reason;
} RoutePotentialEdge;

typedef struct {
    int node_count;
    int edge_count;
    int shallow_edges;
    int deep_edges;
    int rejected_too_far;
    int rejected_no_path;
    int rejected_network_full;
    int rejected_diameter;
    int rejected_truncated;
    int rejected_no_deep_water;
    int rejected_near_shore;
    int rejected_endpoint;
    int deep_bridge_candidates;
    int deep_bridge_count;
    int connected_networks;
    int disconnected_networks;
    int shallow_network_count;
    int small_shallow_network_count;
    int average_shallow_network_size;
    int isolated_small_network_count;
} RoutePotentialStats;

void route_potential_reset(void);
void route_potential_rebuild(void);
const RoutePortNode *route_potential_nodes(int *out_count);
const RoutePortNode *route_potential_node_at(int node_index);
int route_potential_node_count(void);
const RoutePotentialEdge *route_potential_edges(int *out_count);
void route_potential_stats(RoutePotentialStats *out);
int route_potential_region_node(int region_id);
int route_potential_civ_port_count(int civ_id);
int route_potential_region_reachable(int civ_id, int region_id, int allow_deep,
                                     int *out_distance, int *out_type);

#endif
