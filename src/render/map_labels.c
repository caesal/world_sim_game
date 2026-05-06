#include "map_labels.h"

#include "render_map_internal.h"

#define MAX_RENDER_LABELS 72

static int rects_overlap(RECT a, RECT b) {
    return a.left < b.right && a.right > b.left && a.top < b.bottom && a.bottom > b.top;
}

static int label_is_open(RECT *used, int used_count, RECT candidate) {
    int i;

    for (i = 0; i < used_count; i++) {
        if (rects_overlap(used[i], candidate)) return 0;
    }
    return 1;
}

static void remember_label(RECT *used, int *used_count, RECT rect) {
    if (*used_count >= MAX_RENDER_LABELS) return;
    used[*used_count] = rect;
    (*used_count)++;
}

static void draw_label_text(HDC hdc, int x, int y, const char *text, COLORREF color) {
    COLORREF outline = RGB(236, 220, 178);

    draw_text_line(hdc, x + 1, y, text, outline);
    draw_text_line(hdc, x, y + 1, text, outline);
    draw_text_line(hdc, x, y, text, color);
}

static RECT label_rect_for_text(HDC hdc, int x, int y, const char *text, int pad) {
    RECT rect;
    SIZE size;

    measure_text_utf8(hdc, text, &size);
    rect.left = x - pad;
    rect.top = y - pad;
    rect.right = x + size.cx + pad;
    rect.bottom = y + size.cy + pad;
    return rect;
}

static RECT label_rect_for_centered_text(HDC hdc, int cx, int y, const char *text, int pad) {
    SIZE size;

    measure_text_utf8(hdc, text, &size);
    return label_rect_for_text(hdc, cx - size.cx / 2, y, text, pad);
}

static void draw_country_labels(HDC hdc, RECT client, MapLayout layout, RECT *used, int *used_count) {
    int font_height = layout.tile_size >= 5 ? 28 : 21;
    HFONT font = CreateFontW(font_height, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH, L"Microsoft YaHei UI");
    HFONT old_font = SelectObject(hdc, font);
    int civ_id;

    for (civ_id = 0; civ_id < civ_count; civ_id++) {
        long sx = 0;
        long sy = 0;
        int samples = 0;
        int x;
        int y;

        if (!civs[civ_id].alive) continue;
        for (y = 2; y < MAP_H; y += 6) {
            for (x = 2; x < MAP_W; x += 6) {
                if (world[y][x].owner != civ_id) continue;
                sx += layout.map_x + x * layout.draw_w / MAP_W;
                sy += layout.map_y + y * layout.draw_h / MAP_H;
                samples++;
            }
        }
        if (samples < (layout.tile_size < 4 ? 24 : 10)) continue;
        {
            int px = (int)(sx / samples);
            int py = (int)(sy / samples);
            RECT rect = label_rect_for_centered_text(hdc, px, py - font_height / 2, civs[civ_id].name, 7);
            if (rect.right < client.left || rect.left > client.right - side_panel_w) continue;
            if (!label_is_open(used, *used_count, rect)) continue;
            draw_label_text(hdc, rect.left + 7, rect.top + 3, civs[civ_id].name, RGB(56, 42, 30));
            remember_label(used, used_count, rect);
        }
    }
    SelectObject(hdc, old_font);
    DeleteObject(font);
}

static void draw_city_labels(HDC hdc, RECT client, MapLayout layout, RECT *used, int *used_count) {
    HFONT font = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH, L"Microsoft YaHei UI");
    HFONT old_font = SelectObject(hdc, font);
    int i;

    if (layout.tile_size < 6) {
        SelectObject(hdc, old_font);
        DeleteObject(font);
        return;
    }
    for (i = 0; i < city_count; i++) {
        City *city = &cities[i];
        int cx;
        int cy;
        RECT rect;

        if (!city->alive || city->owner < 0 || city->owner >= civ_count || !civs[city->owner].alive) continue;
        if (layout.tile_size < 8 && !city->capital && !city->port && city->population < 420) continue;
        if (layout.tile_size < 10 && !city->capital && !city->port && city->population < 720) continue;
        cx = (tile_left(layout, city->x) + tile_right(layout, city->x)) / 2 + 10;
        cy = (tile_top(layout, city->y) + tile_bottom(layout, city->y)) / 2 - 18;
        if (cx < client.left || cx > client.right - side_panel_w || cy < TOP_BAR_H || cy > client.bottom - BOTTOM_BAR_H) {
            continue;
        }
        rect = label_rect_for_text(hdc, cx, cy, city->name, 5);
        if (!label_is_open(used, *used_count, rect)) continue;
        draw_label_text(hdc, cx, cy, city->name, RGB(58, 45, 30));
        remember_label(used, used_count, rect);
    }
    SelectObject(hdc, old_font);
    DeleteObject(font);
}

void draw_map_labels(HDC hdc, RECT client, MapLayout layout) {
    RECT used[MAX_RENDER_LABELS];
    int used_count = 0;
    int saved_dc = SaveDC(hdc);

    IntersectClipRect(hdc, client.left, TOP_BAR_H, client.right - side_panel_w, client.bottom - BOTTOM_BAR_H);
    SetBkMode(hdc, TRANSPARENT);
    draw_country_labels(hdc, client, layout, used, &used_count);
    draw_city_labels(hdc, client, layout, used, &used_count);
    RestoreDC(hdc, saved_dc);
}
