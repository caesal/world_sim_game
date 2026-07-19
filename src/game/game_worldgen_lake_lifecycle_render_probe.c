#include "game/game_worldgen_lake_lifecycle_render_probe.h"

#include "render/river_geometry_curve.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static uint8_t snapshot_flags(uint16_t flags) {
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

static int snapshot_path(const WorldGenContext *context, const RiverPath *source,
                         SnapshotRiverPath *out) {
    int point;
    memset(out, 0, sizeof(*out));
    if (!source || !source->active || source->point_count < 2 ||
        source->point_count > MAX_RIVER_POINTS) return 0;
    out->flow = source->flow < 0 ? 0u : (uint32_t)source->flow;
    out->width = (uint16_t)source->width;
    out->order = (uint8_t)source->order;
    out->point_count = (uint16_t)source->point_count;
    for (point = 0; point < source->point_count; point++) {
        int x = source->points[point].x;
        int y = source->points[point].y;
        int index;
        uint8_t flags;
        if (x < 0 || x >= context->width || y < 0 || y >= context->height) return 0;
        index = y * context->width + x;
        flags = snapshot_flags(context->river_flags[index]);
        out->points[point].x = (uint16_t)x;
        out->points[point].y = (uint16_t)y;
        out->points[point].semantic_flags = flags;
        out->semantic_flags |= flags;
        if (point == source->point_count - 1) out->end_flags = flags;
    }
    return 1;
}

int game_worldgen_lake_lifecycle_render_check(
    const WorldGenContext *context, const RiverPath *inlet_path,
    const RiverPath *outlet_path, int inlet_receiver, int outlet,
    int *semantic_anchors) {
    RenderSnapshot *snapshot = (RenderSnapshot *)calloc(1, sizeof(*snapshot));
    SnapshotRiverPath inlet;
    SnapshotRiverPath outlet_path_snapshot;
    RiverRenderPath inlet_curve;
    RiverRenderPath outlet_curve;
    RiverGeometryCurveStats inlet_stats;
    RiverGeometryCurveStats outlet_stats;
    int inlet_ok;
    int outlet_ok;
    int i;
    int ok = 0;
    if (semantic_anchors) *semantic_anchors = 0;
    if (!context || !snapshot ||
        !snapshot_path(context, inlet_path, &inlet) ||
        !snapshot_path(context, outlet_path, &outlet_path_snapshot)) goto done;
    snapshot->map_w = context->width;
    snapshot->map_h = context->height;
    for (i = 0; i < context->tile_count; i++) {
        snapshot->tiles[i].geography = context->geography[i];
        snapshot->tiles[i].climate = context->climate[i];
        snapshot->tiles[i].elevation = (unsigned char)
            (context->elevation[i] < 0 ? 0 : context->elevation[i] > 255 ?
             255 : context->elevation[i]);
    }
    inlet_ok = river_geometry_curve_build(snapshot, &inlet, 0x4c414b45, 0,
                                           &inlet_curve, &inlet_stats);
    outlet_ok = river_geometry_curve_build(snapshot, &outlet_path_snapshot,
                                            0x4c414b45, 0, &outlet_curve,
                                            &outlet_stats);
    if (semantic_anchors) {
        *semantic_anchors = inlet_stats.semantic_anchors + outlet_stats.semantic_anchors;
    }
    ok = inlet_ok && outlet_ok && inlet_curve.point_count >= 2 &&
         outlet_curve.point_count >= 2 &&
         inlet_curve.points[inlet_curve.point_count - 1].x10 ==
             (inlet_receiver % context->width) * 10 + 5 &&
         inlet_curve.points[inlet_curve.point_count - 1].y10 ==
             (inlet_receiver / context->width) * 10 + 5 &&
         outlet_curve.points[0].x10 == (outlet % context->width) * 10 + 5 &&
         outlet_curve.points[0].y10 == (outlet / context->width) * 10 + 5 &&
         (inlet.end_flags & SNAPSHOT_RIVER_LAKE) != 0 &&
         (outlet_path_snapshot.points[0].semantic_flags &
          SNAPSHOT_RIVER_LAKE) != 0 &&
         inlet_stats.semantic_anchors > 0 && outlet_stats.semantic_anchors > 0;
done:
    free(snapshot);
    return ok;
}
