#include "world/world_gen_land_mask_refine.h"

#include "world/world_gen_context.h"

#include <limits.h>
#include <stdlib.h>

enum {
    REFINE_GROW = 1,
    REFINE_SHRINK = 2,
    REFINE_RADIUS = 2,
    REFINE_MAX_SWAPS = 256,
    REFINE_SEPARATION = 6
};

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

static int local_ring_components(const WorldGenContext *context, int index,
                                 int state) {
    int x = index % context->width;
    int y = index / context->width;
    unsigned char present[9] = {0};
    unsigned char visited[9] = {0};
    int queue[8];
    int components = 0;
    int cell;
    for (cell = 0; cell < 9; cell++) {
        int nx = x + cell % 3 - 1;
        int ny = y + cell / 3 - 1;
        if (cell == 4 || !world_gen_context_in_bounds(context, nx, ny)) continue;
        present[cell] = (unsigned char)((context->land_mask[
            world_gen_context_index(context, nx, ny)] != 0) == state);
    }
    for (cell = 0; cell < 9; cell++) {
        int head = 0;
        int tail = 0;
        if (!present[cell] || visited[cell] ||
            !(cell == 1 || cell == 3 || cell == 5 || cell == 7)) continue;
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
    int preserved_state = mode == REFINE_GROW ? 0 : 1;
    return local_ring_components(context, index, preserved_state) <= 1;
}

static int threshold_cell(const WorldGenContext *context, int index) {
    return context->scratch_a[index] == 1 || context->scratch_a[index] == 2;
}

static int exact_threshold_cell(const WorldGenContext *context, int index) {
    return threshold_cell(context, index);
}

static int checker_at(const WorldGenContext *context, int index) {
    int x = index % context->width;
    int y = index / context->width;
    int a, b, c, d;
    if (x + 1 >= context->width || y + 1 >= context->height) return 0;
    a = context->land_mask[index] != 0;
    b = context->land_mask[index + 1] != 0;
    c = context->land_mask[index + context->width] != 0;
    d = context->land_mask[index + context->width + 1] != 0;
    return (threshold_cell(context, index) ||
            threshold_cell(context, index + 1) ||
            threshold_cell(context, index + context->width) ||
            threshold_cell(context, index + context->width + 1)) &&
           a == d && b == c && a != b;
}

static int raw_comb(const WorldGenContext *context, int index) {
    int state;
    int cardinal;
    int diagonal;
    int x;
    int y;
    int horizontal;
    int vertical;
    if (!threshold_cell(context, index)) return 0;
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

static int clustered_comb(const WorldGenContext *context, int index) {
    int x = index % context->width;
    int y = index / context->width;
    int dx;
    int dy;
    if (!raw_comb(context, index)) return 0;
    for (dy = -1; dy <= 1; dy++) {
        for (dx = -1; dx <= 1; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            if ((dx == 0 && dy == 0) ||
                !world_gen_context_in_bounds(context, nx, ny)) continue;
            if (raw_comb(context, world_gen_context_index(context, nx, ny)))
                return 1;
        }
    }
    return 0;
}

static int cell_defects(const WorldGenContext *context, int index) {
    int result = checker_at(context, index);
    int state;
    int cardinal;
    int diagonal;
    if (!threshold_cell(context, index)) return result;
    state = context->land_mask[index] != 0;
    cardinal = state_count(context, index, state, 0);
    diagonal = state_count(context, index, state, 1);
    result += cardinal == 0 && diagonal > 0;
    result += state && cardinal <= 1;
    result += clustered_comb(context, index);
    return result;
}

static int local_defects(const WorldGenContext *context, int center) {
    int center_x = center % context->width;
    int center_y = center / context->width;
    int result = 0;
    int dx;
    int dy;
    for (dy = -REFINE_RADIUS; dy <= REFINE_RADIUS; dy++) {
        for (dx = -REFINE_RADIUS; dx <= REFINE_RADIUS; dx++) {
            int x = center_x + dx;
            int y = center_y + dy;
            if (world_gen_context_in_bounds(context, x, y))
                result += cell_defects(
                    context, world_gen_context_index(context, x, y));
        }
    }
    return result;
}

static int total_defects(const WorldGenContext *context) {
    int result = 0;
    int index;
    for (index = 0; index < context->tile_count; index++)
        result += cell_defects(context, index);
    return result;
}

static int neighbor_count8(const WorldGenContext *context, int index,
                           int state) {
    int x = index % context->width;
    int y = index / context->width;
    int count = 0;
    int dx;
    int dy;
    for (dy = -1; dy <= 1; dy++) {
        for (dx = -1; dx <= 1; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            if ((dx == 0 && dy == 0) ||
                !world_gen_context_in_bounds(context, nx, ny)) continue;
            if ((context->land_mask[
                world_gen_context_index(context, nx, ny)] != 0) == state) {
                count++;
            }
        }
    }
    return count;
}

static int find_global_topology_primary(const WorldGenContext *context,
                                        int *new_state_out) {
    int index;
    for (index = 0; index < context->tile_count; index++) {
        int state = context->land_mask[index] != 0;
        int x = index % context->width;
        int y = index / context->width;
        int same;
        if (!threshold_cell(context, index)) continue;
        same = neighbor_count8(context, index, state);
        if (state && same <= 1) {
            *new_state_out = 0;
            return index;
        }
        if (!state && x > 0 && y > 0 && x + 1 < context->width &&
            y + 1 < context->height && same == 0) {
            *new_state_out = 1;
            return index;
        }
    }
    return -1;
}

static int flip_delta(WorldGenContext *context, int index, int new_state) {
    int old_state = context->land_mask[index] != 0;
    int before = local_defects(context, index);
    int after;
    context->land_mask[index] = (unsigned char)new_state;
    after = local_defects(context, index);
    context->land_mask[index] = (unsigned char)old_state;
    return after - before;
}

static int candidate_delta(WorldGenContext *context, int index,
                           int new_state) {
    int old_state;
    int support;
    int mode;
    if (!threshold_cell(context, index)) return INT_MAX;
    old_state = context->land_mask[index] != 0;
    if (old_state == new_state) return INT_MAX;
    mode = new_state ? REFINE_GROW : REFINE_SHRINK;
    if (!topology_safe(context, index, mode)) return INT_MAX;
    support = state_count(context, index, new_state, 0);
    if ((new_state && support < 2) || (!new_state && support < 1))
        return INT_MAX;
    return flip_delta(context, index, new_state);
}

static int find_primary(WorldGenContext *context, int *new_state_out) {
    int best = -1;
    int best_delta = 0;
    int index;
    for (index = 0; index < context->tile_count; index++) {
        int x;
        int y;
        int dx;
        int dy;
        if (cell_defects(context, index) == 0) continue;
        x = index % context->width;
        y = index / context->width;
        for (dy = -REFINE_RADIUS; dy <= REFINE_RADIUS; dy++) {
            for (dx = -REFINE_RADIUS; dx <= REFINE_RADIUS; dx++) {
                int nx = x + dx;
                int ny = y + dy;
                int candidate;
                int new_state;
                int delta;
                if (!world_gen_context_in_bounds(context, nx, ny)) continue;
                candidate = world_gen_context_index(context, nx, ny);
                if (!threshold_cell(context, candidate)) continue;
                new_state = !context->land_mask[candidate];
                delta = candidate_delta(context, candidate, new_state);
                if (delta < best_delta ||
                    (delta == best_delta && delta < 0 && candidate < best)) {
                    best = candidate;
                    best_delta = delta;
                    *new_state_out = new_state;
                }
            }
        }
    }
    return best;
}

static int better_compensation(const WorldGenContext *context,
                               int candidate, int best, int new_state) {
    int candidate_support;
    int best_support;
    if (best < 0) return 1;
    candidate_support = state_count(context, candidate, new_state, 0);
    best_support = state_count(context, best, new_state, 0);
    if (candidate_support != best_support)
        return candidate_support > best_support;
    if (context->scratch_b[candidate] != context->scratch_b[best]) {
        return new_state ? context->scratch_b[candidate] > context->scratch_b[best]
                         : context->scratch_b[candidate] < context->scratch_b[best];
    }
    return candidate < best;
}

static int find_compensation(WorldGenContext *context, int primary,
                             int new_state) {
    int primary_x = primary % context->width;
    int primary_y = primary / context->width;
    int best = -1;
    int index;
    for (index = 0; index < context->tile_count; index++) {
        int x;
        int y;
        int delta;
        if (!exact_threshold_cell(context, index) ||
            (context->land_mask[index] != 0) == new_state) continue;
        x = index % context->width;
        y = index / context->width;
        if (abs(x - primary_x) + abs(y - primary_y) <= REFINE_SEPARATION)
            continue;
        if (!better_compensation(context, index, best, new_state)) continue;
        delta = candidate_delta(context, index, new_state);
        if (delta <= 0) best = index;
    }
    return best;
}

static int repair_global_topology(WorldGenContext *context) {
    int swap;
    for (swap = 0; swap < REFINE_MAX_SWAPS; swap++) {
        int primary_new_state = 0;
        int primary = find_global_topology_primary(
            context, &primary_new_state);
        int compensation;
        if (primary < 0) return 1;
        if (!topology_safe(context, primary,
                           primary_new_state ? REFINE_GROW : REFINE_SHRINK))
            return 0;
        context->land_mask[primary] = (unsigned char)primary_new_state;
        compensation = find_compensation(context, primary,
                                         !primary_new_state);
        if (compensation < 0) {
            context->land_mask[primary] = (unsigned char)!primary_new_state;
            return 0;
        }
        context->land_mask[compensation] = (unsigned char)!primary_new_state;
    }
    return find_global_topology_primary(context, &swap) < 0;
}

int world_gen_land_mask_refine_threshold(WorldGenContext *context) {
    int before;
    int swap;
    if (!context || !context->land_mask || !context->scratch_a ||
        !context->scratch_b) return 0;
    if (!repair_global_topology(context)) return 0;
    before = total_defects(context);
    for (swap = 0; before > 0 && swap < REFINE_MAX_SWAPS; swap++) {
        int primary_new_state = 0;
        int primary = find_primary(context, &primary_new_state);
        int compensation;
        int after;
        if (primary < 0) return 0;
        context->land_mask[primary] = (unsigned char)primary_new_state;
        compensation = find_compensation(context, primary,
                                         !primary_new_state);
        if (compensation < 0) {
            context->land_mask[primary] = (unsigned char)!primary_new_state;
            return 0;
        }
        context->land_mask[compensation] = (unsigned char)!primary_new_state;
        after = total_defects(context);
        if (after >= before) {
            context->land_mask[compensation] = (unsigned char)primary_new_state;
            context->land_mask[primary] = (unsigned char)!primary_new_state;
            return 0;
        }
        before = after;
    }
    return before == 0;
}
