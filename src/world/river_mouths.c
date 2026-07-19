#include "world/river_mouths.h"
#include "world/river_routing.h"

#include <stdlib.h>

static int in_bounds(const RiverGenerationState *state, int x, int y) {
    return x >= 0 && x < state->input.width && y >= 0 && y < state->input.height;
}

static int collect_water_targets(const RiverGenerationState *state, int index,
                                 int targets[8]) {
    int x = index % state->input.width;
    int y = index / state->input.width;
    int primary = state->input.receiver[index];
    int count = 0;
    int slot;

    if (primary >= 0 && primary < state->input.tile_count &&
        !state->input.land_mask[primary]) targets[count++] = primary;
    for (slot = 0; slot < 8; slot++) {
        int direction = river_routing_permuted_direction(
            state, index, slot, RIVER_TIE_DELTA_TARGET);
        int nx = x + river_routing_dx(direction);
        int ny = y + river_routing_dy(direction);
        int target;
        int duplicate = 0;
        int i;
        if (!in_bounds(state, nx, ny)) continue;
        target = ny * state->input.width + nx;
        if (state->input.land_mask[target]) continue;
        for (i = 0; i < count; i++) if (targets[i] == target) duplicate = 1;
        if (!duplicate) targets[count++] = target;
    }
    return count;
}

static int segment_matches(int from, int to, int first, int second) {
    return (from == first && to == second) || (from == second && to == first);
}

static int branch_crosses_existing(const RiverGenerationState *state, int from, int to) {
    int from_x = from % state->input.width;
    int from_y = from / state->input.width;
    int to_x = to % state->input.width;
    int to_y = to / state->input.width;
    int other_a;
    int other_b;
    int i;

    if (abs(from_x - to_x) != 1 || abs(from_y - to_y) != 1) return 0;
    other_a = from_y * state->input.width + to_x;
    other_b = to_y * state->input.width + from_x;
    if (state->input.receiver[other_a] == other_b ||
        state->input.receiver[other_b] == other_a) return 1;
    for (i = 0; i < state->distributary_count; i++) {
        const RiverDistributary *branch = &state->distributaries[i];
        if (segment_matches(branch->from, branch->to, other_a, other_b)) return 1;
    }
    return 0;
}

static int filter_crossing_targets(const RiverGenerationState *state, int from,
                                   int targets[8], int count) {
    int kept = 0;
    int i;

    for (i = 0; i < count; i++) {
        if (!branch_crosses_existing(state, from, targets[i])) targets[kept++] = targets[i];
    }
    return kept;
}

static int delta_eligible(const RiverGenerationState *state, int index, int water_count) {
    int slope = state->input.slope ? abs(state->input.slope[index]) : 0;
    int altitude = state->input.relative_altitude ? state->input.relative_altitude[index] : 0;
    uint64_t threshold = state->diagnostics.channel_threshold;

    if (water_count < 2) return 0;
    if (slope > 6 || altitude > 8) return 0;
    return (uint64_t)state->input.flow[index] >= threshold * 4u;
}

static int append_delta(RiverGenerationState *state, int index,
                        const int targets[8], int water_count) {
    uint32_t total = state->input.flow[index];
    int branch_count = water_count >= RIVER_DELTA_BRANCH_MAX &&
                       (uint64_t)total >=
                           (uint64_t)state->diagnostics.channel_threshold * 8u
        ? RIVER_DELTA_BRANCH_MAX : 2;
    uint32_t base;
    uint32_t remainder;
    uint32_t conserved = 0;
    uint16_t parent_width = state->input.width_field[index];
    uint16_t branch_width;
    int branch;

    if (!river_state_resize_distributaries(
            state, state->distributary_count + branch_count)) {
        state->diagnostics.distributary_allocation_errors++;
        return 0;
    }
    base = total / (uint32_t)branch_count;
    remainder = total % (uint32_t)branch_count;
    branch_width = parent_width / (uint16_t)branch_count;
    if (branch_width < 1) branch_width = 1;
    for (branch = 0; branch < branch_count; branch++) {
        RiverDistributary *out = &state->distributaries[state->distributary_count++];
        out->from = index;
        out->to = targets[branch];
        out->flow = base + (branch < (int)remainder ? 1u : 0u);
        out->width = branch_width;
        out->order = state->input.order[index];
        out->branch_index = (uint8_t)branch;
        out->branch_count = (uint8_t)branch_count;
        state->cell_flags[out->to] |= RIVER_CELL_DISTRIBUTARY;
        conserved += out->flow;
    }
    if (conserved != total) {
        state->diagnostics.flow_conservation_errors++;
        return 0;
    }
    state->cell_flags[index] |= RIVER_CELL_DELTA;
    state->diagnostics.deltas++;
    state->diagnostics.distributaries += branch_count;
    return 1;
}

int river_mouths_build(RiverGenerationState *state) {
    int i;

    if (!state) return 0;
    state->distributary_count = 0;
    for (i = 0; i < state->topological_count; i++) {
        int index = state->input.topological_order[i];
        int receiver = state->input.receiver[index];
        int targets[8];
        int water_count;
        if (!(state->cell_flags[index] & RIVER_CELL_CHANNEL)) continue;
        if (receiver < 0) {
            if (state->cell_flags[index] & RIVER_CELL_EDGE_OUTLET) {
                state->cell_flags[index] |= RIVER_CELL_MOUTH;
                state->diagnostics.mouths++;
            }
            continue;
        }
        if (receiver >= state->input.tile_count || state->input.land_mask[receiver]) continue;
        state->cell_flags[index] |= RIVER_CELL_MOUTH;
        state->diagnostics.mouths++;
        water_count = collect_water_targets(state, index, targets);
        water_count = filter_crossing_targets(state, index, targets, water_count);
        if (delta_eligible(state, index, water_count)) {
            if (!append_delta(state, index, targets, water_count)) return 0;
        }
    }
    return state->diagnostics.flow_conservation_errors == 0;
}
