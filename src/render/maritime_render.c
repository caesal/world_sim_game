#include "render_map_internal.h"

#define MAX_ROUTE_SCREEN_POINTS (MAX_MARITIME_ROUTE_POINTS * 2)

static void route_land_push(MapPoint tile, int *out_x, int *out_y) {
    int radius = 5;
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
    screen.x += push_x * clamp(layout.tile_size * edge_factor, 4, 28) / (max_push * 3);
    screen.y += push_y * clamp(layout.tile_size * edge_factor, 4, 28) / (max_push * 3);
    return screen;
}

static int build_route_screen_points(const MaritimeRoute *route, MapLayout layout, POINT *points) {
    POINT raw[MAX_MARITIME_ROUTE_POINTS];
    int count = 0;
    int i;

    if (!route->active || route->point_count < 2) return 0;
    for (i = 0; i < route->point_count; i++) raw[i] = maritime_screen_point(route, i, layout);
    points[count++] = raw[0];
    for (i = 1; i < route->point_count && count + 2 < MAX_ROUTE_SCREEN_POINTS; i++) {
        POINT q;
        POINT r;
        q.x = (raw[i - 1].x * 3 + raw[i].x) / 4;
        q.y = (raw[i - 1].y * 3 + raw[i].y) / 4;
        r.x = (raw[i - 1].x + raw[i].x * 3) / 4;
        r.y = (raw[i - 1].y + raw[i].y * 3) / 4;
        points[count++] = q;
        points[count++] = r;
    }
    points[count++] = raw[route->point_count - 1];
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
        draw_route_stroke(hdc, &maritime_routes[i], layout, outline,
                          exposure > 18 ? RGB(8, 48, 31) : RGB(116, 102, 77));
    }
    for (i = 0; i < maritime_route_count; i++) {
        int inner = layout.tile_size >= 4 ? 2 : 1;
        int exposure = plague_visual_route_intensity(i);
        draw_route_stroke(hdc, &maritime_routes[i], layout, inner,
                          exposure > 18 ? RGB(20, 92, 54) : RGB(220, 194, 139));
    }
    RestoreDC(hdc, saved_dc);
}
