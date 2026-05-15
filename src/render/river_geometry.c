#include "river_geometry.h"

#include "core/game_types.h"
#include "world/terrain_query.h"

#include <string.h>

static RiverRenderPath render_paths[MAX_RIVER_PATHS];
static int render_path_count;
static HydrologyRenderStats stats;

static unsigned char point_hits[MAX_MAP_H][MAX_MAP_W];

static int in_map(int x, int y) {
    return x >= 0 && y >= 0 && x < MAP_W && y < MAP_H;
}

static RiverRenderPoint center10(int x, int y) {
    RiverRenderPoint p;
    p.x10 = (short)(x * 10 + 5);
    p.y10 = (short)(y * 10 + 5);
    return p;
}

static int same_raw_point(RiverPoint a, RiverPoint b) {
    return a.x == b.x && a.y == b.y;
}

static int raw_style_flags(const RiverPath *river) {
    int mountain = 0, hill = 0, wetland = 0, desert = 0, cold = 0;
    int i;

    for (i = 0; i < river->point_count; i++) {
        int x = river->points[i].x;
        int y = river->points[i].y;
        Geography g;
        Climate c;
        if (!in_map(x, y)) continue;
        g = world[y][x].geography;
        c = world[y][x].climate;
        mountain += g == GEO_MOUNTAIN || g == GEO_PLATEAU || g == GEO_CANYON;
        hill += g == GEO_HILL;
        wetland += g == GEO_WETLAND || g == GEO_DELTA;
        desert += c == CLIMATE_DESERT || c == CLIMATE_SEMI_ARID;
        cold += c == CLIMATE_TUNDRA || c == CLIMATE_ICE_CAP || c == CLIMATE_SUBARCTIC;
    }
    if (mountain * 3 > river->point_count) return RIVER_STYLE_MOUNTAIN;
    if (wetland * 3 > river->point_count) return RIVER_STYLE_WETLAND;
    if (desert * 3 > river->point_count) return RIVER_STYLE_DESERT;
    if (cold * 3 > river->point_count) return RIVER_STYLE_COLD;
    if (hill * 3 > river->point_count) return RIVER_STYLE_HILL;
    return RIVER_STYLE_PLAIN;
}

static int append_render_point(RiverRenderPath *out, RiverRenderPoint p) {
    if (out->point_count >= MAX_RIVER_RENDER_POINTS) return 0;
    out->points[out->point_count++] = p;
    return 1;
}

static void append_smoothed_segment(RiverRenderPath *out, RiverRenderPoint a, RiverRenderPoint b) {
    RiverRenderPoint q;
    RiverRenderPoint r;
    q.x10 = (short)((a.x10 * 3 + b.x10) / 4);
    q.y10 = (short)((a.y10 * 3 + b.y10) / 4);
    r.x10 = (short)((a.x10 + b.x10 * 3) / 4);
    r.y10 = (short)((a.y10 + b.y10 * 3) / 4);
    append_render_point(out, q);
    append_render_point(out, r);
}

static void build_render_path(const RiverPath *river, RiverRenderPath *out) {
    RiverRenderPoint raw[MAX_RIVER_POINTS];
    int raw_count = 0;
    int i;

    memset(out, 0, sizeof(*out));
    if (!river->active || river->point_count < 2) return;
    out->active = 1;
    out->order = river->order;
    out->flow = river->flow;
    out->width = river->width;
    out->style_flags = raw_style_flags(river);
    for (i = 0; i < river->point_count && raw_count < MAX_RIVER_POINTS; i++) {
        RiverPoint rp = river->points[i];
        if (!in_map(rp.x, rp.y)) continue;
        if (raw_count > 0 && same_raw_point(river->points[i], river->points[i - 1])) continue;
        raw[raw_count++] = center10(rp.x, rp.y);
    }
    if (raw_count < 2) {
        out->active = 0;
        return;
    }
    append_render_point(out, raw[0]);
    for (i = 0; i < raw_count - 1; i++) append_smoothed_segment(out, raw[i], raw[i + 1]);
    append_render_point(out, raw[raw_count - 1]);
}

static void collect_point_hits(void) {
    int i, j;
    memset(point_hits, 0, sizeof(point_hits));
    for (i = 0; i < river_path_count; i++) {
        const RiverPath *river = &river_paths[i];
        if (!river->active) continue;
        for (j = 0; j < river->point_count; j++) {
            int x = river->points[j].x;
            int y = river->points[j].y;
            if (in_map(x, y) && point_hits[y][x] < 250) point_hits[y][x]++;
        }
    }
}

static void collect_stats_for_river(const RiverPath *river) {
    int i;
    int end_x;
    int end_y;

    stats.river_count++;
    stats.average_length += river->point_count;
    if (river->point_count > stats.longest_length) stats.longest_length = river->point_count;
    if (river->order >= 3) stats.main_rivers++;
    else stats.tributaries++;
    if (river->point_count < 18) stats.overly_short_rivers++;
    for (i = 0; i < river->point_count - 1; i++) {
        int ax = river->points[i].x, ay = river->points[i].y;
        int bx = river->points[i + 1].x, by = river->points[i + 1].y;
        if (in_map(ax, ay) && in_map(bx, by) && world[by][bx].elevation > world[ay][ax].elevation + 4) {
            stats.invalid_uphill_segments++;
        }
    }
    end_x = river->points[river->point_count - 1].x;
    end_y = river->points[river->point_count - 1].y;
    if (in_map(end_x, end_y) && is_land(world[end_y][end_x].geography) && point_hits[end_y][end_x] <= 1) {
        stats.inland_dead_ends++;
    }
}

void river_geometry_rebuild(void) {
    int i, x, y;
    int previous_count = stats.cache_rebuild_count;
    int previous_ms = stats.cache_rebuild_ms;

    memset(&stats, 0, sizeof(stats));
    stats.cache_rebuild_count = previous_count;
    stats.cache_rebuild_ms = previous_ms;
    render_path_count = 0;
    collect_point_hits();
    for (i = 0; i < river_path_count && render_path_count < MAX_RIVER_PATHS; i++) {
        const RiverPath *river = &river_paths[i];
        if (!river->active || river->point_count < 2) continue;
        collect_stats_for_river(river);
        build_render_path(river, &render_paths[render_path_count]);
        if (render_paths[render_path_count].active) render_path_count++;
    }
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            if (point_hits[y][x] > 1) stats.confluences++;
        }
    }
    if (stats.river_count > 0) stats.average_length /= stats.river_count;
}

const RiverRenderPath *river_geometry_paths(int *count) {
    if (count) *count = render_path_count;
    return render_paths;
}

const HydrologyRenderStats *river_geometry_stats(void) {
    return &stats;
}

void river_geometry_note_cache_rebuild(int ms) {
    stats.cache_rebuild_count++;
    stats.cache_rebuild_ms = ms;
}
