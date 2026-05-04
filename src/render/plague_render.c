#include "render_internal.h"
#include "sim/plague.h"

static unsigned int plague_pixel(COLORREF color, int alpha) {
    unsigned int r = (unsigned int)GetRValue(color) * (unsigned int)alpha / 255;
    unsigned int g = (unsigned int)GetGValue(color) * (unsigned int)alpha / 255;
    unsigned int b = (unsigned int)GetBValue(color) * (unsigned int)alpha / 255;

    return b | (g << 8) | (r << 16) | ((unsigned int)alpha << 24);
}

static void blit_overlay(HDC hdc, HDC surface_dc, MapLayout layout) {
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};

    SetStretchBltMode(hdc, COLORONCOLOR);
    AlphaBlend(hdc, layout.map_x, layout.map_y, layout.draw_w, layout.draw_h,
               surface_dc, 0, 0, MAP_W, MAP_H, blend);
}

void draw_plague_region_overlay(HDC hdc, RECT client, MapLayout layout) {
    BITMAPINFO info;
    unsigned int *pixels;
    HBITMAP bitmap;
    HDC surface_dc;
    HBITMAP old_bitmap;
    int x;
    int y;
    int saved_dc;

    if (layout.draw_w <= 0 || layout.draw_h <= 0) return;
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
    memset(pixels, 0, MAP_W * MAP_H * sizeof(*pixels));

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int severity = plague_tile_severity(x, y);
            if (severity <= 0) continue;
            pixels[y * MAP_W + x] = plague_pixel(RGB(22, 78, 48), clamp(18 + severity * 12, 26, 130));
        }
    }

    saved_dc = SaveDC(hdc);
    IntersectClipRect(hdc, client.left, TOP_BAR_H, client.right - side_panel_w, client.bottom - BOTTOM_BAR_H);
    blit_overlay(hdc, surface_dc, layout);
    RestoreDC(hdc, saved_dc);
    SelectObject(surface_dc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(surface_dc);
}

static RECT circle_rect(int cx, int cy, int radius) {
    RECT rect = {cx - radius, cy - radius, cx + radius, cy + radius};
    return rect;
}

static void draw_city_plague_marker(HDC hdc, int cx, int cy, int severity) {
    int radius = clamp(7 + severity * 3, 10, 42);
    HPEN haze_pen = CreatePen(PS_SOLID, clamp(severity, 2, 8), RGB(56, 128, 78));
    HPEN ring_pen = CreatePen(PS_SOLID, 2, RGB(8, 48, 31));
    HBRUSH core_brush = CreateSolidBrush(RGB(5, 38, 24));
    HBRUSH old_brush;
    HPEN old_pen;
    RECT haze = circle_rect(cx, cy, radius);
    RECT core = circle_rect(cx, cy, clamp(radius / 3, 4, 12));

    old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    old_pen = SelectObject(hdc, haze_pen);
    Ellipse(hdc, haze.left, haze.top, haze.right, haze.bottom);
    SelectObject(hdc, ring_pen);
    Ellipse(hdc, haze.left + 3, haze.top + 3, haze.right - 3, haze.bottom - 3);
    SelectObject(hdc, core_brush);
    Ellipse(hdc, core.left, core.top, core.right, core.bottom);
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    DeleteObject(haze_pen);
    DeleteObject(ring_pen);
    DeleteObject(core_brush);
}

void draw_plague_city_overlay(HDC hdc, RECT client, MapLayout layout) {
    int i;
    int saved_dc;

    saved_dc = SaveDC(hdc);
    IntersectClipRect(hdc, client.left, TOP_BAR_H, client.right - side_panel_w, client.bottom - BOTTOM_BAR_H);
    for (i = 0; i < city_count; i++) {
        int severity = plague_city_severity(i);
        int cx;
        int cy;
        if (severity <= 0) continue;
        cx = (tile_left(layout, cities[i].x) + tile_right(layout, cities[i].x)) / 2;
        cy = (tile_top(layout, cities[i].y) + tile_bottom(layout, cities[i].y)) / 2;
        draw_city_plague_marker(hdc, cx, cy, severity);
    }
    RestoreDC(hdc, saved_dc);
}
