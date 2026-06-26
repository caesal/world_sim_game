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

static short previous_owner[MAX_MAP_W * MAX_MAP_H];
static short previous_province[MAX_MAP_W * MAX_MAP_H];
static short previous_region[MAX_MAP_W * MAX_MAP_H];
static unsigned char previous_land[MAX_MAP_W * MAX_MAP_H];
static int previous_valid;
static int previous_w;
static int previous_h;
static int previous_display;

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

static unsigned int transparent_pixel(void) { return argb(MAP_TRANSPARENT_KEY, 255); }

static void clear_border_pixels(MapLayerCache *cache) {
    int total = cache->width * cache->height;
    int i;
    unsigned int clear = transparent_pixel();
    for (i = 0; i < total; i++) cache->pixels[i] = clear;
}

static void clear_pixel_rect(MapLayerCache *cache, int left, int top, int right, int bottom) {
    int px;
    int py;
    unsigned int clear = transparent_pixel();
    left = clamp(left, 0, cache->width);
    right = clamp(right, 0, cache->width);
    top = clamp(top, 0, cache->height);
    bottom = clamp(bottom, 0, cache->height);
    for (py = top; py < bottom; py++) {
        unsigned int *row = &cache->pixels[py * cache->width];
        for (px = left; px < right; px++) row[px] = clear;
    }
}

static int tile_border_changed(const RenderSnapshot *snapshot, int index) {
    const SnapshotTile *tile = &snapshot->tiles[index];
    unsigned char land_now = (unsigned char)land(tile);
    return previous_land[index] != land_now ||
           previous_owner[index] != tile->owner ||
           previous_province[index] != tile->province_id ||
           previous_region[index] != tile->region_id;
}

static void remember_tile(const RenderSnapshot *snapshot, int index) {
    const SnapshotTile *tile = &snapshot->tiles[index];
    previous_land[index] = (unsigned char)land(tile);
    previous_owner[index] = tile->owner;
    previous_province[index] = tile->province_id;
    previous_region[index] = tile->region_id;
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

static void draw_edge_at(MapLayerCache *cache, const RenderSnapshot *snapshot,
                         int kind, unsigned int color, int width, int x, int y) {
    int map_w = snapshot->map_w;
    int map_h = snapshot->map_h;
    const SnapshotTile *a;
    const SnapshotTile *r;
    const SnapshotTile *b;
    int px;
    int py;
    if (x < 0 || y < 0 || x >= map_w || y >= map_h) return;
    a = &snapshot->tiles[y * map_w + x];
    r = x + 1 < map_w ? a + 1 : NULL;
    b = y + 1 < map_h ? &snapshot->tiles[(y + 1) * map_w + x] : NULL;
    px = (x + 1) * MAP_LAYER_CACHE_SCALE;
    py = (y + 1) * MAP_LAYER_CACHE_SCALE;
    if (edge_differs(snapshot, a, r, kind)) {
        vertical(cache, px, y * MAP_LAYER_CACHE_SCALE, py, width, color);
    }
    if (edge_differs(snapshot, a, b, kind)) {
        horizontal(cache, x * MAP_LAYER_CACHE_SCALE, px, py, width, color);
    }
}

static unsigned int province_border_pixel(void) { return argb(RGB(104, 76, 46), 255); }

static void draw_edges_in_rect(MapLayerCache *cache, const RenderSnapshot *snapshot,
                               int min_x, int min_y, int max_x, int max_y,
                               int province_w, int country_w) {
    int sx;
    int sy;
    min_x = max(0, min_x);
    min_y = max(0, min_y);
    max_x = min(snapshot->map_w - 1, max_x);
    max_y = min(snapshot->map_h - 1, max_y);
    for (sy = min_y; sy <= max_y; sy++) {
        for (sx = min_x; sx <= max_x; sx++) {
            if (display_mode == DISPLAY_REGIONS) {
                draw_edge_at(cache, snapshot, 3, argb(RGB(44, 54, 46), 255), 1, sx, sy);
            }
            draw_edge_at(cache, snapshot, 2, province_border_pixel(), province_w, sx, sy);
            draw_edge_at(cache, snapshot, 1, argb(RGB(34, 30, 24), 255), country_w, sx, sy);
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

static void remember_full_snapshot(const RenderSnapshot *snapshot) {
    int total = snapshot->map_w * snapshot->map_h;
    int i;
    for (i = 0; i < total; i++) remember_tile(snapshot, i);
    previous_valid = 1;
    previous_w = snapshot->map_w;
    previous_h = snapshot->map_h;
    previous_display = display_mode;
}

static int can_incremental_border(const MapLayerCache *cache, const RenderSnapshot *snapshot) {
    return previous_valid && cache && cache->valid &&
           previous_w == snapshot->map_w && previous_h == snapshot->map_h &&
           previous_display == display_mode &&
           cache->width == snapshot->map_w * MAP_LAYER_CACHE_SCALE &&
           cache->height == snapshot->map_h * MAP_LAYER_CACHE_SCALE;
}

static int update_border_incremental(MapLayerCache *cache, const RenderSnapshot *snapshot,
                                     int province_w, int country_w) {
    int total = snapshot->map_w * snapshot->map_h;
    int threshold = max(4096, total / 12);
    int min_x = snapshot->map_w;
    int min_y = snapshot->map_h;
    int max_x = -1;
    int max_y = -1;
    int clear_min_x, clear_min_y, clear_max_x, clear_max_y;
    int redraw_pad = 2 + max(province_w, country_w);
    int area_threshold = max(8192, total / 6);
    int changed = 0;
    int i;
    for (i = 0; i < total; i++) {
        if (tile_border_changed(snapshot, i)) {
            int x = i % snapshot->map_w;
            int y = i / snapshot->map_w;
            if (x < min_x) min_x = x;
            if (y < min_y) min_y = y;
            if (x > max_x) max_x = x;
            if (y > max_y) max_y = y;
            if (++changed > threshold) return 0;
        }
    }
    if (changed <= 0) return 1;
    clear_min_x = max(0, min_x - redraw_pad);
    clear_min_y = max(0, min_y - redraw_pad);
    clear_max_x = min(snapshot->map_w - 1, max_x + redraw_pad);
    clear_max_y = min(snapshot->map_h - 1, max_y + redraw_pad);
    if ((clear_max_x - clear_min_x + 1) * (clear_max_y - clear_min_y + 1) > area_threshold) return 0;
    clear_pixel_rect(cache,
                     clear_min_x * MAP_LAYER_CACHE_SCALE - redraw_pad,
                     clear_min_y * MAP_LAYER_CACHE_SCALE - redraw_pad,
                     (clear_max_x + 1) * MAP_LAYER_CACHE_SCALE + redraw_pad,
                     (clear_max_y + 1) * MAP_LAYER_CACHE_SCALE + redraw_pad);
    draw_edges_in_rect(cache, snapshot, clear_min_x - redraw_pad, clear_min_y - redraw_pad,
                       clear_max_x + redraw_pad, clear_max_y + redraw_pad,
                       province_w, country_w);
    for (i = 0; i < total; i++) {
        if (tile_border_changed(snapshot, i)) {
            remember_tile(snapshot, i);
        }
    }
    draw_grid(cache, snapshot);
    return 1;
}

void render_static_map_cache_build_border_pixels(MapLayerCache *cache,
                                                 const RenderSnapshot *snapshot) {
    int province_w;
    int country_w;
    if (!cache || !cache->pixels || !snapshot || !snapshot->world_generated) return;
    province_w = map_presentation_province_border_width(snapshot->map_w, snapshot->map_h, 1);
    country_w = map_presentation_country_border_width(snapshot->map_w, snapshot->map_h, 2);
    if (can_incremental_border(cache, snapshot) &&
        update_border_incremental(cache, snapshot, province_w, country_w)) return;
    clear_border_pixels(cache);
    if (display_mode == DISPLAY_REGIONS) {
        draw_edge_kind(cache, snapshot, 3, argb(RGB(44, 54, 46), 255), 1);
    }
    draw_edge_kind(cache, snapshot, 2, province_border_pixel(), province_w);
    draw_edge_kind(cache, snapshot, 1, argb(RGB(34, 30, 24), 255), country_w);
    draw_grid(cache, snapshot);
    remember_full_snapshot(snapshot);
}
