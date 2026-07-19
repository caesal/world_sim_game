#ifndef WORLD_SIM_RIVER_GEOMETRY_H
#define WORLD_SIM_RIVER_GEOMETRY_H

#include <stddef.h>

#include "core/render_snapshot.h"

#define MAX_RIVER_RENDER_POINTS (MAX_RIVER_POINTS * 2)

typedef enum {
    RIVER_STYLE_PLAIN = 0,
    RIVER_STYLE_MOUNTAIN = 1 << 0,
    RIVER_STYLE_HILL = 1 << 1,
    RIVER_STYLE_WETLAND = 1 << 2,
    RIVER_STYLE_DESERT = 1 << 3,
    RIVER_STYLE_COLD = 1 << 4
} RiverStyleFlags;

typedef struct {
    short x10;
    short y10;
} RiverRenderPoint;

typedef struct {
    int active;
    int point_count;
    int raw_point_count;
    int order;
    int flow;
    int terminal_inflow;
    int width;
    int style_flags;
    int semantic_flags;
    int end_flags;
    RiverRenderPoint points[MAX_RIVER_RENDER_POINTS];
} RiverRenderPath;

typedef struct {
    int river_count;
    int average_length;
    int longest_length;
    int main_rivers;
    int tributaries;
    int confluences;
    int invalid_uphill_segments;
    int inland_dead_ends;
    int overly_short_rivers;
    int cache_rebuild_count;
    int cache_rebuild_ms;
    int geometry_rebuild_count;
    int geometry_rebuild_ms;
    int curved_paths;
    int offset_anchor_count;
    int semantic_anchor_count;
    int render_segment_count;
    int axis_segment_count;
    int diagonal_segment_count;
    int non_grid_segment_count;
    int corner_turn_count;
    int longest_same_angle_run;
    int longest_grid_angle_run;
    int visible_river_count_last_draw;
    int skipped_by_lod_last_draw;
    int lod_target_last_draw;
    int lod_stem_count_last_draw;
    int lod_connected_path_count_last_draw;
} HydrologyRenderStats;

void river_geometry_rebuild(const RenderSnapshot *snapshot);
int river_geometry_rebuild_if_needed(const RenderSnapshot *snapshot);
int river_geometry_prepare(const RenderSnapshot *snapshot);
void river_geometry_release(void);
size_t river_geometry_retained_bytes(void);
const RiverRenderPath *river_geometry_paths(int *count);
const HydrologyRenderStats *river_geometry_stats(void);
void river_geometry_note_cache_rebuild(int ms);
void river_geometry_note_lod_counts(int visible, int skipped);
void river_geometry_note_lod_policy(int target, int stems, int connected_paths);

#endif
