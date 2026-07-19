#include "world/river_state.h"

#include "core/worldgen_attempt.h"
#include "core/worldgen_fault_injection.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static RiverGenerationState state_storage;
static RiverNetworkView latest_view;

typedef enum {
    RIVER_WORKSPACE_FILLED_ELEVATION = 1,
    RIVER_WORKSPACE_HEAP,
    RIVER_WORKSPACE_MAIN_STEM,
    RIVER_WORKSPACE_DOMINANT_PARENT,
    RIVER_WORKSPACE_CHECK_FLOW,
    RIVER_WORKSPACE_PUBLISHED_RUNOFF,
    RIVER_WORKSPACE_PUBLISHED_UPSTREAM,
    RIVER_WORKSPACE_PUBLISHED_WIDTH,
    RIVER_WORKSPACE_CELL_FLAGS,
    RIVER_WORKSPACE_VISITED,
    RIVER_WORKSPACE_MAX_UPSTREAM_ORDER,
    RIVER_WORKSPACE_MAX_UPSTREAM_COUNT,
    RIVER_WORKSPACE_MOISTURE_INFLUENCE,
    RIVER_WORKSPACE_TEMPERATURE_INFLUENCE
} RiverWorkspaceField;

static uint64_t fixed_array_bytes(int count) {
    return (uint64_t)count *
        (sizeof(*state_storage.filled_elevation) + sizeof(*state_storage.heap) +
         sizeof(*state_storage.main_stem) + sizeof(*state_storage.dominant_parent) +
         sizeof(*state_storage.check_flow) + sizeof(*state_storage.published_runoff) +
         sizeof(*state_storage.published_upstream_count) +
         sizeof(*state_storage.published_width) + sizeof(*state_storage.cell_flags) +
         sizeof(*state_storage.visited) + sizeof(*state_storage.max_upstream_order) +
         sizeof(*state_storage.max_upstream_count) +
         sizeof(*state_storage.moisture_influence) +
         sizeof(*state_storage.temperature_influence));
}

static void refresh_allocated_bytes(RiverGenerationState *state) {
    if (!state) return;
    state->allocated_bytes = fixed_array_bytes(state->capacity) +
        (uint64_t)state->segment_capacity * sizeof(*state->segments) +
        (uint64_t)state->distributary_capacity * sizeof(*state->distributaries);
    state->diagnostics.transient_bytes = state->allocated_bytes;
}

static void free_arrays(RiverGenerationState *state) {
    free(state->filled_elevation);
    free(state->heap);
    free(state->main_stem);
    free(state->dominant_parent);
    free(state->check_flow);
    free(state->published_runoff);
    free(state->published_upstream_count);
    free(state->published_width);
    free(state->cell_flags);
    free(state->visited);
    free(state->max_upstream_order);
    free(state->max_upstream_count);
    free(state->moisture_influence);
    free(state->temperature_influence);
    free(state->segments);
    free(state->distributaries);
    memset(state, 0, sizeof(*state));
}

static int allocate_workspace_field(RiverGenerationState *state, void **field,
                                    int count, size_t item_size,
                                    RiverWorkspaceField field_id) {
    uint64_t bytes = (uint64_t)(size_t)count * item_size;
    if (worldgen_fault_injection_should_fail(
            WORLDGEN_FAULT_RIVER_WORKSPACE_ALLOCATION)) {
        state->diagnostics.workspace_allocation_failure_field = (int)field_id;
        state->diagnostics.workspace_allocation_errors++;
        worldgen_attempt_record_failure(
            WORLDGEN_FAILURE_RIVER_WORKSPACE_ALLOCATION);
        return 0;
    }
    *field = calloc((size_t)count, item_size);
    if (!*field) {
        state->diagnostics.workspace_allocation_failure_field = (int)field_id;
        state->diagnostics.workspace_allocation_errors++;
        worldgen_attempt_record_failure(
            WORLDGEN_FAILURE_RIVER_WORKSPACE_ALLOCATION);
        return 0;
    }
    state->allocated_bytes += bytes;
    state->diagnostics.workspace_bytes_allocated = state->allocated_bytes;
    state->diagnostics.transient_bytes = state->allocated_bytes;
    return 1;
}

static int allocate_arrays(RiverGenerationState *state, int count) {
    RiverGenerationDiagnostics failure;
    state->diagnostics.workspace_bytes_required = fixed_array_bytes(count);
#define ALLOCATE_WORKSPACE(name, id) \
    allocate_workspace_field(state, (void **)&state->name, count, \
                             sizeof(*state->name), id)
    if (!ALLOCATE_WORKSPACE(filled_elevation, RIVER_WORKSPACE_FILLED_ELEVATION) ||
        !ALLOCATE_WORKSPACE(heap, RIVER_WORKSPACE_HEAP) ||
        !ALLOCATE_WORKSPACE(main_stem, RIVER_WORKSPACE_MAIN_STEM) ||
        !ALLOCATE_WORKSPACE(dominant_parent, RIVER_WORKSPACE_DOMINANT_PARENT) ||
        !ALLOCATE_WORKSPACE(check_flow, RIVER_WORKSPACE_CHECK_FLOW) ||
        !ALLOCATE_WORKSPACE(published_runoff, RIVER_WORKSPACE_PUBLISHED_RUNOFF) ||
        !ALLOCATE_WORKSPACE(published_upstream_count, RIVER_WORKSPACE_PUBLISHED_UPSTREAM) ||
        !ALLOCATE_WORKSPACE(published_width, RIVER_WORKSPACE_PUBLISHED_WIDTH) ||
        !ALLOCATE_WORKSPACE(cell_flags, RIVER_WORKSPACE_CELL_FLAGS) ||
        !ALLOCATE_WORKSPACE(visited, RIVER_WORKSPACE_VISITED) ||
        !ALLOCATE_WORKSPACE(max_upstream_order, RIVER_WORKSPACE_MAX_UPSTREAM_ORDER) ||
        !ALLOCATE_WORKSPACE(max_upstream_count, RIVER_WORKSPACE_MAX_UPSTREAM_COUNT) ||
        !ALLOCATE_WORKSPACE(moisture_influence, RIVER_WORKSPACE_MOISTURE_INFLUENCE) ||
        !ALLOCATE_WORKSPACE(temperature_influence,
                            RIVER_WORKSPACE_TEMPERATURE_INFLUENCE)) {
        failure = state->diagnostics;
        free_arrays(state);
        state->diagnostics = failure;
        return 0;
    }
#undef ALLOCATE_WORKSPACE
    state->capacity = count;
    refresh_allocated_bytes(state);
    return 1;
}

static int semantic_edge_limit(const RiverGenerationState *state) {
    if (!state || state->input.tile_count <= 0 ||
        state->input.tile_count > INT_MAX / RIVER_DELTA_BRANCH_MAX) return 0;
    return state->input.tile_count * RIVER_DELTA_BRANCH_MAX;
}

int river_state_resize_segments(RiverGenerationState *state, int required) {
    RiverNetworkSegment *resized;
    if (!state || required < state->segment_count ||
        required > semantic_edge_limit(state)) return 0;
    if (required == state->segment_capacity) return 1;
    if (required == 0) {
        free(state->segments);
        state->segments = NULL;
    } else {
        if (worldgen_fault_injection_should_fail(
                WORLDGEN_FAULT_RIVER_SEGMENT_ALLOCATION)) {
            worldgen_attempt_record_failure(
                WORLDGEN_FAILURE_RIVER_SEGMENT_ALLOCATION);
            return 0;
        }
        resized = realloc(state->segments, (size_t)required * sizeof(*resized));
        if (!resized) {
            worldgen_attempt_record_failure(
                WORLDGEN_FAILURE_RIVER_SEGMENT_ALLOCATION);
            return 0;
        }
        state->segments = resized;
    }
    state->segment_capacity = required;
    refresh_allocated_bytes(state);
    return 1;
}

int river_state_resize_distributaries(RiverGenerationState *state, int required) {
    RiverDistributary *resized;
    int limit;
    int capacity;
    if (!state || required < state->distributary_count ||
        required > semantic_edge_limit(state)) return 0;
    if (required <= state->distributary_capacity) return 1;
    if (required == 0) {
        free(state->distributaries);
        state->distributaries = NULL;
    } else {
        limit = semantic_edge_limit(state);
        capacity = state->distributary_capacity > 0 ?
            state->distributary_capacity : 16;
        while (capacity < required && capacity <= limit / 2) capacity *= 2;
        if (capacity < required) capacity = required;
        if (worldgen_fault_injection_should_fail(
                WORLDGEN_FAULT_RIVER_DISTRIBUTARY_ALLOCATION)) {
            worldgen_attempt_record_failure(
                WORLDGEN_FAILURE_RIVER_DISTRIBUTARY_ALLOCATION);
            return 0;
        }
        resized = realloc(state->distributaries,
                          (size_t)capacity * sizeof(*resized));
        if (!resized) {
            worldgen_attempt_record_failure(
                WORLDGEN_FAILURE_RIVER_DISTRIBUTARY_ALLOCATION);
            return 0;
        }
        state->distributaries = resized;
        required = capacity;
    }
    state->distributary_capacity = required;
    refresh_allocated_bytes(state);
    return 1;
}

static int valid_input(const RiverGenerationInput *input) {
    if (!input || input->width <= 0 || input->height <= 0) return 0;
    if (input->tile_count != input->width * input->height) return 0;
    if (input->generation_token == 0) return 0;
    return input->land_mask && input->elevation && input->precipitation &&
           input->receiver && input->basin && input->topological_order &&
           input->runoff && input->flow && input->upstream_count &&
           input->order && input->width_field && input->soil_fertility;
}

void river_state_clear_outputs(RiverGenerationState *state) {
    int count;

    if (!state) return;
    free(state->segments);
    free(state->distributaries);
    state->segments = NULL;
    state->distributaries = NULL;
    state->segment_capacity = 0;
    state->distributary_capacity = 0;
    count = state->input.tile_count;
    memset(state->input.receiver, 0xff, (size_t)count * sizeof(*state->input.receiver));
    memset(state->input.basin, 0xff, (size_t)count * sizeof(*state->input.basin));
    memset(state->input.topological_order, 0xff,
           (size_t)count * sizeof(*state->input.topological_order));
    memset(state->input.runoff, 0, (size_t)count * sizeof(*state->input.runoff));
    memset(state->input.flow, 0, (size_t)count * sizeof(*state->input.flow));
    memset(state->input.upstream_count, 0,
           (size_t)count * sizeof(*state->input.upstream_count));
    memset(state->input.order, 0, (size_t)count * sizeof(*state->input.order));
    memset(state->input.width_field, 0,
           (size_t)count * sizeof(*state->input.width_field));
    memset(state->input.soil_fertility, 0,
           (size_t)count * sizeof(*state->input.soil_fertility));
    memset(state->filled_elevation, 0, (size_t)count * sizeof(*state->filled_elevation));
    memset(state->main_stem, 0xff, (size_t)count * sizeof(*state->main_stem));
    memset(state->dominant_parent, 0xff, (size_t)count * sizeof(*state->dominant_parent));
    memset(state->check_flow, 0, (size_t)count * sizeof(*state->check_flow));
    memset(state->published_runoff, 0,
           (size_t)count * sizeof(*state->published_runoff));
    memset(state->published_upstream_count, 0,
           (size_t)count * sizeof(*state->published_upstream_count));
    memset(state->published_width, 0,
           (size_t)count * sizeof(*state->published_width));
    memset(state->cell_flags, 0, (size_t)count * sizeof(*state->cell_flags));
    memset(state->visited, 0, (size_t)count * sizeof(*state->visited));
    memset(state->max_upstream_order, 0,
           (size_t)count * sizeof(*state->max_upstream_order));
    memset(state->max_upstream_count, 0,
           (size_t)count * sizeof(*state->max_upstream_count));
    memset(state->moisture_influence, 0,
           (size_t)count * sizeof(*state->moisture_influence));
    memset(state->temperature_influence, 0,
           (size_t)count * sizeof(*state->temperature_influence));
    state->land_count = 0;
    state->topological_count = 0;
    state->heap_count = 0;
    state->segment_count = 0;
    state->distributary_count = 0;
    memset(&state->diagnostics, 0, sizeof(state->diagnostics));
    state->diagnostics.workspace_bytes_required = fixed_array_bytes(state->capacity);
    state->diagnostics.workspace_bytes_allocated = fixed_array_bytes(state->capacity);
    refresh_allocated_bytes(state);
}

RiverGenerationState *river_state_prepare(const RiverGenerationInput *input) {
    RiverGenerationState *state = &state_storage;

    if (!valid_input(input)) return NULL;
    if (state->capacity < input->tile_count) {
        free_arrays(state);
        if (!allocate_arrays(state, input->tile_count)) return NULL;
    }
    state->input = *input;
    river_state_clear_outputs(state);
    return state;
}

RiverGenerationState *river_state_current(void) {
    return state_storage.capacity > 0 ? &state_storage : NULL;
}

void river_state_publish_view(RiverGenerationState *state) {
    int count;

    if (!state) return;
    count = state->input.tile_count;
    memcpy(state->filled_elevation, state->input.receiver,
           (size_t)count * sizeof(*state->filled_elevation));
    memcpy(state->heap, state->input.basin, (size_t)count * sizeof(*state->heap));
    memcpy(state->dominant_parent, state->input.topological_order,
           (size_t)count * sizeof(*state->dominant_parent));
    memcpy(state->published_runoff, state->input.runoff,
           (size_t)count * sizeof(*state->published_runoff));
    memcpy(state->check_flow, state->input.flow,
           (size_t)count * sizeof(*state->check_flow));
    memcpy(state->published_upstream_count, state->input.upstream_count,
           (size_t)count * sizeof(*state->published_upstream_count));
    memcpy(state->max_upstream_order, state->input.order,
           (size_t)count * sizeof(*state->max_upstream_order));
    memcpy(state->published_width, state->input.width_field,
           (size_t)count * sizeof(*state->published_width));
    latest_view.width = state->input.width;
    latest_view.height = state->input.height;
    latest_view.tile_count = state->input.tile_count;
    latest_view.generation_token = state->input.generation_token;
    latest_view.receiver = state->filled_elevation;
    latest_view.basin = state->heap;
    latest_view.topological_order = state->dominant_parent;
    latest_view.runoff = state->published_runoff;
    latest_view.flow = state->check_flow;
    latest_view.upstream_count = state->published_upstream_count;
    latest_view.order = state->max_upstream_order;
    latest_view.width_field = state->published_width;
    latest_view.cell_flags = state->cell_flags;
    latest_view.main_stem = state->main_stem;
    latest_view.segments = state->segments;
    latest_view.segment_count = state->segment_count;
    latest_view.distributaries = state->distributaries;
    latest_view.distributary_count = state->distributary_count;
    latest_view.diagnostics = state->diagnostics;
}

void river_state_refresh_view_diagnostics(RiverGenerationState *state) {
    if (state != &state_storage ||
        latest_view.generation_token != state->input.generation_token) return;
    latest_view.diagnostics = state->diagnostics;
}

const RiverNetworkView *river_state_latest_view(void) {
    return latest_view.tile_count > 0 ? &latest_view : NULL;
}

int river_state_view_matches(uint64_t generation_token) {
    return generation_token != 0 && latest_view.tile_count > 0 &&
           latest_view.generation_token == generation_token;
}

void river_state_last_diagnostics(RiverGenerationDiagnostics *out) {
    if (out) *out = state_storage.diagnostics;
}

void river_state_release(void) {
    free_arrays(&state_storage);
    memset(&latest_view, 0, sizeof(latest_view));
}
