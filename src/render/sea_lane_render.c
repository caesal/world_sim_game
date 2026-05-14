#include "render/sea_lane_render.h"

#include "render/render_common.h"
#include "render/render_context.h"
#include "core/game_state.h"
#include "sim/regions.h"
#include "sim/route_potential.h"

#include <stdlib.h>
#include <string.h>

#define SEA_LANE_SCREEN_POINTS (MAX_SEA_LANE_POINTS * 2)

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

static int smooth_points(const POINT *src, int src_count, POINT *dst, int max_count) {
    int count = 0;
    int i;
    if (src_count < 2) return 0;
    dst[count++] = src[0];
    for (i = 1; i < src_count && count < max_count - 2; i++) {
        POINT q = {(src[i - 1].x * 3 + src[i].x) / 4, (src[i - 1].y * 3 + src[i].y) / 4};
        POINT r = {(src[i - 1].x + src[i].x * 3) / 4, (src[i - 1].y + src[i].y * 3) / 4};
        dst[count++] = q;
        dst[count++] = r;
    }
    dst[count - 1] = src[src_count - 1];
    return count;
}

static int lane_screen_points(const RenderSnapshot *snapshot, const SnapshotSeaLane *lane,
                              MapLayout layout, POINT *out) {
    POINT raw[SEA_LANE_SCREEN_POINTS];
    POINT smooth[SEA_LANE_SCREEN_POINTS];
    int i;
    int count = min(lane->point_count, SEA_LANE_SCREEN_POINTS);
    int smooth_count;
    for (i = 0; i < count; i++) raw[i] = tile_point(snapshot, lane->points[i], layout);
    smooth_count = smooth_points(raw, count, smooth, SEA_LANE_SCREEN_POINTS);
    if (smooth_count >= 2 && !path_crosses_land(snapshot, smooth, smooth_count, layout)) {
        memcpy(out, smooth, (size_t)smooth_count * sizeof(out[0]));
        return smooth_count;
    }
    memcpy(out, raw, (size_t)count * sizeof(out[0]));
    return path_crosses_land(snapshot, out, count, layout) ? 0 : count;
}

static void dashed_line(HDC hdc, POINT a, POINT b, int dash, int gap) {
    int dx = b.x - a.x, dy = b.y - a.y;
    int length = max(abs(dx), abs(dy));
    int pos;
    if (length <= 0) return;
    for (pos = 0; pos < length; pos += dash + gap) {
        int end = min(pos + dash, length);
        POINT p = {a.x + dx * pos / length, a.y + dy * pos / length};
        POINT q = {a.x + dx * end / length, a.y + dy * end / length};
        MoveToEx(hdc, p.x, p.y, NULL);
        LineTo(hdc, q.x, q.y);
    }
}

static void dashed_path(HDC hdc, const POINT *points, int count, int dash, int gap) {
    int i;
    for (i = 1; i < count; i++) dashed_line(hdc, points[i - 1], points[i], dash, gap);
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

static void draw_lane_stroke(HDC hdc, const POINT *points, int count, COLORREF color, int width, int dash, int gap) {
    HPEN pen = CreatePen(PS_SOLID, width, color);
    HPEN old_pen = SelectObject(hdc, pen);
    dashed_path(hdc, points, count, dash, gap);
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
    POINT points[MAX_ROUTE_POTENTIAL_POINTS];
    int i;

    potential_edges = route_potential_edges(&edge_count);
    potential_nodes = route_potential_nodes(&node_count);
    for (i = 0; i < edge_count; i++) {
        const RoutePotentialEdge *edge = &potential_edges[i];
        COLORREF color;
        int width;
        int dash;
        int gap;
        int j;
        if (!edge->active || edge->point_count < 2) continue;
        for (j = 0; j < edge->point_count && j < MAX_ROUTE_POTENTIAL_POINTS; j++) {
            points[j] = tile_point(snapshot, edge->points[j], layout);
        }
        if (edge->type == ROUTE_POTENTIAL_DEEP) {
            color = RGB(112, 116, 218);
            width = max(2, layout.tile_size / 5);
            dash = 28;
            gap = 16;
        } else {
            color = RGB(72, 190, 212);
            width = max(2, layout.tile_size / 6);
            dash = 18;
            gap = 12;
        }
        draw_lane_stroke(hdc, points, edge->point_count, color, width, dash, gap);
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
    for (i = 0; i < snapshot->lane_count; i++) {
        int count = lane_screen_points(snapshot, &snapshot->lanes[i], layout, points);
        int dash = snapshot->lanes[i].type == SEA_LANE_DEEP ? 30 : 22;
        int gap = snapshot->lanes[i].type == SEA_LANE_DEEP ? 18 : 12;
        if (count < 2) continue;
        draw_lane_stroke(hdc, points, count, RGB(122, 108, 78), max(3, layout.tile_size / 3), dash, gap);
    }
    for (i = 0; i < snapshot->lane_count; i++) {
        int count = lane_screen_points(snapshot, &snapshot->lanes[i], layout, points);
        int deep = snapshot->lanes[i].type == SEA_LANE_DEEP;
        int dash = deep ? 30 : 22;
        int gap = deep ? 18 : 12;
        int exposure;
        HPEN pen;
        HPEN old_pen;
        if (count < 2) continue;
        draw_lane_stroke(hdc, points, count, deep ? RGB(228, 196, 132) : RGB(214, 184, 118),
                         max(2, layout.tile_size / 4), dash, gap);
        exposure = snapshot->lanes[i].exposure;
        if (exposure > 0) {
            COLORREF color = exposure > 24 ? RGB(18, 78, 48) : RGB(40, 105, 66);
            draw_lane_stroke(hdc, points, count, color, max(2, layout.tile_size / 5), dash, gap);
        }
        pen = CreatePen(PS_SOLID, 1, deep ? RGB(228, 196, 132) : RGB(214, 184, 118));
        old_pen = SelectObject(hdc, pen);
        draw_lane_branches(hdc, snapshot, &snapshot->lanes[i], layout);
        SelectObject(hdc, old_pen);
        DeleteObject(pen);
    }
    RestoreDC(hdc, saved);
}
