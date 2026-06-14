#include "render/country_target_arrow.h"

#include "render/render_common.h"
#include "ui/ui_country_target.h"

static POINT tile_center(MapLayout layout, const RenderSnapshot *snapshot, int x, int y) {
    POINT point;
    point.x = layout.map_x + (x * 2 + 1) * layout.draw_w / max(1, snapshot->map_w * 2);
    point.y = layout.map_y + (y * 2 + 1) * layout.draw_h / max(1, snapshot->map_h * 2);
    return point;
}

static COLORREF mode_color(UiCountryTargetMode mode) {
    return mode == UI_COUNTRY_TARGET_VASSALIZE ? RGB(168, 92, 220) : RGB(218, 62, 58);
}

static int source_anchor(MapLayout layout, const RenderSnapshot *snapshot, int source_civ, POINT *out) {
    const SnapshotCiv *civ;
    const SnapshotCity *city;
    if (!snapshot || !out || source_civ < 0 || source_civ >= snapshot->civ_count) return 0;
    civ = &snapshot->civs[source_civ];
    if (!civ->alive || civ->capital_city < 0 || civ->capital_city >= snapshot->city_count) return 0;
    city = &snapshot->cities[civ->capital_city];
    if (!city->alive || city->owner != source_civ) return 0;
    *out = tile_center(layout, snapshot, city->x, city->y);
    return 1;
}

static int int_abs(int value) {
    return value < 0 ? -value : value;
}

static void draw_arrow_head(HDC hdc, POINT from, POINT to, COLORREF color) {
    int dx = to.x - from.x;
    int dy = to.y - from.y;
    int len = max(1, max(int_abs(dx), int_abs(dy)));
    double ux;
    double uy;
    double px;
    double py;
    POINT head[3];
    HBRUSH brush;
    HBRUSH old_brush;
    ux = (double)dx / (double)len;
    uy = (double)dy / (double)len;
    px = -uy;
    py = ux;
    head[0] = to;
    head[1].x = to.x - (int)(ux * 18.0 + px * 8.0);
    head[1].y = to.y - (int)(uy * 18.0 + py * 8.0);
    head[2].x = to.x - (int)(ux * 18.0 - px * 8.0);
    head[2].y = to.y - (int)(uy * 18.0 - py * 8.0);
    brush = CreateSolidBrush(color);
    old_brush = SelectObject(hdc, brush);
    Polygon(hdc, head, 3);
    SelectObject(hdc, old_brush);
    DeleteObject(brush);
}

void draw_country_target_arrow(HDC hdc, RECT client, MapLayout layout,
                               const RenderSnapshot *snapshot) {
    UiCountryTargetView target = ui_country_target_view();
    RECT viewport = get_map_viewport_rect(client);
    POINT from;
    POINT to;
    COLORREF color;
    HPEN pen;
    HPEN old_pen;
    int saved;
    if (!target.active || !snapshot || !snapshot->world_generated) return;
    if (target.mouse_x < 0 || target.mouse_y < 0) return;
    if (!source_anchor(layout, snapshot, target.source_civ, &from)) return;
    to.x = clamp(target.mouse_x, viewport.left + 4, viewport.right - 4);
    to.y = clamp(target.mouse_y, viewport.top + 4, viewport.bottom - 4);
    color = mode_color(target.mode);
    saved = SaveDC(hdc);
    IntersectClipRect(hdc, viewport.left, viewport.top, viewport.right, viewport.bottom);
    pen = CreatePen(PS_SOLID, 4, color);
    old_pen = SelectObject(hdc, pen);
    MoveToEx(hdc, from.x, from.y, NULL);
    LineTo(hdc, to.x, to.y);
    draw_arrow_head(hdc, from, to, color);
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
    RestoreDC(hdc, saved);
}
