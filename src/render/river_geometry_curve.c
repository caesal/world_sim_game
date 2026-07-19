#include "render/river_geometry_curve.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static uint32_t mix32(uint32_t value) {
    value ^= value >> 16;
    value *= UINT32_C(0x7feb352d);
    value ^= value >> 15;
    value *= UINT32_C(0x846ca68b);
    return value ^ (value >> 16);
}

static int clamp_int(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static int max_int(int left, int right) {
    return left > right ? left : right;
}

static int in_map(const RenderSnapshot *snapshot, int x, int y) {
    return snapshot && x >= 0 && y >= 0 &&
           x < snapshot->map_w && y < snapshot->map_h;
}

static const SnapshotTile *tile_at(const RenderSnapshot *snapshot, int x, int y) {
    return in_map(snapshot, x, y) ? &snapshot->tiles[y * snapshot->map_w + x] : NULL;
}

static RiverRenderPoint tile_center(int x, int y) {
    RiverRenderPoint point;
    point.x10 = (short)(x * 10 + 5);
    point.y10 = (short)(y * 10 + 5);
    return point;
}

static int steep_geography(const SnapshotTile *tile) {
    Geography geography;
    if (!tile) return 1;
    geography = (Geography)tile->geography;
    return geography == GEO_MOUNTAIN || geography == GEO_PLATEAU ||
           geography == GEO_CANYON || geography == GEO_VOLCANO;
}

static int anchor_amplitude(const RenderSnapshot *snapshot,
                            const SnapshotRiverPath *river, int point,
                            int style_flags, uint32_t hash) {
    const SnapshotRiverPoint *previous = &river->points[point - 1];
    const SnapshotRiverPoint *current = &river->points[point];
    const SnapshotRiverPoint *next = &river->points[point + 1];
    const SnapshotTile *before = tile_at(snapshot, previous->x, previous->y);
    const SnapshotTile *tile = tile_at(snapshot, current->x, current->y);
    const SnapshotTile *after = tile_at(snapshot, next->x, next->y);
    int grade;
    int maximum;
    if (!before || !tile || !after || current->semantic_flags != 0) return 0;
    grade = abs((int)tile->elevation - (int)before->elevation);
    if (abs((int)tile->elevation - (int)after->elevation) > grade) {
        grade = abs((int)tile->elevation - (int)after->elevation);
    }
    if (steep_geography(tile) || grade >= 8) return 0;
    if ((style_flags & RIVER_STYLE_MOUNTAIN) || grade >= 5) maximum = 1;
    else if ((style_flags & RIVER_STYLE_HILL) || grade >= 3) maximum = 2;
    else maximum = 4;
    if ((hash & 7u) < 3u) return 0;
    if (maximum <= 1) return (hash & 3u) == 3u ? 1 : 0;
    if (maximum == 2) return 1 + (int)((hash >> 4) & 1u);
    return 2 + (int)((hash >> 4) % 3u);
}

static RiverRenderPoint constrained_anchor(const RenderSnapshot *snapshot,
                                           const SnapshotRiverPath *river,
                                           int point, int revision,
                                           int style_flags, int *offset) {
    const SnapshotRiverPoint *previous = &river->points[point - 1];
    const SnapshotRiverPoint *current = &river->points[point];
    const SnapshotRiverPoint *next = &river->points[point + 1];
    RiverRenderPoint anchor = tile_center(current->x, current->y);
    uint32_t hash = mix32((uint32_t)revision ^
                          (uint32_t)current->x * UINT32_C(0x9e3779b9) ^
                          (uint32_t)current->y * UINT32_C(0x85ebca6b));
    int tangent_x = (int)next->x - (int)previous->x;
    int tangent_y = (int)next->y - (int)previous->y;
    int normal_x = -tangent_y;
    int normal_y = tangent_x;
    int divisor = max_int(abs(normal_x), abs(normal_y));
    int amplitude = anchor_amplitude(snapshot, river, point, style_flags, hash);
    int sign = (hash & UINT32_C(0x80000000)) ? -1 : 1;
    int center_x = current->x * 10 + 5;
    int center_y = current->y * 10 + 5;
    if (offset) *offset = 0;
    if (amplitude <= 0 || divisor <= 0) return anchor;
    anchor.x10 = (short)clamp_int(center_x + sign * normal_x * amplitude / divisor,
                                  center_x - 4, center_x + 4);
    anchor.y10 = (short)clamp_int(center_y + sign * normal_y * amplitude / divisor,
                                  center_y - 4, center_y + 4);
    if (offset) *offset = anchor.x10 != center_x || anchor.y10 != center_y;
    return anchor;
}

static int append_point(RiverRenderPath *out, RiverRenderPoint point) {
    if (out->point_count > 0 &&
        out->points[out->point_count - 1].x10 == point.x10 &&
        out->points[out->point_count - 1].y10 == point.y10) return 1;
    if (out->point_count >= MAX_RIVER_RENDER_POINTS) return 0;
    out->points[out->point_count++] = point;
    return 1;
}

static RiverRenderPoint midpoint(RiverRenderPoint first, RiverRenderPoint second) {
    RiverRenderPoint result;
    result.x10 = (short)(((int)first.x10 + second.x10) / 2);
    result.y10 = (short)(((int)first.y10 + second.y10) / 2);
    return result;
}

static RiverRenderPoint quadratic_third(RiverRenderPoint start,
                                        RiverRenderPoint control,
                                        RiverRenderPoint end, int second) {
    RiverRenderPoint result;
    int start_weight = second ? 1 : 4;
    int end_weight = second ? 4 : 1;
    result.x10 = (short)((start_weight * start.x10 + 4 * control.x10 +
                          end_weight * end.x10) / 9);
    result.y10 = (short)((start_weight * start.y10 + 4 * control.y10 +
                          end_weight * end.y10) / 9);
    return result;
}

static int greatest_common_divisor(int first, int second) {
    first = abs(first);
    second = abs(second);
    while (second != 0) {
        int remainder = first % second;
        first = second;
        second = remainder;
    }
    return first > 0 ? first : 1;
}

static void collect_angle_stats(const RiverRenderPath *out,
                                RiverGeometryCurveStats *stats) {
    int previous_dx = INT_MAX;
    int previous_dy = INT_MAX;
    int same_run = 0;
    int grid_run = 0;
    int i;
    for (i = 0; i + 1 < out->point_count; i++) {
        int dx = out->points[i + 1].x10 - out->points[i].x10;
        int dy = out->points[i + 1].y10 - out->points[i].y10;
        int divisor;
        int axis;
        int diagonal;
        if (dx == 0 && dy == 0) continue;
        divisor = greatest_common_divisor(dx, dy);
        dx /= divisor;
        dy /= divisor;
        axis = dx == 0 || dy == 0;
        diagonal = abs(dx) == abs(dy);
        stats->render_segments++;
        stats->axis_segments += axis;
        stats->diagonal_segments += diagonal;
        stats->non_grid_segments += !axis && !diagonal;
        if (dx == previous_dx && dy == previous_dy) same_run++;
        else {
            if (previous_dx != INT_MAX) stats->corner_turns++;
            same_run = 1;
        }
        if (axis || diagonal) grid_run++;
        else grid_run = 0;
        if (same_run > stats->longest_same_angle_run)
            stats->longest_same_angle_run = same_run;
        if (grid_run > stats->longest_grid_angle_run)
            stats->longest_grid_angle_run = grid_run;
        previous_dx = dx;
        previous_dy = dy;
    }
}

int river_geometry_curve_build(const RenderSnapshot *snapshot,
                               const SnapshotRiverPath *river,
                               int river_revision, int style_flags,
                               RiverRenderPath *out,
                               RiverGeometryCurveStats *out_stats) {
    RiverRenderPoint raw[MAX_RIVER_POINTS];
    uint8_t raw_semantic_flags[MAX_RIVER_POINTS];
    RiverGeometryCurveStats local_stats;
    int raw_count = 0;
    int i;
    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    memset(&local_stats, 0, sizeof(local_stats));
    if (!snapshot || !river || river->point_count < 2) return 0;
    out->order = river->order;
    out->raw_point_count = river->point_count;
    out->flow = river->flow > (uint32_t)INT_MAX ? INT_MAX : (int)river->flow;
    out->terminal_inflow = river->terminal_inflow > (uint32_t)INT_MAX ?
        INT_MAX : (int)river->terminal_inflow;
    out->width = river->width;
    out->style_flags = style_flags;
    out->semantic_flags = river->semantic_flags;
    out->end_flags = river->end_flags;
    for (i = 0; i < river->point_count && raw_count < MAX_RIVER_POINTS; i++) {
        const SnapshotRiverPoint *point = &river->points[i];
        int offset = 0;
        if (!in_map(snapshot, point->x, point->y)) continue;
        if (i == 0 || i == river->point_count - 1 || point->semantic_flags != 0) {
            raw[raw_count] = tile_center(point->x, point->y);
            local_stats.semantic_anchors += point->semantic_flags != 0;
        } else {
            raw[raw_count] = constrained_anchor(snapshot, river, i, river_revision,
                                                style_flags, &offset);
            local_stats.offset_anchors += offset;
        }
        raw_semantic_flags[raw_count] = point->semantic_flags;
        raw_count++;
    }
    if (raw_count < 2 || !append_point(out, raw[0])) return 0;
    for (i = 1; i + 1 < raw_count; i++) {
        if (raw_semantic_flags[i] != 0) {
            if (!append_point(out, raw[i])) return 0;
        } else {
            RiverRenderPoint start = midpoint(raw[i - 1], raw[i]);
            RiverRenderPoint end = midpoint(raw[i], raw[i + 1]);
            if (!append_point(out, quadratic_third(start, raw[i], end, 0)) ||
                !append_point(out, quadratic_third(start, raw[i], end, 1))) return 0;
        }
    }
    if (!append_point(out, raw[raw_count - 1]) || out->point_count < 2) return 0;
    out->active = 1;
    collect_angle_stats(out, &local_stats);
    if (out_stats) *out_stats = local_stats;
    return 1;
}
