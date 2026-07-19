#include "world/river_drainage.h"
#include "world/river_lake_final_validation.h"
#include "world/river_lakes.h"
#include "world/river_routing.h"

static int in_bounds(const RiverGenerationState *state, int x, int y) {
    return x >= 0 && x < state->input.width && y >= 0 && y < state->input.height;
}

static int index_at(const RiverGenerationState *state, int x, int y) {
    return y * state->input.width + x;
}

static int heap_less(const RiverGenerationState *state, int left, int right) {
    int le = state->filled_elevation[left];
    int re = state->filled_elevation[right];
    uint64_t left_key;
    uint64_t right_key;
    if (le != re) return le < re;
    left_key = river_routing_tie_key(state, left, RIVER_TIE_HEAP);
    right_key = river_routing_tie_key(state, right, RIVER_TIE_HEAP);
    return left_key < right_key || (left_key == right_key && left < right);
}

static void heap_push(RiverGenerationState *state, int index) {
    int slot = state->heap_count++;

    while (slot > 0) {
        int parent = (slot - 1) / 2;
        if (heap_less(state, state->heap[parent], index)) break;
        state->heap[slot] = state->heap[parent];
        slot = parent;
    }
    state->heap[slot] = index;
}

static int heap_pop(RiverGenerationState *state) {
    int result = state->heap[0];
    int last = state->heap[--state->heap_count];
    int slot = 0;

    while (state->heap_count > 0) {
        int child = slot * 2 + 1;
        if (child >= state->heap_count) break;
        if (child + 1 < state->heap_count &&
            heap_less(state, state->heap[child + 1], state->heap[child])) child++;
        if (heap_less(state, last, state->heap[child])) break;
        state->heap[slot] = state->heap[child];
        slot = child;
    }
    if (state->heap_count > 0) state->heap[slot] = last;
    return result;
}

static void seed_outlets(RiverGenerationState *state) {
    int x;
    int y;

    for (y = 0; y < state->input.height; y++) {
        for (x = 0; x < state->input.width; x++) {
            int index = index_at(state, x, y);
            int water_receiver;
            int edge;
            if (!state->input.land_mask[index]) continue;
            state->land_count++;
            state->cell_flags[index] = RIVER_CELL_LAND;
            state->filled_elevation[index] = state->input.elevation[index];
            water_receiver = river_routing_choose_water_receiver(state, x, y);
            edge = x == 0 || y == 0 || x == state->input.width - 1 ||
                   y == state->input.height - 1;
            if (water_receiver < 0 && !edge) continue;
            state->visited[index] = 1;
            state->input.receiver[index] = water_receiver;
            if (water_receiver < 0) state->cell_flags[index] |= RIVER_CELL_EDGE_OUTLET;
            heap_push(state, index);
        }
    }
}

static void priority_flood(RiverGenerationState *state) {
    while (state->heap_count > 0) {
        int current = heap_pop(state);
        int cx = current % state->input.width;
        int cy = current / state->input.width;
        int slot;

        state->input.topological_order[state->topological_count++] = current;
        for (slot = 0; slot < 8; slot++) {
            int direction = river_routing_permuted_direction(
                state, current, slot, RIVER_TIE_FLOOD_NEIGHBOR);
            int nx = cx + river_routing_dx(direction);
            int ny = cy + river_routing_dy(direction);
            int neighbor;
            if (!in_bounds(state, nx, ny)) continue;
            neighbor = index_at(state, nx, ny);
            if (!state->input.land_mask[neighbor] || state->visited[neighbor]) continue;
            state->visited[neighbor] = 1;
            state->input.receiver[neighbor] = current;
            state->filled_elevation[neighbor] = state->input.elevation[neighbor];
            if (state->filled_elevation[neighbor] < state->filled_elevation[current]) {
                state->filled_elevation[neighbor] = state->filled_elevation[current];
            }
            heap_push(state, neighbor);
        }
    }
}

static int diagonal_source(const RiverGenerationState *state, int first, int second) {
    if (state->input.receiver[first] == second) return first;
    if (state->input.receiver[second] == first) return second;
    return -1;
}

static int repair_one_crossing(RiverGenerationState *state,
                               int source_a, int source_b) {
    int target_a = state->input.receiver[source_a];
    int target_b = state->input.receiver[source_b];
    int rank_a = state->dominant_parent[source_a];
    int rank_b = state->dominant_parent[source_b];
    int can_a_merge_b = state->dominant_parent[target_b] < rank_a;
    int can_b_merge_a = state->dominant_parent[target_a] < rank_b;

    int prefer_a = river_routing_tie_key(state, source_a, RIVER_TIE_CROSSING) <
                   river_routing_tie_key(state, source_b, RIVER_TIE_CROSSING);

    if (can_a_merge_b && (!can_b_merge_a || prefer_a)) {
        state->input.receiver[source_a] = target_b;
        return 1;
    }
    if (can_b_merge_a) {
        state->input.receiver[source_b] = target_a;
        return 1;
    }
    return 0;
}

static void repair_diagonal_crossings(RiverGenerationState *state) {
    int x;
    int y;
    int i;

    for (i = 0; i < state->topological_count; i++) {
        state->dominant_parent[state->input.topological_order[i]] = i;
    }
    for (y = 0; y + 1 < state->input.height; y++) {
        for (x = 0; x + 1 < state->input.width; x++) {
            int top_left = index_at(state, x, y);
            int top_right = top_left + 1;
            int bottom_left = top_left + state->input.width;
            int bottom_right = bottom_left + 1;
            int source_a = diagonal_source(state, top_left, bottom_right);
            int source_b = diagonal_source(state, top_right, bottom_left);
            if (source_a < 0 || source_b < 0) continue;
            if (repair_one_crossing(state, source_a, source_b)) {
                state->diagnostics.crossing_repairs++;
            } else {
                state->diagnostics.crossing_errors++;
            }
        }
    }
}

static void assign_basins_and_validate(RiverGenerationState *state) {
    int basin_count = 0;
    int i;

    for (i = 0; i < state->topological_count; i++) {
        int index = state->input.topological_order[i];
        int receiver = state->input.receiver[index];
        if (receiver >= state->input.tile_count) {
            state->diagnostics.invalid_receivers++;
            state->input.basin[index] = basin_count++;
        } else if (receiver < 0 || !state->input.land_mask[receiver]) {
            state->input.basin[index] = basin_count++;
        } else {
            state->input.basin[index] = state->input.basin[receiver];
            if (state->dominant_parent[receiver] >= state->dominant_parent[index]) {
                state->diagnostics.cycle_errors++;
            }
        }
        if (receiver < 0 && !(state->cell_flags[index] &
            (RIVER_CELL_CLOSED_BASIN | RIVER_CELL_EDGE_OUTLET))) {
            state->diagnostics.inland_dead_ends++;
        }
    }
}

int river_drainage_build(RiverGenerationState *state) {
    int final_lakes_valid;
    int i;

    if (!state) return 0;
    seed_outlets(state);
    if (state->land_count <= 0) return 1;
    priority_flood(state);
    if (state->topological_count != state->land_count) return 0;
    if (!river_routing_assign_receivers(state)) return 0;
    if (!river_lakes_resolve(state)) return 0;
    repair_diagonal_crossings(state);
    assign_basins_and_validate(state);
    river_routing_collect_diagnostics(state);
    state->diagnostics.land_cells = state->land_count;
    state->diagnostics.topological_cells = state->topological_count;
    state->diagnostics.lake_cells = 0;
    state->diagnostics.closed_basins = 0;
    state->diagnostics.salt_lakes = 0;
    for (i = 0; i < state->topological_count; i++) {
        int index = state->input.topological_order[i];
        if (state->cell_flags[index] & RIVER_CELL_LAKE) state->diagnostics.lake_cells++;
        if (state->cell_flags[index] & RIVER_CELL_CLOSED_BASIN) {
            state->diagnostics.closed_basins++;
            if (!(state->cell_flags[index] & RIVER_CELL_LAKE) ||
                state->input.receiver[index] >= 0) state->diagnostics.invalid_receivers++;
        }
        if (state->cell_flags[index] & RIVER_CELL_SALT_LAKE) {
            state->diagnostics.salt_lakes++;
            if (!(state->cell_flags[index] & RIVER_CELL_CLOSED_BASIN)) {
                state->diagnostics.invalid_receivers++;
            }
        }
    }
    if (state->diagnostics.lake_cells > state->diagnostics.depression_cells) {
        state->diagnostics.invalid_receivers++;
    }
    final_lakes_valid = river_lake_final_validation_run(state, NULL);
    return final_lakes_valid && state->diagnostics.invalid_receivers == 0 &&
           state->diagnostics.cycle_errors == 0 &&
           state->diagnostics.inland_dead_ends == 0 && state->diagnostics.crossing_errors == 0;
}
