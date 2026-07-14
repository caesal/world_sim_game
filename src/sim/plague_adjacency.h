#ifndef WORLD_SIM_PLAGUE_ADJACENCY_H
#define WORLD_SIM_PLAGUE_ADJACENCY_H

#include "sim/plague_types.h"

typedef struct {
    int target_city;
    int lane_id;
} PlagueAdjacencyContact;

void plague_adjacency_reset(void);
int plague_adjacency_refresh(void);
const PlagueAdjacencyContact *plague_adjacency_contacts(int source_city,
                                                        PlagueRouteType type,
                                                        int *out_count);
int plague_adjacency_revision(void);
int plague_adjacency_rebuild_count(void);
int plague_adjacency_last_rebuild_us(void);

#endif
