#ifndef WORLD_SIM_RIVER_TYPES_H
#define WORLD_SIM_RIVER_TYPES_H

#include <stdint.h>

enum {
    RIVER_DELTA_BRANCH_MAX = 3
};

typedef enum {
    RIVER_CELL_LAND = 1u << 0,
    RIVER_CELL_LAKE = 1u << 1,
    RIVER_CELL_CLOSED_BASIN = 1u << 2,
    RIVER_CELL_SALT_LAKE = 1u << 3,
    RIVER_CELL_CHANNEL = 1u << 4,
    RIVER_CELL_SOURCE = 1u << 5,
    RIVER_CELL_CONFLUENCE = 1u << 6,
    RIVER_CELL_MOUTH = 1u << 7,
    RIVER_CELL_DELTA = 1u << 8,
    RIVER_CELL_DISTRIBUTARY = 1u << 9,
    RIVER_CELL_EDGE_OUTLET = 1u << 10,
    RIVER_CELL_DEPRESSION = 1u << 11
} RiverCellFlag;

typedef enum {
    RIVER_SEGMENT_ORDINARY = 0,
    RIVER_SEGMENT_DISTRIBUTARY = 1
} RiverSegmentKind;

typedef struct {
    int width;
    int height;
    int tile_count;
    const uint8_t *land_mask;
    const int16_t *elevation;
    const int16_t *relative_altitude;
    const int16_t *slope;
    int16_t *moisture;
    int16_t *temperature;
    int16_t *precipitation;
    uint32_t seed;
    uint64_t generation_token;
    int moisture_bias;
    int wetland_bias;
    uint32_t channel_threshold;

    int32_t *receiver;
    int32_t *basin;
    int32_t *topological_order;
    uint32_t *runoff;
    uint32_t *flow;
    uint32_t *upstream_count;
    uint8_t *order;
    uint16_t *width_field;
    uint8_t *soil_fertility;
} RiverGenerationInput;

typedef struct {
    int from;
    int to;
    uint32_t flow;
    uint16_t width;
    uint8_t order;
    uint8_t branch_index;
    uint8_t branch_count;
} RiverDistributary;

typedef struct {
    int from;
    int to;
    int main_stem;
    uint32_t flow;
    uint16_t width;
    uint16_t flags;
    uint8_t order;
    uint8_t kind;
} RiverNetworkSegment;

typedef struct {
    uint64_t transient_bytes;
    uint64_t workspace_bytes_required;
    uint64_t workspace_bytes_allocated;
    int workspace_allocation_failure_field;
    int workspace_allocation_errors;
    int land_cells;
    int topological_cells;
    int depression_cells;
    int lake_candidate_components;
    int lake_qualified_components;
    int lake_rejected_components;
    int lake_pruned_cells;
    int lake_rejected_cells;
    int lake_reject_area;
    int lake_reject_depth;
    int lake_reject_deep_cells;
    int lake_reject_catchment;
    int lake_reject_support;
    int lake_reject_shape;
    int lake_final_invalid_components;
    int lake_cells;
    int closed_basins;
    int salt_lakes;
    int channel_cells;
    int sources;
    int confluences;
    int mouths;
    int deltas;
    int distributaries;
    int distributary_allocation_errors;
    int segment_allocation_errors;
    int ordinary_segments;
    int invalid_receivers;
    int inland_dead_ends;
    int cycle_errors;
    int flow_conservation_errors;
    int width_regressions;
    int order_errors;
    int duplicate_edges;
    int crossing_repairs;
    int crossing_errors;
    int receiver_edges;
    int flat_receiver_edges;
    int receiver_lower_index_edges;
    int flat_receiver_lower_index_edges;
    int max_same_direction_run;
    int max_flat_same_direction_run;
    uint32_t receiver_direction_histogram[8];
    uint32_t flat_direction_histogram[8];
    int legacy_paths_required;
    int legacy_paths_truncated;
    uint32_t channel_threshold;
    uint32_t max_flow;
    int max_order;
    int max_width;
    int bounded_influence_visits;
} RiverGenerationDiagnostics;

typedef struct {
    int width;
    int height;
    int tile_count;
    uint64_t generation_token;
    const int32_t *receiver;
    const int32_t *basin;
    const int32_t *topological_order;
    const uint32_t *runoff;
    const uint32_t *flow;
    const uint32_t *upstream_count;
    const uint8_t *order;
    const uint16_t *width_field;
    const uint16_t *cell_flags;
    const int32_t *main_stem;
    const RiverNetworkSegment *segments;
    int segment_count;
    const RiverDistributary *distributaries;
    int distributary_count;
    RiverGenerationDiagnostics diagnostics;
} RiverNetworkView;

#endif
