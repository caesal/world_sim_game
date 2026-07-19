#include "world/river_lake_qualification.h"

#include "world/river_lake_shape.h"
#include "world/river_routing.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

static const int CARDINAL_DX[4] = {1, 0, -1, 0};
static const int CARDINAL_DY[4] = {0, 1, 0, -1};
static const int ADJACENT_DX[8] = {1, 1, 0, -1, -1, -1, 0, 1};
static const int ADJACENT_DY[8] = {0, 1, 1, 1, 0, -1, -1, -1};

static int in_bounds(const RiverGenerationState *state, int x, int y) {
    return x >= 0 && x < state->input.width &&
           y >= 0 && y < state->input.height;
}

static int depression_depth(const RiverGenerationState *state, int index) {
    return state->filled_elevation[index] - state->input.elevation[index];
}

static uint32_t saturating_add(uint32_t left, uint32_t right) {
    return UINT_MAX - left < right ? UINT_MAX : left + right;
}

static uint64_t saturating_add64(uint64_t left, uint64_t right) {
    return UINT64_MAX - left < right ? UINT64_MAX : left + right;
}

static uint32_t clamp_u64_to_u32(uint64_t value) {
    return value > UINT_MAX ? UINT_MAX : (uint32_t)value;
}

static uint32_t local_support(const RiverGenerationState *state, int index) {
    int precipitation = state->input.precipitation[index];
    if (precipitation < 0) precipitation = 0;
    if (precipitation > 100) precipitation = 100;
    return (uint32_t)(1 + precipitation / 25);
}

void river_lake_qualification_prepare_support(RiverGenerationState *state) {
    int count;
    int i;
    if (!state) return;
    count = state->input.tile_count;
    memset(state->published_upstream_count, 0,
           (size_t)count * sizeof(*state->published_upstream_count));
    memset(state->check_flow, 0,
           (size_t)count * sizeof(*state->check_flow));
    for (i = 0; i < state->topological_count; i++) {
        int index = state->input.topological_order[i];
        state->published_upstream_count[index] = 1;
        state->check_flow[index] = local_support(state, index);
    }
    for (i = state->topological_count - 1; i >= 0; i--) {
        int index = state->input.topological_order[i];
        int receiver = state->input.receiver[index];
        if (receiver < 0 || receiver >= count ||
            !state->input.land_mask[receiver]) continue;
        state->published_upstream_count[receiver] = saturating_add(
            state->published_upstream_count[receiver],
            state->published_upstream_count[index]);
        state->check_flow[receiver] = saturating_add(
            state->check_flow[receiver], state->check_flow[index]);
    }
}

static int collect_equal_spill_core(RiverGenerationState *state, int seed,
                                    int spill_level) {
    int head = 0;
    int tail = 0;
    state->dominant_parent[seed] = seed;
    state->heap[tail++] = seed;
    while (head < tail) {
        int current = state->heap[head++];
        int cx = current % state->input.width;
        int cy = current / state->input.width;
        int direction;
        for (direction = 0; direction < 4; direction++) {
            int nx = cx + CARDINAL_DX[direction];
            int ny = cy + CARDINAL_DY[direction];
            int neighbor;
            if (!in_bounds(state, nx, ny)) continue;
            neighbor = ny * state->input.width + nx;
            if (!state->input.land_mask[neighbor] ||
                state->dominant_parent[neighbor] >= 0 ||
                depression_depth(state, neighbor) < RIVER_LAKE_MIN_CELL_DEPTH ||
                state->filled_elevation[neighbor] != spill_level) continue;
            state->dominant_parent[neighbor] = seed;
            state->heap[tail++] = neighbor;
        }
    }
    return tail;
}

static int cardinal_degree(const RiverGenerationState *state, int index,
                           int label, int active_only) {
    int x = index % state->input.width;
    int y = index / state->input.width;
    int degree = 0;
    int direction;
    for (direction = 0; direction < 4; direction++) {
        int nx = x + CARDINAL_DX[direction];
        int ny = y + CARDINAL_DY[direction];
        int neighbor;
        if (!in_bounds(state, nx, ny)) continue;
        neighbor = ny * state->input.width + nx;
        if (state->dominant_parent[neighbor] != label) continue;
        if (active_only && state->visited[neighbor] == 0) continue;
        degree++;
    }
    return degree;
}

static int retained_member(const RiverGenerationState *state, int index,
                           int label) {
    return index >= 0 && index < state->input.tile_count &&
           state->dominant_parent[index] == label && state->visited[index] != 0;
}

static int adjacent_receiver(const RiverGenerationState *state, int from,
                             int receiver) {
    int dx;
    int dy;
    if (receiver < 0 || receiver >= state->input.tile_count) return 0;
    dx = from % state->input.width - receiver % state->input.width;
    dy = from / state->input.width - receiver / state->input.width;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    return (dx != 0 || dy != 0) && dx <= 1 && dy <= 1;
}

static int prune_thin_cells(RiverGenerationState *state, int label, int area) {
    int queue_head = 0;
    int queue_tail = 0;
    int i;
    for (i = 0; i < area; i++) {
        int index = state->heap[i];
        int degree = cardinal_degree(state, index, label, 0);
        state->visited[index] = (uint8_t)(degree + 1);
        if (degree < 2) state->main_stem[queue_tail++] = index;
    }
    while (queue_head < queue_tail) {
        int current = state->main_stem[queue_head++];
        int x;
        int y;
        int direction;
        if (state->visited[current] == 0 || state->visited[current] - 1 >= 2) continue;
        state->visited[current] = 0;
        x = current % state->input.width;
        y = current / state->input.width;
        for (direction = 0; direction < 4; direction++) {
            int nx = x + CARDINAL_DX[direction];
            int ny = y + CARDINAL_DY[direction];
            int neighbor;
            int old_degree;
            int new_degree;
            if (!in_bounds(state, nx, ny)) continue;
            neighbor = ny * state->input.width + nx;
            if (state->dominant_parent[neighbor] != label ||
                state->visited[neighbor] == 0) continue;
            old_degree = state->visited[neighbor] - 1;
            state->visited[neighbor]--;
            new_degree = state->visited[neighbor] - 1;
            if (old_degree >= 2 && new_degree < 2) {
                state->main_stem[queue_tail++] = neighbor;
            }
        }
    }
    return queue_tail;
}

static int compact_active_cells(RiverGenerationState *state, int area) {
    int write = 0;
    int i;
    for (i = 0; i < area; i++) {
        int index = state->heap[i];
        if (state->visited[index]) state->heap[write++] = index;
    }
    return write;
}

static void clear_outlet_markers(RiverGenerationState *state,
                                 const RiverLakeQualification *candidate) {
    int i;
    for (i = 0; i < candidate->area; i++) {
        int index = state->heap[i];
        int x = index % state->input.width;
        int y = index / state->input.width;
        int direction;
        for (direction = 0; direction < 8; direction++) {
            int nx = x + ADJACENT_DX[direction];
            int ny = y + ADJACENT_DY[direction];
            int neighbor;
            if (!in_bounds(state, nx, ny)) continue;
            neighbor = ny * state->input.width + nx;
            if (!retained_member(state, neighbor, candidate->label)) {
                state->main_stem[neighbor] = -1;
            }
        }
    }
}

void river_lake_qualification_collect_final_support(
    RiverGenerationState *state, RiverLakeQualification *candidate,
    int terminal_sink) {
    int i;
    clear_outlet_markers(state, candidate);
    for (i = 0; i < candidate->area; i++) {
        int index = state->heap[i];
        int receiver = state->input.receiver[index];
        int x = index % state->input.width;
        int y = index / state->input.width;
        int direction;
        candidate->direct_support_units = saturating_add64(
            candidate->direct_support_units, local_support(state, index));
        if (!retained_member(state, receiver, candidate->label)) {
            candidate->outlet_edges++;
            if (index == terminal_sink && receiver == -1) continue;
            if (!adjacent_receiver(state, index, receiver)) {
                candidate->invalid_outlet_edges++;
                continue;
            }
            if (state->main_stem[receiver] != candidate->label) {
                state->main_stem[receiver] = candidate->label;
                candidate->outlet_receiver_count++;
            }
        }
        for (direction = 0; direction < 8; direction++) {
            int nx = x + ADJACENT_DX[direction];
            int ny = y + ADJACENT_DY[direction];
            int neighbor;
            if (!in_bounds(state, nx, ny)) continue;
            neighbor = ny * state->input.width + nx;
            if (!state->input.land_mask[neighbor] ||
                retained_member(state, neighbor, candidate->label) ||
                state->input.receiver[neighbor] != index) continue;
            candidate->external_inlet_edges++;
            candidate->external_catchment_cells = saturating_add64(
                candidate->external_catchment_cells,
                state->published_upstream_count[neighbor]);
            candidate->external_support_units = saturating_add64(
                candidate->external_support_units,
                state->check_flow[neighbor]);
        }
    }
    candidate->catchment_cells = clamp_u64_to_u32(
        (uint64_t)candidate->area + candidate->external_catchment_cells);
    candidate->support_units = clamp_u64_to_u32(
        saturating_add64(candidate->direct_support_units,
                         candidate->external_support_units));
}

int river_lake_qualification_collect(RiverGenerationState *state, int seed,
                                     RiverLakeQualification *out) {
    int retained;
    if (!state || !out || seed < 0 || seed >= state->input.tile_count ||
        !state->input.land_mask[seed] ||
        depression_depth(state, seed) < RIVER_LAKE_MIN_CELL_DEPTH ||
        state->dominant_parent[seed] >= 0) return 0;
    memset(out, 0, sizeof(*out));
    out->label = seed;
    out->spill_level = state->filled_elevation[seed];
    out->surface_level = out->spill_level - 1;
    out->initial_area = collect_equal_spill_core(state, seed, out->spill_level);
    prune_thin_cells(state, seed, out->initial_area);
    retained = compact_active_cells(state, out->initial_area);
    out->area = retained;
    out->pruned_cells = out->initial_area - retained;
    river_lake_shape_collect(state, out);
    river_lake_qualification_collect_final_support(state, out, -1);
    return 1;
}

uint32_t river_lake_qualification_model_reject_reasons(
    RiverLakeQualification *candidate) {
    int required_deep;
    int required_total_catchment;
    int required_external_catchment;
    int required_interior;
    uint64_t total_support;
    uint32_t reasons = RIVER_LAKE_REJECT_NONE;
    if (!candidate) return UINT32_MAX;
    required_deep = (candidate->area + 7) / 8;
    if (required_deep < 2) required_deep = 2;
    required_total_catchment = (candidate->area *
                                RIVER_LAKE_CATCHMENT_RATIO_NUMERATOR +
                                RIVER_LAKE_CATCHMENT_RATIO_DENOMINATOR - 1) /
                               RIVER_LAKE_CATCHMENT_RATIO_DENOMINATOR;
    if (required_total_catchment <
        candidate->area + RIVER_LAKE_MIN_EXTERNAL_CATCHMENT) {
        required_total_catchment =
            candidate->area + RIVER_LAKE_MIN_EXTERNAL_CATCHMENT;
    }
    required_external_catchment = required_total_catchment - candidate->area;
    required_interior = 0;
    if (candidate->area > RIVER_LAKE_MAX_NO_INTERIOR_AREA) {
        required_interior =
            (candidate->area * RIVER_LAKE_MIN_INTERIOR_RATIO_NUMERATOR +
             RIVER_LAKE_MIN_INTERIOR_RATIO_DENOMINATOR - 1) /
            RIVER_LAKE_MIN_INTERIOR_RATIO_DENOMINATOR;
        if (required_interior < 1) required_interior = 1;
    }
    total_support = saturating_add64(candidate->direct_support_units,
                                     candidate->external_support_units);
    if (candidate->area < RIVER_LAKE_MIN_AREA) reasons |= RIVER_LAKE_REJECT_AREA;
    if ((candidate->pruned_cells > 0 && candidate->area < RIVER_LAKE_MIN_AREA) ||
        candidate->cardinal_edges < candidate->area ||
        candidate->interior_cells < required_interior ||
        (candidate->area > RIVER_LAKE_MAX_NO_INTERIOR_AREA &&
         candidate->max_interior_distance > RIVER_LAKE_MAX_INTERIOR_DISTANCE)) {
        reasons |= RIVER_LAKE_REJECT_SHAPE;
    }
    if (candidate->bbox_area <= 0 ||
        (int64_t)candidate->area * 100 <
            (int64_t)candidate->bbox_area * RIVER_LAKE_MIN_BBOX_FILL_PERCENT ||
        (int64_t)candidate->perimeter_edges * candidate->perimeter_edges >
            (int64_t)candidate->area *
                RIVER_LAKE_MAX_PERIMETER_SQUARED_PER_AREA) {
        reasons |= RIVER_LAKE_REJECT_SHAPE |
                   RIVER_LAKE_REJECT_COMPACTNESS;
    }
    if (candidate->interior_cells > 0 &&
        (int64_t)candidate->interior_perimeter_edges *
            candidate->interior_perimeter_edges >
        (int64_t)candidate->interior_cells *
            RIVER_LAKE_MAX_INTERIOR_PERIMETER_SQUARED_PER_AREA) {
        reasons |= RIVER_LAKE_REJECT_SHAPE |
                   RIVER_LAKE_REJECT_COMPACTNESS;
    }
    if (candidate->hole_count > RIVER_LAKE_MAX_HOLES) {
        reasons |= RIVER_LAKE_REJECT_SHAPE | RIVER_LAKE_REJECT_HOLES;
    }
    if (candidate->max_depth < RIVER_LAKE_MIN_MAX_DEPTH) reasons |= RIVER_LAKE_REJECT_DEPTH;
    if (candidate->deep_cells < required_deep ||
        candidate->largest_deep_core < required_deep) {
        reasons |= RIVER_LAKE_REJECT_DEEP_CELLS;
        if (candidate->largest_deep_core < required_deep) {
            reasons |= RIVER_LAKE_REJECT_DEEP_CORE;
        }
    }
    if (candidate->external_catchment_cells <
        (uint64_t)required_external_catchment) {
        reasons |= RIVER_LAKE_REJECT_CATCHMENT;
    }
    if (candidate->external_support_units <
            (uint64_t)required_external_catchment *
                RIVER_LAKE_MIN_EXTERNAL_SUPPORT_PER_CELL ||
        total_support <
            (uint64_t)candidate->area * RIVER_LAKE_MIN_SUPPORT_PER_CELL) {
        reasons |= RIVER_LAKE_REJECT_SUPPORT;
    }
    candidate->reject_reasons = reasons;
    return reasons;
}

int river_lake_qualification_accepts(RiverLakeQualification *candidate) {
    uint32_t reasons;
    if (!candidate) return 0;
    reasons = river_lake_qualification_model_reject_reasons(candidate);
    if (candidate->invalid_outlet_edges > 0 ||
        candidate->outlet_receiver_count != RIVER_LAKE_MIN_OUTLET_RECEIVERS) {
        reasons |= RIVER_LAKE_REJECT_SHAPE | RIVER_LAKE_REJECT_OUTLETS;
    }
    candidate->reject_reasons = reasons;
    return reasons == RIVER_LAKE_REJECT_NONE;
}
