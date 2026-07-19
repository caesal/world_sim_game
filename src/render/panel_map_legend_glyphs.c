#include "render/panel_map_legend_glyphs.h"

#include "render/render_common.h"
#include "ui/ui_types.h"

static RECT centered_rect(int cx, int cy, int size) {
    RECT rect = {cx - size / 2, cy - size / 2,
                 cx + size / 2, cy + size / 2};
    return rect;
}

static void draw_marker_ellipse(HDC hdc, RECT rect, COLORREF fill,
                                COLORREF outline, int width) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, width, outline);
    HBRUSH old_brush = SelectObject(hdc, brush);
    HPEN old_pen = SelectObject(hdc, pen);
    Ellipse(hdc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    DeleteObject(pen);
    DeleteObject(brush);
}

static void draw_city_stage_glyph(HDC hdc, RECT rect, IconId icon) {
    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;
    int stroke = clamp(min(w, h) / 7, 1, 3);
    int inset = max(1, min(w, h) / 6);
    RECT body = {rect.left + inset, rect.top + inset,
                 rect.right - inset, rect.bottom - inset};
    HPEN pen = CreatePen(PS_SOLID, stroke, RGB(31, 28, 20));
    HBRUSH brush = CreateSolidBrush(RGB(36, 32, 22));
    HPEN old_pen = SelectObject(hdc, pen);
    HBRUSH old_brush = SelectObject(hdc, brush);

    if (icon == ICON_CITY_OUTPOST) {
        POINT tent[3] = {{body.left + w / 5, body.bottom},
                         {(body.left + body.right) / 2, body.top},
                         {body.right - w / 5, body.bottom}};
        Polygon(hdc, tent, 3);
    } else if (icon == ICON_CITY_VILLAGE) {
        POINT roof[3] = {{body.left, body.top + h / 2},
                         {(body.left + body.right) / 2, body.top},
                         {body.right, body.top + h / 2}};
        Polygon(hdc, roof, 3);
        Rectangle(hdc, body.left + w / 8, body.top + h / 2,
                  body.right - w / 8, body.bottom);
    } else if (icon == ICON_CITY_TOWN) {
        POINT left_roof[3] = {
            {body.left, body.top + h / 2},
            {body.left + w / 4, body.top + h / 5},
            {body.left + w / 2, body.top + h / 2}
        };
        POINT right_roof[3] = {
            {body.left + w / 2, body.top + h / 2},
            {body.right - w / 4, body.top},
            {body.right, body.top + h / 2}
        };
        Polygon(hdc, left_roof, 3);
        Polygon(hdc, right_roof, 3);
        Rectangle(hdc, body.left + w / 12, body.top + h / 2,
                  body.right - w / 12, body.bottom);
    } else {
        Rectangle(hdc, body.left, body.top + h / 3,
                  body.right, body.bottom);
        Rectangle(hdc, body.left, body.top + h / 5,
                  body.left + w / 5, body.top + h / 2);
        Rectangle(hdc, (body.left + body.right) / 2 - w / 10, body.top,
                  (body.left + body.right) / 2 + w / 10,
                  body.top + h / 2);
        Rectangle(hdc, body.right - w / 5, body.top + h / 5,
                  body.right, body.top + h / 2);
    }
    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    DeleteObject(brush);
    DeleteObject(pen);
}

static void draw_harbor_glyph(HDC hdc, RECT rect) {
    int cx = (rect.left + rect.right) / 2;
    int cy = (rect.top + rect.bottom) / 2;
    int s = min(rect.right - rect.left, rect.bottom - rect.top);
    int top = cy - s / 3;
    int bottom = cy + s / 3;
    int arm = s / 3;
    HPEN pen = CreatePen(PS_SOLID, clamp(s / 6, 2, 4), RGB(20, 38, 38));
    HBRUSH old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    HPEN old_pen = SelectObject(hdc, pen);
    Ellipse(hdc, cx - s / 8, top - s / 8, cx + s / 8, top + s / 8);
    MoveToEx(hdc, cx, top + s / 8, NULL);
    LineTo(hdc, cx, bottom);
    MoveToEx(hdc, cx - arm, cy, NULL);
    LineTo(hdc, cx + arm, cy);
    MoveToEx(hdc, cx - arm, bottom - s / 7, NULL);
    LineTo(hdc, cx, bottom);
    LineTo(hdc, cx + arm, bottom - s / 7);
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    DeleteObject(pen);
}

static void draw_city_marker(HDC hdc, int cx, int cy, IconId icon,
                             int capital, int plate_size, int glyph_size) {
    RECT plate = centered_rect(cx, cy, plate_size);
    RECT glyph = centered_rect(cx, cy, glyph_size);
    draw_marker_ellipse(hdc, plate, RGB(232, 218, 176), RGB(28, 27, 23),
                        capital && plate_size >= 18 ? 2 : 1);
    if (capital) {
        RECT inner = plate;
        InflateRect(&inner, -max(2, plate_size / 6),
                    -max(2, plate_size / 6));
        draw_marker_ellipse(hdc, inner, RGB(241, 229, 188),
                            RGB(28, 27, 23), 1);
    }
    draw_city_stage_glyph(hdc, glyph, icon);
}

static void draw_harbor_marker(HDC hdc, int cx, int cy, int capital,
                               int plate_size, int glyph_size) {
    RECT plate = centered_rect(cx, cy, plate_size);
    RECT glyph = centered_rect(cx, cy, glyph_size);
    draw_marker_ellipse(hdc, plate, RGB(218, 226, 205), RGB(31, 50, 48), 1);
    if (capital) {
        RECT inner = plate;
        InflateRect(&inner, -max(2, plate_size / 6),
                    -max(2, plate_size / 6));
        draw_marker_ellipse(hdc, inner, RGB(226, 234, 217),
                            RGB(28, 27, 23), 1);
    }
    draw_harbor_glyph(hdc, glyph);
}

void panel_map_legend_draw_glyph_item(HDC hdc, int x, int y, IconId icon,
                                      int harbor, int capital,
                                      const char *name) {
    RECT label = {x + 54, y, x + 158, y + 20};
    int cx = x + 9;
    int cy = y + 10;
    if (harbor)
        draw_harbor_marker(hdc, cx, cy, capital,
                           capital ? 20 : 18, capital ? 13 : 12);
    else
        draw_city_marker(hdc, cx, cy, icon, capital,
                         capital ? 20 : 18, capital ? 13 : 12);
    draw_text_rect(hdc, label, name, RGB(232, 238, 242),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}

void panel_map_legend_draw_capital_pair_item(HDC hdc, int x, int y) {
    RECT label = {x + 54, y, x + 158, y + 20};
    int cy = y + 10;
    draw_city_marker(hdc, x + 9, cy, ICON_CITY_CAPITAL, 1, 18, 12);
    draw_harbor_marker(hdc, x + 33, cy, 1, 18, 12);
    draw_text_rect(hdc, label, tr("Capital", "首都"), RGB(232, 238, 242),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}
