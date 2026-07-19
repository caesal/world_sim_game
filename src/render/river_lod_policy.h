#ifndef WORLD_SIM_RIVER_LOD_POLICY_H
#define WORLD_SIM_RIVER_LOD_POLICY_H

#include "render/river_topology.h"

typedef struct {
    int target_paths;
    int visible_paths;
    int visible_stems;
    int connected_paths;
    int close_candidate_stems;
    int close_kept_candidate_stems;
    int close_suppressed_stems;
    int close_protected_paths;
    int close_protected_hidden_paths;
    int close_parallel_conflicts_before;
    int close_parallel_conflicts_after;
} RiverLodPolicyMetrics;

int river_lod_policy_prepare(const RiverRenderPath *paths, int count,
                             const RiverTopologyView *topology,
                             int viewport_w, int viewport_h);
int river_lod_policy_path_visible(int path_index, int lod);
RiverLodPolicyMetrics river_lod_policy_metrics(int lod);
void river_lod_policy_release(void);
size_t river_lod_policy_retained_bytes(void);

#endif
