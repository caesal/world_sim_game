#ifndef WORLD_SIM_RIVER_LAKE_QUALIFICATION_H
#define WORLD_SIM_RIVER_LAKE_QUALIFICATION_H

#include "world/river_state.h"

#include <stdint.h>

enum {
    RIVER_LAKE_MIN_CELL_DEPTH = 2,
    RIVER_LAKE_MIN_AREA = 6,
    RIVER_LAKE_MIN_MAX_DEPTH = 4,
    RIVER_LAKE_DEEP_CELL_DEPTH = 3,
    RIVER_LAKE_CATCHMENT_RATIO_NUMERATOR = 5,
    RIVER_LAKE_CATCHMENT_RATIO_DENOMINATOR = 4,
    RIVER_LAKE_MIN_EXTERNAL_CATCHMENT = 4,
    RIVER_LAKE_MIN_SUPPORT_PER_CELL = 2,
    RIVER_LAKE_MIN_EXTERNAL_SUPPORT_PER_CELL = 1,
    RIVER_LAKE_MIN_BBOX_FILL_PERCENT = 35,
    RIVER_LAKE_MAX_PERIMETER_SQUARED_PER_AREA = 96,
    RIVER_LAKE_MAX_INTERIOR_PERIMETER_SQUARED_PER_AREA = 64,
    RIVER_LAKE_MIN_INTERIOR_RATIO_NUMERATOR = 1,
    RIVER_LAKE_MIN_INTERIOR_RATIO_DENOMINATOR = 12,
    RIVER_LAKE_MAX_NO_INTERIOR_AREA = 12,
    RIVER_LAKE_MAX_INTERIOR_DISTANCE = 3,
    RIVER_LAKE_MAX_HOLES = 0,
    RIVER_LAKE_MIN_OUTLET_RECEIVERS = 1
};

typedef enum {
    RIVER_LAKE_REJECT_NONE = 0,
    RIVER_LAKE_REJECT_AREA = 1u << 0,
    RIVER_LAKE_REJECT_DEPTH = 1u << 1,
    RIVER_LAKE_REJECT_DEEP_CELLS = 1u << 2,
    RIVER_LAKE_REJECT_CATCHMENT = 1u << 3,
    RIVER_LAKE_REJECT_SUPPORT = 1u << 4,
    RIVER_LAKE_REJECT_SHAPE = 1u << 5,
    RIVER_LAKE_REJECT_COMPACTNESS = 1u << 6,
    RIVER_LAKE_REJECT_HOLES = 1u << 7,
    RIVER_LAKE_REJECT_OUTLETS = 1u << 8,
    RIVER_LAKE_REJECT_DEEP_CORE = 1u << 9
} RiverLakeRejectReason;

typedef struct {
    int label;
    int sink;
    int spill_level;
    int surface_level;
    int initial_area;
    int area;
    int pruned_cells;
    int max_depth;
    int deep_cells;
    int largest_deep_core;
    int cardinal_edges;
    int perimeter_edges;
    int interior_cells;
    int interior_cardinal_edges;
    int interior_perimeter_edges;
    int max_interior_distance;
    int solid_2x2_blocks;
    int hole_count;
    int bbox_min_x;
    int bbox_min_y;
    int bbox_max_x;
    int bbox_max_y;
    int bbox_area;
    int mean_precipitation;
    int external_inlet_edges;
    int outlet_edges;
    int invalid_outlet_edges;
    int outlet_receiver_count;
    uint64_t direct_support_units;
    uint64_t external_catchment_cells;
    uint64_t external_support_units;
    uint32_t catchment_cells;
    uint32_t support_units;
    uint32_t reject_reasons;
} RiverLakeQualification;

void river_lake_qualification_prepare_support(RiverGenerationState *state);
int river_lake_qualification_collect(RiverGenerationState *state, int seed,
                                     RiverLakeQualification *out);
void river_lake_qualification_collect_final_support(
    RiverGenerationState *state, RiverLakeQualification *candidate,
    int terminal_sink);
uint32_t river_lake_qualification_model_reject_reasons(
    RiverLakeQualification *candidate);
int river_lake_qualification_accepts(RiverLakeQualification *candidate);

#endif
