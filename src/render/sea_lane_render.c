#include "render/sea_lane_render.h"
#include "render/plague_visual.h"
#include "core/plague_perf.h"
#include "render/render_common.h"
#include "render/render_context.h"
#include "render/sea_lane_dash_cache.h"
#include "sim/route_potential.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SEA_LANE_SCREEN_POINTS (MAX_SEA_LANE_POINTS * 2)
#define SHALLOW_LANE_DASH_UNITS 50
#define SHALLOW_LANE_GAP_UNITS 30
#define DEEP_LANE_DASH_UNITS 50
#define DEEP_LANE_GAP_UNITS 30
#define SHALLOW_LANE_WIDTH 2
#define SHALLOW_LANE_OUTLINE_WIDTH 3
#define DEEP_LANE_WIDTH 3
#define DEEP_LANE_HALO_WIDTH 4
typedef struct {
    int valid;
    unsigned int key;
    int count;
    MapPoint map_points[SEA_LANE_SCREEN_POINTS];
    POINT screen_points[SEA_LANE_SCREEN_POINTS];
} CachedLanePath;
static CachedLanePath lane_path_cache[MAX_SEA_LANES];
static int lane_cache_hits, lane_cache_misses, lane_last_render_ms, lane_dash_segments;
static int lane_visible_routes, lane_visible_shallow_routes, lane_visible_deep_routes, lane_infected_routes, lane_infected_draw_ms;
static int lane_miss_initial, lane_miss_route, lane_miss_other;
static const char *lane_last_reason = "none";
static unsigned int mix_key(unsigned int key, int value) {
    return key * 1000003u ^ (unsigned int)value;
}
static unsigned int mix_point_key(unsigned int key, MapPoint point) { return mix_key(mix_key(key, point.x), point.y); }
static unsigned int stable_route_key(int type, int from_region, int to_region, MapPoint from_port,
                                     MapPoint to_port, const MapPoint *points, int count, int map_w, int map_h) {
    int reversed = from_region > to_region ||
                   (from_region == to_region && (from_port.x > to_port.x ||
                    (from_port.x == to_port.x && from_port.y > to_port.y)));
    int a_region = reversed ? to_region : from_region, b_region = reversed ? from_region : to_region;
    MapPoint a_port = reversed ? to_port : from_port, b_port = reversed ? from_port : to_port;
    unsigned int key = 2166136261u; int i;
    key = mix_key(mix_key(mix_key(key, type), map_w), map_h);
    key = mix_key(mix_key(key, a_region), b_region); key = mix_point_key(mix_point_key(key, a_port), b_port);
    key = mix_key(key, count);
    for (i = 0; points && count > 0 && i < 6 && i < count; i++) {
        int idx = count == 1 ? 0 : i * (count - 1) / 5;
        key = mix_point_key(key, points[reversed ? count - 1 - idx : idx]);
    }
    return key;
}
static unsigned int lane_layout_key(const RenderSnapshot *snapshot, const SnapshotSeaLane *lane,
                                    int lane_index, int deep, MapLayout layout) {
    (void)layout;
    (void)lane_index;
    return stable_route_key(lane ? lane->type : deep,
                            lane ? lane->from_region : -1, lane ? lane->to_region : -1,
                            lane ? lane->from_port : (MapPoint){-1, -1},
                            lane ? lane->to_port : (MapPoint){-1, -1},
                            lane ? lane->points : NULL, lane ? lane->point_count : 0,
                            snapshot ? snapshot->map_w : 0, snapshot ? snapshot->map_h : 0);
}
static POINT tile_point(const RenderSnapshot *snapshot, MapPoint tile, MapLayout layout) {
    POINT point;
    point.x = layout.map_x + (tile.x * 2 + 1) * layout.draw_w / (max(1, snapshot->map_w) * 2);
    point.y = layout.map_y + (tile.y * 2 + 1) * layout.draw_h / (max(1, snapshot->map_h) * 2);
    return point;
}
static int screen_to_tile(const RenderSnapshot *snapshot, POINT point, MapLayout layout, int *tx, int *ty) {
    if (point.x < layout.map_x || point.y < layout.map_y ||
        point.x >= layout.map_x + layout.draw_w || point.y >= layout.map_y + layout.draw_h) return 0;
    *tx = (point.x - layout.map_x) * snapshot->map_w / layout.draw_w;
    *ty = (point.y - layout.map_y) * snapshot->map_h / layout.draw_h;
    return *tx >= 0 && *tx < snapshot->map_w && *ty >= 0 && *ty < snapshot->map_h;
}
static int snapshot_water_at(const RenderSnapshot *snapshot, int x, int y) {
    const SnapshotTile *tile = render_snapshot_tile_at(snapshot, x, y);
    return tile && !is_land((Geography)tile->geography);
}
static int segment_crosses_land(const RenderSnapshot *snapshot, POINT a, POINT b,
                                MapLayout layout, int allow_port_land) {
    int dx = b.x - a.x;
    int dy = b.y - a.y;
    int steps = max(abs(dx), abs(dy)) / 5 + 1;
    int i;
    for (i = 0; i <= steps; i++) {
        POINT p = {a.x + dx * i / steps, a.y + dy * i / steps};
        int tx, ty;
        if (!screen_to_tile(snapshot, p, layout, &tx, &ty)) return 1;
        if (snapshot_water_at(snapshot, tx, ty)) continue;
        if (allow_port_land && (i == 0 || i == steps)) continue;
        return 1;
    }
    return 0;
}
static int path_crosses_land(const RenderSnapshot *snapshot, const POINT *points, int count, MapLayout layout) {
    int i;
    for (i = 1; i < count; i++) {
        if (segment_crosses_land(snapshot, points[i - 1], points[i], layout, 0)) return 1;
    }
    return 0;
}
static int screen_path_visible(const POINT *points, int count, RECT content, int pad) {
    RECT bounds;
    int i;
    if (!points || count <= 0) return 0;
    bounds.left = bounds.right = points[0].x;
    bounds.top = bounds.bottom = points[0].y;
    for (i = 1; i < count; i++) {
        if (points[i].x < bounds.left) bounds.left = points[i].x;
        if (points[i].x > bounds.right) bounds.right = points[i].x;
        if (points[i].y < bounds.top) bounds.top = points[i].y;
        if (points[i].y > bounds.bottom) bounds.bottom = points[i].y;
    }
    bounds.left -= pad;
    bounds.top -= pad;
    bounds.right += pad;
    bounds.bottom += pad;
    return bounds.right >= content.left && bounds.left <= content.right &&
           bounds.bottom >= content.top && bounds.top <= content.bottom;
}
static int append_map_point(MapPoint *out, int *count, int max_count, MapPoint p) {
    if (*count > 0 && out[*count - 1].x == p.x && out[*count - 1].y == p.y) return 1;
    if (*count >= max_count) return 0;
    out[(*count)++] = p;
    return 1;
}
static int densify_map_path(const MapPoint *src, int src_count, int is_deep, MapPoint *out, int max_count) {
    int count = 0;
    int allow_dense = src_count * 3 < max_count;
    int step_tiles = is_deep ? 5 : 3;
    if (src_count < 2 || max_count < 2) return 0;
    append_map_point(out, &count, max_count, src[0]);
    for (int i = 1; i < src_count; i++) {
        int dx = src[i].x - src[i - 1].x;
        int dy = src[i].y - src[i - 1].y;
        int span = max(abs(dx), abs(dy));
        int steps = allow_dense ? max(1, (span + step_tiles - 1) / step_tiles) : 1;
        for (int s = 1; s <= steps; s++) {
            MapPoint p = {src[i - 1].x + dx * s / steps, src[i - 1].y + dy * s / steps};
            if (!append_map_point(out, &count, max_count, p)) {
                out[max_count - 1] = src[src_count - 1];
                return max_count;
            }
        }
    }
    return count;
}
static void smooth_screen_path(const POINT *src, POINT *out, int count) {
    if (count <= 0) return;
    out[0] = src[0];
    for (int i = 1; i < count - 1; i++) {
        out[i].x = (src[i - 1].x + src[i].x * 2 + src[i + 1].x) / 4;
        out[i].y = (src[i - 1].y + src[i].y * 2 + src[i + 1].y) / 4;
    }
    if (count > 1) out[count - 1] = src[count - 1];
}
static void refresh_screen_points(const RenderSnapshot *snapshot, CachedLanePath *cache, MapLayout layout) {
    POINT raw[SEA_LANE_SCREEN_POINTS];
    int i;
    if (!cache || cache->count < 2) return;
    for (i = 0; i < cache->count; i++) raw[i] = tile_point(snapshot, cache->map_points[i], layout);
    smooth_screen_path(raw, cache->screen_points, cache->count);
}
static int map_path_render_points(const RenderSnapshot *snapshot, const MapPoint *src_map,
                                  int src_count, int is_deep, MapLayout layout, MapPoint *out_map,
                                  POINT *out_screen, int max_count) {
    POINT raw[SEA_LANE_SCREEN_POINTS];
    int count = densify_map_path(src_map, src_count, is_deep, out_map, max_count);
    if (count < 2) return 0;
    for (int i = 0; i < count; i++) raw[i] = tile_point(snapshot, out_map[i], layout);
    if (path_crosses_land(snapshot, raw, count, layout)) return 0;
    smooth_screen_path(raw, out_screen, count);
    if (path_crosses_land(snapshot, out_screen, count, layout)) memcpy(out_screen, raw, (size_t)count * sizeof(raw[0]));
    return count;
}
static const CachedLanePath *cached_lane_path(const RenderSnapshot *snapshot,
                                              const SnapshotSeaLane *lane, int lane_index,
                                              int deep, MapLayout layout) {
    CachedLanePath *cache;
    unsigned int key;
    if (!lane || lane_index < 0 || lane_index >= MAX_SEA_LANES) return NULL;
    cache = &lane_path_cache[lane_index];
    key = lane_layout_key(snapshot, lane, lane_index, deep, layout);
    if (cache->valid && cache->key == key) {
        lane_cache_hits++;
        refresh_screen_points(snapshot, cache, layout);
        return cache->count >= 2 ? cache : NULL;
    }
    if (!cache->valid) { lane_miss_initial++; lane_last_reason = "initial"; }
    else if ((cache->key ^ key) & 0xffff0000u) { lane_miss_route++; lane_last_reason = "route"; }
    else { lane_miss_other++; lane_last_reason = "style"; }
    cache->count = map_path_render_points(snapshot, lane->points, lane->point_count,
                                          deep, layout, cache->map_points,
                                          cache->screen_points, SEA_LANE_SCREEN_POINTS);
    cache->key = key;
    cache->valid = 1;
    lane_cache_misses++;
    return cache->count >= 2 ? cache : NULL;
}
static void draw_harbor_connector(HDC hdc, const RenderSnapshot *snapshot, MapPoint port_tile,
                                  MapPoint sea_entry, MapLayout layout) {
    POINT port;
    POINT sea;
    int tile_dist;
    if (port_tile.x < 0 || port_tile.y < 0) return;
    port = tile_point(snapshot, port_tile, layout);
    sea = tile_point(snapshot, sea_entry, layout);
    tile_dist = abs(port_tile.x - sea_entry.x) + abs(port_tile.y - sea_entry.y);
    if (tile_dist <= 3 && !segment_crosses_land(snapshot, port, sea, layout, 1)) {
        MoveToEx(hdc, port.x, port.y, NULL);
        LineTo(hdc, sea.x, sea.y);
    }
}
static unsigned int dash_route_key(unsigned int route_key, int style) {
    return mix_key(route_key, style);
}
static void draw_lane_stroke(HDC hdc, int cache_id, unsigned int route_key,
                             const MapPoint *map_points, const POINT *screen_points,
                             int count, COLORREF color, int width, int dash_units, int gap_units) {
    HPEN pen = CreatePen(PS_SOLID, width, color);
    HPEN old_pen = SelectObject(hdc, pen);
    sea_lane_dash_cache_draw(hdc, cache_id, route_key, map_points, screen_points,
                             count, dash_units, gap_units);
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
}
static void offset_points(const POINT *src, POINT *dst, int count, int shift) {
    int dx;
    int dy;
    int ox;
    int oy;
    int i;
    if (count <= 0) return;
    dx = src[count - 1].x - src[0].x;
    dy = src[count - 1].y - src[0].y;
    ox = -dy * shift / max(1, max(abs(dx), abs(dy)));
    oy = dx * shift / max(1, max(abs(dx), abs(dy)));
    if (!ox && !oy && shift) {
        ox = abs(dx) >= abs(dy) ? 0 : shift;
        oy = abs(dx) >= abs(dy) ? shift : 0;
    }
    for (i = 0; i < count; i++) {
        dst[i].x = src[i].x + ox;
        dst[i].y = src[i].y + oy;
    }
}
static int route_visual_shift(unsigned int key, int deep) {
    static const int shifts[4] = {-3, -1, 1, 3};
    return shifts[(key ^ (deep ? 0x9e37u : 0x51edu)) & 3u];
}
static void draw_lane_stroke_shifted(HDC hdc, int cache_id, unsigned int route_key,
                                     const MapPoint *map_points, const POINT *screen_points,
                                     int count, COLORREF color, int width,
                                     int dash_units, int gap_units, int shift) {
    POINT shifted[SEA_LANE_SCREEN_POINTS];
    if (shift) {
        offset_points(screen_points, shifted, count, shift);
        screen_points = shifted;
    }
    draw_lane_stroke(hdc, cache_id, route_key, map_points, screen_points,
                     count, color, width, dash_units, gap_units);
}
static void draw_lane_infection_overlay(HDC hdc, const MapPoint *map_points,
                                        const POINT *screen_points, int count,
                                        int cache_id, unsigned int route_key, int deep,
                                        int exposure, int dash_units, int gap_units) {
    POINT shifted[SEA_LANE_SCREEN_POINTS];
    COLORREF color;
    int width;
    int shift;

    if (exposure <= 0 || count < 2) return;
    color = deep ? RGB(16, 74, 45) : RGB(28, 92, 58);
    width = deep && exposure >= 35 ? 2 : 1;
    shift = deep ? 3 : 2;
    offset_points(screen_points, shifted, count, shift);
    draw_lane_stroke(hdc, cache_id, route_key, map_points, shifted, count,
                     color, width, dash_units, gap_units);
    offset_points(screen_points, shifted, count, -shift);
    draw_lane_stroke(hdc, cache_id, route_key, map_points, shifted, count,
                     color, width, dash_units, gap_units);
}
static void draw_lane_branches(HDC hdc, const RenderSnapshot *snapshot,
                               const SnapshotSeaLane *lane, MapLayout layout) {
    if (!lane->active || lane->point_count < 2) return;
    draw_harbor_connector(hdc, snapshot, lane->from_port, lane->from_sea_entry, layout);
    draw_harbor_connector(hdc, snapshot, lane->to_port, lane->to_sea_entry, layout);
}
static COLORREF color32_to_ref(Color32 color) {
    return RGB((int)(color & 0xff), (int)((color >> 8) & 0xff), (int)((color >> 16) & 0xff));
}
static COLORREF route_node_color(const RenderSnapshot *snapshot, int region_id) {
    const SnapshotRegion *region;
    int owner;
    if (!snapshot || region_id < 0 || region_id >= snapshot->region_count) return RGB(130, 138, 144);
    region = &snapshot->regions[region_id];
    owner = region->owner;
    if (owner >= 0 && owner < snapshot->civ_count && snapshot->civs[owner].alive) {
        return color32_to_ref(snapshot->civs[owner].color);
    }
    return RGB(132, 140, 146);
}
static unsigned int potential_edge_key(const RenderSnapshot *snapshot, const RoutePotentialEdge *edge) {
    MapPoint from = edge && edge->point_count > 0 ? edge->points[0] : (MapPoint){-1, -1};
    MapPoint to = edge && edge->point_count > 0 ? edge->points[edge->point_count - 1] : (MapPoint){-1, -1};
    return stable_route_key(edge ? edge->type : 0,
                            edge ? edge->from_region : -1, edge ? edge->to_region : -1,
                            from, to, edge ? edge->points : NULL, edge ? edge->point_count : 0,
                            snapshot ? snapshot->map_w : 0, snapshot ? snapshot->map_h : 0);
}
static void draw_route_potential_overlay(HDC hdc, const RenderSnapshot *snapshot,
                                         MapLayout layout, RECT content) {
    const RoutePotentialEdge *potential_edges;
    const RoutePortNode *potential_nodes;
    int edge_count;
    int node_count;
    MapPoint map_points[SEA_LANE_SCREEN_POINTS];
    POINT points[SEA_LANE_SCREEN_POINTS];
    int i;

    potential_edges = route_potential_edges(&edge_count);
    potential_nodes = route_potential_nodes(&node_count);
    for (int pass = 0; pass < 2; pass++) {
        for (i = 0; i < edge_count; i++) {
            const RoutePotentialEdge *edge = &potential_edges[i];
            COLORREF color;
            COLORREF outline;
            int width;
            int dash;
            int gap;
            int count;
            int visual_shift;
            unsigned int key;
            if (!edge->active || edge->point_count < 2) continue;
            if ((pass == 1) != (edge->type == ROUTE_POTENTIAL_DEEP)) continue;
            count = map_path_render_points(snapshot, edge->points, edge->point_count,
                                           edge->type == ROUTE_POTENTIAL_DEEP,
                                           layout, map_points, points, SEA_LANE_SCREEN_POINTS);
            if (count < 2) continue;
            if (!screen_path_visible(points, count, content, 24)) continue;
            if (edge->type == ROUTE_POTENTIAL_DEEP) {
                outline = RGB(92, 96, 102);
                color = RGB(70, 74, 78);
                width = DEEP_LANE_WIDTH;
                dash = DEEP_LANE_DASH_UNITS;
                gap = DEEP_LANE_GAP_UNITS;
            } else {
                outline = RGB(145, 140, 118);
                color = RGB(240, 238, 218);
                width = SHALLOW_LANE_WIDTH;
                dash = SHALLOW_LANE_DASH_UNITS;
                gap = SHALLOW_LANE_GAP_UNITS;
            }
            key = potential_edge_key(snapshot, edge);
            visual_shift = route_visual_shift(key, edge->type == ROUTE_POTENTIAL_DEEP);
            draw_lane_stroke_shifted(hdc, SEA_LANE_DASH_CACHE_POTENTIAL_BASE + i,
                                     key, map_points, points, count, outline,
                                     edge->type == ROUTE_POTENTIAL_DEEP ? DEEP_LANE_HALO_WIDTH : SHALLOW_LANE_OUTLINE_WIDTH,
                                     dash, gap, visual_shift);
            draw_lane_stroke_shifted(hdc, SEA_LANE_DASH_CACHE_POTENTIAL_BASE + i,
                                     key, map_points, points, count, color, width, dash, gap,
                                     visual_shift);
        }
    }
    for (i = 0; i < node_count; i++) {
        MapPoint port = {potential_nodes[i].port_x, potential_nodes[i].port_y};
        POINT point = tile_point(snapshot, port, layout);
        int r = max(3, layout.tile_size / 3);
        RECT mark = {point.x - r, point.y - r, point.x + r, point.y + r};
        int owner = potential_nodes[i].region_id >= 0 && potential_nodes[i].region_id < snapshot->region_count ?
                    snapshot->regions[potential_nodes[i].region_id].owner : -1;
        HBRUSH brush = CreateSolidBrush(route_node_color(snapshot, potential_nodes[i].region_id));
        HBRUSH old_brush = SelectObject(hdc, brush);
        HPEN pen = CreatePen(PS_SOLID, 1, owner == selected_civ ?
                             RGB(255, 244, 190) : RGB(28, 34, 38));
        HPEN old_pen = SelectObject(hdc, pen);
        Ellipse(hdc, mark.left, mark.top, mark.right, mark.bottom);
        SelectObject(hdc, old_pen);
        SelectObject(hdc, old_brush);
        DeleteObject(pen);
        DeleteObject(brush);
    }
}
void draw_sea_lanes(HDC hdc, RECT client, MapLayout layout) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    DWORD start = GetTickCount();
    RECT content;
    int saved;
    int i;
    lane_dash_segments = 0;
    lane_visible_routes = lane_visible_shallow_routes = lane_visible_deep_routes = 0;
    lane_infected_routes = 0;
    lane_infected_draw_ms = 0;
    sea_lane_dash_cache_begin_frame();
    if (layout.tile_size < 1) return;
    if (!snapshot || !snapshot->world_generated) return;
    saved = SaveDC(hdc);
    content = get_map_content_rect(client);
    IntersectClipRect(hdc, content.left, content.top, content.right, content.bottom);
    SetBkMode(hdc, TRANSPARENT);
    if (display_mode == DISPLAY_ROUTE_POTENTIAL) {
        draw_route_potential_overlay(hdc, snapshot, layout, content);
        RestoreDC(hdc, saved);
        lane_dash_segments = sea_lane_dash_cache_segments_drawn();
        lane_last_render_ms = (int)(GetTickCount() - start);
        return;
    }
    if (snapshot->lane_count <= 0) {
        RestoreDC(hdc, saved);
        lane_last_render_ms = (int)(GetTickCount() - start);
        return;
    }
    for (int pass = 0; pass < 2; pass++) {
        for (i = 0; i < snapshot->lane_count; i++) {
            const SnapshotSeaLane *lane = &snapshot->lanes[i];
            int deep = lane->type == SEA_LANE_DEEP;
            int dash;
            int gap;
            int width;
            COLORREF outline;
            COLORREF inner;
            const CachedLanePath *path;
            unsigned int key;
            int visual_shift;
            if ((pass == 1) != deep) continue;
            path = cached_lane_path(snapshot, lane, i, deep, layout);
            dash = deep ? DEEP_LANE_DASH_UNITS : SHALLOW_LANE_DASH_UNITS;
            gap = deep ? DEEP_LANE_GAP_UNITS : SHALLOW_LANE_GAP_UNITS;
            if (!path) continue;
            if (!screen_path_visible(path->screen_points, path->count, content, 28)) continue;
            outline = deep ? RGB(92, 96, 102) : RGB(145, 140, 118);
            inner = deep ? RGB(70, 74, 78) : RGB(240, 238, 218);
            width = deep ? DEEP_LANE_WIDTH : SHALLOW_LANE_WIDTH;
            key = dash_route_key(path->key, 1);
            visual_shift = route_visual_shift(key, deep);
            draw_lane_stroke_shifted(hdc, i, key, path->map_points, path->screen_points,
                                     path->count, outline,
                                     deep ? DEEP_LANE_HALO_WIDTH : SHALLOW_LANE_OUTLINE_WIDTH,
                                     dash, gap, visual_shift);
            draw_lane_stroke_shifted(hdc, i, key, path->map_points, path->screen_points,
                                     path->count, inner, width, dash, gap, visual_shift);
            lane_visible_routes++;
            if (deep) lane_visible_deep_routes++; else lane_visible_shallow_routes++;
        }
    }
    {
        DWORD infected_start = GetTickCount();
        for (i = 0; i < snapshot->lane_count; i++) {
            const SnapshotSeaLane *lane = &snapshot->lanes[i];
            int deep = lane->type == SEA_LANE_DEEP;
            int dash = deep ? DEEP_LANE_DASH_UNITS : SHALLOW_LANE_DASH_UNITS;
            int gap = deep ? DEEP_LANE_GAP_UNITS : SHALLOW_LANE_GAP_UNITS;
            int exposure = plague_perf_visuals_allowed() ?
                           max(lane->exposure, plague_visual_route_intensity(i) / 100) : 0;
            const CachedLanePath *path;
            if (exposure <= 0) continue;
            path = cached_lane_path(snapshot, lane, i, deep, layout);
            if (!path || !screen_path_visible(path->screen_points, path->count, content, 32)) continue;
            draw_lane_infection_overlay(hdc, path->map_points, path->screen_points, path->count,
                                        i, dash_route_key(path->key, 1),
                                        deep, exposure, dash, gap);
            lane_infected_routes++;
        }
        lane_infected_draw_ms = (int)(GetTickCount() - infected_start);
    }
    for (i = 0; i < snapshot->lane_count; i++) {
        const SnapshotSeaLane *lane = &snapshot->lanes[i];
        int deep = lane->type == SEA_LANE_DEEP;
        POINT from_point = tile_point(snapshot, lane->from_port, layout);
        POINT to_point = tile_point(snapshot, lane->to_port, layout);
        if (!screen_path_visible(&from_point, 1, content, 32) &&
            !screen_path_visible(&to_point, 1, content, 32)) continue;
        HPEN pen = CreatePen(PS_SOLID, 1, deep ? RGB(70, 74, 78) : RGB(240, 238, 218));
        HPEN old_pen = SelectObject(hdc, pen);
        draw_lane_branches(hdc, snapshot, lane, layout);
        SelectObject(hdc, old_pen);
        DeleteObject(pen);
    }
    RestoreDC(hdc, saved);
    lane_dash_segments = sea_lane_dash_cache_segments_drawn();
    lane_last_render_ms = (int)(GetTickCount() - start);
}
int sea_lane_render_cache_hits(void) { return lane_cache_hits; }
int sea_lane_render_cache_misses(void) { return lane_cache_misses; }
int sea_lane_render_last_ms(void) { return lane_last_render_ms; }
int sea_lane_render_dash_segments(void) { return lane_dash_segments; }
const char *sea_lane_render_last_reason(void) { return lane_last_reason; }
const char *sea_lane_render_reason_summary(void) { static char text[96]; snprintf(text, sizeof(text), "init %d / route %d / style %d", lane_miss_initial, lane_miss_route, lane_miss_other); return text; }
int sea_lane_render_dash_cache_hits(void) { return sea_lane_dash_cache_hits(); }
int sea_lane_render_dash_cache_misses(void) { return sea_lane_dash_cache_misses(); }
int sea_lane_render_dash_rebuild_ms(void) { return sea_lane_dash_cache_last_rebuild_ms(); }
const char *sea_lane_render_dash_reason(void) { return sea_lane_dash_cache_last_reason(); }
const char *sea_lane_render_dash_reason_summary(void) { return sea_lane_dash_cache_reason_summary(); }
int sea_lane_render_visible_routes(void) { return lane_visible_routes; }
int sea_lane_render_visible_shallow_routes(void) { return lane_visible_shallow_routes; } int sea_lane_render_visible_deep_routes(void) { return lane_visible_deep_routes; }
int sea_lane_render_infected_routes(void) { return lane_infected_routes; }
int sea_lane_render_infected_draw_ms(void) { return lane_infected_draw_ms; }
