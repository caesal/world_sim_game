#include "render/snapshot_map_layers.h"

#include "render/map_display_policy.h"
#include "render/map_ownership_surface.h"
#include "render/map_presentation_policy.h"
#include "render/render_context.h"
#include "render/render_map_internal.h"
#include "render/river_render.h"
#include "render/wind_render.h"
#include "world/terrain_query.h"

#include <stddef.h>

static const SnapshotTile *snap_tile(const RenderSnapshot *snapshot, int x, int y) {
    if (!snapshot || x < 0 || y < 0 || x >= snapshot->map_w || y >= snapshot->map_h) return NULL;
    return &snapshot->tiles[y * snapshot->map_w + x];
}

static int snap_land(const SnapshotTile *tile) {
    return tile && is_land((Geography)tile->geography);
}

static int snap_alive_owner(const RenderSnapshot *snapshot, int owner) {
    return snapshot && owner >= 0 && owner < snapshot->civ_count && snapshot->civs[owner].alive;
}

static int snap_owner_for_tile(const RenderSnapshot *snapshot, const SnapshotTile *tile) {
    ptrdiff_t idx;
    if (!snapshot || !tile) return -1;
    idx = tile - snapshot->tiles;
    if (idx < 0 || idx >= (ptrdiff_t)(snapshot->map_w * snapshot->map_h)) return -1;
    return map_ownership_surface_snapshot_owner(snapshot, (int)(idx % snapshot->map_w),
                                                (int)(idx / snapshot->map_w), NULL);
}

static int sx(MapLayout layout, const RenderSnapshot *snapshot, int x) {
    return layout.map_x + x * layout.draw_w / max(1, snapshot->map_w);
}

static int sy(MapLayout layout, const RenderSnapshot *snapshot, int y) {
    return layout.map_y + y * layout.draw_h / max(1, snapshot->map_h);
}

static COLORREF snapshot_tile_color(const RenderSnapshot *snapshot, int x, int y) {
    const SnapshotTile *tile = snap_tile(snapshot, x, y);
    return map_display_policy_snapshot_tile_color(snapshot, tile, display_mode);
}

static void draw_snapshot_tiles(HDC hdc, RECT client, MapLayout layout) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    RECT map_rect = {layout.map_x, layout.map_y,
                     layout.map_x + layout.draw_w,
                     layout.map_y + layout.draw_h};
    RECT viewport = get_map_content_rect(client);
    RECT visible;
    HBRUSH brush = (HBRUSH)GetStockObject(DC_BRUSH);
    COLORREF old_brush = GetDCBrushColor(hdc);
    int px, py;
    if (!IntersectRect(&visible, &map_rect, &viewport)) return;
    if (!snapshot || !snapshot->world_generated) {
        SetDCBrushColor(hdc, RGB(64, 133, 178));
        FillRect(hdc, &visible, brush);
        SetDCBrushColor(hdc, old_brush);
        return;
    }
    for (py = visible.top; py < visible.bottom; py++) {
        int tile_y = clamp((int)((long long)(py - layout.map_y) *
                         snapshot->map_h / max(1, layout.draw_h)),
                         0, snapshot->map_h - 1);
        for (px = visible.left; px < visible.right;) {
            int tile_x = clamp((int)((long long)(px - layout.map_x) *
                             snapshot->map_w / max(1, layout.draw_w)),
                             0, snapshot->map_w - 1);
            int right = layout.map_x + (int)(((long long)(tile_x + 1) *
                        layout.draw_w + snapshot->map_w - 1) /
                        snapshot->map_w);
            RECT run = {px, py, min(visible.right, max(px + 1, right)), py + 1};
            SetDCBrushColor(hdc,
                            snapshot_tile_color(snapshot, tile_x, tile_y));
            FillRect(hdc, &run, brush);
            px = run.right;
        }
    }
    SetDCBrushColor(hdc, old_brush);
}

static void edge_line(HDC hdc, MapLayout layout, const RenderSnapshot *snapshot,
                      int x1, int y1, int x2, int y2) {
    MoveToEx(hdc, sx(layout, snapshot, x1), sy(layout, snapshot, y1), NULL);
    LineTo(hdc, sx(layout, snapshot, x2), sy(layout, snapshot, y2));
}

static int edge_differs(const RenderSnapshot *snapshot, const SnapshotTile *a,
                        const SnapshotTile *b, int kind) {
    if (!a || !b) return 0;
    if (kind == 0) return snap_land(a) != snap_land(b);
    if (kind == 1) {
        int owner_a, owner_b;
        if (!snap_land(a) || !snap_land(b)) return 0;
        owner_a = snap_owner_for_tile(snapshot, a);
        owner_b = snap_owner_for_tile(snapshot, b);
        if (!snap_alive_owner(snapshot, owner_a) || !snap_alive_owner(snapshot, owner_b)) return 0;
        return owner_a != owner_b;
    }
    if (kind == 2) {
        if (!snap_land(a) || !snap_land(b)) return 0;
        if (!snap_alive_owner(snapshot, a->owner) || a->owner != b->owner) return 0;
        return a->province_id >= 0 && b->province_id >= 0 && a->province_id != b->province_id;
    }
    if (!snap_land(a) || !snap_land(b)) return 0;
    return a->region_id >= 0 && b->region_id >= 0 && a->region_id != b->region_id;
}

static void draw_edges(HDC hdc, RECT client, MapLayout layout, int kind, COLORREF color, int width) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    HPEN pen;
    HGDIOBJ old_pen;
    int x;
    int y;
    (void)client;
    if (!snapshot || !snapshot->world_generated) return;
    pen = CreatePen(PS_SOLID, width, color);
    old_pen = SelectObject(hdc, pen);
    for (y = 0; y < snapshot->map_h; y++) {
        for (x = 0; x < snapshot->map_w; x++) {
            const SnapshotTile *a = snap_tile(snapshot, x, y);
            const SnapshotTile *r = snap_tile(snapshot, x + 1, y);
            const SnapshotTile *b = snap_tile(snapshot, x, y + 1);
            if (edge_differs(snapshot, a, r, kind)) edge_line(hdc, layout, snapshot, x + 1, y, x + 1, y + 1);
            if (edge_differs(snapshot, a, b, kind)) edge_line(hdc, layout, snapshot, x, y + 1, x + 1, y + 1);
        }
    }
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
}

void draw_snapshot_terrain_layer(HDC hdc, RECT client, MapLayout layout) {
    draw_snapshot_tiles(hdc, client, layout);
}

void draw_snapshot_coast_layer(HDC hdc, RECT client, MapLayout layout) {
    draw_edges(hdc, client, layout, 0, RGB(196, 202, 184), 1);
}

void draw_snapshot_political_layer(HDC hdc, RECT client, MapLayout layout) {
    (void)hdc;
    (void)client;
    (void)layout;
}

void draw_snapshot_hydrology_layer(HDC hdc, RECT client, MapLayout layout) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    if (map_display_policy_shows_wind(display_mode))
        wind_render_draw_layer(hdc, client, layout, snapshot);
    river_render_draw_layer(hdc, client, layout, snapshot);
}

static void draw_snapshot_grid_overlay(HDC hdc, MapLayout layout) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    int step = 100;
    int i;
    HPEN pen;
    HGDIOBJ old_pen;
    if (!snapshot || !snapshot->world_generated) return;
    pen = CreatePen(PS_DOT, 1, layout.tile_size <= 2 ? RGB(118, 123, 112) : RGB(132, 136, 124));
    old_pen = SelectObject(hdc, pen);
    SetBkMode(hdc, TRANSPARENT);
    for (i = step; i < snapshot->map_w; i += step) {
        int gx = layout.map_x + i * layout.draw_w / max(1, snapshot->map_w);
        MoveToEx(hdc, gx, layout.map_y, NULL);
        LineTo(hdc, gx, layout.map_y + layout.draw_h);
    }
    for (i = step; i < snapshot->map_h; i += step) {
        int gy = layout.map_y + i * layout.draw_h / max(1, snapshot->map_h);
        MoveToEx(hdc, layout.map_x, gy, NULL);
        LineTo(hdc, layout.map_x + layout.draw_w, gy);
    }
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
}

void draw_snapshot_border_layer(HDC hdc, RECT client, MapLayout layout) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    int province_w;
    int country_w;
    (void)client;
    if (!snapshot || !snapshot->world_generated) return;
    province_w = map_presentation_province_border_width(snapshot->map_w, snapshot->map_h, 1);
    country_w = map_presentation_country_border_width(snapshot->map_w, snapshot->map_h, 2);
    if (display_mode == DISPLAY_REGIONS) draw_edges(hdc, client, layout, 3, RGB(44, 54, 46), 1);
    draw_edges(hdc, client, layout, 2, RGB(70, 62, 50), province_w);
    draw_edges(hdc, client, layout, 1, RGB(34, 30, 24), country_w);
    draw_snapshot_grid_overlay(hdc, layout);
}
