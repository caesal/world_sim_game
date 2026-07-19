#include "world/river_lakes.h"
#include "world/river_lake_qualification.h"
#include "world/river_routing.h"

#include <stdint.h>
#include <string.h>

enum {
    CLOSED_CANDIDATE_CAP = 6,
    CLOSED_MIN_DEPTH = 4,
    CLOSED_MAX_PRECIPITATION = 64,
    SALT_MAX_PRECIPITATION = 27,
    COAST_EXCLUSION_RADIUS = 3
};

typedef struct {
    int label;
    int area;
    int sink;
    int mean_precipitation;
    int score;
    uint64_t tie_key;
} ClosedCandidate;

static const int CARDINAL_DX[4] = {1, 0, -1, 0};
static const int CARDINAL_DY[4] = {0, 1, 0, -1};

static int in_bounds(const RiverGenerationState *state, int x, int y) {
    return x >= 0 && x < state->input.width && y >= 0 && y < state->input.height;
}

static int depression_depth(const RiverGenerationState *state, int index) {
    return state->filled_elevation[index] - state->input.elevation[index];
}

static uint32_t mix32(uint32_t value) {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ (value >> 16);
}

static int water_within(const RiverGenerationState *state, int index) {
    int x = index % state->input.width;
    int y = index / state->input.width;
    int dx;
    int dy;

    for (dy = -COAST_EXCLUSION_RADIUS; dy <= COAST_EXCLUSION_RADIUS; dy++) {
        for (dx = -COAST_EXCLUSION_RADIUS; dx <= COAST_EXCLUSION_RADIUS; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            if (!in_bounds(state, nx, ny)) continue;
            if (!state->input.land_mask[ny * state->input.width + nx]) return 1;
        }
    }
    return 0;
}

static int candidate_before(const ClosedCandidate *left, const ClosedCandidate *right) {
    return left->score > right->score ||
           (left->score == right->score && left->tie_key < right->tie_key);
}

static void retain_candidate(ClosedCandidate best[CLOSED_CANDIDATE_CAP], int *count,
                             const RiverLakeQualification *component,
                             const RiverGenerationState *state) {
    ClosedCandidate candidate;
    int slot;

    candidate.label = component->label;
    candidate.area = component->area;
    candidate.sink = component->sink;
    candidate.mean_precipitation = component->mean_precipitation;
    candidate.score = component->max_depth * 100 +
                      (CLOSED_MAX_PRECIPITATION + 1 -
                       component->mean_precipitation) * 3 +
                      (int)(mix32(state->input.seed ^
                                 (uint32_t)component->label * 0x9e3779b9u) % 97u);
    candidate.tie_key = river_routing_tie_key(
        state, component->sink, RIVER_TIE_CLOSED_CANDIDATE);
    if (*count < CLOSED_CANDIDATE_CAP) {
        slot = (*count)++;
    } else {
        if (!candidate_before(&candidate, &best[*count - 1])) return;
        slot = *count - 1;
    }
    while (slot > 0 && candidate_before(&candidate, &best[slot - 1])) {
        best[slot] = best[slot - 1];
        slot--;
    }
    best[slot] = candidate;
}

static int component_near_water(const RiverGenerationState *state, int area) {
    int i;
    for (i = 0; i < area; i++) {
        if (water_within(state, state->heap[i])) return 1;
    }
    return 0;
}

static int route_component_to_root(RiverGenerationState *state, int label,
                                   int area, int root,
                                   int external_receiver,
                                   int clear_component_distances) {
    int head = 0;
    int tail = 0;
    int i;

    if (clear_component_distances) {
        for (i = 0; i < area; i++) state->main_stem[state->heap[i]] = -1;
    }
    state->main_stem[root] = 0;
    state->heap[tail++] = root;
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
            if (state->dominant_parent[neighbor] != label ||
                !state->visited[neighbor] ||
                state->main_stem[neighbor] >= 0) continue;
            state->main_stem[neighbor] = state->main_stem[current] + 1;
            state->heap[tail++] = neighbor;
        }
    }
    if (tail != area) return 0;
    for (i = 0; i < tail; i++) {
        int current = state->heap[i];
        int x = current % state->input.width;
        int y = current / state->input.width;
        int best = -1;
        uint64_t best_key = UINT64_MAX;
        int direction;
        if (current == root) {
            state->input.receiver[current] = external_receiver;
            continue;
        }
        for (direction = 0; direction < 4; direction++) {
            int nx = x + CARDINAL_DX[direction];
            int ny = y + CARDINAL_DY[direction];
            int neighbor;
            uint64_t key;
            if (!in_bounds(state, nx, ny)) continue;
            neighbor = ny * state->input.width + nx;
            if (state->dominant_parent[neighbor] != label ||
                !state->visited[neighbor] ||
                state->main_stem[neighbor] != state->main_stem[current] - 1) continue;
            key = river_routing_pair_key(state, current, neighbor,
                                         RIVER_TIE_LAKE_ROUTE);
            if (best < 0 || key < best_key ||
                (key == best_key && neighbor < best)) {
                best = neighbor;
                best_key = key;
            }
        }
        if (best < 0) return 0;
        state->input.receiver[current] = best;
    }
    return 1;
}

static int route_component_to_outlet(RiverGenerationState *state,
                                     const RiverLakeQualification *component) {
    uint32_t best_rank = UINT32_MAX;
    int outlet = -1;
    int external_receiver;
    int i;

    for (i = 0; i < component->area; i++) {
        int index = state->heap[i];
        int receiver = state->input.receiver[index];
        uint32_t rank = state->published_runoff[index];
        if (receiver < 0 || receiver >= state->input.tile_count ||
            (state->dominant_parent[receiver] == component->label &&
             state->visited[receiver])) continue;
        if (rank != 0 && (outlet < 0 || rank < best_rank)) {
            outlet = index;
            best_rank = rank;
        }
    }
    if (outlet < 0) return 0;
    external_receiver = state->input.receiver[outlet];
    if (external_receiver < 0 || external_receiver >= state->input.tile_count ||
        (state->input.land_mask[external_receiver] &&
        state->dominant_parent[external_receiver] == component->label &&
        state->visited[external_receiver])) return 0;
    return route_component_to_root(state, component->label, component->area,
                                   outlet, external_receiver, 1);
}

static int rebuild_topological_order(RiverGenerationState *state) {
    int head = 0;
    int tail = 0;
    int count = state->input.tile_count;
    int i;

    memset(state->main_stem, 0xff, (size_t)count * sizeof(*state->main_stem));
    memset(state->dominant_parent, 0xff,
           (size_t)count * sizeof(*state->dominant_parent));
    memset(state->visited, 0, (size_t)count * sizeof(*state->visited));
    for (i = count - 1; i >= 0; i--) {
        int receiver;
        if (!state->input.land_mask[i]) continue;
        receiver = state->input.receiver[i];
        if (receiver < 0 || receiver >= count || !state->input.land_mask[receiver]) continue;
        state->dominant_parent[i] = state->main_stem[receiver];
        state->main_stem[receiver] = i;
    }
    for (i = 0; i < count; i++) {
        int receiver;
        if (!state->input.land_mask[i]) continue;
        receiver = state->input.receiver[i];
        if (receiver < 0 || receiver >= count || !state->input.land_mask[receiver]) {
            state->heap[tail++] = i;
        }
    }
    state->topological_count = 0;
    while (head < tail) {
        int current = state->heap[head++];
        int child;
        if (state->visited[current]) continue;
        state->visited[current] = 1;
        state->input.topological_order[state->topological_count++] = current;
        child = state->main_stem[current];
        while (child >= 0) {
            state->heap[tail++] = child;
            child = state->dominant_parent[child];
        }
    }
    state->heap_count = 0;
    return state->topological_count == state->land_count;
}

static void record_rejection(RiverGenerationDiagnostics *diagnostics,
                             uint32_t reasons) {
    if (reasons & RIVER_LAKE_REJECT_AREA) diagnostics->lake_reject_area++;
    if (reasons & RIVER_LAKE_REJECT_DEPTH) diagnostics->lake_reject_depth++;
    if (reasons & RIVER_LAKE_REJECT_DEEP_CELLS) {
        diagnostics->lake_reject_deep_cells++;
    }
    if (reasons & RIVER_LAKE_REJECT_CATCHMENT) {
        diagnostics->lake_reject_catchment++;
    }
    if (reasons & RIVER_LAKE_REJECT_SUPPORT) diagnostics->lake_reject_support++;
    if (reasons & RIVER_LAKE_REJECT_SHAPE) diagnostics->lake_reject_shape++;
}

int river_lakes_resolve(RiverGenerationState *state) {
    ClosedCandidate candidates[CLOSED_CANDIDATE_CAP];
    int closed_candidate_count = 0;
    int lake_components = 0;
    int qualified_cells = 0;
    int closed_limit;
    int i;

    if (!state) return 0;
    memset(state->dominant_parent, 0xff,
           (size_t)state->input.tile_count * sizeof(*state->dominant_parent));
    memset(state->main_stem, 0xff,
           (size_t)state->input.tile_count * sizeof(*state->main_stem));
    memset(state->visited, 0,
           (size_t)state->input.tile_count * sizeof(*state->visited));
    river_lake_qualification_prepare_support(state);
    memset(state->published_runoff, 0,
           (size_t)state->input.tile_count * sizeof(*state->published_runoff));
    for (i = 0; i < state->topological_count; i++) {
        int index = state->input.topological_order[i];
        state->published_runoff[index] = (uint32_t)i + 1u;
    }
    state->diagnostics.depression_cells = 0;
    for (i = 0; i < state->topological_count; i++) {
        int index = state->input.topological_order[i];
        if (depression_depth(state, index) <= 0) continue;
        state->diagnostics.depression_cells++;
        state->cell_flags[index] |= RIVER_CELL_DEPRESSION;
    }
    for (i = 0; i < state->topological_count; i++) {
        int index = state->input.topological_order[i];
        RiverLakeQualification component;
        int j;

        if (depression_depth(state, index) < RIVER_LAKE_MIN_CELL_DEPTH) continue;
        if (state->dominant_parent[index] >= 0) continue;
        if (!river_lake_qualification_collect(state, index, &component)) continue;
        state->diagnostics.lake_candidate_components++;
        state->diagnostics.lake_pruned_cells += component.pruned_cells;
        if (!river_lake_qualification_accepts(&component)) {
            state->diagnostics.lake_rejected_components++;
            record_rejection(&state->diagnostics, component.reject_reasons);
            continue;
        }
        lake_components++;
        qualified_cells += component.area;
        state->diagnostics.lake_qualified_components++;
        for (j = 0; j < component.area; j++) {
            state->cell_flags[state->heap[j]] |= RIVER_CELL_LAKE;
        }
        if (component.max_depth >= CLOSED_MIN_DEPTH &&
            component.mean_precipitation <= CLOSED_MAX_PRECIPITATION &&
            !component_near_water(state, component.area)) {
            retain_candidate(candidates, &closed_candidate_count, &component, state);
        }
        if (!route_component_to_outlet(state, &component)) return 0;
    }
    state->diagnostics.lake_rejected_cells =
        state->diagnostics.depression_cells - qualified_cells;
    memset(state->main_stem, 0xff,
           (size_t)state->input.tile_count * sizeof(*state->main_stem));
    closed_limit = state->land_count / 150000;
    if (closed_limit < 1) closed_limit = 1;
    if (closed_limit > CLOSED_CANDIDATE_CAP) closed_limit = CLOSED_CANDIDATE_CAP;
    if (lake_components / 5 > 0 && closed_limit > lake_components / 5) {
        closed_limit = lake_components / 5;
    } else if (lake_components == 0) {
        closed_limit = 0;
    }
    if (closed_limit > closed_candidate_count) closed_limit = closed_candidate_count;
    for (i = 0; i < closed_limit; i++) {
        int sink = candidates[i].sink;
        if (!route_component_to_root(state, candidates[i].label,
                                     candidates[i].area, sink, -1, 0)) return 0;
        state->cell_flags[sink] |= RIVER_CELL_LAKE | RIVER_CELL_CLOSED_BASIN;
        if (candidates[i].mean_precipitation <= SALT_MAX_PRECIPITATION) {
            state->cell_flags[sink] |= RIVER_CELL_SALT_LAKE;
        }
    }
    return rebuild_topological_order(state);
}
