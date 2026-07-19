#ifndef WORLD_SIM_RIVER_TOPOLOGY_H
#define WORLD_SIM_RIVER_TOPOLOGY_H

#include <stdint.h>

#include "render/river_geometry.h"

typedef struct {
    int downstream_path;
    int dominant_upstream_path;
    int stem_id;
    unsigned char continuation_required;
} RiverTopologyPathLink;

typedef struct {
    int downstream_stem;
    int path_count;
    int raw_length;
    int max_order;
    int stable_path;
    uint32_t max_flow;
    uint32_t max_terminal_inflow;
    unsigned char has_outlet;
} RiverTopologyStem;

typedef struct {
    int revision;
    int path_count;
    int stem_count;
    const RiverTopologyPathLink *paths;
    const RiverTopologyStem *stems;
} RiverTopologyView;

int river_topology_rebuild(const RenderSnapshot *snapshot,
                           const RiverRenderPath *paths, int count, int revision);
const RiverTopologyView *river_topology_view(void);
void river_topology_release(void);
size_t river_topology_retained_bytes(void);

#endif
