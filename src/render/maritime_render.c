#include "render_internal.h"
#include "sim/plague.h"

static POINT maritime_screen_point(POINT tile, MapLayout layout) {
    POINT screen;

    screen.x = (tile_left(layout, tile.x) + tile_right(layout, tile.x)) / 2;
    screen.y = (tile_top(layout, tile.y) + tile_bottom(layout, tile.y)) / 2;
    return screen;
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
    int i;

    if (!route->active || route->point_count < 2) return;
    pen = CreatePen(PS_SOLID, width, color);
    old_pen = SelectObject(hdc, pen);
    for (i = 1; i < route->point_count; i++) {
        POINT a = maritime_screen_point(route->points[i - 1], layout);
        POINT b = maritime_screen_point(route->points[i], layout);
        draw_dashed_segment(hdc, a, b, 18, 12);
    }
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
        int outline = layout.tile_size >= 4 ? 4 : 2;
        int exposure = plague_route_exposure(i);
        draw_route_stroke(hdc, &maritime_routes[i], layout, outline,
                          exposure > 0 ? RGB(8, 48, 31) : RGB(116, 102, 77));
    }
    for (i = 0; i < maritime_route_count; i++) {
        int inner = layout.tile_size >= 4 ? 2 : 1;
        int exposure = plague_route_exposure(i);
        draw_route_stroke(hdc, &maritime_routes[i], layout, inner,
                          exposure > 0 ? RGB(20, 92, 54) : RGB(220, 194, 139));
    }
    RestoreDC(hdc, saved_dc);
}
