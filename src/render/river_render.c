#include "river_render.h"

#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "world/terrain_query.h"

static int river_lod_tile_size;

void river_render_set_lod_tile_size(int tile_size) {
    river_lod_tile_size = tile_size;
}

int river_render_lod_bucket_for_tile_size(int tile_size) {
    if (tile_size <= 2) return 0;
    if (tile_size <= 4) return 1;
    if (tile_size <= 8) return 2;
    return 3;
}

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
    int width = path->order <= 3 ? 1 : 2;
    if (path->flow > 3200 && path->order >= 5) width = 3;
    if (path->style_flags & RIVER_STYLE_MOUNTAIN) width = max(1, width - 1);
    return clamp(width, 1, 3);
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

static int river_lod_tile_size_for_draw(MapLayout layout) {
    return river_lod_tile_size > 0 ? river_lod_tile_size : layout.tile_size;
}

static int river_min_order_for_lod(int tile_size) {
    if (tile_size <= 2) return 4;
    if (tile_size <= 4) return 3;
    if (tile_size <= 8) return 2;
    return 1;
}

static int river_path_visible_lod(const RiverRenderPath *path, int tile_size) {
    int min_order = river_min_order_for_lod(tile_size);
    if (!path->active || path->point_count < 2) return 0;
    if (path->order < min_order) return 0;
    if (tile_size <= 4 && path->point_count < 18 && path->order < 3) return 0;
    return 1;
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

    if (path->order < 4 || path->point_count < 2) return;
    end = path->points[path->point_count - 1];
    tx = (end.x10 - 5) / 10;
    ty = (end.y10 - 5) / 10;
    if (tx < 0 || ty < 0 || tx >= MAP_W || ty >= MAP_H || is_land(world[ty][tx].geography)) return;
    cx = screen_x(layout, end.x10);
    cy = screen_y(layout, end.y10);
    radius = clamp(river_width(path), 1, 3);
    brush = CreateSolidBrush(river_main_color(path));
    pen = CreatePen(PS_SOLID, 1, RGB(72, 94, 100));
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
    int lod_tile_size = river_lod_tile_size_for_draw(layout);
    for (order = 1; order <= 5; order++) {
        int i;
        for (i = 0; i < count; i++) {
            const RiverRenderPath *path = &paths[i];
            int path_order = clamp(path->order, 1, 5);
            int width;
            if (path_order != order) continue;
            if (!river_path_visible_lod(path, lod_tile_size)) continue;
            width = river_width(path);
            if (pass == 0 && width > 1) stroke_path(hdc, path, layout, width + 1, RGB(64, 86, 92));
            else if (pass == 1) stroke_path(hdc, path, layout, width, river_main_color(path));
            else if (lod_tile_size >= 10 && path->order >= 5 && path->flow > 2600)
                stroke_path(hdc, path, layout, 1, RGB(145, 181, 190));
        }
    }
}

void river_render_draw_layer(HDC hdc, RECT client, MapLayout layout) {
    DWORD start = GetTickCount();
    const RiverRenderPath *paths;
    int count;
    int saved_dc;
    int i;
    int visible = 0;
    int skipped = 0;
    int lod_tile_size = river_lod_tile_size_for_draw(layout);

    river_geometry_rebuild_if_needed(dirty_revision_hydrology());
    paths = river_geometry_paths(&count);
    if (count <= 0) {
        river_geometry_note_lod_counts(0, 0);
        river_geometry_note_cache_rebuild((int)(GetTickCount() - start));
        return;
    }
    for (i = 0; i < count; i++) {
        if (river_path_visible_lod(&paths[i], lod_tile_size)) visible++;
        else skipped++;
    }
    river_geometry_note_lod_counts(visible, skipped);
    saved_dc = SaveDC(hdc);
    IntersectClipRect(hdc, client.left, client.top, client.right, client.bottom);
    SetBkMode(hdc, TRANSPARENT);
    draw_pass(hdc, paths, count, layout, 0);
    draw_pass(hdc, paths, count, layout, 1);
    draw_pass(hdc, paths, count, layout, 2);
    for (i = 0; i < count; i++) {
        if (river_path_visible_lod(&paths[i], lod_tile_size)) draw_mouth_cap(hdc, &paths[i], layout);
    }
    RestoreDC(hdc, saved_dc);
    river_geometry_note_cache_rebuild((int)(GetTickCount() - start));
}

const HydrologyRenderStats *river_render_stats(void) {
    return river_geometry_stats();
}
