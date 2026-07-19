#include "world/world_gen_land_mask.h"

#include "world/world_gen_context.h"

#include <string.h>

static int state_count(const WorldGenContext *context, int index,
                       int state, int diagonal) {
    static const int cardinal_dx[4] = {1, -1, 0, 0};
    static const int cardinal_dy[4] = {0, 0, 1, -1};
    static const int diagonal_dx[4] = {-1, 1, -1, 1};
    static const int diagonal_dy[4] = {-1, -1, 1, 1};
    const int *dx = diagonal ? diagonal_dx : cardinal_dx;
    const int *dy = diagonal ? diagonal_dy : cardinal_dy;
    int x = index % context->width;
    int y = index / context->width;
    int count = 0;
    int direction;
    for (direction = 0; direction < 4; direction++) {
        int nx = x + dx[direction];
        int ny = y + dy[direction];
        if (world_gen_context_in_bounds(context, nx, ny) &&
            (context->land_mask[
                world_gen_context_index(context, nx, ny)] != 0) == state) {
            count++;
        }
    }
    return count;
}

static int threshold_cell(const WorldGenContext *context, int index) {
    return context->scratch_a[index] == 1 || context->scratch_a[index] == 2;
}

static int raw_checker(const WorldGenContext *context, int index) {
    int a = context->land_mask[index] != 0;
    int b = context->land_mask[index + 1] != 0;
    int c = context->land_mask[index + context->width] != 0;
    int d = context->land_mask[index + context->width + 1] != 0;
    return a == d && b == c && a != b;
}

static int threshold_checker(const WorldGenContext *context, int index) {
    int touches_threshold = threshold_cell(context, index) ||
        threshold_cell(context, index + 1) ||
        threshold_cell(context, index + context->width) ||
        threshold_cell(context, index + context->width + 1);
    return touches_threshold && raw_checker(context, index);
}

static int raw_comb(const WorldGenContext *context, int index) {
    int state;
    int cardinal;
    int diagonal;
    int x;
    int y;
    int horizontal;
    int vertical;
    state = context->land_mask[index] != 0;
    cardinal = state_count(context, index, state, 0);
    diagonal = state_count(context, index, state, 1);
    if (cardinal != 2 || diagonal < 2) return 0;
    x = index % context->width;
    y = index / context->width;
    horizontal = x > 0 && x + 1 < context->width &&
        (context->land_mask[index - 1] != 0) == state &&
        (context->land_mask[index + 1] != 0) == state;
    vertical = y > 0 && y + 1 < context->height &&
        (context->land_mask[index - context->width] != 0) == state &&
        (context->land_mask[index + context->width] != 0) == state;
    return horizontal || vertical;
}

static int clustered_checker(const WorldGenContext *context, int index) {
    int x = index % context->width;
    int y = index / context->width;
    int matches = 0;
    int dx;
    int dy;
    if (!raw_checker(context, index)) return 0;
    for (dy = -2; dy <= 2; dy++) {
        for (dx = -2; dx <= 2; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            if (nx < 0 || ny < 0 ||
                nx + 1 >= context->width || ny + 1 >= context->height) continue;
            if (raw_checker(context, world_gen_context_index(context, nx, ny)))
                matches++;
        }
    }
    return matches >= 4;
}

static int raw_lattice(const WorldGenContext *context, int index) {
    int state = context->land_mask[index] != 0;
    return state_count(context, index, state, 0) == 0 &&
           state_count(context, index, state, 1) >= 2;
}

static int clustered_lattice(const WorldGenContext *context, int index) {
    int x = index % context->width;
    int y = index / context->width;
    int matches = 0;
    int dx;
    int dy;
    if (!raw_lattice(context, index)) return 0;
    for (dy = -2; dy <= 2; dy++) {
        for (dx = -2; dx <= 2; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            if (!world_gen_context_in_bounds(context, nx, ny)) continue;
            if (raw_lattice(context, world_gen_context_index(context, nx, ny)))
                matches++;
        }
    }
    return matches >= 4;
}

static int clustered_comb(const WorldGenContext *context, int index) {
    int x = index % context->width;
    int y = index / context->width;
    int matches = 0;
    int dx;
    int dy;
    if (!raw_comb(context, index)) return 0;
    for (dy = -2; dy <= 2; dy++) {
        for (dx = -2; dx <= 2; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            if (!world_gen_context_in_bounds(context, nx, ny)) continue;
            if (raw_comb(context, world_gen_context_index(context, nx, ny)))
                matches++;
        }
    }
    return matches >= 4;
}

static int count_components(WorldGenContext *context, int state) {
    int components = 0;
    int index;
    memset(context->scratch_b, 0,
           (size_t)context->tile_count * sizeof(*context->scratch_b));
    for (index = 0; index < context->tile_count; index++) {
        int head = 0;
        int tail = 0;
        if (context->scratch_b[index] ||
            (context->land_mask[index] != 0) != state) continue;
        components++;
        context->scratch_b[index] = 1;
        context->topological_order[tail++] = index;
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
                if (context->scratch_b[next] ||
                    (context->land_mask[next] != 0) != state) continue;
                context->scratch_b[next] = 1;
                context->topological_order[tail++] = next;
            }
        }
    }
    return components;
}

void world_gen_land_mask_measure(WorldGenContext *context) {
    WorldGenLandMaskDiagnostics *diagnostics;
    int index;
    if (!context) return;
    diagnostics = &context->land_mask_diagnostics;
    diagnostics->final_land_tiles = 0;
    diagnostics->coastal_lowland_tiles = 0;
    diagnostics->lattice_cells = 0;
    diagnostics->comb_cells = 0;
    diagnostics->mesh_cells = 0;
    diagnostics->tendril_cells = 0;
    diagnostics->threshold_lattice_cells = 0;
    diagnostics->threshold_comb_cells = 0;
    diagnostics->threshold_mesh_cells = 0;
    diagnostics->threshold_tendril_cells = 0;
    diagnostics->topology_errors = 0;
    diagnostics->mask_hash = UINT64_C(1469598103934665603);
    for (index = 0; index < context->tile_count; index++) {
        int state = context->land_mask[index] != 0;
        int cardinal = state_count(context, index, state, 0);
        int diagonal = state_count(context, index, state, 1);
        int x = index % context->width;
        int y = index / context->width;
        int adjusted = threshold_cell(context, index);
        diagnostics->mask_hash ^= (uint64_t)(state + 1);
        diagnostics->mask_hash *= UINT64_C(1099511628211);
        if (state) diagnostics->final_land_tiles++;
        if (context->coastal_lowland_hint[index])
            diagnostics->coastal_lowland_tiles++;
        if (adjusted && ((state && cardinal + diagonal <= 1) ||
            (!state && x > 0 && y > 0 && x + 1 < context->width &&
             y + 1 < context->height && cardinal + diagonal == 0))) {
            diagnostics->topology_errors++;
        }
        if (x + 1 < context->width && y + 1 < context->height &&
            threshold_checker(context, index)) {
            diagnostics->threshold_mesh_cells++;
            diagnostics->topology_errors++;
        }
        if (x + 1 < context->width && y + 1 < context->height &&
            clustered_checker(context, index)) diagnostics->mesh_cells++;
        if (clustered_lattice(context, index)) diagnostics->lattice_cells++;
        if (clustered_comb(context, index)) diagnostics->comb_cells++;
        if (!adjusted) continue;
        if (cardinal == 0 && diagonal >= 2)
            diagnostics->threshold_lattice_cells++;
        if (state && cardinal <= 1) {
            diagnostics->tendril_cells++;
            diagnostics->threshold_tendril_cells++;
        }
        if (clustered_comb(context, index)) diagnostics->threshold_comb_cells++;
        if (cardinal == 0 && diagonal > 0) diagnostics->topology_errors++;
    }
    diagnostics->land_components = count_components(context, 1);
    diagnostics->water_components = count_components(context, 0);
    diagnostics->target_drift = diagnostics->final_land_tiles -
                                diagnostics->target_land_tiles;
}
