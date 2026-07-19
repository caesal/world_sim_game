#ifndef WORLD_SIM_RIVER_STATE_H
#define WORLD_SIM_RIVER_STATE_H

#include "world/river_types.h"

typedef struct {
    RiverGenerationInput input;
    uint64_t allocated_bytes;
    int capacity;
    int land_count;
    int topological_count;
    int heap_count;
    int segment_count;
    int segment_capacity;
    int distributary_count;
    int distributary_capacity;
    int32_t *filled_elevation;
    int32_t *heap;
    int32_t *main_stem;
    int32_t *dominant_parent;
    uint32_t *check_flow;
    uint32_t *published_runoff;
    uint32_t *published_upstream_count;
    uint16_t *published_width;
    uint16_t *cell_flags;
    uint8_t *visited;
    uint8_t *max_upstream_order;
    uint8_t *max_upstream_count;
    uint8_t *moisture_influence;
    int8_t *temperature_influence;
    RiverNetworkSegment *segments;
    RiverDistributary *distributaries;
    RiverGenerationDiagnostics diagnostics;
} RiverGenerationState;

RiverGenerationState *river_state_prepare(const RiverGenerationInput *input);
RiverGenerationState *river_state_current(void);
int river_state_resize_segments(RiverGenerationState *state, int required);
int river_state_resize_distributaries(RiverGenerationState *state, int required);
void river_state_clear_outputs(RiverGenerationState *state);
void river_state_publish_view(RiverGenerationState *state);
void river_state_refresh_view_diagnostics(RiverGenerationState *state);
const RiverNetworkView *river_state_latest_view(void);
int river_state_view_matches(uint64_t generation_token);
void river_state_last_diagnostics(RiverGenerationDiagnostics *out);
void river_state_release(void);

#endif
