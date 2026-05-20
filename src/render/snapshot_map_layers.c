#include "render/snapshot_map_layers.h"

#include "render/render_context.h"
#include "render/render_map_internal.h"
#include "render/river_render.h"
#include "world/terrain_query.h"

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

static int sx(MapLayout layout, const RenderSnapshot *snapshot, int x) {
    return layout.map_x + x * layout.draw_w / max(1, snapshot->map_w);
}

static int sy(MapLayout layout, const RenderSnapshot *snapshot, int y) {
    return layout.map_y + y * layout.draw_h / max(1, snapshot->map_h);
}

static COLORREF snapshot_water_color(const SnapshotTile *tile) {
    if (!tile) return RGB(38, 92, 154);
    if (tile->water_depth == WATER_DEPTH_NONE) return RGB(38, 92, 154);
    return blend_color(RGB(92, 177, 214), RGB(38, 92, 154),
                       clamp(tile->water_deep_percent, 0, 100));
}

static COLORREF snapshot_overview_color(const SnapshotTile *tile) {
    COLORREF base;
    COLORREF climate;
    int blend;
    int elev;
    if (!tile) return RGB(38, 92, 154);
    base = snap_land(tile) ? geography_color((Geography)tile->geography) : snapshot_water_color(tile);
    climate = climate_color((Climate)tile->climate);
    blend = snap_land(tile) ? 48 : 18;
    elev = tile->elevation;
    base = blend_color(base, climate, blend);
    if (!snap_land(tile)) return base;
    if (elev > 55) return blend_color(base, RGB(38, 35, 32), clamp((elev - 55) / 3, 0, 18));
    return blend_color(base, RGB(236, 230, 198), clamp((55 - elev) / 4, 0, 12));
}

static COLORREF snapshot_tile_color(const RenderSnapshot *snapshot, int x, int y) {
    const SnapshotTile *tile = snap_tile(snapshot, x, y);
    COLORREF base;
    if (!tile) return RGB(38, 92, 154);
    switch (display_mode) {
        case DISPLAY_CLIMATE:
            return climate_color((Climate)tile->climate);
        case DISPLAY_GEOGRAPHY:
            return snap_land(tile) ? geography_color((Geography)tile->geography) : snapshot_water_color(tile);
        case DISPLAY_REGIONS:
            base = snapshot_overview_color(tile);
            if (tile->region_id >= 0) {
                int id = tile->region_id;
                COLORREF region = RGB(92 + (id * 37) % 112, 105 + (id * 53) % 96, 86 + (id * 29) % 104);
                base = blend_color(base, region, 44);
            }
            return base;
        case DISPLAY_POLITICAL:
        case DISPLAY_ALL:
            base = snapshot_overview_color(tile);
            if (snap_land(tile) && tile->owner >= 0 && tile->owner < snapshot->civ_count &&
                snapshot->civs[tile->owner].alive) {
                base = blend_color(base, (COLORREF)snapshot->civs[tile->owner].color, 46);
            }
            return base;
        default:
            return snapshot_overview_color(tile);
    }
}

static void draw_snapshot_tiles(HDC hdc, RECT client, MapLayout layout) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    int x;
    int y;
    if (!snapshot || !snapshot->world_generated) {
        fill_rect(hdc, client, RGB(64, 133, 178));
        return;
    }
    fill_rect(hdc, client, RGB(79, 160, 215));
    for (y = 0; y < snapshot->map_h; y++) {
        for (x = 0; x < snapshot->map_w; x++) {
            RECT r = {sx(layout, snapshot, x), sy(layout, snapshot, y),
                      sx(layout, snapshot, x + 1), sy(layout, snapshot, y + 1)};
            fill_rect(hdc, r, snapshot_tile_color(snapshot, x, y));
        }
    }
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
        if (!snap_land(a) || !snap_land(b)) return 0;
        if (!snap_alive_owner(snapshot, a->owner) || !snap_alive_owner(snapshot, b->owner)) return 0;
        return a->owner != b->owner;
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
    draw_edges(hdc, client, layout, 0, RGB(244, 232, 190), 2);
}

void draw_snapshot_political_layer(HDC hdc, RECT client, MapLayout layout) {
    (void)hdc;
    (void)client;
    (void)layout;
}

void draw_snapshot_hydrology_layer(HDC hdc, RECT client, MapLayout layout) {
    river_render_draw_layer(hdc, client, layout);
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
    (void)client;
    if (display_mode == DISPLAY_REGIONS) draw_edges(hdc, client, layout, 3, RGB(44, 54, 46), 1);
    draw_edges(hdc, client, layout, 2, RGB(70, 62, 50), 1);
    draw_edges(hdc, client, layout, 1, RGB(34, 30, 24), 2);
    draw_snapshot_grid_overlay(hdc, layout);
}
