#include "game/game_worldgen_lake_semantics_probe.h"

#include "core/world_types.h"
#include "world/river_lake_qualification.h"
#include "world/rivers.h"
#include "world/world_gen_classify.h"
#include "world/world_gen_context.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const int CARDINAL_DX[4] = {1, 0, -1, 0};
static const int CARDINAL_DY[4] = {0, 1, 0, -1};

typedef struct {
    int components;
    int cells;
    int min_area;
    int max_area;
    int min_fill_percent;
    int max_perimeter_sq_per_area;
    int outlet_errors;
    int invalid_outlets;
    int shape_errors;
    int hole_errors;
    int branch_errors;
    int max_interior_distance;
    int semantic_errors;
    int diagonal_contacts;
    int inlets;
    int open_components;
    int closed_components;
    int underlying[GEO_COUNT];
} LakeSemanticStats;

static int in_bounds(const WorldGenContext *context, int x, int y) {
    return x >= 0 && x < context->width && y >= 0 && y < context->height;
}

static int is_lake(const RiverNetworkView *view, int index) {
    return (view->cell_flags[index] & RIVER_CELL_LAKE) != 0;
}

static void collect_component(const WorldGenContext *context,
                              const RiverNetworkView *view, int seed, int label,
                              int32_t *labels, int32_t *queue,
                              int32_t *distances, int32_t *distance_queue,
                              LakeSemanticStats *stats) {
    int head = 0;
    int tail = 0;
    int min_x = context->width;
    int min_y = context->height;
    int max_x = -1;
    int max_y = -1;
    int cardinal_edges = 0;
    int solid_2x2_blocks = 0;
    int interior_cells = 0;
    int closed_sinks = 0;
    int outlets = 0;
    int distance_head = 0;
    int distance_tail = 0;
    int max_distance = 0;
    int area;
    int i;

    labels[seed] = label;
    queue[tail++] = seed;
    while (head < tail) {
        int current = queue[head++];
        int x = current % context->width;
        int y = current / context->width;
        int direction;
        if (x < min_x) min_x = x;
        if (x > max_x) max_x = x;
        if (y < min_y) min_y = y;
        if (y > max_y) max_y = y;
        for (direction = 0; direction < 4; direction++) {
            int nx = x + CARDINAL_DX[direction];
            int ny = y + CARDINAL_DY[direction];
            int neighbor;
            if (!in_bounds(context, nx, ny)) continue;
            neighbor = ny * context->width + nx;
            if (!is_lake(view, neighbor)) continue;
            cardinal_edges++;
            if (labels[neighbor] == 0) {
                labels[neighbor] = label;
                queue[tail++] = neighbor;
            }
        }
    }
    cardinal_edges /= 2;
    area = tail;
    for (i = 0; i < tail; i++) {
        int current = queue[i];
        int receiver = view->receiver[current];
        int x = current % context->width;
        int y = current / context->width;
        int degree = 0;
        int dy;
        int dx;
        if (receiver < 0) {
            if (view->cell_flags[current] & RIVER_CELL_CLOSED_BASIN) closed_sinks++;
            else stats->invalid_outlets++;
        } else if (receiver >= view->tile_count) {
            stats->invalid_outlets++;
        } else if (labels[receiver] != label) {
            outlets++;
        }
        for (dx = 0; dx < 4; dx++) {
            int nx = x + CARDINAL_DX[dx];
            int ny = y + CARDINAL_DY[dx];
            if (in_bounds(context, nx, ny) &&
                labels[ny * context->width + nx] == label) degree++;
        }
        interior_cells += degree == 4;
        distances[current] = -1;
        if (degree == 4) {
            distances[current] = 0;
            distance_queue[distance_tail++] = current;
        }
        if (x + 1 < context->width && y + 1 < context->height &&
            labels[current + 1] == label &&
            labels[current + context->width] == label &&
            labels[current + context->width + 1] == label) solid_2x2_blocks++;
        for (dy = -1; dy <= 1; dy++) {
            for (dx = -1; dx <= 1; dx++) {
                int neighbor;
                if ((dx == 0 && dy == 0) || !in_bounds(context, x + dx, y + dy)) continue;
                neighbor = (y + dy) * context->width + x + dx;
                if (labels[neighbor] == label || is_lake(view, neighbor)) continue;
                if (view->receiver[neighbor] == current) stats->inlets++;
            }
        }
    }
    {
        int bbox_area = (max_x - min_x + 1) * (max_y - min_y + 1);
        int fill_percent = bbox_area > 0 ? area * 100 / bbox_area : 0;
        int perimeter = area * 4 - cardinal_edges * 2;
        int perimeter_ratio = area > 0 ?
            (int)(((int64_t)perimeter * perimeter) / area) : 0;
        int holes = 1 - area + cardinal_edges - solid_2x2_blocks;
        while (distance_head < distance_tail) {
            int current = distance_queue[distance_head++];
            int x = current % context->width;
            int y = current / context->width;
            int direction;
            if (distances[current] > max_distance) max_distance = distances[current];
            for (direction = 0; direction < 4; direction++) {
                int nx = x + CARDINAL_DX[direction];
                int ny = y + CARDINAL_DY[direction];
                int neighbor;
                if (!in_bounds(context, nx, ny)) continue;
                neighbor = ny * context->width + nx;
                if (labels[neighbor] != label || distances[neighbor] >= 0) continue;
                distances[neighbor] = distances[current] + 1;
                distance_queue[distance_tail++] = neighbor;
            }
        }
        if (distance_tail != area && area > RIVER_LAKE_MAX_NO_INTERIOR_AREA) {
            max_distance = area;
        }
        if (max_distance > stats->max_interior_distance) {
            stats->max_interior_distance = max_distance;
        }
        if (holes > RIVER_LAKE_MAX_HOLES) stats->hole_errors++;
        if (area > RIVER_LAKE_MAX_NO_INTERIOR_AREA &&
            max_distance > RIVER_LAKE_MAX_INTERIOR_DISTANCE) stats->branch_errors++;
        {
        int shape_ok = area >= RIVER_LAKE_MIN_AREA &&
                       fill_percent >= RIVER_LAKE_MIN_BBOX_FILL_PERCENT &&
                       (int64_t)perimeter * perimeter <=
                           (int64_t)RIVER_LAKE_MAX_PERIMETER_SQUARED_PER_AREA * area &&
                       holes <= RIVER_LAKE_MAX_HOLES &&
                       (area <= RIVER_LAKE_MAX_NO_INTERIOR_AREA ||
                        (interior_cells * RIVER_LAKE_MIN_INTERIOR_RATIO_DENOMINATOR >= area &&
                         max_distance <= RIVER_LAKE_MAX_INTERIOR_DISTANCE));
        int outlet_ok = (closed_sinks == 1 && outlets == 0) ||
                        (closed_sinks == 0 && outlets == 1);
        if (stats->min_area == 0 || area < stats->min_area) stats->min_area = area;
        if (area > stats->max_area) stats->max_area = area;
        if (stats->min_fill_percent == 0 || fill_percent < stats->min_fill_percent) {
            stats->min_fill_percent = fill_percent;
        }
        if (perimeter_ratio > stats->max_perimeter_sq_per_area) {
            stats->max_perimeter_sq_per_area = perimeter_ratio;
        }
        if (!shape_ok) stats->shape_errors++;
        if (!outlet_ok) stats->outlet_errors++;
        if (closed_sinks == 1 && outlets == 0) stats->closed_components++;
        if (closed_sinks == 0 && outlets == 1) stats->open_components++;
        }
    }
    stats->components++;
    stats->cells += area;
}

static void collect_semantics(const WorldGenContext *context,
                              const RiverNetworkView *view, int32_t *labels,
                              LakeSemanticStats *stats) {
    int i;
    for (i = 0; i < context->tile_count; i++) {
        int flagged = is_lake(view, i);
        int semantic = context->geography[i] == GEO_LAKE;
        if (flagged != semantic || flagged !=
            ((context->river_flags[i] & WORLD_GEN_RIVER_LAKE) != 0)) {
            stats->semantic_errors++;
        }
        if (flagged) {
            int x = i % context->width;
            int y = i / context->width;
            Geography underlying = world_gen_classify_underlying_land(
                context, i, x, y, (Climate)context->climate[i]);
            if (underlying >= 0 && underlying < GEO_COUNT) stats->underlying[underlying]++;
        }
    }
    for (i = 0; i < context->tile_count; i++) {
        int x;
        int y;
        int dx;
        int dy;
        if (!is_lake(view, i)) continue;
        x = i % context->width;
        y = i / context->width;
        for (dy = -1; dy <= 1; dy += 2) {
            for (dx = -1; dx <= 1; dx += 2) {
                int neighbor;
                if (!in_bounds(context, x + dx, y + dy)) continue;
                neighbor = (y + dy) * context->width + x + dx;
                if (is_lake(view, neighbor) && labels[neighbor] != labels[i]) {
                    stats->diagonal_contacts++;
                }
            }
        }
    }
    stats->diagonal_contacts /= 2;
}

int game_worldgen_lake_semantics_probe_check(FILE *file, const char *label,
                                             const WorldGenContext *context) {
    const RiverNetworkView *view = river_network_latest_view();
    LakeSemanticStats stats;
    int32_t *labels;
    int32_t *queue;
    int32_t *distances;
    int32_t *distance_queue;
    int next_label = 1;
    int diagnostics_ok;
    int ok;
    int i;
    if (!file || !label || !context || !view ||
        !river_network_view_matches_context(context)) return 0;
    labels = (int32_t *)calloc((size_t)context->tile_count, sizeof(*labels));
    queue = (int32_t *)malloc((size_t)context->tile_count * sizeof(*queue));
    distances = (int32_t *)malloc((size_t)context->tile_count * sizeof(*distances));
    distance_queue = (int32_t *)malloc((size_t)context->tile_count *
                                      sizeof(*distance_queue));
    if (!labels || !queue || !distances || !distance_queue) {
        free(labels);
        free(queue);
        free(distances);
        free(distance_queue);
        return 0;
    }
    memset(&stats, 0, sizeof(stats));
    for (i = 0; i < context->tile_count; i++) {
        if (is_lake(view, i) && labels[i] == 0) {
            collect_component(context, view, i, next_label++, labels, queue,
                              distances, distance_queue, &stats);
        }
    }
    collect_semantics(context, view, labels, &stats);
    diagnostics_ok = stats.components == view->diagnostics.lake_qualified_components &&
                      stats.cells == view->diagnostics.lake_cells &&
                      view->diagnostics.lake_final_invalid_components == 0 &&
                      view->diagnostics.lake_rejected_cells >= 0;
    ok = stats.shape_errors == 0 && stats.outlet_errors == 0 &&
         stats.invalid_outlets == 0 &&
         stats.semantic_errors == 0 && diagnostics_ok;
    fprintf(file, "case=lake_semantics label=%s components=%d cells=%d area_min=%d "
                  "area_max=%d fill_min_pct=%d perimeter_sq_per_area_max=%d "
                  "open=%d closed=%d outlets_bad=%d invalid_outlets=%d "
                  "shape_bad=%d holes_bad=%d branches_bad=%d max_core_distance=%d semantic_bad=%d "
                  "diagonal_contacts=%d inlets=%d candidates=%d rejected_components=%d "
                  "rejected_cells=%d final_invalid=%d diagnostics_ok=%d ok=%d\n",
            label, stats.components, stats.cells, stats.min_area, stats.max_area,
            stats.min_fill_percent, stats.max_perimeter_sq_per_area,
            stats.open_components, stats.closed_components, stats.outlet_errors,
            stats.invalid_outlets, stats.shape_errors, stats.hole_errors,
            stats.branch_errors, stats.max_interior_distance,
            stats.semantic_errors, stats.diagonal_contacts,
            stats.inlets, view->diagnostics.lake_candidate_components,
            view->diagnostics.lake_rejected_components,
            view->diagnostics.lake_rejected_cells,
            view->diagnostics.lake_final_invalid_components, diagnostics_ok, ok);
    fprintf(file, "case=lake_underlying label=%s plain=%d basin=%d wetland=%d hill=%d "
                  "plateau=%d oasis=%d coast=%d delta=%d other=%d ok=%d\n",
            label, stats.underlying[GEO_PLAIN], stats.underlying[GEO_BASIN],
            stats.underlying[GEO_WETLAND], stats.underlying[GEO_HILL],
            stats.underlying[GEO_PLATEAU], stats.underlying[GEO_OASIS],
            stats.underlying[GEO_COAST], stats.underlying[GEO_DELTA],
            stats.cells - stats.underlying[GEO_PLAIN] - stats.underlying[GEO_BASIN] -
            stats.underlying[GEO_WETLAND] - stats.underlying[GEO_HILL] -
            stats.underlying[GEO_PLATEAU] - stats.underlying[GEO_OASIS] -
            stats.underlying[GEO_COAST] - stats.underlying[GEO_DELTA], ok);
    free(labels);
    free(queue);
    free(distances);
    free(distance_queue);
    return ok;
}
