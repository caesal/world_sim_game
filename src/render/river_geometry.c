#include "render/river_geometry.h"
#include "render/river_geometry_curve.h"
#include "render/river_lod_policy.h"
#include "render/river_topology.h"

#include "world/river_path_validation.h"
#include "world/terrain_query.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdlib.h>
#include <string.h>

static RiverRenderPath *render_paths;
static int render_path_capacity;
static int render_path_count;
static HydrologyRenderStats stats;
static int geometry_valid;
static int last_geometry_revision;
static int last_map_w;
static int last_map_h;
static int last_river_path_count;
static unsigned char point_hits[MAX_MAP_H][MAX_MAP_W];

static int resize_render_paths(int count, int map_w, int map_h) {
    RiverRenderPath *paths;
    if (!river_path_count_valid(count, map_w, map_h)) return 0;
    if (count == 0) {
        free(render_paths);
        render_paths = NULL;
        render_path_capacity = 0;
        return 1;
    }
    if (render_paths && render_path_capacity == count) return 1;
    paths = (RiverRenderPath *)realloc(
        render_paths, (size_t)count * sizeof(*render_paths));
    if (!paths) return 0;
    render_paths = paths;
    render_path_capacity = count;
    return 1;
}

static int in_map(const RenderSnapshot *snapshot, int x, int y) {
    return snapshot && x >= 0 && y >= 0 && x < snapshot->map_w && y < snapshot->map_h;
}

static const SnapshotTile *tile_at(const RenderSnapshot *snapshot, int x, int y) {
    return in_map(snapshot, x, y) ? &snapshot->tiles[y * snapshot->map_w + x] : NULL;
}

static int raw_style_flags(const RenderSnapshot *snapshot, const SnapshotRiverPath *river) {
    int mountain = 0, hill = 0, wetland = 0, desert = 0, cold = 0;
    int i;
    for (i = 0; i < river->point_count; i++) {
        const SnapshotTile *tile = tile_at(snapshot, river->points[i].x, river->points[i].y);
        Geography geography;
        Climate climate;
        if (!tile) continue;
        geography = (Geography)tile->geography;
        climate = (Climate)tile->climate;
        mountain += geography == GEO_MOUNTAIN || geography == GEO_PLATEAU ||
                    geography == GEO_CANYON;
        hill += geography == GEO_HILL;
        wetland += geography == GEO_WETLAND || geography == GEO_DELTA;
        desert += climate == CLIMATE_DESERT || climate == CLIMATE_SEMI_ARID;
        cold += climate == CLIMATE_TUNDRA || climate == CLIMATE_ICE_CAP ||
                climate == CLIMATE_SUBARCTIC;
    }
    if (mountain * 3 > river->point_count) return RIVER_STYLE_MOUNTAIN;
    if (wetland * 3 > river->point_count) return RIVER_STYLE_WETLAND;
    if (desert * 3 > river->point_count) return RIVER_STYLE_DESERT;
    if (cold * 3 > river->point_count) return RIVER_STYLE_COLD;
    if (hill * 3 > river->point_count) return RIVER_STYLE_HILL;
    return RIVER_STYLE_PLAIN;
}

static void build_render_path(const RenderSnapshot *snapshot,
                              const SnapshotRiverPath *river, RiverRenderPath *out,
                              RiverGeometryCurveStats *curve_stats) {
    int style_flags = raw_style_flags(snapshot, river);
    river_geometry_curve_build(snapshot, river, snapshot->rivers.revision,
                               style_flags, out, curve_stats);
}

static void merge_curve_stats(const RiverGeometryCurveStats *curve) {
    if (!curve) return;
    stats.curved_paths += curve->offset_anchors > 0 || curve->non_grid_segments > 0;
    stats.offset_anchor_count += curve->offset_anchors;
    stats.semantic_anchor_count += curve->semantic_anchors;
    stats.render_segment_count += curve->render_segments;
    stats.axis_segment_count += curve->axis_segments;
    stats.diagonal_segment_count += curve->diagonal_segments;
    stats.non_grid_segment_count += curve->non_grid_segments;
    stats.corner_turn_count += curve->corner_turns;
    if (curve->longest_same_angle_run > stats.longest_same_angle_run)
        stats.longest_same_angle_run = curve->longest_same_angle_run;
    if (curve->longest_grid_angle_run > stats.longest_grid_angle_run)
        stats.longest_grid_angle_run = curve->longest_grid_angle_run;
}

static void collect_point_hits(const RenderSnapshot *snapshot) {
    int i, j;
    memset(point_hits, 0, sizeof(point_hits));
    for (i = 0; i < snapshot->rivers.path_count; i++) {
        const SnapshotRiverPath *river = &snapshot->rivers.paths[i];
        for (j = 0; j < river->point_count; j++) {
            int x = river->points[j].x;
            int y = river->points[j].y;
            if (in_map(snapshot, x, y)) {
                int count = point_hits[y][x] & 0x7f;
                int confluence = point_hits[y][x] & 0x80;
                if (count < 0x7f) count++;
                if (river->points[j].semantic_flags & SNAPSHOT_RIVER_CONFLUENCE)
                    confluence = 0x80;
                point_hits[y][x] = (unsigned char)(count | confluence);
            }
        }
    }
}

static void collect_stats_for_river(const RenderSnapshot *snapshot,
                                    const SnapshotRiverPath *river) {
    int i;
    int end_x = river->points[river->point_count - 1].x;
    int end_y = river->points[river->point_count - 1].y;
    stats.river_count++;
    stats.average_length += river->point_count;
    if (river->point_count > stats.longest_length) stats.longest_length = river->point_count;
    if (river->order >= 3) stats.main_rivers++;
    else stats.tributaries++;
    if (river->point_count < 18) stats.overly_short_rivers++;
    for (i = 0; i < river->point_count - 1; i++) {
        const SnapshotTile *a = tile_at(snapshot, river->points[i].x, river->points[i].y);
        const SnapshotTile *b = tile_at(snapshot, river->points[i + 1].x,
                                        river->points[i + 1].y);
        if (a && b && !(river->points[i].semantic_flags & SNAPSHOT_RIVER_LAKE) &&
            !(river->points[i + 1].semantic_flags & SNAPSHOT_RIVER_LAKE) &&
            b->elevation > a->elevation + 4) stats.invalid_uphill_segments++;
    }
    if (in_map(snapshot, end_x, end_y) &&
        is_land((Geography)tile_at(snapshot, end_x, end_y)->geography) &&
        (point_hits[end_y][end_x] & 0x7f) <= 1 &&
        !(river->end_flags & (SNAPSHOT_RIVER_MOUTH | SNAPSHOT_RIVER_DELTA))) {
        stats.inland_dead_ends++;
    }
}

static void river_geometry_rebuild_now(const RenderSnapshot *snapshot) {
    int i, x, y;
    int previous_cache_count = stats.cache_rebuild_count;
    int previous_cache_ms = stats.cache_rebuild_ms;
    int previous_geometry_count = stats.geometry_rebuild_count;
    DWORD start = GetTickCount();
    memset(&stats, 0, sizeof(stats));
    stats.cache_rebuild_count = previous_cache_count;
    stats.cache_rebuild_ms = previous_cache_ms;
    stats.geometry_rebuild_count = previous_geometry_count + 1;
    render_path_count = 0;
    if (snapshot && snapshot->rivers.valid) {
        if (!river_path_count_valid(snapshot->rivers.path_count,
                                    snapshot->map_w, snapshot->map_h) ||
            snapshot->rivers.capacity != snapshot->rivers.path_count ||
            (snapshot->rivers.path_count > 0 && !snapshot->rivers.paths) ||
            (snapshot->rivers.path_count == 0 && snapshot->rivers.paths)) {
            river_lod_policy_release();
            river_topology_release();
            geometry_valid = 0;
            return;
        }
        if (!resize_render_paths(snapshot->rivers.path_count,
                                 snapshot->map_w, snapshot->map_h)) {
            river_lod_policy_release();
            river_topology_release();
            geometry_valid = 0;
            return;
        }
        collect_point_hits(snapshot);
        for (i = 0; i < snapshot->rivers.path_count; i++) {
            const SnapshotRiverPath *river = &snapshot->rivers.paths[i];
            RiverGeometryCurveStats curve_stats;
            if (river->point_count < 2) continue;
            memset(&curve_stats, 0, sizeof(curve_stats));
            collect_stats_for_river(snapshot, river);
            build_render_path(snapshot, river, &render_paths[render_path_count],
                              &curve_stats);
            if (render_paths[render_path_count].active) {
                merge_curve_stats(&curve_stats);
                render_path_count++;
            }
        }
        for (y = 0; y < snapshot->map_h; y++) {
            for (x = 0; x < snapshot->map_w; x++) {
                if (point_hits[y][x] & 0x80) stats.confluences++;
            }
        }
        if (render_path_capacity != render_path_count &&
            !resize_render_paths(render_path_count,
                                 snapshot->map_w, snapshot->map_h)) {
            render_path_count = 0;
            river_lod_policy_release();
            river_topology_release();
            geometry_valid = 0;
            return;
        }
    } else if (snapshot) resize_render_paths(0, snapshot->map_w, snapshot->map_h);
    if (stats.river_count > 0) stats.average_length /= stats.river_count;
    if (render_path_count > 0 &&
        !river_topology_rebuild(snapshot, render_paths, render_path_count,
                                 snapshot ? snapshot->rivers.revision : 0)) {
        river_lod_policy_release();
        geometry_valid = 0;
        return;
    }
    if (render_path_count == 0) {
        river_lod_policy_release();
        river_topology_release();
    }
    stats.geometry_rebuild_ms = (int)(GetTickCount() - start);
    last_geometry_revision = snapshot ? snapshot->rivers.revision : 0;
    last_map_w = snapshot ? snapshot->map_w : 0;
    last_map_h = snapshot ? snapshot->map_h : 0;
    last_river_path_count = snapshot ? snapshot->rivers.path_count : 0;
    geometry_valid = 1;
}

void river_geometry_rebuild(const RenderSnapshot *snapshot) {
    geometry_valid = 0;
    river_geometry_rebuild_now(snapshot);
}

static int geometry_matches_snapshot(const RenderSnapshot *snapshot) {
    return geometry_valid && snapshot && snapshot->rivers.valid &&
           snapshot->rivers.revision == last_geometry_revision &&
           snapshot->map_w == last_map_w && snapshot->map_h == last_map_h &&
           snapshot->rivers.path_count == last_river_path_count;
}

int river_geometry_rebuild_if_needed(const RenderSnapshot *snapshot) {
    if (geometry_matches_snapshot(snapshot)) return 0;
    river_geometry_rebuild_now(snapshot);
    return 1;
}

int river_geometry_prepare(const RenderSnapshot *snapshot) {
    river_geometry_rebuild_if_needed(snapshot);
    return geometry_matches_snapshot(snapshot);
}

void river_geometry_release(void) {
    free(render_paths);
    render_paths = NULL;
    render_path_count = 0;
    render_path_capacity = 0;
    geometry_valid = 0;
    last_geometry_revision = 0;
    last_map_w = last_map_h = last_river_path_count = 0;
    river_lod_policy_release();
    river_topology_release();
}

size_t river_geometry_retained_bytes(void) {
    return (size_t)render_path_capacity * sizeof(*render_paths);
}

const RiverRenderPath *river_geometry_paths(int *count) {
    if (count) *count = render_path_count;
    return render_paths;
}

const HydrologyRenderStats *river_geometry_stats(void) { return &stats; }

void river_geometry_note_cache_rebuild(int ms) {
    stats.cache_rebuild_count++;
    stats.cache_rebuild_ms = ms;
}

void river_geometry_note_lod_counts(int visible, int skipped) {
    stats.visible_river_count_last_draw = visible;
    stats.skipped_by_lod_last_draw = skipped;
}

void river_geometry_note_lod_policy(int target, int stems, int connected_paths) {
    stats.lod_target_last_draw = target;
    stats.lod_stem_count_last_draw = stems;
    stats.lod_connected_path_count_last_draw = connected_paths;
}
