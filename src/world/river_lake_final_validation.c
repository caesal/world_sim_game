#include "world/river_lake_final_validation.h"

#include "world/river_lake_qualification.h"
#include "world/river_lake_shape.h"

#include <string.h>

static const int CARDINAL_DX[4] = {1, 0, -1, 0};
static const int CARDINAL_DY[4] = {0, 1, 0, -1};

static int in_bounds(const RiverGenerationState *state, int x, int y) {
    return x >= 0 && x < state->input.width &&
           y >= 0 && y < state->input.height;
}

static int collect_flagged_component(RiverGenerationState *state, int seed) {
    int head = 0;
    int tail = 0;
    state->dominant_parent[seed] = seed;
    state->visited[seed] = 1;
    state->heap[tail++] = seed;
    while (head < tail) {
        int current = state->heap[head++];
        int x = current % state->input.width;
        int y = current / state->input.width;
        int direction;
        for (direction = 0; direction < 4; direction++) {
            int nx = x + CARDINAL_DX[direction];
            int ny = y + CARDINAL_DY[direction];
            int neighbor;
            if (!in_bounds(state, nx, ny)) continue;
            neighbor = ny * state->input.width + nx;
            if (!(state->cell_flags[neighbor] & RIVER_CELL_LAKE) ||
                state->dominant_parent[neighbor] >= 0) continue;
            state->dominant_parent[neighbor] = seed;
            state->visited[neighbor] = 1;
            state->heap[tail++] = neighbor;
        }
    }
    return tail;
}

static int component_flag_errors(const RiverGenerationState *state,
                                 const RiverLakeQualification *candidate,
                                 int *closed_sink, int *closed_count) {
    int salt_count = 0;
    int errors = 0;
    int i;
    *closed_sink = -1;
    *closed_count = 0;
    for (i = 0; i < candidate->area; i++) {
        int index = state->heap[i];
        uint16_t flags = state->cell_flags[index];
        if (!state->input.land_mask[index] ||
            !(flags & RIVER_CELL_DEPRESSION) ||
            state->filled_elevation[index] != candidate->spill_level) errors++;
        if (flags & RIVER_CELL_CLOSED_BASIN) {
            *closed_sink = index;
            (*closed_count)++;
        }
        if (flags & RIVER_CELL_SALT_LAKE) {
            salt_count++;
            if (!(flags & RIVER_CELL_CLOSED_BASIN)) errors++;
        }
    }
    if (salt_count > 1) errors++;
    if (*closed_count == 0 && salt_count != 0) errors++;
    return errors;
}

static int component_topology_ok(
    const RiverGenerationState *state,
    const RiverLakeQualification *candidate,
    int closed_sink, int closed_count) {
    if (candidate->invalid_outlet_edges != 0 ||
        candidate->outlet_edges != 1) return 0;
    if (closed_count == 0) {
        return candidate->outlet_receiver_count == 1;
    }
    return closed_count == 1 && closed_sink == candidate->sink &&
           state->input.receiver[closed_sink] == -1 &&
           candidate->outlet_receiver_count == 0;
}

static int orphan_flag_errors(const RiverGenerationState *state) {
    int errors = 0;
    int i;
    for (i = 0; i < state->input.tile_count; i++) {
        uint16_t flags = state->cell_flags[i];
        if ((flags & (RIVER_CELL_CLOSED_BASIN | RIVER_CELL_SALT_LAKE)) &&
            !(flags & RIVER_CELL_LAKE)) errors++;
    }
    return errors;
}

int river_lake_final_validation_run(
    RiverGenerationState *state, RiverLakeFinalValidationReport *report) {
    RiverLakeFinalValidationReport local;
    int orphan_errors;
    int i;
    if (!state) return 0;
    memset(&local, 0, sizeof(local));
    memset(state->dominant_parent, 0xff,
           (size_t)state->input.tile_count * sizeof(*state->dominant_parent));
    memset(state->main_stem, 0xff,
           (size_t)state->input.tile_count * sizeof(*state->main_stem));
    memset(state->visited, 0,
           (size_t)state->input.tile_count * sizeof(*state->visited));
    river_lake_qualification_prepare_support(state);
    for (i = 0; i < state->input.tile_count; i++) {
        RiverLakeQualification candidate;
        int index = i;
        int closed_sink;
        int closed_count;
        int flag_errors;
        int model_ok;
        int topology_ok;
        if (!(state->cell_flags[index] & RIVER_CELL_LAKE) ||
            state->dominant_parent[index] >= 0) continue;
        memset(&candidate, 0, sizeof(candidate));
        candidate.label = index;
        candidate.initial_area = collect_flagged_component(state, index);
        candidate.area = candidate.initial_area;
        candidate.spill_level = state->filled_elevation[index];
        candidate.surface_level = candidate.spill_level - 1;
        river_lake_shape_collect(state, &candidate);
        flag_errors = component_flag_errors(
            state, &candidate, &closed_sink, &closed_count);
        river_lake_qualification_collect_final_support(
            state, &candidate, closed_count == 1 ? closed_sink : -1);
        model_ok = river_lake_qualification_model_reject_reasons(&candidate) ==
                   RIVER_LAKE_REJECT_NONE;
        topology_ok = component_topology_ok(
            state, &candidate, closed_sink, closed_count);
        local.components++;
        local.closed_components += closed_count == 1;
        local.open_components += closed_count == 0;
        local.flag_errors += flag_errors != 0;
        local.model_errors += !model_ok;
        local.topology_errors += !topology_ok;
        local.invalid_components +=
            flag_errors != 0 || !model_ok || !topology_ok;
    }
    orphan_errors = orphan_flag_errors(state);
    local.flag_errors += orphan_errors;
    local.invalid_components += orphan_errors != 0;
    state->diagnostics.lake_final_invalid_components =
        local.invalid_components;
    state->heap_count = 0;
    if (report) *report = local;
    return local.invalid_components == 0 && local.flag_errors == 0;
}
