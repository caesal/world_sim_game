#include "cartography_layers.h"

#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "render/contour_paths.h"
#include "sim/regions.h"
#include "world/terrain_query.h"

typedef struct {
    HDC dc;
    HBITMAP bitmap;
    HBITMAP old_bitmap;
    unsigned int *pixels;
    int width;
    int height;
    int scale;
    int revision;
    int display;
    int valid;
} CartoCache;

static CartoCache political_cache;
static CartoCache coast_halo_cache;
static unsigned char mask_a[MAX_MAP_W * MAX_MAP_H];
static unsigned char mask_b[MAX_MAP_W * MAX_MAP_H];

static void release_cache(CartoCache *cache) {
    if (cache->dc && cache->old_bitmap) SelectObject(cache->dc, cache->old_bitmap);
    if (cache->bitmap) DeleteObject(cache->bitmap);
    if (cache->dc) DeleteDC(cache->dc);
    memset(cache, 0, sizeof(*cache));
}

static int ensure_cache(HDC hdc, CartoCache *cache, int scale) {
    BITMAPINFO info;
    void *bits = NULL;
    int width = MAP_W * scale;
    int height = MAP_H * scale;

    if (cache->dc && cache->bitmap && cache->pixels &&
        cache->width == width && cache->height == height && cache->scale == scale) return 1;
    release_cache(cache);
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    cache->dc = CreateCompatibleDC(hdc);
    cache->bitmap = CreateDIBSection(hdc, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!cache->dc || !cache->bitmap || !bits) {
        release_cache(cache);
        return 0;
    }
    cache->old_bitmap = SelectObject(cache->dc, cache->bitmap);
    cache->pixels = (unsigned int *)bits;
    cache->width = width;
    cache->height = height;
    cache->scale = scale;
    return 1;
}

static int cache_current(const CartoCache *cache, int display_sensitive, int revision) {
    return cache->valid && cache->revision == revision &&
           (!display_sensitive || cache->display == display_mode);
}

static void clear_cache(CartoCache *cache, int revision) {
    memset(cache->pixels, 0, (size_t)cache->width * cache->height * sizeof(unsigned int));
    cache->revision = revision;
    cache->display = display_mode;
    cache->valid = 1;
}

static void present_cache(HDC hdc, CartoCache *cache, MapLayout layout) {
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};

    SetStretchBltMode(hdc, layout.tile_size <= 2 ? HALFTONE : COLORONCOLOR);
    SetBrushOrgEx(hdc, 0, 0, NULL);
    AlphaBlend(hdc, layout.map_x, layout.map_y, layout.draw_w, layout.draw_h,
               cache->dc, 0, 0, cache->width, cache->height, blend);
}

static unsigned int premul(COLORREF color, int alpha) {
    unsigned int r = (unsigned int)(GetRValue(color) * alpha / 255);
    unsigned int g = (unsigned int)(GetGValue(color) * alpha / 255);
    unsigned int b = (unsigned int)(GetBValue(color) * alpha / 255);
    return b | (g << 8) | (r << 16) | ((unsigned int)alpha << 24);
}

static void blend_pixel(unsigned int *dst, COLORREF color, int alpha) {
    unsigned int old = *dst;
    int inv = 255 - alpha;
    int b = GetBValue(color) * alpha / 255 + (int)(old & 255) * inv / 255;
    int g = GetGValue(color) * alpha / 255 + (int)((old >> 8) & 255) * inv / 255;
    int r = GetRValue(color) * alpha / 255 + (int)((old >> 16) & 255) * inv / 255;
    int a = alpha + (int)(old >> 24) * inv / 255;
    *dst = (unsigned int)b | ((unsigned int)g << 8) | ((unsigned int)r << 16) | ((unsigned int)a << 24);
}

static int alive_owner(int owner) {
    return owner >= 0 && owner < civ_count && civs[owner].alive;
}

static void build_land_mask(void) {
    int x, y;
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) mask_a[y * MAP_W + x] = is_land(world[y][x].geography) ? 255 : 0;
    }
}

static void blur_once(const unsigned char *src, unsigned char *dst) {
    int x, y;
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int sum = 0;
            int dy, dx;
            for (dy = -1; dy <= 1; dy++) {
                int sy = clamp(y + dy, 0, MAP_H - 1);
                for (dx = -1; dx <= 1; dx++) {
                    int sx = clamp(x + dx, 0, MAP_W - 1);
                    sum += src[sy * MAP_W + sx];
                }
            }
            dst[y * MAP_W + x] = (unsigned char)(sum / 9);
        }
    }
}

static void blur_mask(int passes) {
    int i;
    for (i = 0; i < passes; i++) {
        blur_once(mask_a, mask_b);
        memcpy(mask_a, mask_b, (size_t)MAP_W * MAP_H);
    }
}

static void filter_mask(int passes, int fill_threshold, int keep_threshold) {
    int pass;
    for (pass = 0; pass < passes; pass++) {
        int x, y;
        for (y = 0; y < MAP_H; y++) {
            for (x = 0; x < MAP_W; x++) {
                int neighbors = 0;
                int active = mask_a[y * MAP_W + x] >= 128;
                int dy, dx;
                for (dy = -1; dy <= 1; dy++) {
                    int sy = clamp(y + dy, 0, MAP_H - 1);
                    for (dx = -1; dx <= 1; dx++) {
                        int sx = clamp(x + dx, 0, MAP_W - 1);
                        if (mask_a[sy * MAP_W + sx] >= 128) neighbors++;
                    }
                }
                mask_b[y * MAP_W + x] = (active ? neighbors >= keep_threshold :
                                                   neighbors >= fill_threshold) ? 255 : 0;
            }
        }
        memcpy(mask_a, mask_b, (size_t)MAP_W * MAP_H);
    }
}

static void rebuild_political(HDC hdc) {
    int x;
    int y;
    int alpha = display_mode == DISPLAY_POLITICAL ? 144 : 88;
    if (!ensure_cache(hdc, &political_cache, 1)) return;
    clear_cache(&political_cache, dirty_revision_ownership());
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int owner = world[y][x].owner;
            if (alive_owner(owner)) {
                blend_pixel(&political_cache.pixels[y * MAP_W + x], civs[owner].color, alpha);
            }
        }
    }
}

static void rebuild_coast_halo(HDC hdc) {
    int x, y;
    if (!ensure_cache(hdc, &coast_halo_cache, 1)) return;
    clear_cache(&coast_halo_cache, dirty_revision_coast());
    build_land_mask();
    filter_mask(1, 6, 3);
    blur_mask(2);
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int idx = y * MAP_W + x;
            int alpha;
            if (is_land(world[y][x].geography)) continue;
            alpha = clamp(mask_a[idx] - 28, 0, 96) * 18 / 96;
            if (alpha > 0) coast_halo_cache.pixels[idx] = premul(RGB(142, 190, 177), alpha);
        }
    }
}

void draw_cartography_political_fills(HDC hdc, RECT client, MapLayout layout) {
    (void)client;
    if (display_mode != DISPLAY_ALL && display_mode != DISPLAY_POLITICAL) return;
    if (dirty_render_political() ||
        !cache_current(&political_cache, 1, dirty_revision_ownership())) rebuild_political(hdc);
    if (political_cache.valid) present_cache(hdc, &political_cache, layout);
}

void draw_cartography_coast_halo(HDC hdc, RECT client, MapLayout layout) {
    (void)client;
    if (!cache_current(&coast_halo_cache, 0, dirty_revision_coast())) rebuild_coast_halo(hdc);
    if (coast_halo_cache.valid) present_cache(hdc, &coast_halo_cache, layout);
}

void draw_cartography_region_borders(HDC hdc, RECT client, MapLayout layout) {
    if (display_mode != DISPLAY_REGIONS && !(display_mode == DISPLAY_ALL && civ_count == 0)) return;
    contour_paths_draw_region_borders(hdc, client, layout);
}

void draw_cartography_province_borders(HDC hdc, RECT client, MapLayout layout) {
    if (layout.tile_size < 7) return;
    contour_paths_draw_province_borders(hdc, client, layout);
}

void draw_cartography_country_borders(HDC hdc, RECT client, MapLayout layout) {
    contour_paths_draw_country_borders(hdc, client, layout);
}

void draw_cartography_coastline(HDC hdc, RECT client, MapLayout layout) {
    contour_paths_draw_coastline(hdc, client, layout);
}

void draw_map_grid_overlay(HDC hdc, RECT client, MapLayout layout) {
    int step = 100;
    int i;

    (void)client;
    for (i = step; i < MAP_W; i += step) {
        int sx = layout.map_x + i * layout.draw_w / MAP_W;
        RECT line = {sx, layout.map_y, sx + 1, layout.map_y + layout.draw_h};
        fill_rect_alpha(hdc, line, RGB(210, 214, 196), layout.tile_size <= 2 ? 30 : 22);
    }
    for (i = step; i < MAP_H; i += step) {
        int sy = layout.map_y + i * layout.draw_h / MAP_H;
        RECT line = {layout.map_x, sy, layout.map_x + layout.draw_w, sy + 1};
        fill_rect_alpha(hdc, line, RGB(210, 214, 196), layout.tile_size <= 2 ? 28 : 20);
    }
}
