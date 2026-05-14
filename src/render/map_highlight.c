#include "render/map_highlight.h"

#include "core/country_focus.h"
#include "core/game_state.h"
#include "render/render_map_internal.h"
#include "sim/vassal.h"

static int highlight_civ(void) {
    if (map_highlight_civ >= 0 && map_highlight_civ < civ_count) return map_highlight_civ;
    if (selected_civ >= 0 && selected_civ < civ_count) return selected_civ;
    return -1;
}

static int pulse_elapsed(int start_ms) {
    int elapsed;
    if (start_ms <= 0) return 3000;
    elapsed = (int)GetTickCount() - start_ms;
    return elapsed < 0 ? 3000 : elapsed;
}

static void draw_ring(HDC hdc, int cx, int cy, int r, int width, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, width, color);
    HGDIOBJ old_pen = SelectObject(hdc, pen);
    HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Ellipse(hdc, cx - r, cy - r, cx + r, cy + r);
    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
}

static COLORREF mix_color(COLORREF a, COLORREF b, int b_percent) {
    int ap = clamp(100 - b_percent, 0, 100);
    int bp = clamp(b_percent, 0, 100);
    return RGB((GetRValue(a) * ap + GetRValue(b) * bp) / 100,
               (GetGValue(a) * ap + GetGValue(b) * bp) / 100,
               (GetBValue(a) * ap + GetBValue(b) * bp) / 100);
}

static COLORREF civ_color(int civ_id) {
    if (civ_id < 0 || civ_id >= civ_count) return RGB(238, 220, 146);
    return (COLORREF)civs[civ_id].color;
}

static COLORREF civ_highlight_color(int civ_id, int strong) {
    COLORREF base = civ_color(civ_id);
    return mix_color(base, RGB(255, 255, 245), strong ? 36 : 52);
}

static COLORREF civ_shadow_color(int civ_id) {
    return mix_color(civ_color(civ_id), RGB(24, 22, 18), 68);
}

static void draw_focus_ring(HDC hdc, MapLayout layout, int civ_id, int strong, int pulse_start) {
    int x;
    int y;
    int cx;
    int cy;
    int base;
    int elapsed;
    int phase;
    COLORREF ring = civ_highlight_color(civ_id, strong);
    COLORREF warm = mix_color(ring, RGB(255, 220, 92), 24);
    COLORREF shadow = civ_shadow_color(civ_id);

    if (!country_focus_point(civ_id, &x, &y)) return;
    cx = layout.map_x + (x * 2 + 1) * layout.draw_w / (MAP_W * 2);
    cy = layout.map_y + (y * 2 + 1) * layout.draw_h / (MAP_H * 2);
    base = clamp(layout.tile_size * (strong ? 5 : 4), 14, 42);
    elapsed = pulse_elapsed(pulse_start);
    draw_ring(hdc, cx, cy, base, strong ? 3 : 2, ring);
    draw_ring(hdc, cx, cy, base + 7, 1, shadow);
    if (elapsed < 3000) {
        phase = (elapsed % 1000) * 26 / 1000;
        draw_ring(hdc, cx, cy, base + phase + 8, 2, mix_color(ring, RGB(255, 255, 255), 18));
        draw_ring(hdc, cx, cy, base + phase / 2 + 18, 1, warm);
    }
}

static int tile_owned_by(int x, int y, int civ_id) {
    if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) return 0;
    return world[y][x].owner == civ_id;
}

static void draw_edge_line(HDC hdc, int x1, int y1, int x2, int y2) {
    MoveToEx(hdc, x1, y1, NULL);
    LineTo(hdc, x2, y2);
}

static void draw_country_edges_with_pen(HDC hdc, MapLayout layout, int civ_id,
                                        int min_x, int max_x, int min_y, int max_y, HPEN pen) {
    int x;
    int y;
    HGDIOBJ old_pen = SelectObject(hdc, pen);
    for (y = min_y; y <= max_y; y++) {
        for (x = min_x; x <= max_x; x++) {
            int l;
            int r;
            int t;
            int b;
            if (world[y][x].owner != civ_id) continue;
            l = tile_left(layout, x);
            r = tile_right(layout, x);
            t = tile_top(layout, y);
            b = tile_bottom(layout, y);
            if (!tile_owned_by(x, y - 1, civ_id)) draw_edge_line(hdc, l, t, r, t);
            if (!tile_owned_by(x, y + 1, civ_id)) draw_edge_line(hdc, l, b, r, b);
            if (!tile_owned_by(x - 1, y, civ_id)) draw_edge_line(hdc, l, t, l, b);
            if (!tile_owned_by(x + 1, y, civ_id)) draw_edge_line(hdc, r, t, r, b);
        }
    }
    SelectObject(hdc, old_pen);
}

static void draw_country_highlight_one(HDC hdc, RECT client, MapLayout layout,
                                       int civ_id, int strong, int pulse_start) {
    int min_x;
    int max_x;
    int min_y;
    int max_y;
    int x;
    int y;
    HPEN outer_pen;
    HPEN inner_pen;
    COLORREF fill = civ_highlight_color(civ_id, strong);
    COLORREF inner = civ_highlight_color(civ_id, strong);
    COLORREF outer = civ_shadow_color(civ_id);

    if (civ_id < 0) return;
    if (!visible_tile_bounds(client, layout, &min_x, &max_x, &min_y, &max_y)) return;
    for (y = min_y; y <= max_y; y++) {
        for (x = min_x; x <= max_x; x++) {
            RECT tile;
            if (world[y][x].owner != civ_id) continue;
            tile = (RECT){tile_left(layout, x), tile_top(layout, y),
                          tile_right(layout, x), tile_bottom(layout, y)};
            fill_rect_alpha(hdc, tile, fill, (BYTE)(strong ? 112 : 58));
        }
    }
    outer_pen = CreatePen(PS_SOLID, strong ? 5 : 3, outer);
    inner_pen = CreatePen(PS_SOLID, strong ? 3 : 2, inner);
    draw_country_edges_with_pen(hdc, layout, civ_id, min_x, max_x, min_y, max_y, outer_pen);
    draw_country_edges_with_pen(hdc, layout, civ_id, min_x, max_x, min_y, max_y, inner_pen);
    DeleteObject(outer_pen);
    DeleteObject(inner_pen);
    draw_focus_ring(hdc, layout, civ_id, strong, pulse_start);
}

static void dim_other_countries(HDC hdc, RECT client, MapLayout layout, int primary, int secondary) {
    int min_x;
    int max_x;
    int min_y;
    int max_y;
    int x;
    int y;
    if (primary < 0 || !visible_tile_bounds(client, layout, &min_x, &max_x, &min_y, &max_y)) return;
    for (y = min_y; y <= max_y; y++) {
        for (x = min_x; x <= max_x; x++) {
            int owner = world[y][x].owner;
            RECT tile;
            if (owner < 0 || owner == primary || owner == secondary) continue;
            tile = (RECT){tile_left(layout, x), tile_top(layout, y),
                          tile_right(layout, x), tile_bottom(layout, y)};
            fill_rect_alpha(hdc, tile, RGB(0, 0, 0), 18);
        }
    }
}

static POINT focus_screen_point(MapLayout layout, int civ_id, int *ok) {
    int x;
    int y;
    POINT p = {0, 0};
    *ok = country_focus_point(civ_id, &x, &y);
    if (!*ok) return p;
    p.x = layout.map_x + (x * 2 + 1) * layout.draw_w / (MAP_W * 2);
    p.y = layout.map_y + (y * 2 + 1) * layout.draw_h / (MAP_H * 2);
    return p;
}

static void draw_relation_line(HDC hdc, MapLayout layout, int from_civ, int to_civ, COLORREF color, int strong) {
    int ok_a;
    int ok_b;
    POINT a = focus_screen_point(layout, from_civ, &ok_a);
    POINT b = focus_screen_point(layout, to_civ, &ok_b);
    HPEN pen;
    HGDIOBJ old_pen;
    int mx;
    int my;
    if (!ok_a || !ok_b) return;
    pen = CreatePen(strong ? PS_SOLID : PS_DOT, strong ? 3 : 2, color);
    old_pen = SelectObject(hdc, pen);
    MoveToEx(hdc, a.x, a.y, NULL);
    LineTo(hdc, b.x, b.y);
    mx = (a.x + b.x) / 2;
    my = (a.y + b.y) / 2;
    Ellipse(hdc, mx - 4, my - 4, mx + 4, my + 4);
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
}

static void draw_vassal_relation_highlights(HDC hdc, RECT client, MapLayout layout) {
    int ids[MAX_CIVS];
    int count;
    int i;
    int overlord;
    COLORREF tribute = RGB(226, 196, 104);
    if (selected_civ < 0 || selected_civ >= civ_count) return;
    overlord = vassal_overlord(selected_civ);
    if (overlord >= 0) {
        count = vassal_collect_direct(overlord, ids, MAX_CIVS);
        draw_country_highlight_one(hdc, client, layout, overlord, 0, selected_civ_pulse_start_ms);
        for (i = 0; i < count; i++) {
            if (ids[i] != selected_civ) draw_country_highlight_one(hdc, client, layout, ids[i], 0, 0);
        }
        draw_relation_line(hdc, layout, selected_civ, overlord, tribute, 1);
        return;
    }
    count = vassal_collect_direct(selected_civ, ids, MAX_CIVS);
    for (i = 0; i < count; i++) {
        draw_country_highlight_one(hdc, client, layout, ids[i], 0, 0);
        draw_relation_line(hdc, layout, ids[i], selected_civ, tribute, 0);
    }
}

void draw_country_highlight(HDC hdc, RECT client, MapLayout layout) {
    int primary = highlight_civ();
    int secondary = (map_highlight_civ >= 0 && selected_civ >= 0 &&
                     selected_civ < civ_count && selected_civ != map_highlight_civ) ? selected_civ : -1;
    if (primary < 0) return;
    if (map_highlight_civ >= 0) dim_other_countries(hdc, client, layout, primary, secondary);
    if (secondary >= 0) {
        draw_country_highlight_one(hdc, client, layout, secondary, 0, selected_civ_pulse_start_ms);
    }
    draw_vassal_relation_highlights(hdc, client, layout);
    draw_country_highlight_one(hdc, client, layout, primary,
                               primary == map_highlight_civ || map_highlight_civ < 0,
                               primary == map_highlight_civ ? map_highlight_pulse_start_ms : selected_civ_pulse_start_ms);
}
