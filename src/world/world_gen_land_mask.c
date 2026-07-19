#include "world_gen_land_mask.h"
#include "core/worldgen_attempt.h"
#include "world/world_gen_context.h"
#include "world/world_gen_land_mask_refine.h"
#include <limits.h>
#include <string.h>

enum {
    MASK_ADJUST_GROW = 1,
    MASK_ADJUST_SHRINK = 2,
    LAND_RATIO_SCALE = 10000,
    OCEAN_RATIO_SPAN = 72
};
static int clamp_int(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

int world_gen_land_mask_target_tiles(int ocean_amount, int tile_count) {
    int ocean;
    int land_units;
    if (tile_count <= 0) return 0;
    ocean = clamp_int(ocean_amount, 0, 100);
    land_units = LAND_RATIO_SCALE - ocean * OCEAN_RATIO_SPAN;
    return clamp_int((int)(((int64_t)tile_count * land_units +
                            LAND_RATIO_SCALE / 2) / LAND_RATIO_SCALE),
                     1, tile_count);
}

static int cardinal_state_count(const WorldGenContext *context, int index,
                                int state) {
    static const int dx[4] = {1, -1, 0, 0};
    static const int dy[4] = {0, 0, 1, -1};
    int x = index % context->width;
    int y = index / context->width;
    int count = 0;
    int direction;
    for (direction = 0; direction < 4; direction++) {
        int nx = x + dx[direction];
        int ny = y + dy[direction];
        if (world_gen_context_in_bounds(context, nx, ny) &&
            (context->land_mask[world_gen_context_index(context, nx, ny)] != 0) == state) {
            count++;
        }
    }
    return count;
}

static int diagonal_state_count(const WorldGenContext *context, int index,
                                int state) {
    static const int dx[4] = {-1, 1, -1, 1};
    static const int dy[4] = {-1, -1, 1, 1};
    int x = index % context->width;
    int y = index / context->width;
    int count = 0;
    int direction;
    for (direction = 0; direction < 4; direction++) {
        int nx = x + dx[direction];
        int ny = y + dy[direction];
        if (world_gen_context_in_bounds(context, nx, ny) &&
            (context->land_mask[world_gen_context_index(context, nx, ny)] != 0) == state) {
            count++;
        }
    }
    return count;
}

static int local_ring_components(const WorldGenContext *context, int index,
                                 int state) {
    int x = index % context->width;
    int y = index / context->width;
    uint8_t present[9] = {0};
    uint8_t visited[9] = {0};
    int queue[8];
    int components = 0;
    int cell;
    for (cell = 0; cell < 9; cell++) {
        int dx = cell % 3 - 1;
        int dy = cell / 3 - 1;
        int nx = x + dx;
        int ny = y + dy;
        if (cell == 4 || !world_gen_context_in_bounds(context, nx, ny)) continue;
        present[cell] = (uint8_t)((context->land_mask[
            world_gen_context_index(context, nx, ny)] != 0) == state);
    }
    for (cell = 0; cell < 9; cell++) {
        int head = 0;
        int tail = 0;
        if (!present[cell] || visited[cell]) continue;
        if (!(cell == 1 || cell == 3 || cell == 5 || cell == 7)) continue;
        components++;
        visited[cell] = 1;
        queue[tail++] = cell;
        while (head < tail) {
            static const int dx[4] = {1, -1, 0, 0};
            static const int dy[4] = {0, 0, 1, -1};
            int current = queue[head++];
            int cx = current % 3;
            int cy = current / 3;
            int direction;
            for (direction = 0; direction < 4; direction++) {
                int nx = cx + dx[direction];
                int ny = cy + dy[direction];
                int next;
                if (nx < 0 || nx >= 3 || ny < 0 || ny >= 3) continue;
                next = ny * 3 + nx;
                if (next == 4 || !present[next] || visited[next]) continue;
                visited[next] = 1;
                queue[tail++] = next;
            }
        }
    }
    return components;
}

static int topology_safe(const WorldGenContext *context, int index, int mode) {
    int preserved_state = mode == MASK_ADJUST_GROW ? 0 : 1;
    return local_ring_components(context, index, preserved_state) <= 1;
}

static int subheight_score(const WorldGenContext *context, int index) {
    int x = index % context->width;
    int y = index / context->width;
    int weighted = 0;
    int weight = 0;
    int dy;
    int dx;
    for (dy = -1; dy <= 1; dy++) {
        for (dx = -1; dx <= 1; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            int item_weight;
            if ((dx == 0 && dy == 0) ||
                !world_gen_context_in_bounds(context, nx, ny)) continue;
            item_weight = (dx == 0 || dy == 0) ? 3 : 2;
            weighted += context->elevation[
                world_gen_context_index(context, nx, ny)] * item_weight;
            weight += item_weight;
        }
    }
    if (weight == 0) return 0;
    return clamp_int((weighted * 65535 + weight * 50) / (weight * 100), 0, 65535);
}

static int candidate_better(const WorldGenContext *context, int left, int right,
                            int mode) {
    int left_safe = topology_safe(context, left, mode);
    int right_safe = topology_safe(context, right, mode);
    int state = mode == MASK_ADJUST_GROW ? 1 : 0;
    int left_frontier = cardinal_state_count(context, left, state);
    int right_frontier = cardinal_state_count(context, right, state);
    int left_diagonal = diagonal_state_count(context, left, state);
    int right_diagonal = diagonal_state_count(context, right, state);
    if (left_safe != right_safe) return left_safe > right_safe;
    if ((left_frontier > 0) != (right_frontier > 0)) {
        return left_frontier > 0;
    }
    if (left_frontier != right_frontier) return left_frontier > right_frontier;
    if (left_diagonal != right_diagonal) return left_diagonal > right_diagonal;
    if (context->scratch_b[left] != context->scratch_b[right]) {
        return mode == MASK_ADJUST_GROW
            ? context->scratch_b[left] > context->scratch_b[right]
            : context->scratch_b[left] < context->scratch_b[right];
    }
    /* Exact local plateaus use a stable final order only after topology,
       frontier compactness, diagonal support, and continuous subheight.
       Do not reintroduce a periodic coordinate or hashed restoration key. */
    return left < right;
}

static void heap_swap(int *heap, int *positions, int left, int right) {
    int temporary = heap[left];
    heap[left] = heap[right];
    heap[right] = temporary;
    positions[heap[left]] = left;
    positions[heap[right]] = right;
}

static void heap_sift_up(WorldGenContext *context, int size, int position, int mode) {
    int *heap = context->topological_order;
    int *positions = context->scratch_a;
    (void)size;
    while (position > 0) {
        int parent = (position - 1) / 2;
        if (!candidate_better(context, heap[position], heap[parent], mode)) break;
        heap_swap(heap, positions, position, parent);
        position = parent;
    }
}

static void heap_sift_down(WorldGenContext *context, int size, int position, int mode) {
    int *heap = context->topological_order;
    int *positions = context->scratch_a;
    for (;;) {
        int left = position * 2 + 1;
        int right = left + 1;
        int best = position;
        if (left < size && candidate_better(context, heap[left], heap[best], mode)) best = left;
        if (right < size && candidate_better(context, heap[right], heap[best], mode)) best = right;
        if (best == position) break;
        heap_swap(heap, positions, position, best);
        position = best;
    }
}

static int heap_pop(WorldGenContext *context, int *size, int mode) {
    int *heap = context->topological_order;
    int *positions = context->scratch_a;
    int result = heap[0];
    (*size)--;
    positions[result] = -1;
    if (*size > 0) {
        heap[0] = heap[*size];
        positions[heap[0]] = 0;
        heap_sift_down(context, *size, 0, mode);
    }
    return result;
}

static void heap_repair(WorldGenContext *context, int size, int index, int mode) {
    int position = context->scratch_a[index];
    if (position < 0 || position >= size) return;
    heap_sift_up(context, size, position, mode);
    position = context->scratch_a[index];
    heap_sift_down(context, size, position, mode);
}

static void repair_adjacent_candidates(WorldGenContext *context, int size,
                                       int index, int mode) {
    int x = index % context->width;
    int y = index / context->width;
    int dx;
    int dy;
    for (dy = -1; dy <= 1; dy++) {
        for (dx = -1; dx <= 1; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            if ((dx == 0 && dy == 0) ||
                !world_gen_context_in_bounds(context, nx, ny)) continue;
            heap_repair(context, size,
                        world_gen_context_index(context, nx, ny), mode);
        }
    }
}

static int adjust_threshold(WorldGenContext *context, int threshold,
                            int target, int above, int equal) {
    WorldGenLandMaskDiagnostics *diagnostics = &context->land_mask_diagnostics;
    int grow_count = target - above;
    int shrink_count = above + equal - target;
    int mode = grow_count <= shrink_count ? MASK_ADJUST_GROW : MASK_ADJUST_SHRINK;
    int required = mode == MASK_ADJUST_GROW ? grow_count : shrink_count;
    int heap_size = 0;
    int index;
    for (index = 0; index < context->tile_count; index++) {
        context->land_mask[index] = (uint8_t)(mode == MASK_ADJUST_GROW
            ? context->elevation[index] > threshold
            : context->elevation[index] >= threshold);
        context->scratch_a[index] = -1;
        context->scratch_b[index] = subheight_score(context, index);
        if (context->elevation[index] == threshold) {
            context->topological_order[heap_size] = index;
            context->scratch_a[index] = heap_size++;
        }
    }
    diagnostics->initial_land_tiles = mode == MASK_ADJUST_GROW ? above : above + equal;
    for (index = heap_size / 2; index-- > 0;) {
        heap_sift_down(context, heap_size, index, mode);
    }
    while (required > 0) {
        int selected;
        int frontier;
        if (heap_size <= 0 ||
            !topology_safe(context, context->topological_order[0], mode)) {
            diagnostics->failure = mode == MASK_ADJUST_GROW
                ? WORLD_GEN_LAND_MASK_GROWTH_STALLED
                : WORLD_GEN_LAND_MASK_SHRINK_STALLED;
            return 0;
        }
        selected = heap_pop(context, &heap_size, mode);
        frontier = cardinal_state_count(context, selected,
            mode == MASK_ADJUST_GROW ? 1 : 0);
        if (frontier == 0) diagnostics->forced_frontier_seeds++;
        else diagnostics->frontier_resolutions++;
        context->land_mask[selected] = (uint8_t)(mode == MASK_ADJUST_GROW);
        if (mode == MASK_ADJUST_GROW) diagnostics->frontier_added++;
        else diagnostics->frontier_removed++;
        required--;
        repair_adjacent_candidates(context, heap_size, selected, mode);
    }
    return 1;
}

static void normalize_elevation(WorldGenContext *context, int threshold) {
    int index;
    context->sea_level = 50;
    for (index = 0; index < context->tile_count; index++) {
        int raw = clamp_int(context->elevation[index], 0, 100);
        int value;
        context->coastal_lowland_hint[index] = (uint8_t)(
            context->land_mask[index] && raw == threshold);
        if (context->land_mask[index]) {
            int range = 100 - threshold;
            value = raw <= threshold ? 51
                : 51 + ((raw - threshold) * 49 + range / 2) / (range > 0 ? range : 1);
            value = clamp_int(value, 51, 100);
        } else {
            value = raw >= threshold ? 49
                : (raw * 49 + threshold / 2) / (threshold > 0 ? threshold : 1);
            value = clamp_int(value, 0, 49);
        }
        context->base_elevation[index] = (int16_t)value;
        context->elevation[index] = (int16_t)value;
    }
}

static void mark_refinement_candidates(WorldGenContext *context,
                                       int threshold) {
    int index;
    for (index = 0; index < context->tile_count; index++) {
        context->scratch_a[index] = context->elevation[index] == threshold ?
            (context->land_mask[index] ? 2 : 1) : 0;
    }
}

void world_gen_land_mask_refresh_ocean_distance(WorldGenContext *context) {
    int head = 0;
    int tail = 0;
    int index;
    if (!context) return;
    for (index = 0; index < context->tile_count; index++) {
        if (!context->land_mask[index]) {
            context->ocean_distance[index] = 0;
            context->topological_order[tail++] = index;
        } else {
            context->ocean_distance[index] = INT16_MAX;
        }
    }
    if (tail == 0) {
        for (index = 0; index < context->tile_count; index++) {
            context->ocean_distance[index] = 28;
        }
        return;
    }
    while (head < tail) {
        static const int dx[4] = {1, -1, 0, 0};
        static const int dy[4] = {0, 0, 1, -1};
        int current = context->topological_order[head++];
        int x = current % context->width;
        int y = current / context->width;
        int direction;
        for (direction = 0; direction < 4; direction++) {
            int nx = x + dx[direction];
            int ny = y + dy[direction];
            int next;
            if (!world_gen_context_in_bounds(context, nx, ny)) continue;
            next = world_gen_context_index(context, nx, ny);
            if (context->ocean_distance[next] <= context->ocean_distance[current] + 1) continue;
            context->ocean_distance[next] = (int16_t)(context->ocean_distance[current] + 1);
            context->topological_order[tail++] = next;
        }
    }
}

int world_gen_land_mask_build(WorldGenContext *context) {
    WorldGenLandMaskDiagnostics *diagnostics;
    int histogram[101] = {0};
    int target;
    int threshold = 100;
    int above = 0;
    int equal;
    int index;
    if (!context) return 0;
    diagnostics = &context->land_mask_diagnostics;
    memset(diagnostics, 0, sizeof(*diagnostics));
    if (!context->land_mask || !context->coastal_lowland_hint ||
        !context->elevation || !context->scratch_a || !context->scratch_b ||
        !context->topological_order || context->tile_count <= 0) {
        diagnostics->failure = WORLD_GEN_LAND_MASK_INVALID_CONTEXT;
        worldgen_attempt_record_failure(WORLDGEN_FAILURE_LAND_MASK_INVALID_TARGET);
        return 0;
    }
    target = world_gen_land_mask_target_tiles(
        context->config.ocean, context->tile_count);
    diagnostics->target_land_tiles = target;
    worldgen_attempt_note_target(context->width, context->height, target, context->tile_count - target);
    for (index = 0; index < context->tile_count; index++) {
        histogram[clamp_int(context->elevation[index], 0, 100)]++;
    }
    while (threshold > 0 && above + histogram[threshold] < target) {
        above += histogram[threshold--];
    }
    equal = histogram[threshold];
    diagnostics->threshold_elevation = threshold;
    diagnostics->threshold_candidates = equal;
    if (target < above || target > above + equal) {
        diagnostics->failure = WORLD_GEN_LAND_MASK_INVALID_TARGET;
        worldgen_attempt_record_failure(WORLDGEN_FAILURE_LAND_MASK_INVALID_TARGET);
        return 0;
    }
    if (!adjust_threshold(context, threshold, target, above, equal)) {
        mark_refinement_candidates(context, threshold);
        world_gen_land_mask_measure(context);
        worldgen_attempt_record_failure(WORLDGEN_FAILURE_LAND_MASK_FRONTIER_STALLED);
        return 0;
    }
    mark_refinement_candidates(context, threshold);
    if (!world_gen_land_mask_refine_threshold(context)) {
        diagnostics->failure = target - above <= above + equal - target
            ? WORLD_GEN_LAND_MASK_GROWTH_STALLED
            : WORLD_GEN_LAND_MASK_SHRINK_STALLED;
        world_gen_land_mask_measure(context);
        worldgen_attempt_record_failure(WORLDGEN_FAILURE_LAND_MASK_FRONTIER_STALLED);
        return 0;
    }
    diagnostics->threshold_selected = 0;
    for (index = 0; index < context->tile_count; index++) {
        if (context->land_mask[index] && context->elevation[index] == threshold) {
            diagnostics->threshold_selected++;
        }
    }
    normalize_elevation(context, threshold);
    world_gen_land_mask_measure(context);
    if (diagnostics->target_drift != 0) {
        diagnostics->failure = WORLD_GEN_LAND_MASK_TARGET_DRIFT;
        worldgen_attempt_record_failure(WORLDGEN_FAILURE_LAND_MASK_TARGET_DRIFT);
        return 0;
    }
    if (diagnostics->lattice_cells != 0 || diagnostics->comb_cells != 0 ||
        diagnostics->mesh_cells != 0 || diagnostics->tendril_cells != 0 ||
        diagnostics->topology_errors != 0) {
        diagnostics->failure = WORLD_GEN_LAND_MASK_SEMANTIC_ARTIFACT;
        worldgen_attempt_record_failure(WORLDGEN_FAILURE_LAND_MASK_SEMANTIC_ARTIFACT);
        return 0;
    }
    diagnostics->failure = WORLD_GEN_LAND_MASK_OK;
    world_gen_land_mask_refresh_ocean_distance(context);
    return 1;
}

const WorldGenLandMaskDiagnostics *world_gen_land_mask_diagnostics(
    const WorldGenContext *context) {
    return context ? &context->land_mask_diagnostics : NULL;
}
