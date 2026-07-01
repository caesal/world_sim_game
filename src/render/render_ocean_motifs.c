#include "render/render_ocean_motifs.h"

#define MOTIF_TRANSPARENT RGB(255, 0, 255)
#define MOTIF_SPRITE_W 180
#define MOTIF_SPRITE_H 150

typedef struct {
    HDC dc;
    HBITMAP bitmap;
    HBITMAP old_bitmap;
    int valid;
} MotifSprite;

static MotifSprite sprites[OCEAN_MOTIF_COUNT];

static HPEN select_pen(HDC hdc, COLORREF color, int width, HPEN *pen_out) {
    *pen_out = CreatePen(PS_SOLID, width, color);
    return (HPEN)SelectObject(hdc, *pen_out);
}

static void done_pen(HDC hdc, HPEN old_pen, HPEN pen) {
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
}

static COLORREF wash_color(COLORREF ink) {
    int r = (GetRValue(ink) * 2 + 94) / 3;
    int g = (GetGValue(ink) * 2 + 142) / 3;
    int b = (GetBValue(ink) * 2 + 168) / 3;
    return RGB(r, g, b);
}

static void arc_line(HDC hdc, int l, int t, int r, int b, int sx, int sy, int ex, int ey) {
    Arc(hdc, l, t, r, b, sx, sy, ex, ey);
}

void ocean_motif_draw_wave_cluster(HDC hdc, int x, int y, int size,
                                   COLORREF ink, int variant) {
    HPEN pen, old_pen = select_pen(hdc, ink, size > 32 ? 2 : 1, &pen);
    int i;
    for (i = 0; i < 3 + (variant & 1); i++) {
        int yy = y + i * size / 5;
        int xx = x + ((i + variant) & 1) * size / 8;
        arc_line(hdc, xx, yy, xx + size / 2, yy + size / 3,
                 xx, yy + size / 6, xx + size / 2, yy + size / 6);
        arc_line(hdc, xx + size / 2, yy, xx + size, yy + size / 3,
                 xx + size / 2, yy + size / 6, xx + size, yy + size / 6);
    }
    done_pen(hdc, old_pen, pen);
}

static void draw_ship(HDC hdc, int cx, int cy, int s, COLORREF ink) {
    POINT hull[4] = {{cx - s / 2, cy + s / 6}, {cx + s / 2, cy + s / 6},
                     {cx + s / 3, cy + s / 3}, {cx - s / 3, cy + s / 3}};
    HPEN pen, old_pen = select_pen(hdc, ink, max(1, s / 28), &pen);
    HBRUSH brush = CreateSolidBrush(wash_color(ink));
    HBRUSH old_brush = (HBRUSH)SelectObject(hdc, brush);
    Polygon(hdc, hull, 4);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    MoveToEx(hdc, cx, cy + s / 6, NULL);
    LineTo(hdc, cx, cy - s / 2);
    LineTo(hdc, cx + s / 4, cy - s / 8);
    LineTo(hdc, cx, cy - s / 8);
    MoveToEx(hdc, cx + s / 12, cy - s / 4, NULL);
    LineTo(hdc, cx + s / 5, cy - s / 7);
    MoveToEx(hdc, cx, cy - s / 2, NULL);
    LineTo(hdc, cx - s / 4, cy - s / 10);
    LineTo(hdc, cx, cy - s / 10);
    MoveToEx(hdc, cx - s / 5, cy + s / 4, NULL);
    LineTo(hdc, cx + s / 5, cy + s / 4);
    SelectObject(hdc, old_brush);
    DeleteObject(brush);
    done_pen(hdc, old_pen, pen);
}

static void draw_wreck(HDC hdc, int cx, int cy, int s, COLORREF ink) {
    HPEN pen, old_pen = select_pen(hdc, ink, max(1, s / 30), &pen);
    MoveToEx(hdc, cx - s / 2, cy + s / 5, NULL);
    LineTo(hdc, cx + s / 3, cy + s / 5);
    MoveToEx(hdc, cx - s / 8, cy + s / 5, NULL);
    LineTo(hdc, cx + s / 8, cy - s / 2);
    MoveToEx(hdc, cx - s / 6, cy - s / 8, NULL);
    LineTo(hdc, cx + s / 4, cy + s / 12);
    MoveToEx(hdc, cx - s / 3, cy + s / 3, NULL);
    LineTo(hdc, cx - s / 8, cy + s / 4);
    LineTo(hdc, cx + s / 8, cy + s / 3);
    done_pen(hdc, old_pen, pen);
}

static void draw_whale(HDC hdc, int cx, int cy, int s, COLORREF ink) {
    HPEN pen, old_pen = select_pen(hdc, ink, max(1, s / 30), &pen);
    HBRUSH brush = CreateSolidBrush(wash_color(ink));
    HBRUSH old_brush = (HBRUSH)SelectObject(hdc, brush);
    Pie(hdc, cx - s / 2, cy - s / 4, cx + s / 2, cy + s / 2,
        cx - s / 2, cy + s / 8, cx + s / 2, cy + s / 8);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Arc(hdc, cx - s / 2, cy - s / 4, cx + s / 2, cy + s / 2,
        cx - s / 2, cy + s / 8, cx + s / 2, cy + s / 8);
    MoveToEx(hdc, cx + s / 3, cy + s / 8, NULL);
    LineTo(hdc, cx + s / 2, cy - s / 8);
    LineTo(hdc, cx + s / 2, cy + s / 4);
    MoveToEx(hdc, cx - s / 4, cy + s / 8, NULL);
    LineTo(hdc, cx + s / 4, cy + s / 8);
    MoveToEx(hdc, cx - s / 8, cy - s / 4, NULL);
    LineTo(hdc, cx - s / 8, cy - s / 2);
    MoveToEx(hdc, cx - s / 8, cy - s / 2, NULL);
    LineTo(hdc, cx - s / 4, cy - s / 3);
    MoveToEx(hdc, cx - s / 8, cy - s / 2, NULL);
    LineTo(hdc, cx + s / 12, cy - s / 3);
    SelectObject(hdc, old_brush);
    DeleteObject(brush);
    done_pen(hdc, old_pen, pen);
}

static void draw_serpent(HDC hdc, int cx, int cy, int s, COLORREF ink) {
    HPEN pen, old_pen = select_pen(hdc, ink, max(1, s / 24), &pen);
    HBRUSH brush = CreateSolidBrush(wash_color(ink));
    HBRUSH old_brush = (HBRUSH)SelectObject(hdc, brush);
    int i;
    for (i = 0; i < 4; i++) {
        int x = cx - s / 2 + i * s / 4;
        Ellipse(hdc, x, cy - s / 7, x + s / 4, cy + s / 7);
    }
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    for (i = 0; i < 4; i++) {
        int x = cx - s / 2 + i * s / 4;
        Arc(hdc, x, cy - s / 5, x + s / 3, cy + s / 4, x, cy, x + s / 3, cy);
        MoveToEx(hdc, x + s / 10, cy - s / 14, NULL);
        LineTo(hdc, x + s / 7, cy + s / 14);
    }
    Ellipse(hdc, cx + s / 3, cy - s / 5, cx + s / 2, cy - s / 20);
    SelectObject(hdc, old_brush);
    DeleteObject(brush);
    done_pen(hdc, old_pen, pen);
}

static void draw_tentacle(HDC hdc, int cx, int cy, int s, COLORREF ink) {
    HPEN pen, old_pen = select_pen(hdc, ink, max(1, s / 22), &pen);
    Arc(hdc, cx - s / 3, cy - s / 2, cx + s / 3, cy + s / 2,
        cx - s / 5, cy + s / 3, cx + s / 4, cy - s / 3);
    Arc(hdc, cx - s / 6, cy - s / 3, cx + s / 2, cy + s / 3,
        cx + s / 5, cy + s / 4, cx + s / 2, cy - s / 8);
    done_pen(hdc, old_pen, pen);
}

static void draw_whirlpool(HDC hdc, int cx, int cy, int s, COLORREF ink) {
    HPEN pen, old_pen = select_pen(hdc, ink, max(1, s / 34), &pen);
    int i;
    for (i = 0; i < 4; i++) {
        int r = s / 2 - i * s / 10;
        Arc(hdc, cx - r, cy - r / 2, cx + r, cy + r / 2,
            cx - r, cy, cx + r, cy);
    }
    done_pen(hdc, old_pen, pen);
}

static void draw_flying_fish(HDC hdc, int cx, int cy, int s, COLORREF ink) {
    POINT body[5] = {{cx - s / 2, cy}, {cx - s / 8, cy - s / 8},
                     {cx + s / 3, cy - s / 16}, {cx + s / 2, cy},
                     {cx + s / 4, cy + s / 10}};
    HPEN pen, old_pen = select_pen(hdc, ink, max(1, s / 42), &pen);
    HBRUSH brush = CreateSolidBrush(wash_color(ink));
    HBRUSH old_brush = (HBRUSH)SelectObject(hdc, brush);
    Polygon(hdc, body, 5);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Pie(hdc, cx - s / 3, cy - s / 2, cx + s / 7, cy + s / 10,
        cx - s / 3, cy - s / 7, cx + s / 7, cy - s / 7);
    Pie(hdc, cx - s / 12, cy - s / 2, cx + s / 3, cy + s / 12,
        cx - s / 12, cy - s / 7, cx + s / 3, cy - s / 7);
    MoveToEx(hdc, cx + s / 3, cy, NULL);
    LineTo(hdc, cx + s / 2, cy - s / 7);
    LineTo(hdc, cx + s / 2, cy + s / 7);
    SelectObject(hdc, old_brush);
    DeleteObject(brush);
    done_pen(hdc, old_pen, pen);
}

static void draw_beast(HDC hdc, int cx, int cy, int s, COLORREF ink) {
    HPEN pen, old_pen = select_pen(hdc, ink, max(1, s / 28), &pen);
    HBRUSH brush = CreateSolidBrush(wash_color(ink));
    HBRUSH old_brush = (HBRUSH)SelectObject(hdc, brush);
    Ellipse(hdc, cx - s / 3, cy - s / 5, cx + s / 4, cy + s / 4);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    MoveToEx(hdc, cx + s / 4, cy, NULL);
    LineTo(hdc, cx + s / 2, cy - s / 8);
    LineTo(hdc, cx + s / 2, cy + s / 8);
    MoveToEx(hdc, cx - s / 4, cy - s / 5, NULL);
    LineTo(hdc, cx - s / 3, cy - s / 2);
    MoveToEx(hdc, cx - s / 6, cy + s / 6, NULL);
    LineTo(hdc, cx + s / 8, cy - s / 8);
    SelectObject(hdc, old_brush);
    DeleteObject(brush);
    done_pen(hdc, old_pen, pen);
}

static void draw_motif_art(HDC hdc, int type, int cx, int cy, int size,
                           COLORREF ink, int variant) {
    if (type == OCEAN_MOTIF_WAVE) ocean_motif_draw_wave_cluster(hdc, cx - size / 2, cy, size, ink, variant);
    else if (type == OCEAN_MOTIF_SERPENT) draw_serpent(hdc, cx, cy, size, ink);
    else if (type == OCEAN_MOTIF_WHALE) draw_whale(hdc, cx, cy, size, ink);
    else if (type == OCEAN_MOTIF_TENTACLE || type == OCEAN_MOTIF_SPOUT) draw_tentacle(hdc, cx, cy, size, ink);
    else if (type == OCEAN_MOTIF_BEAST) draw_beast(hdc, cx, cy, size, ink);
    else if (type == OCEAN_MOTIF_SHIP) draw_ship(hdc, cx, cy, size, ink);
    else if (type == OCEAN_MOTIF_WRECK) draw_wreck(hdc, cx, cy, size, ink);
    else if (type == OCEAN_MOTIF_FLYING_FISH) draw_flying_fish(hdc, cx, cy, size, ink);
    else if (type == OCEAN_MOTIF_WHIRLPOOL || type == OCEAN_MOTIF_WIND) draw_whirlpool(hdc, cx, cy, size, ink);
}

static int ensure_sprite(HDC target, int type) {
    MotifSprite *sprite;
    HDC screen;
    RECT rect = {0, 0, MOTIF_SPRITE_W, MOTIF_SPRITE_H};
    COLORREF ink = RGB(24, 58, 62);
    if (type < 0 || type >= OCEAN_MOTIF_COUNT) return 0;
    sprite = &sprites[type];
    if (sprite->valid) return 1;
    screen = GetDC(NULL);
    sprite->dc = CreateCompatibleDC(target ? target : screen);
    sprite->bitmap = CreateCompatibleBitmap(target ? target : screen, MOTIF_SPRITE_W, MOTIF_SPRITE_H);
    ReleaseDC(NULL, screen);
    if (!sprite->dc || !sprite->bitmap) return 0;
    sprite->old_bitmap = (HBITMAP)SelectObject(sprite->dc, sprite->bitmap);
    {
        HBRUSH clear = CreateSolidBrush(MOTIF_TRANSPARENT);
        FillRect(sprite->dc, &rect, clear);
        DeleteObject(clear);
    }
    SetBkMode(sprite->dc, TRANSPARENT);
    ocean_motif_draw_wave_cluster(sprite->dc, 35, 100, 92, RGB(34, 77, 80), type);
    draw_motif_art(sprite->dc, type, MOTIF_SPRITE_W / 2, 74, 112, ink, type);
    sprite->valid = 1;
    return 1;
}

void ocean_motif_draw(HDC hdc, int type, int cx, int cy, int size,
                      COLORREF ink, int variant) {
    int w = size;
    int h = max(size * 5 / 6, 18);
    (void)ink;
    (void)variant;
    if (!ensure_sprite(hdc, type)) {
        draw_motif_art(hdc, type, cx, cy, size, RGB(24, 58, 62), variant);
        return;
    }
    TransparentBlt(hdc, cx - w / 2, cy - h / 2, w, h, sprites[type].dc, 0, 0,
                   MOTIF_SPRITE_W, MOTIF_SPRITE_H, MOTIF_TRANSPARENT);
}
