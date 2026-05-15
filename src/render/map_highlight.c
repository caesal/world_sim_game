#include "render/map_highlight.h"

#include "core/game_state.h"
#include "render/render_context.h"
#include "render/render_map_internal.h"
#include "sim/diplomacy.h"

static const RenderSnapshot *highlight_snapshot(void) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    return snapshot && snapshot->world_generated ? snapshot : NULL;
}

static int valid_civ(const RenderSnapshot *snapshot, int civ_id) {
    return snapshot && civ_id >= 0 && civ_id < snapshot->civ_count && snapshot->civs[civ_id].alive;
}

static int highlight_civ(const RenderSnapshot *snapshot) {
    if (valid_civ(snapshot, map_highlight_civ)) return map_highlight_civ;
    if (valid_civ(snapshot, selected_civ)) return selected_civ;
    return -1;
}

static int pulse_elapsed(int start_ms) {
    int elapsed;
    if (start_ms <= 0) return 3000;
    elapsed = (int)GetTickCount() - start_ms;
    return elapsed < 0 ? 3000 : elapsed;
}

static COLORREF mix_color(COLORREF a, COLORREF b, int b_percent) {
    int ap = clamp(100 - b_percent, 0, 100);
    int bp = clamp(b_percent, 0, 100);
    return RGB((GetRValue(a) * ap + GetRValue(b) * bp) / 100,
               (GetGValue(a) * ap + GetGValue(b) * bp) / 100,
               (GetBValue(a) * ap + GetBValue(b) * bp) / 100);
}

static COLORREF civ_color(const RenderSnapshot *snapshot, int civ_id) {
    if (!valid_civ(snapshot, civ_id)) return RGB(238, 220, 146);
    return (COLORREF)snapshot->civs[civ_id].color;
}

static COLORREF civ_highlight_color(const RenderSnapshot *snapshot, int civ_id, int strong) {
    return mix_color(civ_color(snapshot, civ_id), RGB(255, 255, 245), strong ? 36 : 52);
}

static COLORREF civ_shadow_color(const RenderSnapshot *snapshot, int civ_id) {
    return mix_color(civ_color(snapshot, civ_id), RGB(24, 22, 18), 68);
}

static COLORREF relation_border_color(const RenderSnapshot *snapshot, int civ_id, int strong) {
    COLORREF own = civ_highlight_color(snapshot, civ_id, strong);
    if (!valid_civ(snapshot, selected_civ) || civ_id == selected_civ) return own;
    if (snapshot->relations[selected_civ][civ_id].state == DIPLOMACY_WAR ||
        snapshot->wars[selected_civ][civ_id].active) return RGB(214, 70, 58);
    if (snapshot->civs[civ_id].overlord == selected_civ) return RGB(232, 200, 86);
    if (snapshot->civs[selected_civ].overlord == civ_id) return RGB(176, 112, 218);
    return own;
}

static const SnapshotTile *snap_tile(const RenderSnapshot *snapshot, int x, int y) {
    if (!snapshot || x < 0 || y < 0 || x >= snapshot->map_w || y >= snapshot->map_h) return NULL;
    return &snapshot->tiles[y * snapshot->map_w + x];
}

static int tile_owner(const RenderSnapshot *snapshot, int x, int y) {
    const SnapshotTile *tile = snap_tile(snapshot, x, y);
    return tile ? tile->owner : -1;
}

static int tile_owned_by(const RenderSnapshot *snapshot, int x, int y, int civ_id) {
    return tile_owner(snapshot, x, y) == civ_id;
}

static int sx(MapLayout layout, const RenderSnapshot *snapshot, int x) {
    return layout.map_x + x * layout.draw_w / max(1, snapshot->map_w);
}

static int sy(MapLayout layout, const RenderSnapshot *snapshot, int y) {
    return layout.map_y + y * layout.draw_h / max(1, snapshot->map_h);
}

static int visible_bounds_snapshot(const RenderSnapshot *snapshot, RECT client, MapLayout layout,
                                   int *min_x, int *max_x, int *min_y, int *max_y) {
    int left = max(client.left, layout.map_x);
    int top = max(TOP_BAR_H, layout.map_y);
    int right = min(client.right - side_panel_w, layout.map_x + layout.draw_w);
    int bottom = min(client.bottom - BOTTOM_BAR_H, layout.map_y + layout.draw_h);
    if (!snapshot || layout.draw_w <= 0 || layout.draw_h <= 0 || right <= left || bottom <= top) return 0;
    *min_x = clamp((left - layout.map_x) * snapshot->map_w / layout.draw_w - 1, 0, snapshot->map_w - 1);
    *max_x = clamp((right - layout.map_x) * snapshot->map_w / layout.draw_w + 1, 0, snapshot->map_w - 1);
    *min_y = clamp((top - layout.map_y) * snapshot->map_h / layout.draw_h - 1, 0, snapshot->map_h - 1);
    *max_y = clamp((bottom - layout.map_y) * snapshot->map_h / layout.draw_h + 1, 0, snapshot->map_h - 1);
    return 1;
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

static int focus_point(const RenderSnapshot *snapshot, int civ_id, int *out_x, int *out_y) {
    const SnapshotCiv *civ;
    int min_x = snapshot->map_w, min_y = snapshot->map_h, max_x = -1, max_y = -1;
    int x;
    int y;
    if (!valid_civ(snapshot, civ_id)) return 0;
    civ = &snapshot->civs[civ_id];
    if (civ->capital_city >= 0 && civ->capital_city < snapshot->city_count) {
        const SnapshotCity *city = &snapshot->cities[civ->capital_city];
        if (city->alive && city->owner == civ_id) {
            *out_x = city->x;
            *out_y = city->y;
            return 1;
        }
    }
    for (y = 0; y < snapshot->map_h; y++) {
        for (x = 0; x < snapshot->map_w; x++) {
            if (tile_owned_by(snapshot, x, y, civ_id)) {
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

static void draw_focus_ring(HDC hdc, MapLayout layout, const RenderSnapshot *snapshot,
                            int civ_id, int strong, int pulse_start) {
    int x;
    int y;
    int cx;
    int cy;
    int base;
    int elapsed;
    int phase;
    COLORREF ring = civ_highlight_color(snapshot, civ_id, strong);
    COLORREF warm = mix_color(ring, RGB(255, 220, 92), 24);
    COLORREF shadow = civ_shadow_color(snapshot, civ_id);
    if (!focus_point(snapshot, civ_id, &x, &y)) return;
    cx = layout.map_x + (x * 2 + 1) * layout.draw_w / (snapshot->map_w * 2);
    cy = layout.map_y + (y * 2 + 1) * layout.draw_h / (snapshot->map_h * 2);
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

static void edge_line(HDC hdc, int x1, int y1, int x2, int y2) {
    MoveToEx(hdc, x1, y1, NULL);
    LineTo(hdc, x2, y2);
}

static void draw_country_edges_with_pen(HDC hdc, MapLayout layout, const RenderSnapshot *snapshot,
                                        int civ_id, int min_x, int max_x, int min_y, int max_y, HPEN pen) {
    int x;
    int y;
    HGDIOBJ old_pen = SelectObject(hdc, pen);
    for (y = min_y; y <= max_y; y++) {
        for (x = min_x; x <= max_x; x++) {
            int l;
            int r;
            int t;
            int b;
            if (!tile_owned_by(snapshot, x, y, civ_id)) continue;
            l = sx(layout, snapshot, x);
            r = sx(layout, snapshot, x + 1);
            t = sy(layout, snapshot, y);
            b = sy(layout, snapshot, y + 1);
            if (!tile_owned_by(snapshot, x, y - 1, civ_id)) edge_line(hdc, l, t, r, t);
            if (!tile_owned_by(snapshot, x, y + 1, civ_id)) edge_line(hdc, l, b, r, b);
            if (!tile_owned_by(snapshot, x - 1, y, civ_id)) edge_line(hdc, l, t, l, b);
            if (!tile_owned_by(snapshot, x + 1, y, civ_id)) edge_line(hdc, r, t, r, b);
        }
    }
    SelectObject(hdc, old_pen);
}

static void draw_country_highlight_one(HDC hdc, RECT client, MapLayout layout,
                                       const RenderSnapshot *snapshot, int civ_id, int strong, int pulse_start) {
    int min_x;
    int max_x;
    int min_y;
    int max_y;
    int x;
    int y;
    HPEN outer_pen;
    HPEN inner_pen;
    COLORREF fill = civ_highlight_color(snapshot, civ_id, strong);
    COLORREF inner = relation_border_color(snapshot, civ_id, strong);
    COLORREF outer = mix_color(inner, RGB(18, 16, 14), 72);
    if (!valid_civ(snapshot, civ_id)) return;
    if (!visible_bounds_snapshot(snapshot, client, layout, &min_x, &max_x, &min_y, &max_y)) return;
    for (y = min_y; y <= max_y; y++) {
        for (x = min_x; x <= max_x; x++) {
            RECT tile;
            if (!tile_owned_by(snapshot, x, y, civ_id)) continue;
            tile = (RECT){sx(layout, snapshot, x), sy(layout, snapshot, y),
                          sx(layout, snapshot, x + 1), sy(layout, snapshot, y + 1)};
            fill_rect_alpha(hdc, tile, fill, (BYTE)(strong ? 112 : 58));
        }
    }
    outer_pen = CreatePen(PS_SOLID, strong ? 5 : 3, outer);
    inner_pen = CreatePen(PS_SOLID, strong ? 3 : 2, inner);
    draw_country_edges_with_pen(hdc, layout, snapshot, civ_id, min_x, max_x, min_y, max_y, outer_pen);
    draw_country_edges_with_pen(hdc, layout, snapshot, civ_id, min_x, max_x, min_y, max_y, inner_pen);
    DeleteObject(outer_pen);
    DeleteObject(inner_pen);
    draw_focus_ring(hdc, layout, snapshot, civ_id, strong, pulse_start);
}

static void dim_other_countries(HDC hdc, RECT client, MapLayout layout,
                                const RenderSnapshot *snapshot, int primary, int secondary) {
    int min_x;
    int max_x;
    int min_y;
    int max_y;
    int x;
    int y;
    if (!visible_bounds_snapshot(snapshot, client, layout, &min_x, &max_x, &min_y, &max_y)) return;
    for (y = min_y; y <= max_y; y++) {
        for (x = min_x; x <= max_x; x++) {
            int owner = tile_owner(snapshot, x, y);
            RECT tile;
            if (owner < 0 || owner == primary || owner == secondary) continue;
            tile = (RECT){sx(layout, snapshot, x), sy(layout, snapshot, y),
                          sx(layout, snapshot, x + 1), sy(layout, snapshot, y + 1)};
            fill_rect_alpha(hdc, tile, RGB(0, 0, 0), 18);
        }
    }
}

static void draw_vassal_relation_highlights(HDC hdc, RECT client, MapLayout layout,
                                            const RenderSnapshot *snapshot) {
    int i;
    int overlord;
    if (!valid_civ(snapshot, selected_civ)) return;
    overlord = snapshot->civs[selected_civ].overlord;
    if (valid_civ(snapshot, overlord)) {
        draw_country_highlight_one(hdc, client, layout, snapshot, overlord, 0, selected_civ_pulse_start_ms);
        for (i = 0; i < snapshot->civ_count; i++) {
            if (i != selected_civ && snapshot->civs[i].overlord == overlord) {
                draw_country_highlight_one(hdc, client, layout, snapshot, i, 0, 0);
            }
        }
        return;
    }
    for (i = 0; i < snapshot->civ_count; i++) {
        if (snapshot->civs[i].overlord == selected_civ) {
            draw_country_highlight_one(hdc, client, layout, snapshot, i, 0, 0);
        }
    }
}

static void draw_war_relation_highlights(HDC hdc, RECT client, MapLayout layout,
                                         const RenderSnapshot *snapshot) {
    int i;
    if (!valid_civ(snapshot, selected_civ)) return;
    for (i = 0; i < snapshot->civ_count; i++) {
        if (i != selected_civ && snapshot->civs[i].alive &&
            (snapshot->relations[selected_civ][i].state == DIPLOMACY_WAR ||
             snapshot->wars[selected_civ][i].active)) {
            draw_country_highlight_one(hdc, client, layout, snapshot, i, 0, 0);
        }
    }
}

void draw_country_highlight(HDC hdc, RECT client, MapLayout layout) {
    const RenderSnapshot *snapshot = highlight_snapshot();
    int primary = highlight_civ(snapshot);
    int secondary = valid_civ(snapshot, map_highlight_civ) && valid_civ(snapshot, selected_civ) &&
                    selected_civ != map_highlight_civ ? selected_civ : -1;
    if (!snapshot || primary < 0) return;
    if (valid_civ(snapshot, map_highlight_civ)) dim_other_countries(hdc, client, layout, snapshot, primary, secondary);
    if (secondary >= 0) draw_country_highlight_one(hdc, client, layout, snapshot, secondary, 0, selected_civ_pulse_start_ms);
    draw_vassal_relation_highlights(hdc, client, layout, snapshot);
    draw_war_relation_highlights(hdc, client, layout, snapshot);
    draw_country_highlight_one(hdc, client, layout, snapshot, primary,
                               primary == map_highlight_civ || !valid_civ(snapshot, map_highlight_civ),
                               primary == map_highlight_civ ? map_highlight_pulse_start_ms : selected_civ_pulse_start_ms);
}
