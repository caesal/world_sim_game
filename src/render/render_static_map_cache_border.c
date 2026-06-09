#include "render/render_static_map_cache_internal.h"

#include "world/terrain_query.h"

static unsigned int argb(COLORREF color, int alpha) {
    int r = GetRValue(color) * alpha / 255;
    int g = GetGValue(color) * alpha / 255;
    int b = GetBValue(color) * alpha / 255;
    return (unsigned int)b | ((unsigned int)g << 8) |
           ((unsigned int)r << 16) | ((unsigned int)alpha << 24);
}

static int land(const SnapshotTile *tile) {
    return tile && is_land((Geography)tile->geography);
}

static int alive_owner(const RenderSnapshot *snapshot, int owner) {
    return snapshot && owner >= 0 && owner < snapshot->civ_count &&
           snapshot->civs[owner].alive;
}

static int edge_differs(const RenderSnapshot *snapshot, const SnapshotTile *a,
                        const SnapshotTile *b, int kind) {
    if (!a || !b) return 0;
    if (kind == 1) {
        if (!land(a) || !land(b)) return 0;
        if (!alive_owner(snapshot, a->owner) || !alive_owner(snapshot, b->owner)) return 0;
        return a->owner != b->owner;
    }
    if (kind == 2) {
        if (!land(a) || !land(b)) return 0;
        if (!alive_owner(snapshot, a->owner) || a->owner != b->owner) return 0;
        return a->province_id >= 0 && b->province_id >= 0 &&
               a->province_id != b->province_id;
    }
    if (!land(a) || !land(b)) return 0;
    return a->region_id >= 0 && b->region_id >= 0 && a->region_id != b->region_id;
}

static const SnapshotTile *tile_at(const RenderSnapshot *snapshot, int x, int y) {
    if (!snapshot || x < 0 || y < 0 || x >= snapshot->map_w || y >= snapshot->map_h) return NULL;
    return &snapshot->tiles[y * snapshot->map_w + x];
}

static void set_pixel(MapLayerCache *cache, int x, int y, unsigned int color) {
    if (!cache || x < 0 || y < 0 || x >= cache->width || y >= cache->height) return;
    cache->pixels[y * cache->width + x] = color;
}

static void vertical(MapLayerCache *cache, int x, int y0, int y1,
                     int width, unsigned int color) {
    int y;
    int w;
    for (y = y0; y < y1; y++) {
        for (w = 0; w < width; w++) set_pixel(cache, x - w, y, color);
    }
}

static void horizontal(MapLayerCache *cache, int x0, int x1, int y,
                       int width, unsigned int color) {
    int x;
    int w;
    for (x = x0; x < x1; x++) {
        for (w = 0; w < width; w++) set_pixel(cache, x, y - w, color);
    }
}

static void draw_edge_kind(MapLayerCache *cache, const RenderSnapshot *snapshot,
                           int kind, unsigned int color, int width) {
    int x;
    int y;
    for (y = 0; y < snapshot->map_h; y++) {
        for (x = 0; x < snapshot->map_w; x++) {
            const SnapshotTile *a = tile_at(snapshot, x, y);
            const SnapshotTile *r = tile_at(snapshot, x + 1, y);
            const SnapshotTile *b = tile_at(snapshot, x, y + 1);
            int px = (x + 1) * MAP_LAYER_CACHE_SCALE;
            int py = (y + 1) * MAP_LAYER_CACHE_SCALE;
            if (edge_differs(snapshot, a, r, kind)) {
                vertical(cache, px, y * MAP_LAYER_CACHE_SCALE, py, width, color);
            }
            if (edge_differs(snapshot, a, b, kind)) {
                horizontal(cache, x * MAP_LAYER_CACHE_SCALE, px, py, width, color);
            }
        }
    }
}

static void draw_grid(MapLayerCache *cache, const RenderSnapshot *snapshot) {
    unsigned int color = argb(RGB(118, 123, 112), 255);
    int step = 100 * MAP_LAYER_CACHE_SCALE;
    int x;
    int y;
    for (x = step; x < snapshot->map_w * MAP_LAYER_CACHE_SCALE; x += step) {
        for (y = 0; y < cache->height; y += 3) set_pixel(cache, x, y, color);
    }
    for (y = step; y < snapshot->map_h * MAP_LAYER_CACHE_SCALE; y += step) {
        for (x = 0; x < cache->width; x += 3) set_pixel(cache, x, y, color);
    }
}

void render_static_map_cache_build_border_pixels(MapLayerCache *cache,
                                                 const RenderSnapshot *snapshot) {
    if (!cache || !cache->pixels || !snapshot || !snapshot->world_generated) return;
    if (display_mode == DISPLAY_REGIONS) {
        draw_edge_kind(cache, snapshot, 3, argb(RGB(44, 54, 46), 255), 1);
    }
    draw_edge_kind(cache, snapshot, 2, argb(RGB(70, 62, 50), 255), 1);
    draw_edge_kind(cache, snapshot, 1, argb(RGB(34, 30, 24), 255), 2);
    draw_grid(cache, snapshot);
}
