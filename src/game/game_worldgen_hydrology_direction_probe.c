#include "game/game_worldgen_hydrology_direction_probe.h"

#include "core/render_snapshot.h"
#include "core/world_types.h"
#include "render/river_geometry.h"
#include "world/river_path_validation.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    ROUTE_DIRECTION_MIN_PER_MILLE = 30,
    ROUTE_DIRECTION_MAX_PER_MILLE = 300,
    FLAT_LOWER_MIN_PER_MILLE = 350,
    FLAT_LOWER_MAX_PER_MILLE = 650,
    ROUTE_MAX_SAME_DIRECTION_RUN = 96,
    ROUTE_MAX_FLAT_DIRECTION_RUN = 64,
    GEOMETRY_MIN_NON_GRID_PER_MILLE = 30,
    GEOMETRY_MAX_SAME_ANGLE_RUN = 32,
    GEOMETRY_MAX_GRID_ANGLE_RUN = 64,
    PROBE_SEED_CAPACITY = 32
};

typedef struct {
    int cases;
    int geometry_cases;
    int seed_count;
    uint32_t seeds[PROBE_SEED_CAPACITY];
    uint64_t receiver_edges;
    uint64_t flat_edges;
    uint64_t lower_edges;
    uint64_t flat_lower_edges;
    uint64_t directions[8];
    uint64_t flat_directions[8];
    int max_same_run;
    int max_flat_run;
    uint64_t render_segments;
    uint64_t axis_segments;
    uint64_t diagonal_segments;
    uint64_t non_grid_segments;
    uint64_t offset_anchors;
    int max_same_angle_run;
    int max_grid_angle_run;
} DirectionProbeCoverage;

static DirectionProbeCoverage coverage;

static int clamp_int(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static void record_seed(uint32_t seed) {
    int i;
    for (i = 0; i < coverage.seed_count; i++) {
        if (coverage.seeds[i] == seed) return;
    }
    if (coverage.seed_count < PROBE_SEED_CAPACITY) {
        coverage.seeds[coverage.seed_count++] = seed;
    }
}

static int direction_between(const RiverNetworkView *view, int from, int to) {
    static const int dx[8] = {1, 1, 0, -1, -1, -1, 0, 1};
    static const int dy[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    int delta_x;
    int delta_y;
    int direction;
    if (!view || from < 0 || from >= view->tile_count ||
        to < 0 || to >= view->tile_count) return -1;
    delta_x = to % view->width - from % view->width;
    delta_y = to / view->width - from / view->width;
    for (direction = 0; direction < 8; direction++) {
        if (dx[direction] == delta_x && dy[direction] == delta_y) return direction;
    }
    return -1;
}

static int histogram_bounds(const uint32_t histogram[8], uint64_t total) {
    int direction;
    if (total < 256) return 1;
    for (direction = 0; direction < 8; direction++) {
        uint64_t per_mille = (uint64_t)histogram[direction] * 1000u / total;
        if (per_mille < ROUTE_DIRECTION_MIN_PER_MILLE ||
            per_mille > ROUTE_DIRECTION_MAX_PER_MILLE) return 0;
    }
    return 1;
}

static int check_routing(FILE *file, const char *label,
                         const WorldGenContext *context,
                         const RiverNetworkView *view) {
    const RiverGenerationDiagnostics *diagnostics = &view->diagnostics;
    uint64_t direction_total = 0;
    uint64_t flat_total = 0;
    int manual_edges = 0;
    int manual_lower = 0;
    int invalid_direction = 0;
    int direction;
    int i;
    int flat_bias_ok;
    int ok;
    for (i = 0; i < view->tile_count; i++) {
        int receiver;
        if (!context->land_mask[i]) continue;
        receiver = view->receiver[i];
        if (receiver < 0) continue;
        direction = direction_between(view, i, receiver);
        if (direction < 0) invalid_direction++;
        else {
            manual_edges++;
            manual_lower += receiver < i;
        }
    }
    for (direction = 0; direction < 8; direction++) {
        direction_total += diagnostics->receiver_direction_histogram[direction];
        flat_total += diagnostics->flat_direction_histogram[direction];
        coverage.directions[direction] +=
            diagnostics->receiver_direction_histogram[direction];
        coverage.flat_directions[direction] += diagnostics->flat_direction_histogram[direction];
    }
    flat_bias_ok = diagnostics->flat_receiver_edges < 128 ||
        ((uint64_t)diagnostics->flat_receiver_lower_index_edges * 1000u >=
             (uint64_t)diagnostics->flat_receiver_edges * FLAT_LOWER_MIN_PER_MILLE &&
         (uint64_t)diagnostics->flat_receiver_lower_index_edges * 1000u <=
             (uint64_t)diagnostics->flat_receiver_edges * FLAT_LOWER_MAX_PER_MILLE);
    ok = invalid_direction == 0 && manual_edges == diagnostics->receiver_edges &&
         manual_lower == diagnostics->receiver_lower_index_edges &&
         direction_total == (uint64_t)diagnostics->receiver_edges &&
         flat_total == (uint64_t)diagnostics->flat_receiver_edges &&
         diagnostics->flat_receiver_edges <= diagnostics->receiver_edges &&
         diagnostics->flat_receiver_lower_index_edges <= diagnostics->flat_receiver_edges &&
         histogram_bounds(diagnostics->receiver_direction_histogram, direction_total) &&
         flat_bias_ok &&
         diagnostics->max_same_direction_run <= ROUTE_MAX_SAME_DIRECTION_RUN &&
         diagnostics->max_flat_same_direction_run <= ROUTE_MAX_FLAT_DIRECTION_RUN;
    fprintf(file, "case=hydrology_direction label=%s seed=%u edges=%d flat=%d "
                  "lower=%d flat_lower=%d histogram=%u/%u/%u/%u/%u/%u/%u/%u "
                  "flat_histogram=%u/%u/%u/%u/%u/%u/%u/%u runs=%d/%d "
                  "invalid_direction=%d thresholds=30..300permille/350..650permille/96/64 ok=%d\n",
            label, context->phase_seed[WORLD_GEN_PHASE_HYDROLOGY],
            diagnostics->receiver_edges, diagnostics->flat_receiver_edges,
            diagnostics->receiver_lower_index_edges,
            diagnostics->flat_receiver_lower_index_edges,
            diagnostics->receiver_direction_histogram[0],
            diagnostics->receiver_direction_histogram[1],
            diagnostics->receiver_direction_histogram[2],
            diagnostics->receiver_direction_histogram[3],
            diagnostics->receiver_direction_histogram[4],
            diagnostics->receiver_direction_histogram[5],
            diagnostics->receiver_direction_histogram[6],
            diagnostics->receiver_direction_histogram[7],
            diagnostics->flat_direction_histogram[0],
            diagnostics->flat_direction_histogram[1],
            diagnostics->flat_direction_histogram[2],
            diagnostics->flat_direction_histogram[3],
            diagnostics->flat_direction_histogram[4],
            diagnostics->flat_direction_histogram[5],
            diagnostics->flat_direction_histogram[6],
            diagnostics->flat_direction_histogram[7],
            diagnostics->max_same_direction_run,
            diagnostics->max_flat_same_direction_run, invalid_direction, ok);
    coverage.receiver_edges += diagnostics->receiver_edges;
    coverage.flat_edges += diagnostics->flat_receiver_edges;
    coverage.lower_edges += diagnostics->receiver_lower_index_edges;
    coverage.flat_lower_edges += diagnostics->flat_receiver_lower_index_edges;
    if (diagnostics->max_same_direction_run > coverage.max_same_run)
        coverage.max_same_run = diagnostics->max_same_direction_run;
    if (diagnostics->max_flat_same_direction_run > coverage.max_flat_run)
        coverage.max_flat_run = diagnostics->max_flat_same_direction_run;
    return ok;
}

static int check_ordinary_receivers(FILE *file, const char *label,
                                    const WorldGenContext *context,
                                    const RiverNetworkView *view) {
    uint8_t *ordinary = (uint8_t *)calloc((size_t)view->tile_count, 1);
    int expected = 0;
    int actual = 0;
    int errors = 0;
    int i;
    if (!ordinary) return 0;
    for (i = 0; i < view->segment_count; i++) {
        const RiverNetworkSegment *segment = &view->segments[i];
        if (segment->kind != RIVER_SEGMENT_ORDINARY) continue;
        actual++;
        if (segment->from < 0 || segment->from >= view->tile_count ||
            segment->to != view->receiver[segment->from] || ordinary[segment->from] == 1) {
            errors++;
            continue;
        }
        ordinary[segment->from] = 1;
    }
    for (i = 0; i < view->tile_count; i++) {
        int receiver = view->receiver[i];
        int needs_segment;
        if (!(view->cell_flags[i] & RIVER_CELL_CHANNEL)) continue;
        needs_segment = receiver >= 0 && !(view->cell_flags[i] & RIVER_CELL_DELTA) &&
            !(receiver < view->tile_count &&
              (view->cell_flags[i] & RIVER_CELL_LAKE) &&
              (view->cell_flags[receiver] & RIVER_CELL_LAKE));
        expected += needs_segment;
        if (ordinary[i] != (uint8_t)needs_segment) errors++;
    }
    fprintf(file, "case=hydrology_ordinary_receiver label=%s expected=%d actual=%d "
                  "errors=%d exactly_one=%d ok=%d\n",
            label, expected, actual, errors, errors == 0,
            errors == 0 && expected == actual && context->tile_count == view->tile_count);
    free(ordinary);
    return errors == 0 && expected == actual;
}

static uint8_t snapshot_semantic_flags(uint16_t flags) {
    uint8_t result = 0;
    if (flags & WORLD_GEN_RIVER_SOURCE) result |= SNAPSHOT_RIVER_SOURCE;
    if (flags & WORLD_GEN_RIVER_CONFLUENCE) result |= SNAPSHOT_RIVER_CONFLUENCE;
    if (flags & WORLD_GEN_RIVER_LAKE) result |= SNAPSHOT_RIVER_LAKE;
    if (flags & WORLD_GEN_RIVER_MOUTH) result |= SNAPSHOT_RIVER_MOUTH;
    if (flags & WORLD_GEN_RIVER_DELTA) result |= SNAPSHOT_RIVER_DELTA;
    if (flags & WORLD_GEN_RIVER_CLOSED_BASIN) result |= SNAPSHOT_RIVER_CLOSED_BASIN;
    if (flags & WORLD_GEN_RIVER_DISTRIBUTARY) result |= SNAPSHOT_RIVER_DISTRIBUTARY;
    if (flags & WORLD_GEN_RIVER_SALT_LAKE) result |= SNAPSHOT_RIVER_SALT_LAKE;
    return result;
}

static RenderSnapshot *build_geometry_snapshot(const WorldGenContext *context) {
    const RiverPath *source = (const RiverPath *)context->staged_river_paths;
    RenderSnapshot *snapshot;
    int path;
    int i;
    if (context->staged_river_path_count <= 0 ||
        !river_paths_validate(source, context->staged_river_path_count,
                              context->width, context->height)) return NULL;
    snapshot = (RenderSnapshot *)calloc(1, sizeof(*snapshot));
    if (!snapshot) return NULL;
    snapshot->rivers.paths = (SnapshotRiverPath *)calloc(
        (size_t)context->staged_river_path_count, sizeof(*snapshot->rivers.paths));
    if (!snapshot->rivers.paths) {
        free(snapshot);
        return NULL;
    }
    snapshot->map_w = context->width;
    snapshot->map_h = context->height;
    snapshot->river_revision = (int)(context->phase_seed[WORLD_GEN_PHASE_HYDROLOGY] | 1u);
    for (i = 0; i < context->tile_count; i++) {
        snapshot->tiles[i].geography = context->geography[i];
        snapshot->tiles[i].climate = context->climate[i];
        snapshot->tiles[i].elevation = (unsigned char)clamp_int(context->elevation[i], 0, 255);
    }
    snapshot->rivers.valid = 1;
    snapshot->rivers.revision = snapshot->river_revision;
    snapshot->rivers.map_w = context->width;
    snapshot->rivers.map_h = context->height;
    snapshot->rivers.capacity = context->staged_river_path_count;
    snapshot->rivers.path_count = context->staged_river_path_count;
    for (path = 0; path < context->staged_river_path_count; path++) {
        SnapshotRiverPath *out = &snapshot->rivers.paths[path];
        int point;
        if (!source[path].active || source[path].point_count < 2 ||
            source[path].point_count > MAX_RIVER_POINTS) goto failed;
        out->flow = (uint32_t)source[path].flow;
        out->point_count = (uint16_t)source[path].point_count;
        out->width = (uint16_t)source[path].width;
        out->order = (uint8_t)source[path].order;
        for (point = 0; point < source[path].point_count; point++) {
            int x = source[path].points[point].x;
            int y = source[path].points[point].y;
            int index;
            uint8_t flags;
            if (x < 0 || x >= context->width || y < 0 || y >= context->height) goto failed;
            index = y * context->width + x;
            flags = snapshot_semantic_flags(context->river_flags[index]);
            out->points[point].x = (uint16_t)x;
            out->points[point].y = (uint16_t)y;
            out->points[point].semantic_flags = flags;
            out->semantic_flags |= flags;
            if (point == source[path].point_count - 1) out->end_flags = flags;
        }
    }
    return snapshot;
failed:
    free(snapshot->rivers.paths);
    free(snapshot);
    return NULL;
}

static int render_point_matches(RiverRenderPoint point, int x, int y) {
    return point.x10 == x * 10 + 5 && point.y10 == y * 10 + 5;
}

static int check_geometry(FILE *file, const char *label,
                          const WorldGenContext *context) {
    RenderSnapshot *snapshot = build_geometry_snapshot(context);
    const RiverRenderPath *rendered;
    const HydrologyRenderStats *live_stats;
    HydrologyRenderStats stats;
    int rendered_count = 0;
    int endpoint_errors = 0;
    int semantic_points = 0;
    int semantic_missing = 0;
    int point_errors = 0;
    int path;
    int ok = 0;
    if (!snapshot) goto done;
    river_geometry_rebuild(snapshot);
    rendered = river_geometry_paths(&rendered_count);
    live_stats = river_geometry_stats();
    stats = *live_stats;
    if (rendered_count != snapshot->rivers.path_count) point_errors++;
    for (path = 0; path < rendered_count && path < snapshot->rivers.path_count; path++) {
        const SnapshotRiverPath *raw = &snapshot->rivers.paths[path];
        const RiverRenderPath *curve = &rendered[path];
        int point;
        if (curve->point_count < 2 || curve->point_count > MAX_RIVER_RENDER_POINTS) {
            point_errors++;
            continue;
        }
        if (!render_point_matches(curve->points[0], raw->points[0].x, raw->points[0].y) ||
            !render_point_matches(curve->points[curve->point_count - 1],
                                  raw->points[raw->point_count - 1].x,
                                  raw->points[raw->point_count - 1].y)) endpoint_errors++;
        for (point = 0; point < raw->point_count; point++) {
            int render_point;
            int found = 0;
            if (raw->points[point].semantic_flags == 0) continue;
            semantic_points++;
            for (render_point = 0; render_point < curve->point_count; render_point++) {
                if (render_point_matches(curve->points[render_point], raw->points[point].x,
                                         raw->points[point].y)) found = 1;
            }
            semantic_missing += !found;
        }
    }
    ok = rendered_count > 0 && endpoint_errors == 0 && semantic_missing == 0 &&
         point_errors == 0 && stats.semantic_anchor_count == semantic_points &&
         stats.render_segment_count > 0 && stats.offset_anchor_count > 0 &&
         stats.axis_segment_count + stats.diagonal_segment_count +
             stats.non_grid_segment_count == stats.render_segment_count &&
         (uint64_t)stats.non_grid_segment_count * 1000u >=
             (uint64_t)stats.render_segment_count * GEOMETRY_MIN_NON_GRID_PER_MILLE &&
         stats.longest_same_angle_run <= GEOMETRY_MAX_SAME_ANGLE_RUN &&
         stats.longest_grid_angle_run <= GEOMETRY_MAX_GRID_ANGLE_RUN;
    fprintf(file, "case=hydrology_render_angles label=%s paths=%d segments=%d axis=%d "
                  "diagonal=%d non_grid=%d offsets=%d semantic=%d missing=%d "
                  "endpoint_errors=%d point_errors=%d runs=%d/%d "
                  "thresholds=30permille/32/64 ok=%d\n",
            label, rendered_count, stats.render_segment_count, stats.axis_segment_count,
            stats.diagonal_segment_count, stats.non_grid_segment_count,
            stats.offset_anchor_count, semantic_points, semantic_missing, endpoint_errors,
            point_errors, stats.longest_same_angle_run, stats.longest_grid_angle_run, ok);
    coverage.geometry_cases++;
    coverage.render_segments += stats.render_segment_count;
    coverage.axis_segments += stats.axis_segment_count;
    coverage.diagonal_segments += stats.diagonal_segment_count;
    coverage.non_grid_segments += stats.non_grid_segment_count;
    coverage.offset_anchors += stats.offset_anchor_count;
    if (stats.longest_same_angle_run > coverage.max_same_angle_run)
        coverage.max_same_angle_run = stats.longest_same_angle_run;
    if (stats.longest_grid_angle_run > coverage.max_grid_angle_run)
        coverage.max_grid_angle_run = stats.longest_grid_angle_run;
done:
    if (snapshot) {
        free(snapshot->rivers.paths);
        free(snapshot);
    }
    return ok;
}

static int geometry_fixture_label(const char *label) {
    return strcmp(label, "determinism_a1") == 0 ||
           strcmp(label, "determinism_b") == 0 ||
           strcmp(label, "map_size_medium") == 0 ||
           strcmp(label, "map_size_large") == 0 ||
           strcmp(label, "map_size_extreme") == 0;
}

void game_worldgen_hydrology_direction_probe_reset(void) {
    memset(&coverage, 0, sizeof(coverage));
}

int game_worldgen_hydrology_direction_probe_check(
    FILE *file, const char *label, const WorldGenContext *context,
    const RiverNetworkView *view) {
    int ok;
    if (!file || !label || !context || !view) return 0;
    record_seed(context->phase_seed[WORLD_GEN_PHASE_HYDROLOGY]);
    coverage.cases++;
    ok = check_routing(file, label, context, view);
    ok &= check_ordinary_receivers(file, label, context, view);
    if (geometry_fixture_label(label)) ok &= check_geometry(file, label, context);
    return ok;
}

int game_worldgen_hydrology_direction_probe_finish(FILE *file) {
    int direction;
    int direction_ok = coverage.receiver_edges > 0;
    int flat_bias_ok;
    int geometry_ok;
    int ok;
    if (!file) return 0;
    for (direction = 0; direction < 8; direction++) {
        uint64_t per_mille = coverage.receiver_edges > 0
            ? coverage.directions[direction] * 1000u / coverage.receiver_edges : 0;
        if (per_mille < ROUTE_DIRECTION_MIN_PER_MILLE ||
            per_mille > ROUTE_DIRECTION_MAX_PER_MILLE) direction_ok = 0;
    }
    flat_bias_ok = coverage.flat_edges >= 128 &&
        coverage.flat_lower_edges * 1000u >=
            coverage.flat_edges * FLAT_LOWER_MIN_PER_MILLE &&
        coverage.flat_lower_edges * 1000u <=
            coverage.flat_edges * FLAT_LOWER_MAX_PER_MILLE;
    geometry_ok = coverage.geometry_cases >= 5 && coverage.render_segments > 0 &&
        coverage.offset_anchors > 0 &&
        coverage.axis_segments + coverage.diagonal_segments +
            coverage.non_grid_segments == coverage.render_segments &&
        coverage.non_grid_segments * 1000u >=
            coverage.render_segments * GEOMETRY_MIN_NON_GRID_PER_MILLE &&
        coverage.max_same_angle_run <= GEOMETRY_MAX_SAME_ANGLE_RUN &&
        coverage.max_grid_angle_run <= GEOMETRY_MAX_GRID_ANGLE_RUN;
    ok = coverage.cases >= 8 && coverage.seed_count >= 2 && direction_ok && flat_bias_ok &&
         coverage.max_same_run <= ROUTE_MAX_SAME_DIRECTION_RUN &&
         coverage.max_flat_run <= ROUTE_MAX_FLAT_DIRECTION_RUN && geometry_ok;
    fprintf(file, "case=hydrology_direction_coverage cases=%d seeds=%d edges=%llu flat=%llu "
                  "lower=%llu flat_lower=%llu histogram=%llu/%llu/%llu/%llu/%llu/%llu/%llu/%llu "
                  "runs=%d/%d geometry_cases=%d segments=%llu axis=%llu diagonal=%llu "
                  "non_grid=%llu offsets=%llu angle_runs=%d/%d "
                  "thresholds=30..300permille/350..650permille/96/64/30permille/32/64 ok=%d\n",
            coverage.cases, coverage.seed_count,
            (unsigned long long)coverage.receiver_edges,
            (unsigned long long)coverage.flat_edges,
            (unsigned long long)coverage.lower_edges,
            (unsigned long long)coverage.flat_lower_edges,
            (unsigned long long)coverage.directions[0],
            (unsigned long long)coverage.directions[1],
            (unsigned long long)coverage.directions[2],
            (unsigned long long)coverage.directions[3],
            (unsigned long long)coverage.directions[4],
            (unsigned long long)coverage.directions[5],
            (unsigned long long)coverage.directions[6],
            (unsigned long long)coverage.directions[7], coverage.max_same_run,
            coverage.max_flat_run, coverage.geometry_cases,
            (unsigned long long)coverage.render_segments,
            (unsigned long long)coverage.axis_segments,
            (unsigned long long)coverage.diagonal_segments,
            (unsigned long long)coverage.non_grid_segments,
            (unsigned long long)coverage.offset_anchors, coverage.max_same_angle_run,
            coverage.max_grid_angle_run, ok);
    return ok;
}
