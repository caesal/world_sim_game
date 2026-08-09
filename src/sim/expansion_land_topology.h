#ifndef WORLD_SIM_EXPANSION_LAND_TOPOLOGY_H
#define WORLD_SIM_EXPANSION_LAND_TOPOLOGY_H

#include <stdint.h>

typedef struct {
    int viable_regions;
    int global_unowned_regions;
    int land_adjacent_unowned_regions;
    int land_nearby_unowned_regions;
} ExpansionLandTopologyCounts;

typedef struct {
    int valid;
    int ownership_revision;
    int province_revision;
    int civ_visual_revision;
    int region_total;
    int civ_total;
    uint64_t traversal_count;
    uint64_t cache_hit_count;
    uint64_t regions_visited;
    uint64_t last_rebuild_us;
} ExpansionLandTopologyStatus;

void expansion_land_topology_reset(void);
int expansion_land_topology_counts(int civ_id, ExpansionLandTopologyCounts *out);
void expansion_land_topology_status(ExpansionLandTopologyStatus *out);

#endif
