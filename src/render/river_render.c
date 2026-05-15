#include "river_render.h"

#include "core/game_types.h"
#include "world/terrain_query.h"

static int screen_x(MapLayout layout, int x10) {
    return layout.map_x + x10 * layout.draw_w / max(1, MAP_W * 10);
}

static int screen_y(MapLayout layout, int y10) {
    return layout.map_y + y10 * layout.draw_h / max(1, MAP_H * 10);
}

static COLORREF river_main_color(const RiverRenderPath *path) {
    if (path->style_flags & RIVER_STYLE_MOUNTAIN) return RGB(60, 103, 126);
    if (path->style_flags & RIVER_STYLE_WETLAND) return RGB(79, 135, 129);
    if (path->style_flags & RIVER_STYLE_DESERT) return RGB(106, 139, 145);
    if (path->style_flags & RIVER_STYLE_COLD) return RGB(92, 138, 166);
    if (path->style_flags & RIVER_STYLE_HILL) return RGB(70, 119, 140);
    return RGB(76, 129, 149);
}

static int river_width(const RiverRenderPath *path) {
    int width = path->order <= 1 ? 1 : path->order == 2 ? 2 : path->order == 3 ? 3 : 4;
    if (path->flow > 1800) width++;
    if (path->width > 2 && path->order >= 3) width++;
    if (path->style_flags & RIVER_STYLE_MOUNTAIN) width = max(1, width - 1);
    if (path->style_flags & RIVER_STYLE_WETLAND) width++;
    return clamp(width, 1, 5);
}

static int path_to_points(const RiverRenderPath *path, MapLayout layout, POINT *points, int max_points) {
    int i;
    int count = min(path->point_count, max_points);
    for (i = 0; i < count; i++) {
        points[i].x = screen_x(layout, path->points[i].x10);
        points[i].y = screen_y(layout, path->points[i].y10);
    }
    return count;
}

static void stroke_path(HDC hdc, const RiverRenderPath *path, MapLayout layout,
                        int width, COLORREF color) {
    POINT points[MAX_RIVER_RENDER_POINTS];
    HPEN pen;
    HPEN old_pen;
    int count;

    if (!path->active || path->point_count < 2 || width <= 0) return;
    count = path_to_points(path, layout, points, MAX_RIVER_RENDER_POINTS);
    if (count < 2) return;
    pen = CreatePen(PS_SOLID, width, color);
    old_pen = SelectObject(hdc, pen);
    Polyline(hdc, points, count);
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
}

static void draw_mouth_cap(HDC hdc, const RiverRenderPath *path, MapLayout layout) {
    RiverRenderPoint end;
    int tx;
    int ty;
    int cx;
    int cy;
    int radius;
    HBRUSH brush;
    HPEN pen;
    HBRUSH old_brush;
    HPEN old_pen;

    if (path->order < 3 || path->point_count < 2) return;
    end = path->points[path->point_count - 1];
    tx = (end.x10 - 5) / 10;
    ty = (end.y10 - 5) / 10;
    if (tx < 0 || ty < 0 || tx >= MAP_W || ty >= MAP_H || is_land(world[ty][tx].geography)) return;
    cx = screen_x(layout, end.x10);
    cy = screen_y(layout, end.y10);
    radius = clamp(river_width(path) + 1, 3, 7);
    brush = CreateSolidBrush(river_main_color(path));
    pen = CreatePen(PS_SOLID, 1, RGB(35, 54, 61));
    old_brush = SelectObject(hdc, brush);
    old_pen = SelectObject(hdc, pen);
    Ellipse(hdc, cx - radius, cy - radius, cx + radius, cy + radius);
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    DeleteObject(pen);
    DeleteObject(brush);
}

static void draw_pass(HDC hdc, const RiverRenderPath *paths, int count, MapLayout layout, int pass) {
    int order;
    for (order = 1; order <= 5; order++) {
        int i;
        for (i = 0; i < count; i++) {
            const RiverRenderPath *path = &paths[i];
            int path_order = clamp(path->order, 1, 5);
            int width;
            if (path_order != order) continue;
            width = river_width(path);
            if (pass == 0) stroke_path(hdc, path, layout, width + 2, RGB(36, 54, 61));
            else if (pass == 1) stroke_path(hdc, path, layout, width, river_main_color(path));
            else if (path->order >= 3) stroke_path(hdc, path, layout, 1, RGB(145, 181, 190));
        }
    }
}

void river_render_draw_layer(HDC hdc, RECT client, MapLayout layout) {
    DWORD start = GetTickCount();
    const RiverRenderPath *paths;
    int count;
    int saved_dc;
    int i;

    river_geometry_rebuild();
    paths = river_geometry_paths(&count);
    if (count <= 0) {
        river_geometry_note_cache_rebuild((int)(GetTickCount() - start));
        return;
    }
    saved_dc = SaveDC(hdc);
    IntersectClipRect(hdc, client.left, client.top, client.right, client.bottom);
    SetBkMode(hdc, TRANSPARENT);
    draw_pass(hdc, paths, count, layout, 0);
    draw_pass(hdc, paths, count, layout, 1);
    draw_pass(hdc, paths, count, layout, 2);
    for (i = 0; i < count; i++) draw_mouth_cap(hdc, &paths[i], layout);
    RestoreDC(hdc, saved_dc);
    river_geometry_note_cache_rebuild((int)(GetTickCount() - start));
}

const HydrologyRenderStats *river_render_stats(void) {
    return river_geometry_stats();
}
