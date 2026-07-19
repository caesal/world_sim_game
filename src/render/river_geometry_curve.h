#ifndef WORLD_SIM_RIVER_GEOMETRY_CURVE_H
#define WORLD_SIM_RIVER_GEOMETRY_CURVE_H

#include "render/river_geometry.h"

typedef struct {
    int offset_anchors;
    int semantic_anchors;
    int render_segments;
    int axis_segments;
    int diagonal_segments;
    int non_grid_segments;
    int corner_turns;
    int longest_same_angle_run;
    int longest_grid_angle_run;
} RiverGeometryCurveStats;

int river_geometry_curve_build(const RenderSnapshot *snapshot,
                               const SnapshotRiverPath *river,
                               int river_revision, int style_flags,
                               RiverRenderPath *out,
                               RiverGeometryCurveStats *out_stats);

#endif
