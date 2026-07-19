#include "world/river_routing.h"

#include <limits.h>
#include <string.h>

static const int ROUTE_DX[8] = {1, 1, 0, -1, -1, -1, 0, 1};
static const int ROUTE_DY[8] = {0, 1, 1, 1, 0, -1, -1, -1};
static const int ROUTE_STEP[8] = {1000, 1414, 1000, 1414,
                                  1000, 1414, 1000, 1414};

static uint32_t mix32(uint32_t value) {
    value ^= value >> 16;
    value *= UINT32_C(0x7feb352d);
    value ^= value >> 15;
    value *= UINT32_C(0x846ca68b);
    return value ^ (value >> 16);
}

static int in_bounds(const RiverGenerationState *state, int x, int y) {
    return x >= 0 && x < state->input.width &&
           y >= 0 && y < state->input.height;
}

static int direction_between(const RiverGenerationState *state, int from, int to) {
    int dx;
    int dy;
    int direction;
    if (!state || from < 0 || from >= state->input.tile_count ||
        to < 0 || to >= state->input.tile_count) return -1;
    dx = to % state->input.width - from % state->input.width;
    dy = to / state->input.width - from / state->input.width;
    for (direction = 0; direction < 8; direction++) {
        if (ROUTE_DX[direction] == dx && ROUTE_DY[direction] == dy) return direction;
    }
    return -1;
}

int river_routing_dx(int direction) {
    return direction >= 0 && direction < 8 ? ROUTE_DX[direction] : 0;
}

int river_routing_dy(int direction) {
    return direction >= 0 && direction < 8 ? ROUTE_DY[direction] : 0;
}

uint64_t river_routing_seeded_tie_key(uint32_t seed, int index,
                                      RiverRoutingTieDomain domain) {
    uint32_t first = mix32(seed ^ (uint32_t)index * UINT32_C(0x9e3779b9) ^
                           (uint32_t)domain * UINT32_C(0x85ebca6b));
    uint32_t second = mix32(seed ^ (uint32_t)index * UINT32_C(0xc2b2ae35) ^
                            (uint32_t)domain * UINT32_C(0x27d4eb2f) ^
                            UINT32_C(0xa5a5f00d));
    return ((uint64_t)first << 32) | second;
}

uint64_t river_routing_tie_key(const RiverGenerationState *state, int index,
                               RiverRoutingTieDomain domain) {
    return river_routing_seeded_tie_key(
        state ? state->input.seed : 0, index, domain);
}

uint64_t river_routing_pair_key(const RiverGenerationState *state,
                                int from, int to,
                                RiverRoutingTieDomain domain) {
    uint32_t seed = state ? state->input.seed : 0;
    uint32_t first = mix32(seed ^ (uint32_t)from * UINT32_C(0x9e3779b9) ^
                           (uint32_t)to * UINT32_C(0x85ebca6b) ^
                           (uint32_t)domain * UINT32_C(0xc2b2ae35));
    uint32_t second = mix32(seed ^ (uint32_t)from * UINT32_C(0x27d4eb2f) ^
                            (uint32_t)to * UINT32_C(0x165667b1) ^
                            (uint32_t)domain * UINT32_C(0xd3a2646c));
    return ((uint64_t)first << 32) | second;
}

int river_routing_permuted_direction(const RiverGenerationState *state,
                                     int index, int slot,
                                     RiverRoutingTieDomain domain) {
    static const int odd_steps[4] = {1, 3, 5, 7};
    uint64_t key = river_routing_tie_key(state, index, domain);
    int start = (int)(key & 7u);
    int step = odd_steps[(key >> 3) & 3u];
    if (slot < 0 || slot >= 8) return -1;
    return (start + slot * step) & 7;
}

int river_routing_choose_water_receiver(const RiverGenerationState *state,
                                        int x, int y) {
    uint64_t best_key = UINT64_MAX;
    int from;
    int best = -1;
    int slot;
    if (!state || !in_bounds(state, x, y)) return -1;
    from = y * state->input.width + x;
    for (slot = 0; slot < 8; slot++) {
        int direction = river_routing_permuted_direction(
            state, from, slot, RIVER_TIE_WATER_OUTLET);
        int nx = x + ROUTE_DX[direction];
        int ny = y + ROUTE_DY[direction];
        int neighbor;
        uint64_t key;
        if (!in_bounds(state, nx, ny)) continue;
        neighbor = ny * state->input.width + nx;
        if (state->input.land_mask[neighbor]) continue;
        key = river_routing_pair_key(state, from, neighbor,
                                     RIVER_TIE_WATER_OUTLET);
        if (best < 0 || key < best_key) {
            best = neighbor;
            best_key = key;
        }
    }
    return best;
}

static int candidate_better(const RiverGenerationState *state, int from,
                            int candidate, int direction, int best,
                            int best_direction) {
    int filled_drop = state->filled_elevation[from] -
                      state->filled_elevation[candidate];
    int best_filled_drop;
    int original_drop;
    int best_original_drop;
    int64_t left;
    int64_t right;
    uint64_t key;
    uint64_t best_key;
    if (best < 0) return 1;
    best_filled_drop = state->filled_elevation[from] -
                       state->filled_elevation[best];
    left = (int64_t)filled_drop * ROUTE_STEP[best_direction];
    right = (int64_t)best_filled_drop * ROUTE_STEP[direction];
    if (left != right) return left > right;
    original_drop = state->input.elevation[from] -
                    state->input.elevation[candidate];
    best_original_drop = state->input.elevation[from] -
                         state->input.elevation[best];
    left = (int64_t)original_drop * ROUTE_STEP[best_direction];
    right = (int64_t)best_original_drop * ROUTE_STEP[direction];
    if (left != right) return left > right;
    key = river_routing_pair_key(state, from, candidate, RIVER_TIE_RECEIVER);
    best_key = river_routing_pair_key(state, from, best, RIVER_TIE_RECEIVER);
    return key < best_key;
}

int river_routing_assign_receivers(RiverGenerationState *state) {
    int i;
    if (!state || state->topological_count != state->land_count) return 0;
    memset(state->dominant_parent, 0xff,
           (size_t)state->input.tile_count * sizeof(*state->dominant_parent));
    for (i = 0; i < state->topological_count; i++) {
        state->dominant_parent[state->input.topological_order[i]] = i;
    }
    for (i = 0; i < state->topological_count; i++) {
        int index = state->input.topological_order[i];
        int receiver = state->input.receiver[index];
        int x = index % state->input.width;
        int y = index / state->input.width;
        int best = -1;
        int best_direction = -1;
        int slot;
        if (receiver < 0 ||
            (receiver < state->input.tile_count &&
             !state->input.land_mask[receiver])) continue;
        for (slot = 0; slot < 8; slot++) {
            int direction = river_routing_permuted_direction(
                state, index, slot, RIVER_TIE_RECEIVER);
            int nx = x + ROUTE_DX[direction];
            int ny = y + ROUTE_DY[direction];
            int neighbor;
            if (!in_bounds(state, nx, ny)) continue;
            neighbor = ny * state->input.width + nx;
            if (!state->input.land_mask[neighbor] ||
                state->dominant_parent[neighbor] < 0 ||
                state->dominant_parent[neighbor] >= i) continue;
            if (state->filled_elevation[neighbor] >
                state->filled_elevation[index]) continue;
            if (candidate_better(state, index, neighbor, direction,
                                 best, best_direction)) {
                best = neighbor;
                best_direction = direction;
            }
        }
        if (best < 0) return 0;
        state->input.receiver[index] = best;
    }
    return 1;
}

void river_routing_collect_diagnostics(RiverGenerationState *state) {
    int i;
    if (!state) return;
    memset(state->diagnostics.receiver_direction_histogram, 0,
           sizeof(state->diagnostics.receiver_direction_histogram));
    memset(state->diagnostics.flat_direction_histogram, 0,
           sizeof(state->diagnostics.flat_direction_histogram));
    state->diagnostics.receiver_edges = 0;
    state->diagnostics.flat_receiver_edges = 0;
    state->diagnostics.receiver_lower_index_edges = 0;
    state->diagnostics.flat_receiver_lower_index_edges = 0;
    state->diagnostics.max_same_direction_run = 0;
    state->diagnostics.max_flat_same_direction_run = 0;
    memset(state->check_flow, 0,
           (size_t)state->input.tile_count * sizeof(*state->check_flow));
    for (i = 0; i < state->topological_count; i++) {
        int index = state->input.topological_order[i];
        int receiver = state->input.receiver[index];
        int direction;
        int run = 1;
        int flat_run = 0;
        if (receiver < 0 || receiver >= state->input.tile_count) continue;
        direction = direction_between(state, index, receiver);
        if (direction < 0) continue;
        state->diagnostics.receiver_edges++;
        state->diagnostics.receiver_direction_histogram[direction]++;
        if (receiver < index) state->diagnostics.receiver_lower_index_edges++;
        if (state->input.land_mask[receiver]) {
            int next = state->input.receiver[receiver];
            int next_direction = direction_between(state, receiver, next);
            if (next_direction == direction) run += (int)(state->check_flow[receiver] & 0xffffu);
            if (run > 0xffff) run = 0xffff;
            if (state->filled_elevation[index] == state->filled_elevation[receiver]) {
                flat_run = 1;
                state->diagnostics.flat_receiver_edges++;
                if (receiver < index) {
                    state->diagnostics.flat_receiver_lower_index_edges++;
                }
                state->diagnostics.flat_direction_histogram[direction]++;
                if (next_direction == direction && next >= 0 &&
                    state->filled_elevation[receiver] == state->filled_elevation[next]) {
                    flat_run += (int)(state->check_flow[receiver] >> 16);
                }
                if (flat_run > 0xffff) flat_run = 0xffff;
            }
        }
        state->check_flow[index] = (uint32_t)run | ((uint32_t)flat_run << 16);
        if (run > state->diagnostics.max_same_direction_run)
            state->diagnostics.max_same_direction_run = run;
        if (flat_run > state->diagnostics.max_flat_same_direction_run)
            state->diagnostics.max_flat_same_direction_run = flat_run;
    }
}
