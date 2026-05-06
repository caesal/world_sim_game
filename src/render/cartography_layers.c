#include "cartography_layers.h"

#include "core/game_types.h"
#include "sim/regions.h"
#include "world/terrain_query.h"

#define CARTO_CACHE_SCALE 3

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
static CartoCache region_cache;
static CartoCache province_cache;
static CartoCache country_cache;
static CartoCache coastline_cache;
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

static int cache_current(const CartoCache *cache, int display_sensitive) {
    return cache->valid && cache->revision == world_visual_revision &&
           (!display_sensitive || cache->display == display_mode);
}

static void clear_cache(CartoCache *cache) {
    memset(cache->pixels, 0, (size_t)cache->width * cache->height * sizeof(unsigned int));
    cache->revision = world_visual_revision;
    cache->display = display_mode;
    cache->valid = 1;
}

static void present_cache(HDC hdc, CartoCache *cache, MapLayout layout) {
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};

    SetStretchBltMode(hdc, HALFTONE);
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

static void build_owner_mask(int owner) {
    int x, y;
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int active = owner >= 0 && owner < civ_count && civs[owner].alive && world[y][x].owner == owner;
            mask_a[y * MAP_W + x] = active ? 255 : 0;
        }
    }
}

static void build_province_mask(int province_id) {
    int x, y;
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int active = province_id >= 0 && province_id < city_count && cities[province_id].alive &&
                         world[y][x].province_id == province_id;
            mask_a[y * MAP_W + x] = active ? 255 : 0;
        }
    }
}

static void build_region_mask(int region_id) {
    int x, y;
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            mask_a[y * MAP_W + x] = world[y][x].region_id == region_id ? 255 : 0;
        }
    }
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

static float iso_t(int a, int b) {
    if (a == b) return 0.5f;
    return (float)clamp((128 - a) * 1000 / (b - a), 0, 1000) / 1000.0f;
}

static POINT edge_point(int x, int y, int edge, int v[4], int scale) {
    POINT p = {x * scale, y * scale};
    if (edge == 0) p.x += (int)(iso_t(v[0], v[1]) * scale);
    else if (edge == 1) { p.x += scale; p.y += (int)(iso_t(v[1], v[2]) * scale); }
    else if (edge == 2) { p.x += (int)(iso_t(v[3], v[2]) * scale); p.y += scale; }
    else p.y += (int)(iso_t(v[0], v[3]) * scale);
    return p;
}

static void draw_iso_mask(HDC hdc, int scale, COLORREF color, int width) {
    HPEN pen = CreatePen(PS_SOLID, max(1, width), color);
    HPEN old_pen = SelectObject(hdc, pen);
    int x, y;
    for (y = 0; y < MAP_H - 1; y++) {
        for (x = 0; x < MAP_W - 1; x++) {
            int v[4], edges[4], count = 0, e;
            v[0] = mask_a[y * MAP_W + x];
            v[1] = mask_a[y * MAP_W + x + 1];
            v[2] = mask_a[(y + 1) * MAP_W + x + 1];
            v[3] = mask_a[(y + 1) * MAP_W + x];
            for (e = 0; e < 4; e++) {
                int a = v[e], b = v[(e + 1) % 4];
                if ((a < 128 && b >= 128) || (a >= 128 && b < 128)) edges[count++] = e;
            }
            if (count >= 2) {
                POINT a = edge_point(x, y, edges[0], v, scale);
                POINT b = edge_point(x, y, edges[1], v, scale);
                MoveToEx(hdc, a.x, a.y, NULL);
                LineTo(hdc, b.x, b.y);
            }
            if (count == 4) {
                POINT a = edge_point(x, y, edges[2], v, scale);
                POINT b = edge_point(x, y, edges[3], v, scale);
                MoveToEx(hdc, a.x, a.y, NULL);
                LineTo(hdc, b.x, b.y);
            }
        }
    }
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
}

static void apply_alpha(CartoCache *cache, int alpha) {
    int total = cache->width * cache->height;
    int i;
    for (i = 0; i < total; i++) {
        unsigned int rgb = cache->pixels[i] & 0x00ffffff;
        if (rgb) {
            unsigned int b = (rgb & 255) * (unsigned int)alpha / 255;
            unsigned int g = ((rgb >> 8) & 255) * (unsigned int)alpha / 255;
            unsigned int r = ((rgb >> 16) & 255) * (unsigned int)alpha / 255;
            cache->pixels[i] = b | (g << 8) | (r << 16) | ((unsigned int)alpha << 24);
        }
    }
}

static void apply_mask_overlay(CartoCache *cache, COLORREF color, int max_alpha) {
    int i;
    for (i = 0; i < MAP_W * MAP_H; i++) {
        int alpha = mask_a[i] * max_alpha / 255;
        if (alpha > 0) blend_pixel(&cache->pixels[i], color, alpha);
    }
}

static void rebuild_political(HDC hdc) {
    int i;
    if (!ensure_cache(hdc, &political_cache, 1)) return;
    clear_cache(&political_cache);
    for (i = 0; i < civ_count; i++) {
        if (!civs[i].alive) continue;
        build_owner_mask(i);
        blur_mask(1);
        apply_mask_overlay(&political_cache, civs[i].color, 42);
    }
}

static void rebuild_coast_halo(HDC hdc) {
    int x, y;
    if (!ensure_cache(hdc, &coast_halo_cache, 1)) return;
    clear_cache(&coast_halo_cache);
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

static void rebuild_region_borders(HDC hdc) {
    int i;
    COLORREF color = display_mode == DISPLAY_REGIONS ? RGB(42, 49, 42) : RGB(72, 82, 72);
    if (!ensure_cache(hdc, &region_cache, CARTO_CACHE_SCALE)) return;
    clear_cache(&region_cache);
    SetBkMode(region_cache.dc, TRANSPARENT);
    for (i = 0; i < region_count; i++) {
        if (natural_regions[i].tile_count <= 0) continue;
        build_region_mask(i);
        filter_mask(1, 6, 3);
        blur_mask(1);
        draw_iso_mask(region_cache.dc, CARTO_CACHE_SCALE, color, 1);
    }
    apply_alpha(&region_cache, display_mode == DISPLAY_REGIONS ? 84 : 46);
}

static void rebuild_provinces(HDC hdc) {
    int i;
    if (!ensure_cache(hdc, &province_cache, CARTO_CACHE_SCALE)) return;
    clear_cache(&province_cache);
    SetBkMode(province_cache.dc, TRANSPARENT);
    for (i = 0; i < city_count; i++) {
        if (!cities[i].alive || cities[i].owner < 0) continue;
        build_province_mask(i);
        blur_mask(1);
        draw_iso_mask(province_cache.dc, CARTO_CACHE_SCALE, RGB(72, 69, 52), 1);
    }
    apply_alpha(&province_cache, 42);
}

static void rebuild_countries(HDC hdc) {
    int i;
    if (!ensure_cache(hdc, &country_cache, CARTO_CACHE_SCALE)) return;
    clear_cache(&country_cache);
    SetBkMode(country_cache.dc, TRANSPARENT);
    for (i = 0; i < civ_count; i++) {
        if (!civs[i].alive) continue;
        build_owner_mask(i);
        filter_mask(1, 6, 3);
        blur_mask(2);
        draw_iso_mask(country_cache.dc, CARTO_CACHE_SCALE, RGB(42, 38, 31), 2);
        draw_iso_mask(country_cache.dc, CARTO_CACHE_SCALE,
                      blend_color(civs[i].color, RGB(50, 40, 32), 22), 1);
    }
    apply_alpha(&country_cache, 108);
}

static void rebuild_coastline(HDC hdc) {
    if (!ensure_cache(hdc, &coastline_cache, CARTO_CACHE_SCALE)) return;
    clear_cache(&coastline_cache);
    SetBkMode(coastline_cache.dc, TRANSPARENT);
    build_land_mask();
    filter_mask(1, 6, 3);
    blur_mask(2);
    draw_iso_mask(coastline_cache.dc, CARTO_CACHE_SCALE, RGB(24, 45, 39), 2);
    draw_iso_mask(coastline_cache.dc, CARTO_CACHE_SCALE, RGB(67, 113, 96), 1);
    apply_alpha(&coastline_cache, 96);
}

void draw_cartography_political_fills(HDC hdc, RECT client, MapLayout layout) {
    (void)client;
    if (display_mode != DISPLAY_POLITICAL) return;
    if (!cache_current(&political_cache, 1)) rebuild_political(hdc);
    if (political_cache.valid) present_cache(hdc, &political_cache, layout);
}

void draw_cartography_coast_halo(HDC hdc, RECT client, MapLayout layout) {
    (void)client;
    if (!cache_current(&coast_halo_cache, 0)) rebuild_coast_halo(hdc);
    if (coast_halo_cache.valid) present_cache(hdc, &coast_halo_cache, layout);
}

void draw_cartography_region_borders(HDC hdc, RECT client, MapLayout layout) {
    (void)client;
    if (display_mode != DISPLAY_REGIONS && !(display_mode == DISPLAY_ALL && civ_count == 0)) return;
    if (!cache_current(&region_cache, 1)) rebuild_region_borders(hdc);
    if (region_cache.valid) present_cache(hdc, &region_cache, layout);
}

void draw_cartography_province_borders(HDC hdc, RECT client, MapLayout layout) {
    (void)client;
    if (layout.tile_size < 7) return;
    if (!cache_current(&province_cache, 0)) rebuild_provinces(hdc);
    if (province_cache.valid) present_cache(hdc, &province_cache, layout);
}

void draw_cartography_country_borders(HDC hdc, RECT client, MapLayout layout) {
    (void)client;
    if (!cache_current(&country_cache, 0)) rebuild_countries(hdc);
    if (country_cache.valid) present_cache(hdc, &country_cache, layout);
}

void draw_cartography_coastline(HDC hdc, RECT client, MapLayout layout) {
    (void)client;
    if (!cache_current(&coastline_cache, 0)) rebuild_coastline(hdc);
    if (coastline_cache.valid) present_cache(hdc, &coastline_cache, layout);
}
