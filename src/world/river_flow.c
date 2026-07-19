#include "world/river_flow.h"
#include "world/river_routing.h"

#include <limits.h>
#include <string.h>

static int clamp_int(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static int land_receiver(const RiverGenerationState *state, int index) {
    int receiver = state->input.receiver[index];
    if (receiver < 0 || receiver >= state->input.tile_count) return -1;
    return state->input.land_mask[receiver] ? receiver : -1;
}

static uint32_t local_runoff(const RiverGenerationState *state, int index) {
    int precipitation = clamp_int(state->input.precipitation[index], 0, 100);
    int slope = state->input.slope ? state->input.slope[index] : 0;
    int moisture_adjust = clamp_int(state->input.moisture_bias, 0, 100) / 20;
    int runoff = 1 + precipitation / 4 + precipitation * precipitation / 500 +
                 clamp_int(slope, 0, 120) / 8 + moisture_adjust;
    return (uint32_t)clamp_int(runoff, 1, 255);
}

static uint32_t automatic_threshold(RiverGenerationState *state, uint64_t runoff_total) {
    uint32_t mean;
    int contributing;

    if (state->land_count <= 0) return UINT_MAX;
    mean = (uint32_t)(runoff_total / (uint64_t)state->land_count);
    if (mean < 1) mean = 1;
    contributing = 70 + (50 - clamp_int(state->input.moisture_bias, 0, 100)) / 2 -
                   clamp_int(state->input.wetland_bias, 0, 100) / 5;
    contributing = clamp_int(contributing, 35, 110);
    if (state->land_count < contributing * 4) {
        contributing = clamp_int(state->land_count / 8, 4, contributing);
    }
    return mean * (uint32_t)contributing;
}

static void accumulate_flow(RiverGenerationState *state) {
    int i;

    for (i = state->topological_count - 1; i >= 0; i--) {
        int index = state->input.topological_order[i];
        int receiver = land_receiver(state, index);
        if (receiver >= 0) {
            uint64_t sum = (uint64_t)state->input.flow[receiver] + state->input.flow[index];
            state->input.flow[receiver] = sum > UINT_MAX ? UINT_MAX : (uint32_t)sum;
            state->input.upstream_count[receiver]++;
        }
    }
}

static void identify_channels(RiverGenerationState *state, uint32_t threshold) {
    int i;

    memset(state->visited, 0, (size_t)state->input.tile_count * sizeof(*state->visited));
    for (i = 0; i < state->topological_count; i++) {
        int index = state->input.topological_order[i];
        if (state->input.flow[index] < threshold) continue;
        state->cell_flags[index] |= RIVER_CELL_CHANNEL;
        state->diagnostics.channel_cells++;
    }
    for (i = 0; i < state->topological_count; i++) {
        int index = state->input.topological_order[i];
        int receiver;
        if (!(state->cell_flags[index] & RIVER_CELL_CHANNEL)) continue;
        receiver = land_receiver(state, index);
        if (receiver >= 0 && (state->cell_flags[receiver] & RIVER_CELL_CHANNEL) &&
            state->visited[receiver] < 255) {
            state->visited[receiver]++;
        }
    }
    for (i = 0; i < state->topological_count; i++) {
        int index = state->input.topological_order[i];
        if (!(state->cell_flags[index] & RIVER_CELL_CHANNEL)) continue;
        if (state->visited[index] == 0) {
            state->cell_flags[index] |= RIVER_CELL_SOURCE;
            state->diagnostics.sources++;
        }
        if (state->visited[index] >= 2) {
            state->cell_flags[index] |= RIVER_CELL_CONFLUENCE;
            state->diagnostics.confluences++;
        }
    }
}

static uint16_t width_for_flow(uint32_t flow, uint32_t threshold) {
    uint32_t ratio = threshold > 0 ? flow / threshold : flow;
    uint16_t width = 1;

    while (ratio >= 2 && width < 12) {
        width++;
        ratio = (ratio + 1) / 2;
    }
    return width;
}

static void build_order_stems_width(RiverGenerationState *state, uint32_t threshold) {
    int i;

    memset(state->max_upstream_order, 0,
           (size_t)state->input.tile_count * sizeof(*state->max_upstream_order));
    memset(state->max_upstream_count, 0,
           (size_t)state->input.tile_count * sizeof(*state->max_upstream_count));
    memset(state->dominant_parent, 0xff,
           (size_t)state->input.tile_count * sizeof(*state->dominant_parent));
    for (i = state->topological_count - 1; i >= 0; i--) {
        int index = state->input.topological_order[i];
        int receiver;
        uint8_t order;
        uint16_t width;
        if (!(state->cell_flags[index] & RIVER_CELL_CHANNEL)) continue;
        order = state->max_upstream_order[index];
        if (order == 0) order = 1;
        else if (state->max_upstream_count[index] >= 2 && order < 255) order++;
        state->input.order[index] = order;
        if (state->dominant_parent[index] >= 0) {
            state->main_stem[index] = state->main_stem[state->dominant_parent[index]];
        } else {
            state->main_stem[index] = index + 1;
        }
        width = width_for_flow(state->input.flow[index], threshold);
        if (state->input.width_field[index] < width) state->input.width_field[index] = width;
        receiver = land_receiver(state, index);
        if (receiver < 0 || !(state->cell_flags[receiver] & RIVER_CELL_CHANNEL)) continue;
        if (order > state->max_upstream_order[receiver]) {
            state->max_upstream_order[receiver] = order;
            state->max_upstream_count[receiver] = 1;
        } else if (order == state->max_upstream_order[receiver] &&
                   state->max_upstream_count[receiver] < 255) {
            state->max_upstream_count[receiver]++;
        }
        if (state->input.width_field[receiver] < state->input.width_field[index]) {
            state->input.width_field[receiver] = state->input.width_field[index];
        }
        if (state->dominant_parent[receiver] < 0 ||
            state->input.flow[index] > state->input.flow[state->dominant_parent[receiver]] ||
            (state->input.flow[index] == state->input.flow[state->dominant_parent[receiver]] &&
             river_routing_tie_key(state, index, RIVER_TIE_MAIN_STEM) <
             river_routing_tie_key(state, state->dominant_parent[receiver],
                                   RIVER_TIE_MAIN_STEM))) {
            state->dominant_parent[receiver] = index;
        }
    }
}

static void validate_flow(RiverGenerationState *state) {
    int i;

    memset(state->check_flow, 0,
           (size_t)state->input.tile_count * sizeof(*state->check_flow));
    for (i = 0; i < state->topological_count; i++) {
        int index = state->input.topological_order[i];
        state->check_flow[index] = state->input.runoff[index];
    }
    for (i = state->topological_count - 1; i >= 0; i--) {
        int index = state->input.topological_order[i];
        int receiver = land_receiver(state, index);
        if (receiver >= 0) state->check_flow[receiver] += state->check_flow[index];
    }
    for (i = 0; i < state->topological_count; i++) {
        int index = state->input.topological_order[i];
        int receiver = land_receiver(state, index);
        if (state->check_flow[index] != state->input.flow[index]) {
            state->diagnostics.flow_conservation_errors++;
        }
        if (state->input.flow[index] > state->diagnostics.max_flow) {
            state->diagnostics.max_flow = state->input.flow[index];
        }
        if (!(state->cell_flags[index] & RIVER_CELL_CHANNEL)) continue;
        if (state->input.order[index] > state->diagnostics.max_order) {
            state->diagnostics.max_order = state->input.order[index];
        }
        if (state->input.width_field[index] > state->diagnostics.max_width) {
            state->diagnostics.max_width = state->input.width_field[index];
        }
        if (receiver >= 0 && (state->cell_flags[receiver] & RIVER_CELL_CHANNEL)) {
            if (state->input.width_field[receiver] < state->input.width_field[index]) {
                state->diagnostics.width_regressions++;
            }
            if (state->input.order[receiver] < state->input.order[index]) {
                state->diagnostics.order_errors++;
            }
        }
    }
}

int river_flow_build(RiverGenerationState *state) {
    uint64_t runoff_total = 0;
    uint32_t threshold;
    int i;

    if (!state) return 0;
    for (i = 0; i < state->topological_count; i++) {
        int index = state->input.topological_order[i];
        state->input.runoff[index] = local_runoff(state, index);
        state->input.flow[index] = state->input.runoff[index];
        runoff_total += state->input.runoff[index];
    }
    accumulate_flow(state);
    threshold = state->input.channel_threshold;
    if (threshold == 0) threshold = automatic_threshold(state, runoff_total);
    if (threshold < 8) threshold = 8;
    state->diagnostics.channel_threshold = threshold;
    identify_channels(state, threshold);
    build_order_stems_width(state, threshold);
    validate_flow(state);
    return state->diagnostics.flow_conservation_errors == 0 &&
           state->diagnostics.width_regressions == 0 && state->diagnostics.order_errors == 0;
}

static int append_segment(RiverGenerationState *state, int from, int to, uint32_t flow,
                          uint16_t width, uint8_t order, uint16_t flags, uint8_t kind) {
    RiverNetworkSegment *segment;
    if (state->segment_count >= state->segment_capacity) return 0;
    segment = &state->segments[state->segment_count++];
    segment->from = from;
    segment->to = to;
    segment->main_stem = state->main_stem[from];
    segment->flow = flow;
    segment->width = width;
    segment->order = order;
    segment->flags = flags;
    segment->kind = kind;
    return 1;
}

int river_flow_build_segments(RiverGenerationState *state) {
    int required;
    int i;

    if (!state) return 0;
    state->segment_count = 0;
    required = state->distributary_count;
    for (i = 0; i < state->topological_count; i++) {
        int index = state->input.topological_order[i];
        int receiver = state->input.receiver[index];
        if (!(state->cell_flags[index] & RIVER_CELL_CHANNEL) || receiver < 0) continue;
        if (state->cell_flags[index] & RIVER_CELL_DELTA) continue;
        if ((state->cell_flags[index] & RIVER_CELL_LAKE) &&
            receiver < state->input.tile_count &&
            (state->cell_flags[receiver] & RIVER_CELL_LAKE)) continue;
        required++;
    }
    if (!river_state_resize_segments(state, required)) {
        state->diagnostics.segment_allocation_errors++;
        return 0;
    }
    for (i = 0; i < state->topological_count; i++) {
        int index = state->input.topological_order[i];
        int receiver = state->input.receiver[index];
        if (!(state->cell_flags[index] & RIVER_CELL_CHANNEL) || receiver < 0) continue;
        if (state->cell_flags[index] & RIVER_CELL_DELTA) continue;
        if ((state->cell_flags[index] & RIVER_CELL_LAKE) && receiver < state->input.tile_count &&
            (state->cell_flags[receiver] & RIVER_CELL_LAKE)) continue;
        if (!append_segment(state, index, receiver, state->input.flow[index],
                            state->input.width_field[index], state->input.order[index],
                            state->cell_flags[index], RIVER_SEGMENT_ORDINARY)) return 0;
        state->diagnostics.ordinary_segments++;
    }
    for (i = 0; i < state->distributary_count; i++) {
        const RiverDistributary *branch = &state->distributaries[i];
        if (!append_segment(state, branch->from, branch->to, branch->flow,
                            branch->width, state->input.order[branch->from],
                            state->cell_flags[branch->from] | RIVER_CELL_DISTRIBUTARY,
                            RIVER_SEGMENT_DISTRIBUTARY)) return 0;
    }
    return 1;
}
