#include "render/sea_lane_render.h"

#include "render/render_common.h"
#include "render/render_context.h"
#include "core/game_state.h"
#include "sim/regions.h"
#include "sim/route_potential.h"

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

static int world_segment_units(MapPoint a, MapPoint b) {
    int dx = abs(a.x - b.x);
    int dy = abs(a.y - b.y);
    int diagonal = min(dx, dy);
    int straight = max(dx, dy) - diagonal;
    return diagonal * 14 + straight * 10;
}

static POINT interpolate_screen_point(POINT a, POINT b, int pos, int length) {
    POINT point;
    if (length <= 0) return a;
    point.x = a.x + (b.x - a.x) * pos / length;
    point.y = a.y + (b.y - a.y) * pos / length;
    return point;
}

static int dash_phase_for_path(const MapPoint *points, int count, int period) {
    int seed;
    if (count < 2 || period <= 0) return 0;
    seed = points[0].x * 31 + points[0].y * 47 +
           points[count - 1].x * 61 + points[count - 1].y * 89;
    if (seed < 0) seed = -seed;
    return seed % period;
}

static void draw_world_dashed_path(HDC hdc, const MapPoint *map_points, const POINT *screen_points,
                                   int count, int dash_units, int gap_units, int phase_units) {
    int period = dash_units + gap_units;
    int pattern_pos;
    int i;
    if (count < 2 || dash_units <= 0 || gap_units < 0 || period <= 0) return;
    pattern_pos = phase_units % period;
    if (pattern_pos < 0) pattern_pos += period;
    for (i = 1; i < count; i++) {
        int segment_len = world_segment_units(map_points[i - 1], map_points[i]);
        int pos = 0;
        if (segment_len <= 0) continue;
        while (pos < segment_len) {
            int in_dash = pattern_pos < dash_units;
            int remain = in_dash ? dash_units - pattern_pos : period - pattern_pos;
            int take = min(remain, segment_len - pos);
            if (in_dash && take > 0) {
                POINT p = interpolate_screen_point(screen_points[i - 1], screen_points[i], pos, segment_len);
                POINT q = interpolate_screen_point(screen_points[i - 1], screen_points[i], pos + take, segment_len);
                MoveToEx(hdc, p.x, p.y, NULL);
                LineTo(hdc, q.x, q.y);
            }
            pos += take;
            pattern_pos = (pattern_pos + take) % period;
        }
    }
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

static void draw_lane_stroke(HDC hdc, const MapPoint *map_points, const POINT *screen_points,
                             int count, COLORREF color, int width, int dash_units, int gap_units) {
    int phase = dash_phase_for_path(map_points, count, dash_units + gap_units);
    HPEN pen = CreatePen(PS_SOLID, width, color);
    HPEN old_pen = SelectObject(hdc, pen);
    draw_world_dashed_path(hdc, map_points, screen_points, count, dash_units, gap_units, phase);
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
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

static COLORREF route_node_color(int region_id) {
    int owner;
    if (region_id < 0 || region_id >= region_count) return RGB(130, 138, 144);
    owner = natural_regions[region_id].owner_civ;
    if (owner >= 0 && owner < civ_count && civs[owner].alive) return color32_to_ref(civs[owner].color);
    return RGB(132, 140, 146);
}

static void draw_route_potential_overlay(HDC hdc, const RenderSnapshot *snapshot, MapLayout layout) {
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
            if (!edge->active || edge->point_count < 2) continue;
            if ((pass == 1) != (edge->type == ROUTE_POTENTIAL_DEEP)) continue;
            count = map_path_render_points(snapshot, edge->points, edge->point_count,
                                           edge->type == ROUTE_POTENTIAL_DEEP,
                                           layout, map_points, points, SEA_LANE_SCREEN_POINTS);
            if (count < 2) continue;
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
            draw_lane_stroke(hdc, map_points, points, count, outline,
                             edge->type == ROUTE_POTENTIAL_DEEP ? DEEP_LANE_HALO_WIDTH : SHALLOW_LANE_OUTLINE_WIDTH,
                             dash, gap);
            draw_lane_stroke(hdc, map_points, points, count, color, width, dash, gap);
        }
    }
    for (i = 0; i < node_count; i++) {
        MapPoint port = {potential_nodes[i].port_x, potential_nodes[i].port_y};
        POINT point = tile_point(snapshot, port, layout);
        int r = max(3, layout.tile_size / 3);
        RECT mark = {point.x - r, point.y - r, point.x + r, point.y + r};
        int owner = potential_nodes[i].region_id >= 0 && potential_nodes[i].region_id < region_count ?
                    natural_regions[potential_nodes[i].region_id].owner_civ : -1;
        HBRUSH brush = CreateSolidBrush(route_node_color(potential_nodes[i].region_id));
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
    MapPoint map_points[SEA_LANE_SCREEN_POINTS];
    POINT points[SEA_LANE_SCREEN_POINTS];
    int saved;
    int i;
    if (layout.tile_size < 1) return;
    if (!snapshot || !snapshot->world_generated) return;
    saved = SaveDC(hdc);
    IntersectClipRect(hdc, client.left, TOP_BAR_H, client.right - side_panel_w, client.bottom - BOTTOM_BAR_H);
    SetBkMode(hdc, TRANSPARENT);
    if (display_mode == DISPLAY_ROUTE_POTENTIAL) {
        draw_route_potential_overlay(hdc, snapshot, layout);
        RestoreDC(hdc, saved);
        return;
    }
    if (snapshot->lane_count <= 0) {
        RestoreDC(hdc, saved);
        return;
    }
    for (int pass = 0; pass < 2; pass++) {
        for (i = 0; i < snapshot->lane_count; i++) {
            const SnapshotSeaLane *lane = &snapshot->lanes[i];
            int deep = lane->type == SEA_LANE_DEEP;
            int count;
            int dash;
            int gap;
            int width;
            int exposure;
            COLORREF outline;
            COLORREF inner;
            if ((pass == 1) != deep) continue;
            count = map_path_render_points(snapshot, lane->points, lane->point_count,
                                           deep,
                                           layout, map_points, points, SEA_LANE_SCREEN_POINTS);
            dash = deep ? DEEP_LANE_DASH_UNITS : SHALLOW_LANE_DASH_UNITS;
            gap = deep ? DEEP_LANE_GAP_UNITS : SHALLOW_LANE_GAP_UNITS;
            if (count < 2) continue;
            outline = deep ? RGB(92, 96, 102) : RGB(145, 140, 118);
            inner = deep ? RGB(70, 74, 78) : RGB(240, 238, 218);
            width = deep ? DEEP_LANE_WIDTH : SHALLOW_LANE_WIDTH;
            draw_lane_stroke(hdc, map_points, points, count, outline,
                             deep ? DEEP_LANE_HALO_WIDTH : SHALLOW_LANE_OUTLINE_WIDTH,
                             dash, gap);
            draw_lane_stroke(hdc, map_points, points, count, inner, width, dash, gap);
            exposure = snapshot->lanes[i].exposure;
            if (exposure > 0) {
                COLORREF color = exposure > 24 ? RGB(18, 78, 48) : RGB(40, 105, 66);
                draw_lane_stroke(hdc, map_points, points, count, color,
                                 max(2, layout.tile_size / 5), dash, gap);
            }
        }
    }
    for (i = 0; i < snapshot->lane_count; i++) {
        const SnapshotSeaLane *lane = &snapshot->lanes[i];
        int deep = lane->type == SEA_LANE_DEEP;
        HPEN pen = CreatePen(PS_SOLID, 1, deep ? RGB(70, 74, 78) : RGB(240, 238, 218));
        HPEN old_pen = SelectObject(hdc, pen);
        draw_lane_branches(hdc, snapshot, lane, layout);
        SelectObject(hdc, old_pen);
        DeleteObject(pen);
    }
    RestoreDC(hdc, saved);
}
