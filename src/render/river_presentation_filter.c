#include "render/river_presentation_filter.h"

#include <string.h>

#define FILTER_PROTECTED_FLAGS \
    (SNAPSHOT_RIVER_CONFLUENCE | SNAPSHOT_RIVER_LAKE | \
     SNAPSHOT_RIVER_DELTA | SNAPSHOT_RIVER_CLOSED_BASIN | \
     SNAPSHOT_RIVER_DISTRIBUTARY | SNAPSHOT_RIVER_SALT_LAKE)

enum {
    FILTER_MAX_MINOR_ORDER = 2,
    FILTER_MAX_MINOR_RAW_POINTS = 17
};

static RiverPresentationFilterMetrics last_metrics;

static int absolute_value(int value) { return value < 0 ? -value : value; }

static int terminal_vector(const RiverRenderPath *path, int *dx, int *dy) {
    RiverRenderPoint end;
    int point;
    if (!path || path->point_count < 2 || !dx || !dy) return 0;
    end = path->points[path->point_count - 1];
    for (point = path->point_count - 2; point >= 0; point--) {
        *dx = (int)end.x10 - path->points[point].x10;
        *dy = (int)end.y10 - path->points[point].y10;
        if (absolute_value(*dx) >= 10 || absolute_value(*dy) >= 10)
            return 1;
    }
    return *dx != 0 || *dy != 0;
}

static int protected_semantics(const RiverRenderPath *path) {
    return path && (path->semantic_flags & FILTER_PROTECTED_FLAGS) != 0;
}

static int close_candidate(const RiverRenderPath *paths, int count,
                           const RiverTopologyView *topology,
                           int path_index) {
    const RiverRenderPath *path;
    const RiverTopologyPathLink *link;
    const RiverTopologyStem *stem;
    int dx, dy;
    if (!paths || !topology || !topology->paths || !topology->stems ||
        topology->path_count != count || path_index < 0 ||
        path_index >= count) return 0;
    path = &paths[path_index];
    link = &topology->paths[path_index];
    if (!path->active || path->point_count < 2 || path->raw_point_count < 2 ||
        path->order < 1 || path->order > FILTER_MAX_MINOR_ORDER ||
        path->raw_point_count > FILTER_MAX_MINOR_RAW_POINTS ||
        !(path->semantic_flags & SNAPSHOT_RIVER_MOUTH) ||
        protected_semantics(path) || link->stem_id < 0 ||
        link->stem_id >= topology->stem_count || link->downstream_path >= 0 ||
        link->dominant_upstream_path >= 0) return 0;
    stem = &topology->stems[link->stem_id];
    return stem->path_count == 1 && stem->downstream_stem < 0 &&
           terminal_vector(path, &dx, &dy);
}

void river_presentation_filter_apply_close(
    const RiverRenderPath *paths, int count,
    const RiverTopologyView *topology, unsigned char *visible_mask,
    RiverPresentationFilterMetrics *out_metrics) {
    RiverPresentationFilterMetrics result = {0};
    int path;
    result.retained_bytes = sizeof(last_metrics);
    if (!paths || !topology || !visible_mask || count <= 0 ||
        topology->path_count != count) goto done;
    for (path = 0; path < count; path++) {
        if (!visible_mask[path]) continue;
        result.protected_semantic_paths += protected_semantics(&paths[path]);
        if (close_candidate(paths, count, topology, path)) {
            result.candidate_stems++;
            result.kept_candidate_stems++;
        }
    }
    for (path = 0; path < count; path++) {
        if (protected_semantics(&paths[path]) && !visible_mask[path])
            result.protected_hidden_paths++;
    }
done:
    last_metrics = result;
    if (out_metrics) *out_metrics = result;
}

RiverPresentationFilterMetrics river_presentation_filter_last_metrics(void) {
    return last_metrics;
}

void river_presentation_filter_reset(void) {
    memset(&last_metrics, 0, sizeof(last_metrics));
    last_metrics.retained_bytes = sizeof(last_metrics);
}
