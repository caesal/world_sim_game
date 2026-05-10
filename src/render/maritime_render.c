/*
 * Deprecated duplicate maritime route renderer.
 *
 * The active build uses src/render/route_render.c for draw_maritime_routes().
 * Keep this file out of Makefile/build.bat unless it is deliberately removed
 * or migrated; editing it will not affect the visible route layer.
 */
#include "render_map_internal.h"

#define MAX_ROUTE_SCREEN_POINTS (MAX_MARITIME_ROUTE_POINTS * 8)
#define ROUTE_SMOOTH_PASSES 3

static int route_endpoint_valid(int city_id) {
    City *city;
    if (city_id < 0 || city_id >= city_count) return 0;
    city = &cities[city_id];
    if (!city->alive || city->owner < 0 || city->owner >= civ_count || !civs[city->owner].alive) return 0;
    return city->port && city->port_x >= 0 && city->port_y >= 0;
}

static int route_duplicate_before(int route_index) {
    const MaritimeRoute *route = &maritime_routes[route_index];
    int i;

    if (!route->active) return 1;
    for (i = 0; i < route_index; i++) {
        const MaritimeRoute *other = &maritime_routes[i];
        if (!other->active) continue;
        if ((other->from_city == route->from_city && other->to_city == route->to_city) ||
            (other->from_city == route->to_city && other->to_city == route->from_city)) {
            return 1;
        }
    }
    return 0;
}

static void route_separate_harbor(int *px, int *py, int cx, int cy, int size) {
    int dx = *px - cx;
    int dy = *py - cy;
    int distance_sq = dx * dx + dy * dy;
    int min_distance = clamp(size + 10, 18, 34);

    if (distance_sq >= min_distance * min_distance) return;
    if (dx == 0 && dy == 0) {
        dx = 1;
        dy = -1;
    }
    *px += dx >= 0 ? min_distance / 2 : -min_distance / 2;
    *py += dy >= 0 ? min_distance / 2 : -min_distance / 2;
}

static POINT route_port_screen_point(int city_id, MapLayout layout) {
    City *city = &cities[city_id];
    POINT point;
    int cx = (tile_left(layout, city->x) + tile_right(layout, city->x)) / 2;
    int cy = (tile_top(layout, city->y) + tile_bottom(layout, city->y)) / 2;
    int px = (tile_left(layout, city->port_x) + tile_right(layout, city->port_x)) / 2;
    int py = (tile_top(layout, city->port_y) + tile_bottom(layout, city->port_y)) / 2;

    route_separate_harbor(&px, &py, cx, cy, clamp(layout.tile_size + 8, 16, 26));
    point.x = px;
    point.y = py;
    return point;
}

static void route_land_push(MapPoint tile, int *out_x, int *out_y) {
    int radius = 7;
    int dx;
    int dy;

    *out_x = 0;
    *out_y = 0;
    for (dy = -radius; dy <= radius; dy++) {
        for (dx = -radius; dx <= radius; dx++) {
            int x = tile.x + dx;
            int y = tile.y + dy;
            int weight;
            if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) continue;
            if (!is_land(world[y][x].geography)) continue;
            weight = radius + 1 - max(abs(dx), abs(dy));
            if (weight <= 0) continue;
            *out_x -= dx * weight;
            *out_y -= dy * weight;
        }
    }
}

static POINT maritime_screen_point(const MaritimeRoute *route, int index, MapLayout layout) {
    MapPoint tile = route->points[index];
    POINT screen;
    int push_x;
    int push_y;
    int max_push;
    int edge_factor;

    screen.x = (tile_left(layout, tile.x) + tile_right(layout, tile.x)) / 2;
    screen.y = (tile_top(layout, tile.y) + tile_bottom(layout, tile.y)) / 2;
    route_land_push(tile, &push_x, &push_y);
    max_push = max(abs(push_x), abs(push_y));
    if (max_push <= 0) return screen;
    edge_factor = min(index, route->point_count - 1 - index);
    if (edge_factor <= 0) edge_factor = 1;
    edge_factor = clamp(edge_factor, 1, 4);
    screen.x += push_x * clamp(layout.tile_size * edge_factor, 6, 42) / (max_push * 2);
    screen.y += push_y * clamp(layout.tile_size * edge_factor, 6, 42) / (max_push * 2);
    return screen;
}

static int route_add_point(POINT *points, int count, int max_count, POINT point) {
    if (count > 0 && points[count - 1].x == point.x && points[count - 1].y == point.y) return count;
    if (count >= max_count) return count;
    points[count++] = point;
    return count;
}

static int smooth_route_pass(const POINT *src, int src_count, POINT *dst, int max_count) {
    int count = 0;
    int i;

    if (src_count <= 0 || max_count <= 0) return 0;
    count = route_add_point(dst, count, max_count, src[0]);
    for (i = 1; i < src_count && count + 2 < max_count; i++) {
        POINT q;
        POINT r;
        q.x = (src[i - 1].x * 3 + src[i].x) / 4;
        q.y = (src[i - 1].y * 3 + src[i].y) / 4;
        r.x = (src[i - 1].x + src[i].x * 3) / 4;
        r.y = (src[i - 1].y + src[i].y * 3) / 4;
        count = route_add_point(dst, count, max_count, q);
        count = route_add_point(dst, count, max_count, r);
    }
    return route_add_point(dst, count, max_count, src[src_count - 1]);
}

static int simplify_route_points(const POINT *src, int src_count, POINT *dst, int max_count) {
    int count = 0;
    int i;

    if (src_count <= 0 || max_count <= 0) return 0;
    count = route_add_point(dst, count, max_count, src[0]);
    for (i = 1; i < src_count - 1 && count < max_count; i++) {
        POINT a = dst[count - 1];
        POINT b = src[i];
        POINT c = src[i + 1];
        int abx = b.x - a.x;
        int aby = b.y - a.y;
        int bcx = c.x - b.x;
        int bcy = c.y - b.y;
        int close = abs(abx) + abs(aby) < 5 && abs(bcx) + abs(bcy) < 5;
        int collinear = abs(abx * bcy - aby * bcx) < 12;
        if (close || collinear) continue;
        count = route_add_point(dst, count, max_count, b);
    }
    return route_add_point(dst, count, max_count, src[src_count - 1]);
}

static void offset_route_corridor(const MaritimeRoute *route, POINT *points,
                                  int count, MapLayout layout) {
    int base = ((route->from_city * 17 + route->to_city * 31) % 5) - 2;
    int offset = base * clamp(layout.tile_size / 3, 1, 4);
    int i;

    if (offset == 0 || count < 3) return;
    for (i = 1; i < count - 1; i++) {
        int dx = points[i + 1].x - points[i - 1].x;
        int dy = points[i + 1].y - points[i - 1].y;
        int len = max(abs(dx), abs(dy));
        if (len <= 0) continue;
        points[i].x += -dy * offset / len;
        points[i].y += dx * offset / len;
    }
}

static int build_route_screen_points(const MaritimeRoute *route, MapLayout layout, POINT *points) {
    POINT buffer_a[MAX_ROUTE_SCREEN_POINTS];
    POINT buffer_b[MAX_ROUTE_SCREEN_POINTS];
    POINT simplified[MAX_ROUTE_SCREEN_POINTS];
    const POINT *src = buffer_a;
    POINT *dst = buffer_b;
    int count = 0;
    int pass;
    int i;

    if (!route->active || route->point_count < 2 ||
        !route_endpoint_valid(route->from_city) || !route_endpoint_valid(route->to_city)) return 0;
    for (i = 0; i < route->point_count && count < MAX_ROUTE_SCREEN_POINTS; i++) {
        if (i == 0) {
            count = route_add_point(buffer_a, count, MAX_ROUTE_SCREEN_POINTS,
                                    route_port_screen_point(route->from_city, layout));
            continue;
        }
        if (i == route->point_count - 1) {
            count = route_add_point(buffer_a, count, MAX_ROUTE_SCREEN_POINTS,
                                    route_port_screen_point(route->to_city, layout));
            continue;
        }
        count = route_add_point(buffer_a, count, MAX_ROUTE_SCREEN_POINTS,
                                maritime_screen_point(route, i, layout));
    }
    count = simplify_route_points(buffer_a, count, simplified, MAX_ROUTE_SCREEN_POINTS);
    src = simplified;
    for (pass = 0; pass < ROUTE_SMOOTH_PASSES && count >= 2; pass++) {
        int next_count = smooth_route_pass(src, count, dst, MAX_ROUTE_SCREEN_POINTS);
        const POINT *next_src = dst;
        dst = (POINT *)(src == buffer_a ? buffer_a : buffer_b);
        src = next_src;
        count = next_count;
    }
    for (i = 0; i < count; i++) {
        points[i] = src[i];
    }
    offset_route_corridor(route, points, count, layout);
    return count;
}

static void draw_dashed_segment(HDC hdc, POINT a, POINT b, int dash, int gap) {
    int dx = b.x - a.x;
    int dy = b.y - a.y;
    int length = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
    int pos;

    if (length <= 0) return;
    for (pos = 0; pos < length; pos += dash + gap) {
        int end = pos + dash;
        POINT from;
        POINT to;
        if (end > length) end = length;
        from.x = a.x + dx * pos / length;
        from.y = a.y + dy * pos / length;
        to.x = a.x + dx * end / length;
        to.y = a.y + dy * end / length;
        MoveToEx(hdc, from.x, from.y, NULL);
        LineTo(hdc, to.x, to.y);
    }
}

static void draw_route_stroke(HDC hdc, const MaritimeRoute *route, MapLayout layout,
                              int width, COLORREF color) {
    HPEN pen;
    HPEN old_pen;
    POINT points[MAX_ROUTE_SCREEN_POINTS];
    int point_count;
    int i;

    point_count = build_route_screen_points(route, layout, points);
    if (point_count < 2) return;
    pen = CreatePen(PS_SOLID, width, color);
    old_pen = SelectObject(hdc, pen);
    for (i = 1; i < point_count; i++) draw_dashed_segment(hdc, points[i - 1], points[i], 18, 12);
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
}

void draw_maritime_routes(HDC hdc, RECT client, MapLayout layout) {
    int saved_dc;
    int i;

    if (maritime_route_count <= 0 || layout.tile_size < 1) return;
    saved_dc = SaveDC(hdc);
    IntersectClipRect(hdc, client.left, TOP_BAR_H, client.right - side_panel_w, client.bottom - BOTTOM_BAR_H);
    SetBkMode(hdc, TRANSPARENT);
    for (i = 0; i < maritime_route_count; i++) {
        int outline = layout.tile_size >= 4 ? 3 : 2;
        int exposure = plague_visual_route_intensity(i);
        if (route_duplicate_before(i)) continue;
        draw_route_stroke(hdc, &maritime_routes[i], layout, outline,
                          exposure > 18 ? RGB(8, 48, 31) : RGB(116, 102, 77));
    }
    for (i = 0; i < maritime_route_count; i++) {
        int inner = layout.tile_size >= 4 ? 2 : 1;
        int exposure = plague_visual_route_intensity(i);
        if (route_duplicate_before(i)) continue;
        draw_route_stroke(hdc, &maritime_routes[i], layout, inner,
                          exposure > 18 ? RGB(20, 92, 54) : RGB(220, 194, 139));
    }
    RestoreDC(hdc, saved_dc);
}
