#include "plague_visual.h"

#include "render_internal.h"
#include "sim/plague.h"

static int city_visual[MAX_CITIES];
static int route_visual[MAX_MARITIME_ROUTES];
static int pulse_ms;
static int visual_active;

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
    if (delta == 0) return current;
    return current + delta * step / 260;
}

int plague_visual_tick(int elapsed_ms) {
    int i;
    int changed = 0;
    int any = 0;

    pulse_ms = (pulse_ms + clamp(elapsed_ms, 0, 250)) % 4000;
    for (i = 0; i < MAX_CITIES; i++) {
        int target = i < city_count ? plague_city_severity(i) * 100 : 0;
        int next = approach(city_visual[i], target, elapsed_ms);
        if (next != city_visual[i]) changed = 1;
        city_visual[i] = next;
        if (next > 4) any = 1;
    }
    for (i = 0; i < MAX_MARITIME_ROUTES; i++) {
        int target = i < maritime_route_count ? plague_route_exposure(i) * 100 : 0;
        int next = approach(route_visual[i], target, elapsed_ms);
        if (next != route_visual[i]) changed = 1;
        route_visual[i] = next;
        if (next > 4) any = 1;
    }
    visual_active = any;
    return changed || any;
}

int plague_visual_active(void) {
    return visual_active;
}

int plague_visual_route_intensity(int route_id) {
    if (route_id < 0 || route_id >= MAX_MARITIME_ROUTES) return 0;
    return route_visual[route_id];
}

static int blob_offset(int seed, int radius) {
    int value = (seed * 1103515245 + 12345) & 0x7fffffff;
    return radius ? value % (radius * 2 + 1) - radius : 0;
}

static void draw_blob(unsigned int *pixels, int cx, int cy, int radius, int intensity, int seed) {
    COLORREF color = intensity > 650 ? RGB(4, 42, 25) : (intensity > 330 ? RGB(10, 70, 38) : RGB(22, 90, 48));
    int max_alpha = clamp(24 + intensity / 8, 28, 155);
    int pulse = abs((pulse_ms / 30 + seed * 7) % 80 - 40);
    int r = clamp(radius + pulse / 8, 5, 72);
    int x;
    int y;

    for (y = max(cy - r, 0); y <= min(cy + r, MAP_H - 1); y++) {
        for (x = max(cx - r, 0); x <= min(cx + r, MAP_W - 1); x++) {
            int dx = x - cx;
            int dy = y - cy;
            int dist2 = dx * dx + dy * dy;
            int r2 = r * r;
            int alpha;
            if (dist2 > r2) continue;
            alpha = max_alpha * (r2 - dist2) / r2;
            if (alpha > 0) blend_pixel(&pixels[y * MAP_W + x], color, alpha);
        }
    }
}

static void add_city_cloud(unsigned int *pixels, int city_id) {
    int intensity = city_visual[city_id];
    int radius = clamp(9 + intensity / 28 + cities[city_id].radius, 8, 58);
    int blobs = clamp(4 + intensity / 180, 4, 10);
    int i;

    if (intensity <= 4 || !cities[city_id].alive) return;
    for (i = 0; i < blobs; i++) {
        int seed = city_id * 97 + i * 37;
        int bx = cities[city_id].x + blob_offset(seed, radius / 2);
        int by = cities[city_id].y + blob_offset(seed + 19, radius / 2);
        int br = clamp(radius / 2 + abs(blob_offset(seed + 41, radius / 2)), 5, radius);
        draw_blob(pixels, clamp(bx, 0, MAP_W - 1), clamp(by, 0, MAP_H - 1), br, intensity, seed);
    }
}

void draw_plague_visual_regions(HDC hdc, RECT client, MapLayout layout) {
    BITMAPINFO info;
    unsigned int *pixels = NULL;
    HBITMAP bitmap;
    HBITMAP old_bitmap;
    HDC surface_dc;
    int i;
    int saved_dc;
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};

    if (!visual_active || layout.draw_w <= 0 || layout.draw_h <= 0) return;
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = MAP_W;
    info.bmiHeader.biHeight = -MAP_H;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    surface_dc = CreateCompatibleDC(hdc);
    bitmap = CreateDIBSection(hdc, &info, DIB_RGB_COLORS, (void **)&pixels, NULL, 0);
    if (!surface_dc || !bitmap || !pixels) {
        if (bitmap) DeleteObject(bitmap);
        if (surface_dc) DeleteDC(surface_dc);
        return;
    }
    old_bitmap = SelectObject(surface_dc, bitmap);
    memset(pixels, 0, (size_t)MAP_W * MAP_H * sizeof(*pixels));
    for (i = 0; i < city_count; i++) add_city_cloud(pixels, i);
    saved_dc = SaveDC(hdc);
    IntersectClipRect(hdc, client.left, TOP_BAR_H, client.right - side_panel_w, client.bottom - BOTTOM_BAR_H);
    SetStretchBltMode(hdc, HALFTONE);
    AlphaBlend(hdc, layout.map_x, layout.map_y, layout.draw_w, layout.draw_h,
               surface_dc, 0, 0, MAP_W, MAP_H, blend);
    RestoreDC(hdc, saved_dc);
    SelectObject(surface_dc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(surface_dc);
}

static RECT circle_rect(int cx, int cy, int radius) {
    RECT rect = {cx - radius, cy - radius, cx + radius, cy + radius};
    return rect;
}

void draw_plague_visual_cities(HDC hdc, RECT client, MapLayout layout) {
    int i;
    int saved_dc;

    if (!visual_active) return;
    saved_dc = SaveDC(hdc);
    IntersectClipRect(hdc, client.left, TOP_BAR_H, client.right - side_panel_w, client.bottom - BOTTOM_BAR_H);
    for (i = 0; i < city_count; i++) {
        int intensity = city_visual[i];
        int cx;
        int cy;
        int radius;
        HBRUSH brush;
        HPEN pen;
        HBRUSH old_brush;
        HPEN old_pen;
        if (intensity <= 25 || !cities[i].alive) continue;
        cx = (tile_left(layout, cities[i].x) + tile_right(layout, cities[i].x)) / 2;
        cy = (tile_top(layout, cities[i].y) + tile_bottom(layout, cities[i].y)) / 2;
        radius = clamp(5 + intensity / 65, 5, 22);
        brush = CreateSolidBrush(RGB(5, 38, 24));
        pen = CreatePen(PS_SOLID, 2, RGB(36, 120, 62));
        old_brush = SelectObject(hdc, brush);
        old_pen = SelectObject(hdc, pen);
        Ellipse(hdc, circle_rect(cx, cy, radius).left, circle_rect(cx, cy, radius).top,
                circle_rect(cx, cy, radius).right, circle_rect(cx, cy, radius).bottom);
        SelectObject(hdc, old_pen);
        SelectObject(hdc, old_brush);
        DeleteObject(pen);
        DeleteObject(brush);
    }
    RestoreDC(hdc, saved_dc);
}
