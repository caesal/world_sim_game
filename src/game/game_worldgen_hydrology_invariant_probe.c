#include "game_worldgen_hydrology_invariant_probe.h"
#include "core/world_types.h"
#include "world/river_routing.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
static const int PROBE_DX[8] = {1, 1, 0, -1, -1, -1, 0, 1};
static const int PROBE_DY[8] = {0, 1, 1, 1, 0, -1, -1, -1};

typedef struct {
    int topology_errors, flow_errors, order_errors, stem_errors, tributary_errors;
    int lake_errors, segment_errors, diagnostic_errors, path_errors;
    int land, channels, sources, confluences, lake_cells, lake_components;
    int closed, salt, mouths, deltas, ordinary_segments, legacy_checked;
} InvariantCounts;
typedef struct {
    int *position;
    int *queue;
    int *dominant;
    int *expected_stem;
    int *ordinary_segment;
    uint64_t *incoming;
    uint8_t *derived_order;
    uint8_t *max_parent_order;
    uint8_t *max_parent_count;
    uint8_t *lake_seen;
    uint16_t *parent_count;
    uint16_t *path_mask;
} InvariantWorkspace;
static int allocate_workspace(InvariantWorkspace *work, int count) {
    memset(work, 0, sizeof(*work));
    work->position = (int *)malloc((size_t)count * sizeof(*work->position));
    work->queue = (int *)malloc((size_t)count * sizeof(*work->queue));
    work->dominant = (int *)malloc((size_t)count * sizeof(*work->dominant));
    work->expected_stem = (int *)malloc((size_t)count * sizeof(*work->expected_stem));
    work->ordinary_segment = (int *)malloc((size_t)count * sizeof(*work->ordinary_segment));
    work->incoming = (uint64_t *)calloc((size_t)count, sizeof(*work->incoming));
    work->derived_order = (uint8_t *)calloc((size_t)count, sizeof(*work->derived_order));
    work->max_parent_order = (uint8_t *)calloc((size_t)count,
                                               sizeof(*work->max_parent_order));
    work->max_parent_count = (uint8_t *)calloc((size_t)count,
                                               sizeof(*work->max_parent_count));
    work->lake_seen = (uint8_t *)calloc((size_t)count, sizeof(*work->lake_seen));
    work->parent_count = (uint16_t *)calloc((size_t)count, sizeof(*work->parent_count));
    work->path_mask = (uint16_t *)calloc((size_t)count, sizeof(*work->path_mask));
    return work->position && work->queue && work->dominant && work->expected_stem &&
           work->ordinary_segment && work->incoming && work->derived_order &&
           work->max_parent_order && work->max_parent_count && work->lake_seen &&
           work->parent_count && work->path_mask;
}
static void free_workspace(InvariantWorkspace *work) {
    free(work->position);
    free(work->queue);
    free(work->dominant);
    free(work->expected_stem);
    free(work->ordinary_segment);
    free(work->incoming);
    free(work->derived_order);
    free(work->max_parent_order);
    free(work->max_parent_count);
    free(work->lake_seen);
    free(work->parent_count);
    free(work->path_mask);
    memset(work, 0, sizeof(*work));
}
static int valid_inputs(const WorldGenContext *context, const RiverNetworkView *view) {
    if (!context || !view || context->tile_count <= 0 ||
        context->width != view->width || context->height != view->height ||
        context->tile_count != view->tile_count ||
        context->topological_count < 0 || context->topological_count > context->tile_count ||
        context->hydrology_token == 0 || context->hydrology_token != view->generation_token) return 0;
    if (!context->land_mask || !context->elevation || !context->precipitation ||
        !view->receiver || !view->basin || !view->topological_order || !view->runoff || !view->flow ||
        !view->order || !view->width_field || !view->cell_flags || !view->main_stem) return 0;
    if (view->segment_count < 0 ||
        view->segment_count > view->tile_count * RIVER_DELTA_BRANCH_MAX ||
        (view->segment_count > 0 && !view->segments) || view->distributary_count < 0 ||
        view->distributary_count > view->tile_count * RIVER_DELTA_BRANCH_MAX ||
        (view->distributary_count > 0 && !view->distributaries)) return 0;
    return 1;
}
static int land_receiver(const WorldGenContext *context,
                         const RiverNetworkView *view, int index) {
    int receiver = view->receiver[index];
    if (receiver < 0 || receiver >= view->tile_count) return -1;
    return context->land_mask[receiver] ? receiver : -1;
}
static void check_topology_and_flow(const WorldGenContext *context,
                                    const RiverNetworkView *view,
    InvariantWorkspace *work, InvariantCounts *counts) {
    int count = view->tile_count;
    int i;
    memset(work->position, 0xff, (size_t)count * sizeof(*work->position));
    for (i = 0; i < context->topological_count; i++) {
        int index = view->topological_order[i];
        if (index < 0 || index >= count || !context->land_mask[index] ||
            work->position[index] >= 0) {
            counts->topology_errors++;
            continue;
        }
        work->position[index] = i;
    }
    for (i = 0; i < count; i++) {
        int receiver;
        if (!context->land_mask[i]) {
            if (work->position[i] >= 0 || (view->cell_flags[i] & RIVER_CELL_LAND)) {
                counts->topology_errors++;
            }
            continue;
        }
        counts->land++;
        if (work->position[i] < 0 || !(view->cell_flags[i] & RIVER_CELL_LAND)) {
            counts->topology_errors++;
        }
        receiver = view->receiver[i];
        if (receiver < 0) {
            if (!(view->cell_flags[i] &
                  (RIVER_CELL_CLOSED_BASIN | RIVER_CELL_EDGE_OUTLET))) {
                counts->topology_errors++;
            }
            continue;
        }
        if (receiver >= count || receiver == i) {
            counts->topology_errors++;
            continue;
        }
        if (!context->land_mask[receiver]) continue;
        if (work->position[receiver] < 0 || work->position[receiver] >= work->position[i]) {
            counts->topology_errors++;
        }
        work->incoming[receiver] += view->flow[i];
    }
    if (counts->land != context->topological_count) counts->topology_errors++;
    for (i = 0; i < count; i++) {
        uint64_t expected;
        if (!context->land_mask[i]) continue;
        expected = (uint64_t)view->runoff[i] + work->incoming[i];
        if (expected > UINT32_MAX || expected != view->flow[i]) counts->flow_errors++;
    }
}
static void check_orders_and_stems(const WorldGenContext *context,
                                   const RiverNetworkView *view,
    InvariantWorkspace *work, InvariantCounts *counts) {
    int count = view->tile_count;
    int i;
    memset(work->dominant, 0xff, (size_t)count * sizeof(*work->dominant));
    memset(work->expected_stem, 0xff, (size_t)count * sizeof(*work->expected_stem));
    for (i = context->topological_count - 1; i >= 0; i--) {
        int index = view->topological_order[i];
        int receiver;
        uint8_t order;
        if (index < 0 || index >= count || !context->land_mask[index] ||
            !(view->cell_flags[index] & RIVER_CELL_CHANNEL)) continue;
        counts->channels++;
        order = work->max_parent_order[index];
        if (order == 0) order = 1;
        else if (work->max_parent_count[index] >= 2 && order < UINT8_MAX) order++;
        work->derived_order[index] = order;
        if (view->order[index] != order) counts->order_errors++;
        if (work->dominant[index] >= 0) {
            work->expected_stem[index] = work->expected_stem[work->dominant[index]];
        } else {
            work->expected_stem[index] = index + 1;
        }
        if (work->expected_stem[index] <= 0 ||
            view->main_stem[index] != work->expected_stem[index]) counts->stem_errors++;
        if (((work->parent_count[index] == 0) !=
             ((view->cell_flags[index] & RIVER_CELL_SOURCE) != 0))) counts->order_errors++;
        if (((work->parent_count[index] >= 2) !=
             ((view->cell_flags[index] & RIVER_CELL_CONFLUENCE) != 0))) {
            counts->order_errors++;
        }
        receiver = land_receiver(context, view, index);
        if (receiver < 0 || !(view->cell_flags[receiver] & RIVER_CELL_CHANNEL)) continue;
        if (work->parent_count[receiver] < UINT16_MAX) work->parent_count[receiver]++;
        if (order > work->max_parent_order[receiver]) {
            work->max_parent_order[receiver] = order;
            work->max_parent_count[receiver] = 1;
        } else if (order == work->max_parent_order[receiver] &&
                   work->max_parent_count[receiver] < UINT8_MAX) {
            work->max_parent_count[receiver]++;
        }
        if (work->dominant[receiver] < 0 ||
            view->flow[index] > view->flow[work->dominant[receiver]] ||
            (view->flow[index] == view->flow[work->dominant[receiver]] &&
             river_routing_seeded_tie_key(
                 context->phase_seed[WORLD_GEN_PHASE_HYDROLOGY], index,
                 RIVER_TIE_MAIN_STEM) < river_routing_seeded_tie_key(
                 context->phase_seed[WORLD_GEN_PHASE_HYDROLOGY],
                 work->dominant[receiver], RIVER_TIE_MAIN_STEM))) {
            work->dominant[receiver] = index;
        }
    }
}
static void check_closed_lake(const WorldGenContext *context, const RiverNetworkView *view,
                              InvariantWorkspace *work, InvariantCounts *counts, int sink) {
    int basin = view->basin[sink], cells = 0, minimum = sink, i;
    int64_t precipitation = 0;
    memset(work->parent_count, 0, (size_t)view->tile_count * sizeof(*work->parent_count));
    work->parent_count[sink] = 1;
    for (i = 0; i < context->topological_count; i++) {
        int index = view->topological_order[i], receiver = view->receiver[index];
        if (!(view->cell_flags[index] & RIVER_CELL_LAKE) || view->basin[index] != basin) continue;
        if (index != sink && (receiver < 0 || receiver >= view->tile_count ||
            !work->parent_count[receiver])) continue;
        work->parent_count[index] = 1;
        cells++;
        precipitation += context->precipitation[index];
        if (context->elevation[index] < context->elevation[minimum] ||
            (context->elevation[index] == context->elevation[minimum] &&
             river_routing_seeded_tie_key(
                 context->phase_seed[WORLD_GEN_PHASE_HYDROLOGY], index,
                 RIVER_TIE_LAKE_SINK) < river_routing_seeded_tie_key(
                 context->phase_seed[WORLD_GEN_PHASE_HYDROLOGY], minimum,
                 RIVER_TIE_LAKE_SINK))) {
            minimum = index;
        }
    }
    if (view->receiver[sink] >= 0 || cells < 2 || minimum != sink) counts->lake_errors++;
    if ((view->cell_flags[sink] & RIVER_CELL_SALT_LAKE) &&
        (cells <= 0 || precipitation / cells > 27)) counts->lake_errors++;
}
static void check_lake_components(const WorldGenContext *context, const RiverNetworkView *view,
                                  InvariantWorkspace *work, InvariantCounts *counts) {
    int i;
    for (i = 0; i < view->tile_count; i++) {
        int head = 0;
        int tail = 0;
        if (!(view->cell_flags[i] & RIVER_CELL_LAKE) || work->lake_seen[i]) continue;
        work->lake_seen[i] = 1;
        work->queue[tail++] = i;
        while (head < tail) {
            int current = work->queue[head++];
            int x = current % view->width;
            int y = current / view->width;
            int direction;
            if (!context->land_mask[current]) counts->lake_errors++;
            if (view->cell_flags[current] & RIVER_CELL_CLOSED_BASIN) {
                counts->closed++;
                check_closed_lake(context, view, work, counts, current);
            }
            if (view->cell_flags[current] & RIVER_CELL_SALT_LAKE) {
                counts->salt++;
                if (!(view->cell_flags[current] & RIVER_CELL_CLOSED_BASIN)) {
                    counts->lake_errors++;
                }
            }
            for (direction = 0; direction < 8; direction++) {
                int nx = x + PROBE_DX[direction];
                int ny = y + PROBE_DY[direction];
                int neighbor;
                if (nx < 0 || nx >= view->width || ny < 0 || ny >= view->height) continue;
                neighbor = ny * view->width + nx;
                if (work->lake_seen[neighbor] ||
                    !(view->cell_flags[neighbor] & RIVER_CELL_LAKE)) continue;
                work->lake_seen[neighbor] = 1;
                work->queue[tail++] = neighbor;
            }
        }
        counts->lake_components++;
        counts->lake_cells += tail;
        if (tail < 2) counts->lake_errors++;
    }
}
static void check_segments(const WorldGenContext *context, const RiverNetworkView *view,
                           InvariantWorkspace *work, InvariantCounts *counts) {
    int branch_segments = 0;
    int i;
    memset(work->ordinary_segment, 0xff,
           (size_t)view->tile_count * sizeof(*work->ordinary_segment));
    for (i = 0; i < view->segment_count; i++) {
        const RiverNetworkSegment *segment = &view->segments[i];
        if (segment->from < 0 || segment->from >= view->tile_count ||
            segment->to < 0 || segment->to >= view->tile_count) {
            counts->segment_errors++;
            continue;
        }
        if (segment->kind == RIVER_SEGMENT_DISTRIBUTARY) {
            branch_segments++;
            continue;
        }
        if (segment->kind != RIVER_SEGMENT_ORDINARY) {
            counts->segment_errors++;
            continue;
        }
        counts->ordinary_segments++;
        if (work->ordinary_segment[segment->from] >= 0 ||
            !(view->cell_flags[segment->from] & RIVER_CELL_CHANNEL) ||
            (view->cell_flags[segment->from] & RIVER_CELL_DELTA) ||
            view->receiver[segment->from] != segment->to ||
            segment->main_stem != work->expected_stem[segment->from] ||
            segment->flow != view->flow[segment->from] ||
            segment->order != view->order[segment->from] ||
            segment->width != view->width_field[segment->from]) {
            counts->segment_errors++;
        }
        work->ordinary_segment[segment->from] = i;
    }
    for (i = 0; i < view->tile_count; i++) {
        int receiver;
        int segment_index = work->ordinary_segment[i];
        if (!(view->cell_flags[i] & RIVER_CELL_CHANNEL)) {
            if (segment_index >= 0) counts->segment_errors++;
            continue;
        }
        receiver = view->receiver[i];
        if (receiver >= 0 && receiver < view->tile_count &&
            !(view->cell_flags[i] & RIVER_CELL_DELTA) &&
            !((view->cell_flags[i] & RIVER_CELL_LAKE) &&
              (view->cell_flags[receiver] & RIVER_CELL_LAKE))) {
            if (segment_index < 0) counts->segment_errors++;
        } else if (segment_index >= 0) {
            counts->segment_errors++;
        }
        if (receiver >= 0 && receiver < view->tile_count &&
            (view->cell_flags[i] & RIVER_CELL_LAKE) &&
            (view->cell_flags[receiver] & RIVER_CELL_LAKE)) continue;
        if (receiver < 0 || receiver >= view->tile_count ||
            !context->land_mask[receiver] ||
            !(view->cell_flags[receiver] & RIVER_CELL_CONFLUENCE) ||
            work->dominant[receiver] == i) continue;
        if (segment_index < 0 || view->segments[segment_index].to != receiver ||
            work->expected_stem[i] == work->expected_stem[receiver]) {
            counts->tributary_errors++;
        }
        segment_index = work->ordinary_segment[receiver];
        if (segment_index >= 0 &&
            view->segments[segment_index].main_stem == work->expected_stem[i]) {
            counts->tributary_errors++;
        }
    }
    if (branch_segments != view->distributary_count) counts->segment_errors++;
}
static void recount_diagnostics(const WorldGenContext *context,
                                const RiverNetworkView *view, InvariantCounts *counts) {
    int sources = 0;
    int confluences = 0;
    int mouths = 0;
    int deltas = 0;
    int i;
    for (i = 0; i < view->tile_count; i++) {
        uint16_t flags = view->cell_flags[i];
        sources += (flags & RIVER_CELL_SOURCE) != 0;
        confluences += (flags & RIVER_CELL_CONFLUENCE) != 0;
        mouths += (flags & RIVER_CELL_MOUTH) != 0;
        deltas += (flags & RIVER_CELL_DELTA) != 0;
        if ((flags & RIVER_CELL_CLOSED_BASIN) && !(flags & RIVER_CELL_LAKE)) {
            counts->diagnostic_errors++;
        }
        if ((flags & (RIVER_CELL_SOURCE | RIVER_CELL_CONFLUENCE |
                      RIVER_CELL_MOUTH | RIVER_CELL_DELTA)) &&
            !(flags & RIVER_CELL_CHANNEL)) counts->diagnostic_errors++;
        if ((flags & RIVER_CELL_SALT_LAKE) && !(flags & RIVER_CELL_CLOSED_BASIN)) {
            counts->diagnostic_errors++;
        }
    }
    counts->sources = sources;
    counts->confluences = confluences;
    counts->mouths = mouths;
    counts->deltas = deltas;
    counts->diagnostic_errors += counts->land != view->diagnostics.land_cells;
    counts->diagnostic_errors += context->topological_count !=
                                 view->diagnostics.topological_cells;
    counts->diagnostic_errors += counts->channels != view->diagnostics.channel_cells;
    counts->diagnostic_errors += sources != view->diagnostics.sources;
    counts->diagnostic_errors += confluences != view->diagnostics.confluences;
    counts->diagnostic_errors += counts->lake_cells != view->diagnostics.lake_cells;
    counts->diagnostic_errors += counts->closed != view->diagnostics.closed_basins;
    counts->diagnostic_errors += counts->salt != view->diagnostics.salt_lakes;
    counts->diagnostic_errors += mouths != view->diagnostics.mouths;
    counts->diagnostic_errors += deltas != view->diagnostics.deltas;
    counts->diagnostic_errors += counts->ordinary_segments !=
                                 view->diagnostics.ordinary_segments;
    counts->diagnostic_errors += view->distributary_count !=
                                 view->diagnostics.distributaries;
}
static int direction_bit(int width, int from, int to) {
    int dx = to % width - from % width;
    int dy = to / width - from / width;
    int bit;
    if (dx < -1 || dx > 1 || dy < -1 || dy > 1 || (dx == 0 && dy == 0)) return -1;
    bit = (dy + 1) * 3 + dx + 1;
    return 1 << bit;
}
static void check_staged_paths(const WorldGenContext *context,
                               const RiverNetworkView *view,
                               InvariantWorkspace *work, InvariantCounts *counts) {
    const RiverPath *paths = (const RiverPath *)context->staged_river_paths;
    uint16_t *branch_mask = work->parent_count;
    int i;
    if (!paths && context->staged_river_path_count == 0 &&
        context->staged_river_paths_required == 0) return;
    counts->legacy_checked = 1;
    if (!paths || context->staged_river_path_count < 0 ||
        context->staged_river_path_count != context->staged_river_paths_required) {
        counts->path_errors++;
        return;
    }
    memset(branch_mask, 0, (size_t)view->tile_count * sizeof(*branch_mask));
    for (i = 0; i < view->distributary_count; i++) {
        const RiverDistributary *branch = &view->distributaries[i];
        int bit;
        if (branch->from < 0 || branch->from >= view->tile_count ||
            branch->to < 0 || branch->to >= view->tile_count) {
            counts->path_errors++;
            continue;
        }
        bit = direction_bit(view->width, branch->from, branch->to);
        if (bit < 0 || (branch_mask[branch->from] & bit)) counts->path_errors++;
        else branch_mask[branch->from] |= (uint16_t)bit;
    }
    for (i = 0; i < context->staged_river_path_count; i++) {
        const RiverPath *path = &paths[i];
        int point;
        if (!path->active || path->point_count < 2 || path->point_count > MAX_RIVER_POINTS) {
            counts->path_errors++;
            continue;
        }
        for (point = 0; point + 1 < path->point_count; point++) {
            int from;
            int to;
            int bit;
            if (path->points[point].x < 0 || path->points[point].x >= view->width ||
                path->points[point].y < 0 || path->points[point].y >= view->height ||
                path->points[point + 1].x < 0 ||
                path->points[point + 1].x >= view->width ||
                path->points[point + 1].y < 0 ||
                path->points[point + 1].y >= view->height) {
                counts->path_errors++;
                continue;
            }
            from = path->points[point].y * view->width + path->points[point].x;
            to = path->points[point + 1].y * view->width + path->points[point + 1].x;
            bit = direction_bit(view->width, from, to);
            if (bit < 0 || (work->path_mask[from] & bit)) counts->path_errors++;
            else work->path_mask[from] |= (uint16_t)bit;
            if (view->receiver[from] == to) {
                if (!(view->cell_flags[from] & RIVER_CELL_CHANNEL)) counts->path_errors++;
            } else if (bit < 0 || !(branch_mask[from] & bit)) {
                counts->path_errors++;
            }
        }
    }
    for (i = 0; i < view->segment_count; i++) {
        const RiverNetworkSegment *segment = &view->segments[i];
        int bit;
        if (segment->from < 0 || segment->from >= view->tile_count ||
            segment->to < 0 || segment->to >= view->tile_count) continue;
        bit = direction_bit(view->width, segment->from, segment->to);
        if (bit < 0 || !(work->path_mask[segment->from] & bit)) counts->path_errors++;
    }
}
int game_worldgen_hydrology_invariant_probe_check(
    FILE *file, const char *label, const WorldGenContext *context,
    const RiverNetworkView *view) {
    InvariantWorkspace work;
    InvariantCounts counts;
    int errors;
    memset(&counts, 0, sizeof(counts));
    if (!file || !label || !valid_inputs(context, view)) {
        if (file) fprintf(file, "case=hydrology_invariants label=%s input_errors=1 ok=0\n",
                          label ? label : "(null)");
        return 0;
    }
    if (!allocate_workspace(&work, view->tile_count)) {
        free_workspace(&work);
        fprintf(file, "case=hydrology_invariants label=%s allocation_errors=1 ok=0\n", label);
        return 0;
    }
    check_topology_and_flow(context, view, &work, &counts);
    check_orders_and_stems(context, view, &work, &counts);
    check_lake_components(context, view, &work, &counts);
    check_segments(context, view, &work, &counts);
    recount_diagnostics(context, view, &counts);
    check_staged_paths(context, view, &work, &counts);
    errors = counts.topology_errors + counts.flow_errors + counts.order_errors +
             counts.stem_errors + counts.tributary_errors +
             counts.lake_errors + counts.segment_errors + counts.diagnostic_errors +
             counts.path_errors;
    fprintf(file, "case=hydrology_invariants label=%s land=%d channels=%d sources=%d "
                  "confluences=%d lake_components=%d lake_cells=%d closed=%d salt=%d "
                  "mouths=%d deltas=%d ordinary=%d topology_errors=%d flow_errors=%d "
                  "order_errors=%d stem_errors=%d tributary_errors=%d lake_errors=%d "
                  "segment_errors=%d diagnostic_errors=%d legacy_checked=%d "
                  "path_errors=%d ok=%d\n",
            label, counts.land, counts.channels, counts.sources, counts.confluences,
            counts.lake_components, counts.lake_cells, counts.closed, counts.salt,
            counts.mouths, counts.deltas, counts.ordinary_segments,
            counts.topology_errors, counts.flow_errors, counts.order_errors,
            counts.stem_errors, counts.tributary_errors, counts.lake_errors,
            counts.segment_errors, counts.diagnostic_errors, counts.legacy_checked,
            counts.path_errors, errors == 0);
    free_workspace(&work);
    return errors == 0;
}
