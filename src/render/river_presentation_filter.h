#ifndef WORLD_SIM_RIVER_PRESENTATION_FILTER_H
#define WORLD_SIM_RIVER_PRESENTATION_FILTER_H

#include "render/river_topology.h"

typedef struct {
    int candidate_stems;
    int kept_candidate_stems;
    int suppressed_stems;
    int protected_semantic_paths;
    int protected_hidden_paths;
    int parallel_conflicts_before;
    int parallel_conflicts_after;
    uint64_t retained_bytes;
} RiverPresentationFilterMetrics;

/* Audits close-LOD conflicts without clearing any visible_mask entry. */
void river_presentation_filter_apply_close(
    const RiverRenderPath *paths, int count,
    const RiverTopologyView *topology, unsigned char *visible_mask,
    RiverPresentationFilterMetrics *out_metrics);
RiverPresentationFilterMetrics river_presentation_filter_last_metrics(void);
void river_presentation_filter_reset(void);

#endif
