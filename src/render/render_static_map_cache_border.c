#include "render/render_static_map_cache_internal.h"

#include "render/map_presentation_policy.h"
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

static void vertical(MapLayerCache *cache, int x, int y0, int y1,
                     int width, unsigned int color) {
    int y;
    int w;
    int left = max(0, x - width + 1);
    int right = min(cache->width, x + 1);
    y0 = clamp(y0, 0, cache->height);
    y1 = clamp(y1, 0, cache->height);
    for (y = y0; y < y1; y++) {
        unsigned int *row = &cache->pixels[y * cache->width];
        for (w = left; w < right; w++) row[w] = color;
    }
}

static void horizontal(MapLayerCache *cache, int x0, int x1, int y,
                       int width, unsigned int color) {
    int x;
    int w;
    int top = max(0, y - width + 1);
    int bottom = min(cache->height, y + 1);
    x0 = clamp(x0, 0, cache->width);
    x1 = clamp(x1, 0, cache->width);
    if (x0 >= x1) return;
    for (w = top; w < bottom; w++) {
        unsigned int *row = &cache->pixels[w * cache->width];
        for (x = x0; x < x1; x++) row[x] = color;
    }
}

static void set_grid_pixel(MapLayerCache *cache, int x, int y, unsigned int color) {
    if (x >= 0 && y >= 0 && x < cache->width && y < cache->height) {
        cache->pixels[y * cache->width + x] = color;
    }
}

static void draw_edge_kind(MapLayerCache *cache, const RenderSnapshot *snapshot,
                           int kind, unsigned int color, int width) {
    int x;
    int y;
    int map_w = snapshot->map_w;
    int map_h = snapshot->map_h;
    const SnapshotTile *tiles = snapshot->tiles;
    for (y = 0; y < map_h; y++) {
        for (x = 0; x < map_w; x++) {
            const SnapshotTile *a = &tiles[y * map_w + x];
            const SnapshotTile *r = x + 1 < map_w ? a + 1 : NULL;
            const SnapshotTile *b = y + 1 < map_h ? &tiles[(y + 1) * map_w + x] : NULL;
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
        for (y = 0; y < cache->height; y += 3) set_grid_pixel(cache, x, y, color);
    }
    for (y = step; y < snapshot->map_h * MAP_LAYER_CACHE_SCALE; y += step) {
        for (x = 0; x < cache->width; x += 3) set_grid_pixel(cache, x, y, color);
    }
}

void render_static_map_cache_build_border_pixels(MapLayerCache *cache,
                                                 const RenderSnapshot *snapshot) {
    int province_w;
    int country_w;
    if (!cache || !cache->pixels || !snapshot || !snapshot->world_generated) return;
    province_w = map_presentation_province_border_width(snapshot->map_w, snapshot->map_h, 1);
    country_w = map_presentation_country_border_width(snapshot->map_w, snapshot->map_h, 2);
    if (display_mode == DISPLAY_REGIONS) {
        draw_edge_kind(cache, snapshot, 3, argb(RGB(44, 54, 46), 255), 1);
    }
    draw_edge_kind(cache, snapshot, 2, argb(RGB(70, 62, 50), 255), province_w);
    draw_edge_kind(cache, snapshot, 1, argb(RGB(34, 30, 24), 255), country_w);
    draw_grid(cache, snapshot);
}
