#include "render/diplomacy_map_marker_cache.h"

#include <string.h>

#define MARKER_CACHE_SIZE 12
#define MARKER_SIZE 24
#define MARKER_KEY RGB(255, 0, 255)

typedef struct {
    int valid;
    COLORREF color;
    IconId icon;
    HDC dc;
    HBITMAP bitmap;
    HGDIOBJ old_bitmap;
} CachedMarker;

static CachedMarker markers[MARKER_CACHE_SIZE];
static int next_marker;

static COLORREF mix_marker_color(COLORREF a, COLORREF b, int percent_b) {
    int percent_a = 100 - percent_b;
    int r = (GetRValue(a) * percent_a + GetRValue(b) * percent_b) / 100;
    int g = (GetGValue(a) * percent_a + GetGValue(b) * percent_b) / 100;
    int bl = (GetBValue(a) * percent_a + GetBValue(b) * percent_b) / 100;
    return RGB(r, g, bl);
}

static void release_marker(CachedMarker *marker) {
    if (marker->dc && marker->old_bitmap) SelectObject(marker->dc, marker->old_bitmap);
    if (marker->bitmap) DeleteObject(marker->bitmap);
    if (marker->dc) DeleteDC(marker->dc);
    memset(marker, 0, sizeof(*marker));
}

static CachedMarker *find_marker(COLORREF color, IconId icon) {
    int i;
    for (i = 0; i < MARKER_CACHE_SIZE; i++) {
        if (markers[i].valid && markers[i].color == color && markers[i].icon == icon) return &markers[i];
    }
    return NULL;
}

static CachedMarker *claim_marker(void) {
    CachedMarker *marker = &markers[next_marker++ % MARKER_CACHE_SIZE];
    release_marker(marker);
    return marker;
}

static int render_marker(HDC hdc, CachedMarker *marker, COLORREF color, IconId icon) {
    RECT outer = {1, 1, MARKER_SIZE - 1, MARKER_SIZE - 1};
    RECT inner = {4, 4, MARKER_SIZE - 4, MARKER_SIZE - 4};
    RECT all = {0, 0, MARKER_SIZE, MARKER_SIZE};
    HBRUSH bg, brush;
    HPEN pen;
    HGDIOBJ old_brush, old_pen;
    marker->dc = CreateCompatibleDC(hdc);
    marker->bitmap = CreateCompatibleBitmap(hdc, MARKER_SIZE, MARKER_SIZE);
    if (!marker->dc || !marker->bitmap) {
        release_marker(marker);
        return 0;
    }
    marker->old_bitmap = SelectObject(marker->dc, marker->bitmap);
    bg = CreateSolidBrush(MARKER_KEY);
    FillRect(marker->dc, &all, bg);
    DeleteObject(bg);
    brush = CreateSolidBrush(mix_marker_color(color, RGB(26, 24, 22), 28));
    pen = CreatePen(PS_SOLID, 1, mix_marker_color(color, RGB(34, 28, 24), 55));
    old_brush = SelectObject(marker->dc, brush);
    old_pen = SelectObject(marker->dc, pen);
    Ellipse(marker->dc, outer.left, outer.top, outer.right, outer.bottom);
    SelectObject(marker->dc, old_pen);
    SelectObject(marker->dc, old_brush);
    DeleteObject(pen);
    DeleteObject(brush);
    draw_icon_fit(marker->dc, icon, inner, RGB(255, 248, 214));
    marker->color = color;
    marker->icon = icon;
    marker->valid = 1;
    return 1;
}

void diplomacy_map_marker_cache_draw(HDC hdc, POINT center, COLORREF color, IconId icon) {
    CachedMarker *marker = find_marker(color, icon);
    if (!marker) {
        marker = claim_marker();
        if (!render_marker(hdc, marker, color, icon)) return;
    }
    TransparentBlt(hdc, center.x - MARKER_SIZE / 2, center.y - MARKER_SIZE / 2,
                   MARKER_SIZE, MARKER_SIZE, marker->dc, 0, 0,
                   MARKER_SIZE, MARKER_SIZE, MARKER_KEY);
}

void diplomacy_map_marker_cache_reset(void) {
    int i;
    for (i = 0; i < MARKER_CACHE_SIZE; i++) release_marker(&markers[i]);
    next_marker = 0;
}
