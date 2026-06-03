#ifndef WORLD_SIM_COLLAPSE_PARTITION_H
#define WORLD_SIM_COLLAPSE_PARTITION_H

#include "core/constants.h"

#define COLLAPSE_MAX_SUCCESSORS 5

typedef struct {
    int successor_count;
    int parent_region_count;
    int parent_regions[MAX_NATURAL_REGIONS];
    int successor_region_count[COLLAPSE_MAX_SUCCESSORS];
    int successor_regions[COLLAPSE_MAX_SUCCESSORS][MAX_NATURAL_REGIONS];
    int successor_seed_region[COLLAPSE_MAX_SUCCESSORS];
    int successor_capital_region[COLLAPSE_MAX_SUCCESSORS];
} CollapsePartitionResult;

int collapse_successor_count_for_owned_regions(int owned_regions);
int collapse_partition_build(int civ_id, int cap_region, int requested_successors,
                             CollapsePartitionResult *out_result);

#endif
