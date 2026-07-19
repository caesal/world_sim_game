#include "render/river_render.h"
#include "render/river_lod_policy.h"
#include "render/river_topology.h"

#include "core/game_types.h"
#include "world/terrain_query.h"

static int river_lod_tile_size;

void river_render_set_lod_tile_size(int tile_size) { river_lod_tile_size = tile_size; }

int river_render_lod_bucket_for_tile_size(int tile_size) {
    if (tile_size <= 2) return 0;
    if (tile_size <= 4) return 1;
    if (tile_size <= 8) return 2;
    return 3;
}

int river_render_lod_bucket_for_zoom(int zoom_percent, int tile_size) {
    (void)tile_size;
    if (zoom_percent >= 300) return 3;
    if (zoom_percent >= 225) return 2;
    if (zoom_percent >= 150) return 1;
    return 0;
}

static int screen_x(MapLayout layout, const RenderSnapshot *snapshot, int x10) {
    return layout.map_x + x10 * layout.draw_w / max(1, snapshot->map_w * 10);
}

static int screen_y(MapLayout layout, const RenderSnapshot *snapshot, int y10) {
    return layout.map_y + y10 * layout.draw_h / max(1, snapshot->map_h * 10);
}

static COLORREF river_main_color(const RiverRenderPath *path) {
    if (path->style_flags & RIVER_STYLE_MOUNTAIN) return RGB(46, 105, 150);
    if (path->style_flags & RIVER_STYLE_WETLAND) return RGB(42, 126, 154);
    if (path->style_flags & RIVER_STYLE_DESERT) return RGB(58, 133, 165);
    if (path->style_flags & RIVER_STYLE_COLD) return RGB(70, 145, 190);
    if (path->style_flags & RIVER_STYLE_HILL) return RGB(50, 121, 162);
    return RGB(45, 130, 174);
}

static int river_width(const RiverRenderPath *path) {
    int generated_width = max(1, path->width);
    return clamp(1 + (generated_width - 1) / 3, 1, 4);
}

static int path_to_points(const RiverRenderPath *path, MapLayout layout,
                          const RenderSnapshot *snapshot, POINT *points, int capacity) {
    int i;
    int count = min(path->point_count, capacity);
    for (i = 0; i < count; i++) {
        points[i].x = screen_x(layout, snapshot, path->points[i].x10);
        points[i].y = screen_y(layout, snapshot, path->points[i].y10);
    }
    return count;
}

static void stroke_path(HDC hdc, const RiverRenderPath *path, MapLayout layout,
                        const RenderSnapshot *snapshot, int width, COLORREF color) {
    POINT points[MAX_RIVER_RENDER_POINTS];
    HPEN pen;
    HPEN old_pen;
    int count;
    if (!path->active || path->point_count < 2 || width <= 0) return;
    count = path_to_points(path, layout, snapshot, points, MAX_RIVER_RENDER_POINTS);
    if (count < 2) return;
    pen = CreatePen(PS_SOLID, width, color);
    old_pen = SelectObject(hdc, pen);
    Polyline(hdc, points, count);
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
}

static int lod_tile_size_for_draw(MapLayout layout) {
    return river_lod_tile_size > 0 ? river_lod_tile_size : layout.tile_size;
}

int river_render_path_visible_at_lod(const RiverRenderPath *path, int lod) {
    const RiverRenderPath *base;
    uintptr_t address;
    uintptr_t base_address;
    int count = 0;
    int index;
    if (!path || !path->active || path->point_count < 2) return 0;
    base = river_geometry_paths(&count);
    address = (uintptr_t)(const void *)path;
    base_address = (uintptr_t)(const void *)base;
    index = address >= base_address ?
        (int)((address - base_address) / sizeof(*base)) : -1;
    if (index >= 0 && index < count && &base[index] == path)
        return river_lod_policy_path_visible(index, clamp(lod, 0, 3));
    return lod >= 3;
}

static int endpoint_is_water(const RiverRenderPath *path, const RenderSnapshot *snapshot) {
    RiverRenderPoint end;
    int x;
    int y;
    if (!snapshot || path->point_count < 1) return 0;
    end = path->points[path->point_count - 1];
    x = (end.x10 - 5) / 10;
    y = (end.y10 - 5) / 10;
    if (x < 0 || y < 0 || x >= snapshot->map_w || y >= snapshot->map_h) return 0;
    return !is_land((Geography)snapshot->tiles[y * snapshot->map_w + x].geography);
}

static void draw_mouth_cap(HDC hdc, const RiverRenderPath *path, MapLayout layout,
                           const RenderSnapshot *snapshot) {
    RiverRenderPoint end;
    int cx;
    int cy;
    int radius;
    HBRUSH brush;
    HBRUSH old_brush;
    HGDIOBJ old_pen;
    if (path->order < 4 || path->point_count < 2) return;
    if (!(path->end_flags & (SNAPSHOT_RIVER_MOUTH | SNAPSHOT_RIVER_DELTA)) &&
        !endpoint_is_water(path, snapshot)) return;
    end = path->points[path->point_count - 1];
    cx = screen_x(layout, snapshot, end.x10);
    cy = screen_y(layout, snapshot, end.y10);
    radius = clamp(river_width(path), 1, 3);
    brush = CreateSolidBrush(river_main_color(path));
    old_brush = SelectObject(hdc, brush);
    old_pen = SelectObject(hdc, GetStockObject(NULL_PEN));
    Ellipse(hdc, cx - radius, cy - radius, cx + radius, cy + radius);
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    DeleteObject(brush);
}

static void draw_pass(HDC hdc, const RiverRenderPath *paths, int count,
                      MapLayout layout, const RenderSnapshot *snapshot,
                      int lod) {
    int order;
    for (order = 1; order <= 5; order++) {
        int i;
        for (i = 0; i < count; i++) {
            const RiverRenderPath *path = &paths[i];
            int width;
            if (clamp(path->order, 1, 5) != order ||
                !river_render_path_visible_at_lod(path, lod)) continue;
            width = river_width(path);
            if (lod == 0 && width < 2) width = 2;
            stroke_path(hdc, path, layout, snapshot, width,
                        river_main_color(path));
        }
    }
}

static int note_prepare_failure(void) {
    river_geometry_note_lod_counts(0, 0);
    river_geometry_note_lod_policy(0, 0, 0);
    return 0;
}

int river_render_draw_layer_lod(HDC hdc, RECT client, MapLayout layout,
                                const RenderSnapshot *snapshot, int lod) {
    DWORD start = GetTickCount();
    const RiverRenderPath *paths;
    int count;
    int saved_dc;
    int i;
    int visible = 0;
    int skipped = 0;
    int lod_ready;
    RiverLodPolicyMetrics lod_metrics;
    if (!hdc || !snapshot || !snapshot->rivers.valid ||
        !river_geometry_prepare(snapshot)) return note_prepare_failure();
    lod = clamp(lod, 0, 3);
    paths = river_geometry_paths(&count);
    if (count <= 0) {
        river_geometry_note_lod_counts(0, 0);
        river_geometry_note_lod_policy(0, 0, 0);
        river_geometry_note_cache_rebuild((int)(GetTickCount() - start));
        return 1;
    }
    /* River content LOD belongs to the immutable world, not to the bitmap or
       viewport used to rasterize it.  Supplying canonical map dimensions keeps
       cached and fallback masks identical for the same world/revision. */
    lod_ready = river_lod_policy_prepare(paths, count, river_topology_view(),
                                         snapshot->map_w, snapshot->map_h);
    if (!lod_ready) return note_prepare_failure();
    for (i = 0; i < count; i++) {
        if (river_render_path_visible_at_lod(&paths[i], lod)) visible++;
        else skipped++;
    }
    lod_metrics = river_lod_policy_metrics(lod);
    river_geometry_note_lod_counts(visible, skipped);
    river_geometry_note_lod_policy(lod_metrics.target_paths,
                                   lod_metrics.visible_stems,
                                   lod_metrics.connected_paths);
    saved_dc = SaveDC(hdc);
    if (!saved_dc) return 0;
    IntersectClipRect(hdc, client.left, client.top, client.right, client.bottom);
    SetBkMode(hdc, TRANSPARENT);
    draw_pass(hdc, paths, count, layout, snapshot, lod);
    for (i = 0; i < count; i++) {
        if (river_render_path_visible_at_lod(&paths[i], lod))
            draw_mouth_cap(hdc, &paths[i], layout, snapshot);
    }
    if (!RestoreDC(hdc, saved_dc)) return 0;
    river_geometry_note_cache_rebuild((int)(GetTickCount() - start));
    return 1;
}

void river_render_draw_layer(HDC hdc, RECT client, MapLayout layout,
                             const RenderSnapshot *snapshot) {
    int tile_size = lod_tile_size_for_draw(layout);
    river_render_draw_layer_lod(hdc, client, layout, snapshot,
                                river_render_lod_bucket_for_zoom(
                                    map_zoom_percent, tile_size));
}

const HydrologyRenderStats *river_render_stats(void) { return river_geometry_stats(); }

RiverPresentationFilterMetrics river_render_close_filter_metrics(void) {
    return river_presentation_filter_last_metrics();
}
