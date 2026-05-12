#ifndef WORLD_SIM_SEA_LANES_H
#define WORLD_SIM_SEA_LANES_H

#include "core/constants.h"
#include "core/value_types.h"

#define MAX_SEA_LANES 192
#define MAX_SEA_LANE_POINTS 192
#define MAX_SEA_LANE_MEMBERS 24

typedef enum {
    SEA_LANE_SHALLOW = 0,
    SEA_LANE_DEEP = 1
} SeaLaneType;

typedef enum {
    SEA_PORT_ISOLATED = 0,
    SEA_PORT_SHALLOW = 1,
    SEA_PORT_DEEP_GATEWAY = 2
} SeaPortStatus;

typedef struct {
    int active;
    int type;
    int network_a;
    int network_b;
    int from_city;
    int to_city;
    MapPoint from_sea_entry;
    MapPoint to_sea_entry;
    int point_count;
    MapPoint points[MAX_SEA_LANE_POINTS];
    int member_route_count;
    int member_routes[MAX_SEA_LANE_MEMBERS];
    int skipped_routes;
    int exposure;
} SeaLane;

typedef struct {
    int logical_routes;
    int visual_lanes;
    int merged_routes;
    int skipped_routes;
    int land_rejections;
    int rebuild_ms;
} SeaLaneStats;

typedef struct {
    int lane_id;
    int target_city;
    int type;
    int distance;
    int deep_plague_percent;
} SeaLanePlagueContact;

const SeaLane *sea_lanes_get(int *out_count);
void sea_lanes_invalidate(void);
void sea_lanes_last_stats(SeaLaneStats *out);
int sea_lanes_city_status(int city_id);
int sea_lanes_city_network(int city_id);
int sea_lanes_connected(int city_a, int city_b, int *out_distance);
int sea_lanes_network_connected(int city_a, int city_b);
int sea_lanes_has_contact(int civ_a, int civ_b);
int sea_lanes_trade_bonus(int civ_a, int civ_b);
int sea_lanes_plague_contacts_from_city(int source_city, SeaLanePlagueContact *out, int max_contacts);
void sea_lanes_add_exposure(int lane_id, int amount);
int sea_lanes_decay_exposure(void);
int sea_lanes_exposure(int lane_id);

#endif
