#include "render/panel_worldgen_fingerprint.h"

#include "render/render_common.h"
#include "ui/ui_clay_primitives.h"
#include "ui/ui_clay_theme.h"
#include "ui/ui_theme.h"

static void select_stock_pen(HDC hdc, COLORREF color,
                             HGDIOBJ *old_pen, COLORREF *old_color) {
    *old_pen = SelectObject(hdc, GetStockObject(DC_PEN));
    *old_color = SetDCPenColor(hdc, color);
}

static void restore_stock_pen(HDC hdc, HGDIOBJ old_pen,
                              COLORREF old_color) {
    SetDCPenColor(hdc, old_color);
    SelectObject(hdc, old_pen);
}

static void scaled_ring(const UiWorldgenFingerprintLayout *layout,
                        int percent, POINT *points) {
    int i;
    for (i = 0; i < UI_WORLDGEN_FINGERPRINT_AXIS_COUNT; i++) {
        points[i].x = layout->center.x +
            (layout->axis_end[i].x - layout->center.x) * percent / 100;
        points[i].y = layout->center.y +
            (layout->axis_end[i].y - layout->center.y) * percent / 100;
    }
}

static void draw_radar_grid(HDC hdc,
                            const UiWorldgenFingerprintLayout *layout) {
    HGDIOBJ old_pen;
    HGDIOBJ old_brush;
    COLORREF old_color;
    POINT ring[UI_WORLDGEN_FINGERPRINT_AXIS_COUNT];
    int i;

    select_stock_pen(hdc, ui_theme_color(UI_COLOR_PANEL_LINE),
                     &old_pen, &old_color);
    old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    for (i = 25; i <= 100; i += 25) {
        scaled_ring(layout, i, ring);
        Polygon(hdc, ring, UI_WORLDGEN_FINGERPRINT_AXIS_COUNT);
    }
    for (i = 0; i < UI_WORLDGEN_FINGERPRINT_AXIS_COUNT; i++) {
        MoveToEx(hdc, layout->center.x, layout->center.y, NULL);
        LineTo(hdc, layout->axis_end[i].x, layout->axis_end[i].y);
    }
    SelectObject(hdc, old_brush);
    restore_stock_pen(hdc, old_pen, old_color);
}

static void draw_current_shape(HDC hdc,
                               const UiWorldgenFingerprintLayout *layout) {
    HGDIOBJ old_pen;
    HGDIOBJ old_brush;
    COLORREF old_pen_color;
    COLORREF old_brush_color;

    old_pen = SelectObject(hdc, GetStockObject(DC_PEN));
    old_brush = SelectObject(hdc, GetStockObject(DC_BRUSH));
    old_pen_color = SetDCPenColor(hdc, RGB(91, 190, 214));
    old_brush_color = SetDCBrushColor(hdc, RGB(36, 77, 85));
    Polygon(hdc, layout->current_point, UI_WORLDGEN_FINGERPRINT_AXIS_COUNT);
    SetDCBrushColor(hdc, old_brush_color);
    SetDCPenColor(hdc, old_pen_color);
    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
}

static void draw_default_shape(HDC hdc,
                               const UiWorldgenFingerprintLayout *layout) {
    HGDIOBJ old_pen;
    HGDIOBJ old_brush;
    COLORREF old_color;

    select_stock_pen(hdc, RGB(170, 178, 181), &old_pen, &old_color);
    old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Polygon(hdc, layout->default_point, UI_WORLDGEN_FINGERPRINT_AXIS_COUNT);
    SelectObject(hdc, old_brush);
    restore_stock_pen(hdc, old_pen, old_color);
}

static RECT expanded_axis_label(const UiWorldgenFingerprintLayout *layout,
                                int axis) {
    RECT rect = layout->axis_label[axis];
    int center_x = (rect.left + rect.right) / 2;
    int desired_width = 96;

    rect.left = center_x - desired_width / 2;
    rect.right = rect.left + desired_width;
    if (axis == UI_WORLDGEN_FINGERPRINT_TEMPERATURE) {
        rect.left = layout->center.x + 4;
        rect.right = layout->legend_default.left - 4;
    } else if (axis == UI_WORLDGEN_FINGERPRINT_HUMIDITY) {
        rect.left = layout->card.left + 4;
        rect.right = layout->center.x - 4;
    }
    if (rect.left < layout->card.left + 4) {
        rect.right += layout->card.left + 4 - rect.left;
        rect.left = layout->card.left + 4;
    }
    if (rect.right > layout->legend_default.left - 4) {
        rect.left -= rect.right - (layout->legend_default.left - 4);
        rect.right = layout->legend_default.left - 4;
    }
    return rect;
}

static void draw_axis_labels(HDC hdc,
                             const UiWorldgenFingerprintLayout *layout) {
    static const char *labels_en[UI_WORLDGEN_FINGERPRINT_AXIS_COUNT] = {
        "Ocean", "Landmass", "Relief", "Temperature", "Humidity",
        "Rivers", "Regions"
    };
    static const char *labels_zh[UI_WORLDGEN_FINGERPRINT_AXIS_COUNT] = {
        "海洋", "陆块", "起伏", "温度", "湿度", "河网", "区域"
    };
    int i;

    for (i = 0; i < UI_WORLDGEN_FINGERPRINT_AXIS_COUNT; i++) {
        RECT label = expanded_axis_label(layout, i);
        draw_text_rect(hdc, label, tr(labels_en[i], labels_zh[i]),
                       ui_clay_muted_text_color(),
                       DT_SINGLELINE | DT_VCENTER | DT_CENTER);
    }
}

static void draw_legend_item(HDC hdc, RECT rect, const char *label,
                             COLORREF color) {
    HGDIOBJ old_pen;
    COLORREF old_color;
    RECT text_rect = rect;
    int y = (rect.top + rect.bottom) / 2;

    select_stock_pen(hdc, color, &old_pen, &old_color);
    MoveToEx(hdc, rect.left, y, NULL);
    LineTo(hdc, rect.left + 14, y);
    restore_stock_pen(hdc, old_pen, old_color);
    text_rect.left += 18;
    draw_text_rect(hdc, text_rect, label, ui_clay_muted_text_color(),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}

void panel_worldgen_fingerprint_draw(
    HDC hdc, const UiWorldgenPanelLayout *layout, HFONT body_font) {
    const UiWorldgenFingerprintLayout *fingerprint;
    int saved;

    if (!hdc || !layout) return;
    fingerprint = &layout->fingerprint;
    saved = SaveDC(hdc);
    if (!saved) return;
    IntersectClipRect(hdc, layout->content_viewport.left,
                      layout->content_viewport.top,
                      layout->content_viewport.right,
                      layout->content_viewport.bottom);
    SelectObject(hdc, body_font);
    ui_clay_draw_card(hdc, fingerprint->card, UI_CLAY_STATE_NORMAL);
    draw_text_rect(hdc, fingerprint->title,
                   tr("World Fingerprint", "世界指纹"),
                   ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_current_shape(hdc, fingerprint);
    draw_radar_grid(hdc, fingerprint);
    draw_default_shape(hdc, fingerprint);
    draw_axis_labels(hdc, fingerprint);
    draw_legend_item(hdc, fingerprint->legend_default,
                     tr("Default", "默认"), RGB(170, 178, 181));
    draw_legend_item(hdc, fingerprint->legend_current,
                     tr("Current", "当前"), RGB(91, 190, 214));
    RestoreDC(hdc, saved);
}
