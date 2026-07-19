#include "world/river_lake_shape.h"

#include "world/river_routing.h"

#include <stdint.h>

static const int CARDINAL_DX[4] = {1, 0, -1, 0};
static const int CARDINAL_DY[4] = {0, 1, 0, -1};

static int in_bounds(const RiverGenerationState *state, int x, int y) {
    return x >= 0 && x < state->input.width &&
           y >= 0 && y < state->input.height;
}

static int depression_depth(const RiverGenerationState *state, int index) {
    return state->filled_elevation[index] - state->input.elevation[index];
}

static int retained_member(const RiverGenerationState *state, int index,
                           int label) {
    return index >= 0 && index < state->input.tile_count &&
           state->dominant_parent[index] == label && state->visited[index] != 0;
}

static int cardinal_degree(const RiverGenerationState *state, int index,
                           int label) {
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
        if (retained_member(state, neighbor, label)) degree++;
    }
    return degree;
}

static int full_2x2_at(const RiverGenerationState *state, int index,
                       int label) {
    int x = index % state->input.width;
    int y = index / state->input.width;
    int right;
    int down;
    int diagonal;
    if (x + 1 >= state->input.width || y + 1 >= state->input.height) return 0;
    right = index + 1;
    down = index + state->input.width;
    diagonal = down + 1;
    return retained_member(state, right, label) &&
           retained_member(state, down, label) &&
           retained_member(state, diagonal, label);
}

static int largest_deep_core(RiverGenerationState *state,
                             const RiverLakeQualification *candidate) {
    int largest = 0;
    int i;
    for (i = 0; i < candidate->area; i++) {
        state->max_upstream_order[state->heap[i]] = 0;
    }
    for (i = 0; i < candidate->area; i++) {
        int seed = state->heap[i];
        int head = 0;
        int tail = 0;
        if (depression_depth(state, seed) < RIVER_LAKE_DEEP_CELL_DEPTH ||
            state->max_upstream_order[seed]) continue;
        state->max_upstream_order[seed] = 1;
        state->main_stem[tail++] = seed;
        while (head < tail) {
            int current = state->main_stem[head++];
            int x = current % state->input.width;
            int y = current / state->input.width;
            int direction;
            for (direction = 0; direction < 4; direction++) {
                int nx = x + CARDINAL_DX[direction];
                int ny = y + CARDINAL_DY[direction];
                int neighbor;
                if (!in_bounds(state, nx, ny)) continue;
                neighbor = ny * state->input.width + nx;
                if (!retained_member(state, neighbor, candidate->label) ||
                    depression_depth(state, neighbor) < RIVER_LAKE_DEEP_CELL_DEPTH ||
                    state->max_upstream_order[neighbor]) continue;
                state->max_upstream_order[neighbor] = 1;
                state->main_stem[tail++] = neighbor;
            }
        }
        if (tail > largest) largest = tail;
    }
    return largest;
}

static int max_distance_to_interior(RiverGenerationState *state,
                                    const RiverLakeQualification *candidate) {
    int head = 0;
    int tail = 0;
    int maximum = 0;
    int i;
    for (i = 0; i < candidate->area; i++) {
        int index = state->heap[i];
        state->max_upstream_count[index] = UINT8_MAX;
        if (cardinal_degree(state, index, candidate->label) == 4) {
            state->max_upstream_count[index] = 0;
            state->main_stem[tail++] = index;
        }
    }
    if (tail == 0) return candidate->area > RIVER_LAKE_MAX_NO_INTERIOR_AREA ?
                          candidate->area : 0;
    while (head < tail) {
        int current = state->main_stem[head++];
        int distance = state->max_upstream_count[current];
        int x = current % state->input.width;
        int y = current / state->input.width;
        int direction;
        if (distance > maximum) maximum = distance;
        for (direction = 0; direction < 4; direction++) {
            int nx = x + CARDINAL_DX[direction];
            int ny = y + CARDINAL_DY[direction];
            int neighbor;
            if (!in_bounds(state, nx, ny)) continue;
            neighbor = ny * state->input.width + nx;
            if (!retained_member(state, neighbor, candidate->label) ||
                state->max_upstream_count[neighbor] != UINT8_MAX) continue;
            state->max_upstream_count[neighbor] =
                (uint8_t)(distance < UINT8_MAX - 1 ? distance + 1 : UINT8_MAX - 1);
            state->main_stem[tail++] = neighbor;
        }
    }
    return tail == candidate->area ? maximum : candidate->area;
}

void river_lake_shape_collect(RiverGenerationState *state,
                              RiverLakeQualification *candidate) {
    int64_t precipitation_total = 0;
    int degree_total = 0;
    int i;
    if (!state || !candidate || candidate->area <= 0) return;
    candidate->sink = state->heap[0];
    candidate->bbox_min_x = candidate->bbox_max_x =
        candidate->sink % state->input.width;
    candidate->bbox_min_y = candidate->bbox_max_y =
        candidate->sink / state->input.width;
    for (i = 0; i < candidate->area; i++) {
        int index = state->heap[i];
        int depth = depression_depth(state, index);
        int x = index % state->input.width;
        int y = index / state->input.width;
        int degree = cardinal_degree(state, index, candidate->label);
        precipitation_total += state->input.precipitation[index];
        if (depth > candidate->max_depth) candidate->max_depth = depth;
        if (depth >= RIVER_LAKE_DEEP_CELL_DEPTH) candidate->deep_cells++;
        degree_total += degree;
        candidate->perimeter_edges += 4 - degree;
        if (degree == 4) {
            int direction;
            candidate->interior_cells++;
            for (direction = 0; direction < 2; direction++) {
                int nx = x + CARDINAL_DX[direction];
                int ny = y + CARDINAL_DY[direction];
                int neighbor;
                if (!in_bounds(state, nx, ny)) continue;
                neighbor = ny * state->input.width + nx;
                if (retained_member(state, neighbor, candidate->label) &&
                    cardinal_degree(state, neighbor, candidate->label) == 4) {
                    candidate->interior_cardinal_edges++;
                }
            }
        }
        candidate->solid_2x2_blocks += full_2x2_at(state, index,
                                                   candidate->label);
        if (x < candidate->bbox_min_x) candidate->bbox_min_x = x;
        if (x > candidate->bbox_max_x) candidate->bbox_max_x = x;
        if (y < candidate->bbox_min_y) candidate->bbox_min_y = y;
        if (y > candidate->bbox_max_y) candidate->bbox_max_y = y;
        if (state->input.elevation[index] < state->input.elevation[candidate->sink] ||
            (state->input.elevation[index] == state->input.elevation[candidate->sink] &&
             river_routing_tie_key(state, index, RIVER_TIE_LAKE_SINK) <
                 river_routing_tie_key(state, candidate->sink,
                                       RIVER_TIE_LAKE_SINK))) candidate->sink = index;
    }
    candidate->cardinal_edges = degree_total / 2;
    candidate->interior_perimeter_edges = candidate->interior_cells * 4 -
                                          candidate->interior_cardinal_edges * 2;
    candidate->bbox_area =
        (candidate->bbox_max_x - candidate->bbox_min_x + 1) *
        (candidate->bbox_max_y - candidate->bbox_min_y + 1);
    candidate->hole_count = 1 - candidate->area +
                            candidate->cardinal_edges -
                            candidate->solid_2x2_blocks;
    if (candidate->hole_count < 0) candidate->hole_count = 0;
    candidate->mean_precipitation = (int)(precipitation_total / candidate->area);
    candidate->largest_deep_core = largest_deep_core(state, candidate);
    candidate->max_interior_distance = max_distance_to_interior(state, candidate);
}
