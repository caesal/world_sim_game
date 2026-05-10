#include "render/sea_lane_render.h"

#include "core/game_state.h"
#include "render/render_common.h"
#include "sim/sea_lanes.h"
#include "sim/sea_nav.h"

#include <stdlib.h>
#include <string.h>

#define SEA_LANE_SCREEN_POINTS (MAX_SEA_LANE_POINTS * 2)

static POINT tile_point(MapPoint tile, MapLayout layout) {
    POINT point;
    point.x = layout.map_x + (tile.x * 2 + 1) * layout.draw_w / (MAP_W * 2);
    point.y = layout.map_y + (tile.y * 2 + 1) * layout.draw_h / (MAP_H * 2);
    return point;
}

static POINT port_point(int city_id, MapLayout layout) {
    City *city = &cities[city_id];
    POINT point;
    point.x = layout.map_x + (city->port_x * 2 + 1) * layout.draw_w / (MAP_W * 2);
    point.y = layout.map_y + (city->port_y * 2 + 1) * layout.draw_h / (MAP_H * 2);
    return point;
}

static int screen_to_tile(POINT point, MapLayout layout, int *tx, int *ty) {
    if (point.x < layout.map_x || point.y < layout.map_y ||
        point.x >= layout.map_x + layout.draw_w || point.y >= layout.map_y + layout.draw_h) return 0;
    *tx = (point.x - layout.map_x) * MAP_W / layout.draw_w;
    *ty = (point.y - layout.map_y) * MAP_H / layout.draw_h;
    return *tx >= 0 && *tx < MAP_W && *ty >= 0 && *ty < MAP_H;
}

static int segment_crosses_land(POINT a, POINT b, MapLayout layout, int allow_port_land) {
    int dx = b.x - a.x;
    int dy = b.y - a.y;
    int steps = max(abs(dx), abs(dy)) / 5 + 1;
    int i;
    for (i = 0; i <= steps; i++) {
        POINT p = {a.x + dx * i / steps, a.y + dy * i / steps};
        int tx, ty;
        if (!screen_to_tile(p, layout, &tx, &ty)) return 1;
        if (sea_nav_is_water(tx, ty)) continue;
        if (allow_port_land && (i == 0 || i == steps)) continue;
        return 1;
    }
    return 0;
}

static int path_crosses_land(const POINT *points, int count, MapLayout layout) {
    int i;
    for (i = 1; i < count; i++) {
        if (segment_crosses_land(points[i - 1], points[i], layout, 0)) return 1;
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

static int lane_screen_points(const SeaLane *lane, MapLayout layout, POINT *out) {
    POINT raw[SEA_LANE_SCREEN_POINTS];
    POINT smooth[SEA_LANE_SCREEN_POINTS];
    int i;
    int count = min(lane->point_count, SEA_LANE_SCREEN_POINTS);
    int smooth_count;
    for (i = 0; i < count; i++) raw[i] = tile_point(lane->points[i], layout);
    smooth_count = smooth_points(raw, count, smooth, SEA_LANE_SCREEN_POINTS);
    if (smooth_count >= 2 && !path_crosses_land(smooth, smooth_count, layout)) {
        memcpy(out, smooth, (size_t)smooth_count * sizeof(out[0]));
        return smooth_count;
    }
    memcpy(out, raw, (size_t)count * sizeof(out[0]));
    return path_crosses_land(out, count, layout) ? 0 : count;
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

static int valid_port_city(int city_id) {
    City *city;
    if (city_id < 0 || city_id >= city_count) return 0;
    city = &cities[city_id];
    return city->alive && city->port && city->port_x >= 0 && city->port_y >= 0;
}

static void draw_harbor_connector(HDC hdc, int city_id, MapPoint sea_entry, MapLayout layout) {
    POINT port;
    POINT sea;
    int tile_dist;
    if (!valid_port_city(city_id)) return;
    port = port_point(city_id, layout);
    sea = tile_point(sea_entry, layout);
    tile_dist = abs(cities[city_id].port_x - sea_entry.x) + abs(cities[city_id].port_y - sea_entry.y);
    if (tile_dist <= 3 && !segment_crosses_land(port, sea, layout, 1)) {
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

static void draw_lane_branches(HDC hdc, const SeaLane *lane, MapLayout layout) {
    if (!lane->active || lane->point_count < 2) return;
    draw_harbor_connector(hdc, lane->from_city, lane->from_sea_entry, layout);
    draw_harbor_connector(hdc, lane->to_city, lane->to_sea_entry, layout);
}

void draw_sea_lanes(HDC hdc, RECT client, MapLayout layout) {
    const SeaLane *lanes;
    POINT points[SEA_LANE_SCREEN_POINTS];
    int lane_count;
    int saved;
    int i;
    if (layout.tile_size < 1) return;
    lanes = sea_lanes_get(&lane_count);
    saved = SaveDC(hdc);
    IntersectClipRect(hdc, client.left, TOP_BAR_H, client.right - side_panel_w, client.bottom - BOTTOM_BAR_H);
    SetBkMode(hdc, TRANSPARENT);
    for (i = 0; i < lane_count; i++) {
        int count = lane_screen_points(&lanes[i], layout, points);
        int dash = lanes[i].type == SEA_LANE_DEEP ? 30 : 22;
        int gap = lanes[i].type == SEA_LANE_DEEP ? 18 : 12;
        if (count < 2) continue;
        draw_lane_stroke(hdc, points, count, RGB(122, 108, 78), max(3, layout.tile_size / 3), dash, gap);
    }
    for (i = 0; i < lane_count; i++) {
        int count = lane_screen_points(&lanes[i], layout, points);
        int deep = lanes[i].type == SEA_LANE_DEEP;
        int dash = deep ? 30 : 22;
        int gap = deep ? 18 : 12;
        int exposure;
        HPEN pen;
        HPEN old_pen;
        if (count < 2) continue;
        draw_lane_stroke(hdc, points, count, deep ? RGB(228, 196, 132) : RGB(214, 184, 118),
                         max(2, layout.tile_size / 4), dash, gap);
        exposure = sea_lanes_exposure(i);
        if (exposure > 0) {
            COLORREF color = exposure > 24 ? RGB(18, 78, 48) : RGB(40, 105, 66);
            draw_lane_stroke(hdc, points, count, color, max(2, layout.tile_size / 5), dash, gap);
        }
        pen = CreatePen(PS_SOLID, 1, deep ? RGB(228, 196, 132) : RGB(214, 184, 118));
        old_pen = SelectObject(hdc, pen);
        draw_lane_branches(hdc, &lanes[i], layout);
        SelectObject(hdc, old_pen);
        DeleteObject(pen);
    }
    RestoreDC(hdc, saved);
}
