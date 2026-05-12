#include "plague_visual.h"

#include "core/dirty_flags.h"
#include "core/render_snapshot.h"
#include "render_map_internal.h"
#include "render/render_context.h"

#include <stdlib.h>
#include <string.h>

static int city_visual[MAX_CITIES];
static int route_visual[MAX_SEA_LANES];
static int visual_active;
static int fog_cache_dirty = 1;
static int fog_cache_w;
static int fog_cache_h;
static int fog_cache_alpha = -1;
static HDC fog_cache_dc;
static HBITMAP fog_cache_bitmap;
static HBITMAP fog_cache_old_bitmap;
static unsigned int *fog_cache_pixels;
static int visual_tick_accum_ms;

static void blend_pixel(unsigned int *dst, COLORREF color, int alpha) {
    unsigned int old = *dst;
    int inv = 255 - alpha;
    int b = GetBValue(color) * alpha / 255 + (int)(old & 255) * inv / 255;
    int g = GetGValue(color) * alpha / 255 + (int)((old >> 8) & 255) * inv / 255;
    int r = GetRValue(color) * alpha / 255 + (int)((old >> 16) & 255) * inv / 255;
    int a = alpha + (int)(old >> 24) * inv / 255;
    *dst = (unsigned int)b | ((unsigned int)g << 8) | ((unsigned int)r << 16) | ((unsigned int)a << 24);
}

static int approach(int current, int target, int elapsed_ms) {
    int step = clamp(elapsed_ms, 16, 120);
    int delta = target - current;
    int move;

    if (delta == 0) return current;
    move = delta * step / 260;
    if (move == 0) move = delta > 0 ? 1 : -1;
    if (abs(move) > abs(delta)) return target;
    return current + move;
}

int plague_visual_tick(int elapsed_ms) {
    const RenderSnapshot *snapshot;
    int i;
    int changed = 0;
    int any = 0;

    visual_tick_accum_ms += clamp(elapsed_ms, 0, 250);
    if (visual_tick_accum_ms < 100) return 0;
    elapsed_ms = min(visual_tick_accum_ms, 250);
    visual_tick_accum_ms = 0;
    snapshot = render_snapshot_acquire();

    for (i = 0; i < MAX_CITIES; i++) {
        int target = snapshot && i < snapshot->city_count ? snapshot->plague_city_severity[i] * 100 : 0;
        int next = approach(city_visual[i], target, elapsed_ms);
        if (next != city_visual[i]) changed = 1;
        city_visual[i] = next;
        if (next > 4) any = 1;
    }
    for (i = 0; i < MAX_SEA_LANES; i++) {
        int target = snapshot && i < snapshot->lane_count ? snapshot->plague_lane_exposure[i] * 100 : 0;
        int next = approach(route_visual[i], target, elapsed_ms);
        if (next != route_visual[i]) changed = 1;
        route_visual[i] = next;
        if (next > 4) any = 1;
    }
    render_snapshot_release(snapshot);
    visual_active = any;
    if (changed) fog_cache_dirty = 1;
    return changed;
}

int plague_visual_active(void) {
    return visual_active;
}

int plague_visual_route_intensity(int route_id) {
    if (route_id < 0 || route_id >= MAX_SEA_LANES) return 0;
    return route_visual[route_id];
}

static int blob_offset(int seed, int radius) {
    int value = (seed * 1103515245 + 12345) & 0x7fffffff;
    return radius ? value % (radius * 2 + 1) - radius : 0;
}

static void draw_blob(unsigned int *pixels, int cx, int cy, int radius, int intensity) {
    COLORREF color = intensity > 650 ? RGB(4, 42, 25) : (intensity > 330 ? RGB(10, 70, 38) : RGB(22, 90, 48));
    int base_alpha = clamp(24 + intensity / 8, 28, 155);
    int max_alpha = clamp(base_alpha * clamp(plague_fog_alpha, 0, 100) / 100, 0, 255);
    int r = clamp(radius, 5, 72);
    int x;
    int y;

    if (max_alpha <= 0) return;
    for (y = max(cy - r, 0); y <= min(cy + r, fog_cache_h - 1); y++) {
        for (x = max(cx - r, 0); x <= min(cx + r, fog_cache_w - 1); x++) {
            int dx = x - cx;
            int dy = y - cy;
            int dist2 = dx * dx + dy * dy;
            int r2 = r * r;
            int alpha;
            if (x >= fog_cache_w || y >= fog_cache_h || dist2 > r2) continue;
            alpha = max_alpha * (r2 - dist2) / r2;
            if (alpha > 0) blend_pixel(&pixels[y * fog_cache_w + x], color, alpha);
        }
    }
}

static void add_city_cloud(unsigned int *pixels, const RenderSnapshot *snapshot, int city_id) {
    int intensity = city_visual[city_id];
    const SnapshotCity *city = &snapshot->cities[city_id];
    int radius = clamp(9 + intensity / 28 + city->radius, 8, 58);
    int blobs = clamp(4 + intensity / 180, 4, 10);
    int i;

    if (intensity <= 4 || !city->alive) return;
    for (i = 0; i < blobs; i++) {
        int seed = city_id * 97 + i * 37;
        int bx = city->x + blob_offset(seed, radius / 2);
        int by = city->y + blob_offset(seed + 19, radius / 2);
        int br = clamp(radius / 2 + abs(blob_offset(seed + 41, radius / 2)), 5, radius);
        draw_blob(pixels, clamp(bx, 0, fog_cache_w - 1), clamp(by, 0, fog_cache_h - 1), br, intensity);
    }
}

static void release_fog_cache(void) {
    if (fog_cache_dc && fog_cache_old_bitmap) SelectObject(fog_cache_dc, fog_cache_old_bitmap);
    if (fog_cache_bitmap) DeleteObject(fog_cache_bitmap);
    if (fog_cache_dc) DeleteDC(fog_cache_dc);
    fog_cache_dc = NULL;
    fog_cache_bitmap = NULL;
    fog_cache_old_bitmap = NULL;
    fog_cache_pixels = NULL;
    fog_cache_w = 0;
    fog_cache_h = 0;
    fog_cache_alpha = -1;
    fog_cache_dirty = 1;
}

static int ensure_fog_cache(HDC hdc, const RenderSnapshot *snapshot) {
    BITMAPINFO info;

    if (fog_cache_dc && fog_cache_bitmap &&
        fog_cache_w == snapshot->map_w && fog_cache_h == snapshot->map_h) return 1;
    release_fog_cache();
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = snapshot->map_w;
    info.bmiHeader.biHeight = -snapshot->map_h;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    fog_cache_dc = CreateCompatibleDC(hdc);
    fog_cache_bitmap = CreateDIBSection(hdc, &info, DIB_RGB_COLORS, (void **)&fog_cache_pixels, NULL, 0);
    if (!fog_cache_dc || !fog_cache_bitmap || !fog_cache_pixels) {
        release_fog_cache();
        return 0;
    }
    fog_cache_old_bitmap = SelectObject(fog_cache_dc, fog_cache_bitmap);
    fog_cache_w = snapshot->map_w;
    fog_cache_h = snapshot->map_h;
    return 1;
}

static void rebuild_fog_cache(const RenderSnapshot *snapshot) {
    int i;

    if (!fog_cache_pixels) return;
    memset(fog_cache_pixels, 0, (size_t)fog_cache_w * fog_cache_h * sizeof(*fog_cache_pixels));
    for (i = 0; i < snapshot->city_count; i++) add_city_cloud(fog_cache_pixels, snapshot, i);
    fog_cache_alpha = plague_fog_alpha;
    fog_cache_dirty = 0;
}

void draw_plague_visual_regions(HDC hdc, RECT client, MapLayout layout) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    int saved_dc;
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};

    if (!snapshot || !snapshot->world_generated) return;
    if (!visual_active || plague_fog_alpha <= 0 || layout.draw_w <= 0 || layout.draw_h <= 0) return;
    if (!ensure_fog_cache(hdc, snapshot)) return;
    if (fog_cache_dirty || fog_cache_alpha != plague_fog_alpha || dirty_render_plague()) rebuild_fog_cache(snapshot);
    saved_dc = SaveDC(hdc);
    IntersectClipRect(hdc, client.left, TOP_BAR_H, client.right - side_panel_w, client.bottom - BOTTOM_BAR_H);
    SetStretchBltMode(hdc, HALFTONE);
    AlphaBlend(hdc, layout.map_x, layout.map_y, layout.draw_w, layout.draw_h,
               fog_cache_dc, 0, 0, fog_cache_w, fog_cache_h, blend);
    RestoreDC(hdc, saved_dc);
}
