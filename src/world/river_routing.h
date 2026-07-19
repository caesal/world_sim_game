#ifndef WORLD_SIM_RIVER_ROUTING_H
#define WORLD_SIM_RIVER_ROUTING_H

#include "world/river_state.h"

typedef enum {
    RIVER_TIE_HEAP = 1,
    RIVER_TIE_FLOOD_NEIGHBOR,
    RIVER_TIE_WATER_OUTLET,
    RIVER_TIE_RECEIVER,
    RIVER_TIE_CROSSING,
    RIVER_TIE_LAKE_COMPONENT,
    RIVER_TIE_LAKE_SINK,
    RIVER_TIE_LAKE_ROUTE,
    RIVER_TIE_CLOSED_CANDIDATE,
    RIVER_TIE_MAIN_STEM,
    RIVER_TIE_DELTA_TARGET
} RiverRoutingTieDomain;

int river_routing_dx(int direction);
int river_routing_dy(int direction);
int river_routing_permuted_direction(const RiverGenerationState *state,
                                     int index, int slot,
                                     RiverRoutingTieDomain domain);
uint64_t river_routing_seeded_tie_key(uint32_t seed, int index,
                                      RiverRoutingTieDomain domain);
uint64_t river_routing_tie_key(const RiverGenerationState *state, int index,
                               RiverRoutingTieDomain domain);
uint64_t river_routing_pair_key(const RiverGenerationState *state,
                                int from, int to,
                                RiverRoutingTieDomain domain);
int river_routing_choose_water_receiver(const RiverGenerationState *state,
                                        int x, int y);
int river_routing_assign_receivers(RiverGenerationState *state);
void river_routing_collect_diagnostics(RiverGenerationState *state);

#endif
