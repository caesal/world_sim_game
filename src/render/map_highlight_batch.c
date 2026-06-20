#include "render/map_highlight_internal.h"

#include "core/game_state.h"
#include "render/map_ownership_surface.h"

#include <string.h>

typedef struct {
    HDC dc;
    HBITMAP bitmap;
    HBITMAP old_bitmap;
    unsigned int *pixels;
    int width;
    int height;
} HighlightOverlaySurface;

typedef struct { COLORREF color; int width; HPEN pen; } HighlightPenEntry;
typedef struct { HighlightPenEntry entries[HIGHLIGHT_REQUEST_MAX * 4]; int count; int creates; } HighlightPenPool;

static HighlightOverlaySurface highlight_overlay;
static HighlightEdgeSegment edge_cache[HIGHLIGHT_EDGE_SEGMENT_MAX];
static unsigned int edge_cache_key;
static int edge_cache_count, edge_cache_hits, edge_cache_misses;
static int highlight_overlay_calls, highlight_overlay_recreates, highlight_overlay_reuses;
static int highlight_overlay_last_ms, highlight_overlay_last_tiles, highlight_overlay_peak_ms;
static int highlight_overlay_total_clears, highlight_overlay_last_clears;
static int highlight_overlay_last_requests, highlight_overlay_last_rects;
static int highlight_edge_calls, highlight_edge_last_requests, highlight_edge_last_tiles;
static int highlight_edge_last_segments, highlight_edge_last_pen_creates;
static int highlight_edge_last_ms, highlight_edge_peak_ms;
static int highlight_edge_last_geometry_ms, highlight_edge_peak_geometry_ms;
static int highlight_focus_calls, highlight_focus_last_requests, highlight_focus_last_rings;
static int highlight_focus_last_pen_creates, highlight_focus_last_fallback_tiles;
static int highlight_focus_last_ms, highlight_focus_peak_ms;
static int highlight_total_paints, highlight_total_start_ms, highlight_total_last_ms, highlight_total_peak_ms;

static const SnapshotTile *snap_tile(const RenderSnapshot *snapshot, int x, int y) {
    if (!snapshot || x < 0 || y < 0 || x >= snapshot->map_w || y >= snapshot->map_h) return NULL;
    return &snapshot->tiles[y * snapshot->map_w + x];
}

static int tile_owner(const RenderSnapshot *snapshot, int x, int y) {
    if (!snap_tile(snapshot, x, y)) return -1;
    return map_ownership_surface_snapshot_owner(snapshot, x, y, NULL);
}

static int sx(MapLayout layout, const RenderSnapshot *snapshot, int x) {
    return layout.map_x + x * layout.draw_w / max(1, snapshot->map_w);
}

static int sy(MapLayout layout, const RenderSnapshot *snapshot, int y) {
    return layout.map_y + y * layout.draw_h / max(1, snapshot->map_h);
}

static unsigned int mix_edge_key(unsigned int key, int value) {
    return key * 1000003u ^ (unsigned int)value;
}

static unsigned int edge_request_key(const RenderSnapshot *snapshot, MapLayout layout,
                                     int min_x, int max_x, int min_y, int max_y,
                                     const HighlightRequest *requests, int request_count) {
    unsigned int key = 2166136261u;
    int i;
    key = mix_edge_key(key, snapshot->map_w); key = mix_edge_key(key, snapshot->map_h);
    key = mix_edge_key(key, map_ownership_surface_snapshot_revision(snapshot));
    key = mix_edge_key(key, layout.map_x); key = mix_edge_key(key, layout.map_y);
    key = mix_edge_key(key, layout.draw_w); key = mix_edge_key(key, layout.draw_h);
    key = mix_edge_key(key, min_x); key = mix_edge_key(key, max_x);
    key = mix_edge_key(key, min_y); key = mix_edge_key(key, max_y);
    key = mix_edge_key(key, request_count);
    for (i = 0; i < request_count; i++) {
        if (requests[i].dim) continue;
        key = mix_edge_key(key, requests[i].civ_id);
        key = mix_edge_key(key, requests[i].priority);
        key = mix_edge_key(key, requests[i].strong);
        key = mix_edge_key(key, requests[i].width_boost);
        key = mix_edge_key(key, (int)requests[i].inner);
        key = mix_edge_key(key, (int)requests[i].outer);
    }
    return key;
}

static void release_overlay_surface(void) {
    if (highlight_overlay.dc && highlight_overlay.old_bitmap) SelectObject(highlight_overlay.dc, highlight_overlay.old_bitmap);
    if (highlight_overlay.bitmap) DeleteObject(highlight_overlay.bitmap);
    if (highlight_overlay.dc) DeleteDC(highlight_overlay.dc);
    memset(&highlight_overlay, 0, sizeof(highlight_overlay));
}

static int ensure_overlay_surface(HDC hdc, int width, int height) {
    BITMAPINFO info;
    void *bits = NULL;
    if (width <= 0 || height <= 0) return 0;
    if (highlight_overlay.dc && highlight_overlay.width == width && highlight_overlay.height == height &&
        highlight_overlay.pixels) {
        highlight_overlay_reuses++;
        return 1;
    }
    release_overlay_surface();
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    highlight_overlay.bitmap = CreateDIBSection(hdc, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    highlight_overlay.dc = CreateCompatibleDC(hdc);
    if (!highlight_overlay.bitmap || !highlight_overlay.dc || !bits) {
        release_overlay_surface();
        return 0;
    }
    highlight_overlay.old_bitmap = SelectObject(highlight_overlay.dc, highlight_overlay.bitmap);
    highlight_overlay.pixels = (unsigned int *)bits;
    highlight_overlay.width = width;
    highlight_overlay.height = height;
    highlight_overlay_recreates++;
    return 1;
}

static void fill_overlay_rect(unsigned int *pixels, int width, int height, RECT rect, unsigned int pixel) {
    int x, y;
    rect.left = clamp(rect.left, 0, width);
    rect.right = clamp(rect.right, 0, width);
    rect.top = clamp(rect.top, 0, height);
    rect.bottom = clamp(rect.bottom, 0, height);
    if (rect.right <= rect.left || rect.bottom <= rect.top) return;
    for (y = rect.top; y < rect.bottom; y++) {
        unsigned int *row = pixels + y * width;
        for (x = rect.left; x < rect.right; x++) row[x] = pixel;
    }
}

static void reset_last_metrics(void) {
    highlight_overlay_last_requests = highlight_overlay_last_clears = 0;
    highlight_overlay_last_rects = highlight_overlay_last_tiles = highlight_overlay_last_ms = 0;
    highlight_edge_last_requests = highlight_edge_last_tiles = 0;
    highlight_edge_last_segments = highlight_edge_last_pen_creates = highlight_edge_last_ms = 0;
    highlight_focus_last_requests = highlight_focus_last_rings = 0;
    highlight_focus_last_pen_creates = highlight_focus_last_fallback_tiles = highlight_focus_last_ms = 0;
    highlight_total_last_ms = 0;
}

void map_highlight_debug_begin(void) {
    reset_last_metrics();
    highlight_total_start_ms = (int)GetTickCount();
}

void map_highlight_debug_end(void) {
    if (highlight_total_start_ms <= 0) {
        highlight_total_last_ms = 0;
        return;
    }
    highlight_total_paints++;
    highlight_total_last_ms = (int)GetTickCount() - highlight_total_start_ms;
    if (highlight_total_last_ms > highlight_total_peak_ms) highlight_total_peak_ms = highlight_total_last_ms;
    highlight_total_start_ms = 0;
}

void map_highlight_blend_overlay_requests(HDC hdc, RECT client, MapLayout layout,
                                          const RenderSnapshot *snapshot, int min_x,
                                          int max_x, int min_y, int max_y,
                                          const HighlightRequest *requests,
                                          int request_count) {
    DWORD start = GetTickCount();
    RECT viewport = get_map_viewport_rect(client);
    RECT overlay = {max(viewport.left, layout.map_x), max(viewport.top, layout.map_y),
                    min(viewport.right, layout.map_x + layout.draw_w),
                    min(viewport.bottom, layout.map_y + layout.draw_h)};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    int width = overlay.right - overlay.left;
    int height = overlay.bottom - overlay.top;
    int drew = 0, tiles = 0, rects = 0, x, y, i;
    int dim_primary = -1, dim_secondary = -1;
    unsigned int dim_pixel = 0;
    unsigned int owner_pixels[MAX_CIVS];
    highlight_overlay_last_requests = request_count;
    if (request_count <= 0 || width <= 0 || height <= 0) return;
    highlight_overlay_calls++;
    if (!ensure_overlay_surface(hdc, width, height)) return;
    memset(highlight_overlay.pixels, 0, (size_t)width * (size_t)height * sizeof(unsigned int));
    memset(owner_pixels, 0, sizeof(owner_pixels));
    for (i = 0; i < request_count; i++) {
        if (requests[i].dim) {
            dim_primary = requests[i].primary;
            dim_secondary = requests[i].secondary;
            dim_pixel = requests[i].pixel;
        } else if (requests[i].civ_id >= 0 && requests[i].civ_id < MAX_CIVS) {
            owner_pixels[requests[i].civ_id] = requests[i].pixel;
        }
    }
    highlight_overlay_total_clears++;
    highlight_overlay_last_clears = 1;
    for (y = min_y; y <= max_y; y++) {
        for (x = min_x; x <= max_x; x++) {
            int owner = tile_owner(snapshot, x, y);
            unsigned int pixel = owner >= 0 && owner < MAX_CIVS ? owner_pixels[owner] : 0;
            RECT tile;
            if (!pixel && dim_pixel && owner >= 0 && owner != dim_primary && owner != dim_secondary) pixel = dim_pixel;
            if (!pixel) continue;
            tile = (RECT){sx(layout, snapshot, x) - overlay.left, sy(layout, snapshot, y) - overlay.top,
                          sx(layout, snapshot, x + 1) - overlay.left, sy(layout, snapshot, y + 1) - overlay.top};
            fill_overlay_rect(highlight_overlay.pixels, width, height, tile, pixel);
            drew = 1;
            tiles++;
            rects++;
        }
    }
    if (drew) {
        AlphaBlend(hdc, overlay.left, overlay.top, width, height,
                   highlight_overlay.dc, 0, 0, width, height, blend);
    }
    highlight_overlay_last_tiles = tiles;
    highlight_overlay_last_rects = rects;
    highlight_overlay_last_ms = (int)(GetTickCount() - start);
    if (highlight_overlay_last_ms > highlight_overlay_peak_ms) highlight_overlay_peak_ms = highlight_overlay_last_ms;
}

static HPEN pen_pool_get(HighlightPenPool *pool, COLORREF color, int width) {
    int i;
    for (i = 0; i < pool->count; i++) {
        if (pool->entries[i].color == color && pool->entries[i].width == width) return pool->entries[i].pen;
    }
    if (pool->count >= (int)(sizeof(pool->entries) / sizeof(pool->entries[0]))) {
        return pool->count > 0 ? pool->entries[pool->count - 1].pen : (HPEN)GetStockObject(BLACK_PEN);
    }
    pool->entries[pool->count].color = color;
    pool->entries[pool->count].width = width;
    pool->entries[pool->count].pen = CreatePen(PS_SOLID, width, color);
    pool->creates++;
    return pool->entries[pool->count++].pen;
}

static void pen_pool_release(HighlightPenPool *pool) {
    int i;
    for (i = 0; i < pool->count; i++) DeleteObject(pool->entries[i].pen);
    pool->count = 0;
}

static void build_owner_requests(const HighlightRequest *requests, int request_count,
                                 const HighlightRequest **owner_requests) {
    int i;
    memset(owner_requests, 0, sizeof(const HighlightRequest *) * MAX_CIVS);
    for (i = 0; i < request_count; i++) {
        int civ_id = requests[i].civ_id;
        if (requests[i].dim || civ_id < 0 || civ_id >= MAX_CIVS) continue;
        if (!owner_requests[civ_id] || requests[i].priority >= owner_requests[civ_id]->priority) {
            owner_requests[civ_id] = &requests[i];
        }
    }
}

static const HighlightRequest *choose_edge_request(const HighlightRequest *a, const HighlightRequest *b) {
    if (!a) return b;
    if (!b) return a;
    return a->priority >= b->priority ? a : b;
}

static void cache_edge_between(const HighlightRequest *requests, const HighlightRequest **owner_requests,
                               int owner_a, int owner_b, int x1, int y1, int x2, int y2) {
    const HighlightRequest *a = owner_a >= 0 && owner_a < MAX_CIVS ? owner_requests[owner_a] : NULL;
    const HighlightRequest *b = owner_b >= 0 && owner_b < MAX_CIVS ? owner_requests[owner_b] : NULL;
    const HighlightRequest *request;
    if (owner_a == owner_b || (!a && !b)) return;
    request = choose_edge_request(a, b);
    if (!request || edge_cache_count >= HIGHLIGHT_EDGE_SEGMENT_MAX) return;
    edge_cache[edge_cache_count++] = (HighlightEdgeSegment){x1, y1, x2, y2, (int)(request - requests)};
}

static void rebuild_edge_cache(MapLayout layout, const RenderSnapshot *snapshot,
                               int min_x, int max_x, int min_y, int max_y,
                               const HighlightRequest *requests,
                               const HighlightRequest **owner_requests) {
    int x, y;
    edge_cache_count = 0;
    for (y = min_y; y <= max_y; y++) {
        for (x = min_x; x <= max_x; x++) {
            int owner = tile_owner(snapshot, x, y);
            int l = sx(layout, snapshot, x);
            int r = sx(layout, snapshot, x + 1);
            int t = sy(layout, snapshot, y);
            int b = sy(layout, snapshot, y + 1);
            highlight_edge_last_tiles++;
            if (x == min_x) {
                cache_edge_between(requests, owner_requests, owner, tile_owner(snapshot, x - 1, y), l, t, l, b);
            }
            if (y == min_y) {
                cache_edge_between(requests, owner_requests, owner, tile_owner(snapshot, x, y - 1), l, t, r, t);
            }
            cache_edge_between(requests, owner_requests, owner, tile_owner(snapshot, x + 1, y), r, t, r, b);
            cache_edge_between(requests, owner_requests, owner, tile_owner(snapshot, x, y + 1), l, b, r, b);
        }
    }
    edge_cache_count = map_highlight_merge_edge_segments(edge_cache, edge_cache_count);
}

static int focus_point(const RenderSnapshot *snapshot, int civ_id, int *out_x, int *out_y) {
    int min_x = snapshot->map_w, min_y = snapshot->map_h, max_x = -1, max_y = -1;
    int x, y;
    if (!map_highlight_valid_civ(snapshot, civ_id)) return 0;
    if (snapshot->civs[civ_id].focus_valid) {
        *out_x = snapshot->civs[civ_id].focus_x;
        *out_y = snapshot->civs[civ_id].focus_y;
        return 1;
    }
    for (y = 0; y < snapshot->map_h; y++) {
        for (x = 0; x < snapshot->map_w; x++) {
            highlight_focus_last_fallback_tiles++;
            if (tile_owner(snapshot, x, y) == civ_id) {
                if (x < min_x) min_x = x;
                if (y < min_y) min_y = y;
                if (x > max_x) max_x = x;
                if (y > max_y) max_y = y;
            }
        }
    }
    if (max_x < min_x || max_y < min_y) return 0;
    *out_x = (min_x + max_x) / 2;
    *out_y = (min_y + max_y) / 2;
    return 1;
}

static int pulse_elapsed(int start_ms) {
    int elapsed;
    if (start_ms <= 0) return 3000;
    elapsed = (int)GetTickCount() - start_ms;
    return elapsed < 0 ? 3000 : elapsed;
}

static void draw_ring(HDC hdc, HighlightPenPool *pool, int cx, int cy, int r, int width, COLORREF color) {
    HPEN pen = pen_pool_get(pool, color, width);
    HGDIOBJ old_pen = SelectObject(hdc, pen);
    HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Ellipse(hdc, cx - r, cy - r, cx + r, cy + r);
    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    highlight_focus_last_rings++;
}

static void draw_focus_ring(HDC hdc, HighlightPenPool *pool, MapLayout layout,
                            const RenderSnapshot *snapshot, const HighlightRequest *request) {
    int x, y, cx, cy, base, elapsed, phase;
    COLORREF warm, shadow;
    if (!focus_point(snapshot, request->civ_id, &x, &y)) return;
    cx = layout.map_x + (x * 2 + 1) * layout.draw_w / (snapshot->map_w * 2);
    cy = layout.map_y + (y * 2 + 1) * layout.draw_h / (snapshot->map_h * 2);
    base = clamp(layout.tile_size * (request->strong ? 5 : 4), 14, 42);
    elapsed = pulse_elapsed(request->pulse_start);
    warm = map_highlight_mix_color(request->inner, RGB(255, 220, 92), 24);
    shadow = map_highlight_civ_shadow_color(snapshot, request->civ_id);
    draw_ring(hdc, pool, cx, cy, base, (request->strong ? 3 : 2) + request->width_boost,
              request->inner);
    draw_ring(hdc, pool, cx, cy, base + 7, 1 + request->width_boost, shadow);
    if (elapsed < 3000) {
        phase = (elapsed % 1000) * 26 / 1000;
        draw_ring(hdc, pool, cx, cy, base + phase + 8, 2 + request->width_boost,
                  map_highlight_mix_color(request->inner, RGB(255, 255, 255), 18));
        draw_ring(hdc, pool, cx, cy, base + phase / 2 + 18, 1 + request->width_boost, warm);
    }
}

void map_highlight_draw_edge_focus_requests(HDC hdc, RECT client, MapLayout layout,
                                            const RenderSnapshot *snapshot, int min_x,
                                            int max_x, int min_y, int max_y,
                                            const HighlightRequest *requests,
                                            int request_count) {
    const HighlightRequest *owner_requests[MAX_CIVS];
    HighlightPenPool focus_pool = {0};
    DWORD start;
    int i;
    if (request_count <= 0) return;
    start = GetTickCount();
    highlight_edge_calls++;
    highlight_edge_last_requests = request_count;
    build_owner_requests(requests, request_count, owner_requests);
    {
        unsigned int key = edge_request_key(snapshot, layout, min_x, max_x, min_y, max_y,
                                            requests, request_count);
        if (key != edge_cache_key) {
            DWORD geom_start = GetTickCount();
            edge_cache_misses++;
            rebuild_edge_cache(layout, snapshot, min_x, max_x, min_y, max_y, requests, owner_requests);
            highlight_edge_last_geometry_ms = (int)(GetTickCount() - geom_start);
            if (highlight_edge_last_geometry_ms > highlight_edge_peak_geometry_ms) {
                highlight_edge_peak_geometry_ms = highlight_edge_last_geometry_ms;
            }
            edge_cache_key = key;
        } else {
            highlight_edge_last_geometry_ms = 0;
            edge_cache_hits++;
        }
    }
    highlight_edge_last_segments = edge_cache_count;
    highlight_edge_last_pen_creates = map_highlight_edge_layer_draw(hdc, client, edge_cache_key,
                                                                    requests, request_count,
                                                                    edge_cache, edge_cache_count);
    highlight_edge_last_ms = (int)(GetTickCount() - start);
    if (highlight_edge_last_ms > highlight_edge_peak_ms) highlight_edge_peak_ms = highlight_edge_last_ms;

    start = GetTickCount();
    highlight_focus_calls++;
    highlight_focus_last_requests = request_count;
    for (i = 0; i < request_count; i++) {
        if (requests[i].dim || !map_highlight_valid_civ(snapshot, requests[i].civ_id)) continue;
        draw_focus_ring(hdc, &focus_pool, layout, snapshot, &requests[i]);
    }
    highlight_focus_last_pen_creates = focus_pool.creates;
    pen_pool_release(&focus_pool);
    highlight_focus_last_ms = (int)(GetTickCount() - start);
    if (highlight_focus_last_ms > highlight_focus_peak_ms) highlight_focus_peak_ms = highlight_focus_last_ms;
}

int map_highlight_overlay_call_count(void) { return highlight_overlay_calls; }
int map_highlight_overlay_recreate_count(void) { return highlight_overlay_recreates; }
int map_highlight_overlay_reuse_count(void) { return highlight_overlay_reuses; }
int map_highlight_overlay_last_ms(void) { return highlight_overlay_last_ms; }
int map_highlight_overlay_last_tiles(void) { return highlight_overlay_last_tiles; }
int map_highlight_overlay_peak_ms(void) { return highlight_overlay_peak_ms; }
int map_highlight_overlay_total_clears(void) { return highlight_overlay_total_clears; }
int map_highlight_overlay_last_clears(void) { return highlight_overlay_last_clears; }
int map_highlight_overlay_last_requests(void) { return highlight_overlay_last_requests; }
int map_highlight_overlay_last_rects(void) { return highlight_overlay_last_rects; }
int map_highlight_edge_call_count(void) { return highlight_edge_calls; }
int map_highlight_edge_last_requests(void) { return highlight_edge_last_requests; }
int map_highlight_edge_last_tiles(void) { return highlight_edge_last_tiles; }
int map_highlight_edge_last_segments(void) { return highlight_edge_last_segments; }
int map_highlight_edge_last_pen_creates(void) { return highlight_edge_last_pen_creates; }
int map_highlight_edge_last_ms(void) { return highlight_edge_last_ms; }
int map_highlight_edge_peak_ms(void) { return highlight_edge_peak_ms; }
int map_highlight_edge_cache_hits(void) { return edge_cache_hits; }
int map_highlight_edge_cache_misses(void) { return edge_cache_misses; }
int map_highlight_edge_geometry_last_ms(void) { return highlight_edge_last_geometry_ms; }
int map_highlight_edge_geometry_peak_ms(void) { return highlight_edge_peak_geometry_ms; }
int map_highlight_focus_call_count(void) { return highlight_focus_calls; }
int map_highlight_focus_last_requests(void) { return highlight_focus_last_requests; }
int map_highlight_focus_last_rings(void) { return highlight_focus_last_rings; }
int map_highlight_focus_last_pen_creates(void) { return highlight_focus_last_pen_creates; }
int map_highlight_focus_last_fallback_tiles(void) { return highlight_focus_last_fallback_tiles; }
int map_highlight_focus_last_ms(void) { return highlight_focus_last_ms; }
int map_highlight_focus_peak_ms(void) { return highlight_focus_peak_ms; }
int map_highlight_total_paint_count(void) { return highlight_total_paints; }
int map_highlight_total_last_ms(void) { return highlight_total_last_ms; }
int map_highlight_total_peak_ms(void) { return highlight_total_peak_ms; }

void map_highlight_overlay_reset_debug(void) {
    highlight_overlay_calls = highlight_overlay_recreates = highlight_overlay_reuses = 0;
    highlight_overlay_last_ms = highlight_overlay_last_tiles = highlight_overlay_peak_ms = 0;
    highlight_overlay_total_clears = highlight_overlay_last_clears = 0;
    highlight_overlay_last_requests = highlight_overlay_last_rects = 0;
    highlight_edge_calls = highlight_edge_last_requests = highlight_edge_last_tiles = 0;
    highlight_edge_last_segments = highlight_edge_last_pen_creates = 0;
    highlight_edge_last_ms = highlight_edge_peak_ms = 0;
    highlight_edge_last_geometry_ms = highlight_edge_peak_geometry_ms = 0;
    edge_cache_key = 0;
    edge_cache_count = edge_cache_hits = edge_cache_misses = 0;
    map_highlight_edge_layer_reset_debug();
    highlight_focus_calls = highlight_focus_last_requests = highlight_focus_last_rings = 0;
    highlight_focus_last_pen_creates = highlight_focus_last_fallback_tiles = 0;
    highlight_focus_last_ms = highlight_focus_peak_ms = 0;
    highlight_total_paints = highlight_total_start_ms = 0;
    highlight_total_last_ms = highlight_total_peak_ms = 0;
}
