#include "render/panel_worldgen_controls.h"

#include "render/render_common.h"
#include "ui/ui_clay_widgets.h"
#include "ui/ui_theme.h"

#include <stdio.h>

#define PANEL_WORLDGEN_MAX_GRID_DIVISIONS 20

static int clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static COLORREF mix_color(COLORREF a, COLORREF b, int b_percent) {
    int a_percent = 100 - b_percent;
    return RGB((GetRValue(a) * a_percent + GetRValue(b) * b_percent) / 100,
               (GetGValue(a) * a_percent + GetGValue(b) * b_percent) / 100,
               (GetBValue(a) * a_percent + GetBValue(b) * b_percent) / 100);
}

static int rect_width(RECT rect) {
    return rect.right - rect.left;
}

static int rect_height(RECT rect) {
    return rect.bottom - rect.top;
}

static RECT centered_square(RECT rect) {
    int width = rect_width(rect);
    int height = rect_height(rect);
    int size = width < height ? width : height;
    RECT square;

    if (size < 0) size = 0;
    square.left = rect.left + (width - size) / 2;
    square.top = rect.top + (height - size) / 2;
    square.right = square.left + size;
    square.bottom = square.top + size;
    return square;
}

static void begin_stock_pen(HDC hdc, COLORREF color,
                            HGDIOBJ *old_pen, COLORREF *old_color) {
    *old_pen = SelectObject(hdc, GetStockObject(DC_PEN));
    *old_color = SetDCPenColor(hdc, color);
}

static void end_stock_pen(HDC hdc, HGDIOBJ old_pen, COLORREF old_color) {
    SetDCPenColor(hdc, old_color);
    SelectObject(hdc, old_pen);
}

UiClayState panel_worldgen_controls_clay_state(
    PanelWorldgenControlFlags flags) {
    return ui_clay_state_from_flags(flags.hovered, flags.pressed,
                                    flags.selected, flags.disabled);
}

void panel_worldgen_controls_draw_focus_ring(HDC hdc, RECT rect,
                                             int radius, int focused) {
    HGDIOBJ old_pen;
    HGDIOBJ old_brush;
    COLORREF old_color;
    int diameter;

    if (!hdc || !focused || rect_width(rect) <= 0 || rect_height(rect) <= 0) {
        return;
    }
    InflateRect(&rect, 2, 2);
    radius = clamp_int(radius, 1, 24);
    diameter = radius * 2;
    begin_stock_pen(hdc, ui_theme_color(UI_COLOR_ACCENT),
                    &old_pen, &old_color);
    old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom,
              diameter, diameter);
    SelectObject(hdc, old_brush);
    end_stock_pen(hdc, old_pen, old_color);
}

void panel_worldgen_controls_draw_text_clipped(
    HDC hdc, RECT clip, RECT rect, const char *text, COLORREF color,
    unsigned int format) {
    int saved;

    if (!hdc || !text || clip.right <= clip.left || clip.bottom <= clip.top) {
        return;
    }
    saved = SaveDC(hdc);
    if (!saved) return;
    IntersectClipRect(hdc, clip.left, clip.top, clip.right, clip.bottom);
    draw_text_rect(hdc, rect, text, color, format);
    RestoreDC(hdc, saved);
}

static RECT glyph_rect(RECT button) {
    int width = rect_width(button);
    int height = rect_height(button);
    int size = (width < height ? width : height) - 12;
    RECT glyph;

    if (size < 10) size = 10;
    glyph.left = button.left + (width - size) / 2;
    glyph.top = button.top + (height - size) / 2;
    glyph.right = glyph.left + size;
    glyph.bottom = glyph.top + size;
    return glyph;
}

static void draw_dice_dot(HDC hdc, int x, int y, int radius) {
    Ellipse(hdc, x - radius, y - radius, x + radius + 1, y + radius + 1);
}

static void draw_dice_glyph(HDC hdc, RECT rect, COLORREF color) {
    HGDIOBJ old_pen;
    HGDIOBJ old_brush;
    COLORREF old_pen_color;
    COLORREF old_brush_color;
    int dot_radius = clamp_int(rect_width(rect) / 12, 1, 3);
    int left = rect.left + rect_width(rect) / 4;
    int center_x = (rect.left + rect.right) / 2;
    int right = rect.right - rect_width(rect) / 4;
    int top = rect.top + rect_height(rect) / 4;
    int center_y = (rect.top + rect.bottom) / 2;
    int bottom = rect.bottom - rect_height(rect) / 4;

    begin_stock_pen(hdc, color, &old_pen, &old_pen_color);
    old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, 6, 6);
    SelectObject(hdc, GetStockObject(DC_BRUSH));
    old_brush_color = SetDCBrushColor(hdc, color);
    draw_dice_dot(hdc, left, top, dot_radius);
    draw_dice_dot(hdc, right, top, dot_radius);
    draw_dice_dot(hdc, center_x, center_y, dot_radius);
    draw_dice_dot(hdc, left, bottom, dot_radius);
    draw_dice_dot(hdc, right, bottom, dot_radius);
    SetDCBrushColor(hdc, old_brush_color);
    SelectObject(hdc, old_brush);
    end_stock_pen(hdc, old_pen, old_pen_color);
}

static void draw_reset_glyph(HDC hdc, RECT rect, COLORREF color) {
    HGDIOBJ old_pen;
    HGDIOBJ old_brush;
    COLORREF old_pen_color;
    COLORREF old_brush_color;
    POINT arrow[3];
    int center_x = (rect.left + rect.right) / 2;
    int center_y = (rect.top + rect.bottom) / 2;
    int arrow_size = clamp_int(rect_width(rect) / 5, 3, 6);

    begin_stock_pen(hdc, color, &old_pen, &old_pen_color);
    old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Arc(hdc, rect.left, rect.top, rect.right, rect.bottom,
        rect.right, center_y, center_x, rect.top);
    Arc(hdc, rect.left, rect.top, rect.right, rect.bottom,
        center_x, rect.top, rect.left, center_y);
    arrow[0].x = rect.left + 1;
    arrow[0].y = center_y;
    arrow[1].x = arrow[0].x + arrow_size * 2;
    arrow[1].y = center_y - arrow_size;
    arrow[2].x = arrow[0].x + arrow_size * 2;
    arrow[2].y = center_y + arrow_size;
    SelectObject(hdc, GetStockObject(DC_BRUSH));
    old_brush_color = SetDCBrushColor(hdc, color);
    Polygon(hdc, arrow, 3);
    SetDCBrushColor(hdc, old_brush_color);
    SelectObject(hdc, old_brush);
    end_stock_pen(hdc, old_pen, old_pen_color);
}

void panel_worldgen_controls_draw_glyph_button(
    HDC hdc, RECT rect, PanelWorldgenGlyph glyph,
    PanelWorldgenControlFlags flags) {
    UiClayState state = panel_worldgen_controls_clay_state(flags);
    RECT icon;
    COLORREF color;

    if (!hdc || rect_width(rect) <= 0 || rect_height(rect) <= 0) return;
    ui_clay_draw_icon_button(hdc, rect, "", state);
    icon = glyph_rect(rect);
    color = ui_clay_text_color(state);
    if (glyph == PANEL_WORLDGEN_GLYPH_RESET) {
        draw_reset_glyph(hdc, icon, color);
    } else {
        draw_dice_glyph(hdc, icon, color);
    }
    panel_worldgen_controls_draw_focus_ring(hdc, rect, 8, flags.focused);
}

RECT panel_worldgen_controls_handle_rect(
    RECT logical_rect, PanelWorldgenHandleIdentity identity, int coincident) {
    RECT rect = centered_square(logical_rect);
    int size = rect_width(rect);
    int offset;

    if (!coincident || size <= 0) return rect;
    offset = clamp_int(size / 4, 3, 6);
    if (identity == PANEL_WORLDGEN_HANDLE_SECONDARY) {
        OffsetRect(&rect, offset / 2, offset);
    } else {
        OffsetRect(&rect, -(offset / 2), -offset);
    }
    return rect;
}

static void handle_colors(PanelWorldgenHandleIdentity identity,
                          UiClayState state,
                          COLORREF *fill, COLORREF *border) {
    UiClayStyle disabled_style;
    COLORREF base_fill = identity == PANEL_WORLDGEN_HANDLE_SECONDARY ?
        RGB(198, 155, 76) : RGB(78, 145, 176);
    COLORREF base_border = identity == PANEL_WORLDGEN_HANDLE_SECONDARY ?
        RGB(244, 216, 154) : RGB(178, 222, 238);

    if (state == UI_CLAY_STATE_DISABLED) {
        disabled_style = ui_clay_style(UI_CLAY_SURFACE_PILL, state);
        *fill = disabled_style.fill;
        *border = disabled_style.border;
    } else if (state == UI_CLAY_STATE_PRESSED) {
        *fill = mix_color(base_fill, RGB(24, 28, 30), 24);
        *border = RGB(250, 244, 220);
    } else if (state == UI_CLAY_STATE_SELECTED) {
        *fill = mix_color(base_fill, RGB(242, 232, 200), 18);
        *border = RGB(255, 248, 226);
    } else if (state == UI_CLAY_STATE_HOVER) {
        *fill = mix_color(base_fill, RGB(238, 244, 242), 16);
        *border = mix_color(base_border, RGB(255, 255, 255), 22);
    } else {
        *fill = base_fill;
        *border = base_border;
    }
}

void panel_worldgen_controls_draw_handle(
    HDC hdc, RECT logical_rect, PanelWorldgenHandleIdentity identity,
    int coincident, PanelWorldgenControlFlags flags) {
    RECT rect = panel_worldgen_controls_handle_rect(
        logical_rect, identity, coincident);
    RECT shadow = rect;
    RECT mark;
    UiClayState state = panel_worldgen_controls_clay_state(flags);
    HGDIOBJ old_pen;
    HGDIOBJ old_brush;
    COLORREF old_pen_color;
    COLORREF old_brush_color;
    COLORREF fill;
    COLORREF border;

    if (!hdc || rect_width(rect) <= 0 || rect_height(rect) <= 0) return;
    handle_colors(identity, state, &fill, &border);
    old_pen = SelectObject(hdc, GetStockObject(DC_PEN));
    old_brush = SelectObject(hdc, GetStockObject(DC_BRUSH));
    old_pen_color = SetDCPenColor(hdc, RGB(30, 36, 38));
    old_brush_color = SetDCBrushColor(hdc, RGB(30, 36, 38));
    OffsetRect(&shadow, 2, 2);
    Ellipse(hdc, shadow.left, shadow.top, shadow.right, shadow.bottom);
    SetDCPenColor(hdc, border);
    SetDCBrushColor(hdc, fill);
    Ellipse(hdc, rect.left, rect.top, rect.right, rect.bottom);
    SetDCPenColor(hdc, ui_clay_text_color(state));
    SetDCBrushColor(hdc, ui_clay_text_color(state));
    mark = rect;
    InflateRect(&mark, -max(3, rect_width(rect) / 3),
                -max(3, rect_height(rect) / 3));
    if (identity == PANEL_WORLDGEN_HANDLE_SECONDARY) {
        MoveToEx(hdc, mark.left, (mark.top + mark.bottom) / 2, NULL);
        LineTo(hdc, mark.right, (mark.top + mark.bottom) / 2);
    } else if (mark.right > mark.left && mark.bottom > mark.top) {
        Ellipse(hdc, mark.left, mark.top, mark.right, mark.bottom);
    }
    SetDCPenColor(hdc, old_pen_color);
    SetDCBrushColor(hdc, old_brush_color);
    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    panel_worldgen_controls_draw_focus_ring(hdc, rect,
                                            rect_width(rect) / 2,
                                            flags.focused);
}

void panel_worldgen_controls_draw_grid(HDC hdc, RECT plot,
                                       int columns, int rows,
                                       COLORREF color) {
    HGDIOBJ old_pen;
    COLORREF old_color;
    int i;

    if (!hdc || rect_width(plot) <= 0 || rect_height(plot) <= 0) return;
    columns = clamp_int(columns, 1, PANEL_WORLDGEN_MAX_GRID_DIVISIONS);
    rows = clamp_int(rows, 1, PANEL_WORLDGEN_MAX_GRID_DIVISIONS);
    begin_stock_pen(hdc, color, &old_pen, &old_color);
    for (i = 1; i < columns; i++) {
        int x = plot.left + rect_width(plot) * i / columns;
        MoveToEx(hdc, x, plot.top, NULL);
        LineTo(hdc, x, plot.bottom);
    }
    for (i = 1; i < rows; i++) {
        int y = plot.top + rect_height(plot) * i / rows;
        MoveToEx(hdc, plot.left, y, NULL);
        LineTo(hdc, plot.right, y);
    }
    end_stock_pen(hdc, old_pen, old_color);
}

void panel_worldgen_controls_draw_axes(HDC hdc, RECT plot,
                                       int vertical_percent,
                                       int horizontal_percent,
                                       COLORREF color) {
    HGDIOBJ old_pen;
    COLORREF old_color;
    int x;
    int y;

    if (!hdc || rect_width(plot) <= 0 || rect_height(plot) <= 0) return;
    vertical_percent = clamp_int(vertical_percent, 0, 100);
    horizontal_percent = clamp_int(horizontal_percent, 0, 100);
    x = plot.left + rect_width(plot) * vertical_percent / 100;
    y = plot.bottom - rect_height(plot) * horizontal_percent / 100;
    begin_stock_pen(hdc, color, &old_pen, &old_color);
    MoveToEx(hdc, x, plot.top, NULL);
    LineTo(hdc, x, plot.bottom);
    MoveToEx(hdc, plot.left, y, NULL);
    LineTo(hdc, plot.right, y);
    end_stock_pen(hdc, old_pen, old_color);
}

void panel_worldgen_controls_draw_ticks(HDC hdc, RECT plot,
                                        int vertical_percent,
                                        int horizontal_percent,
                                        int x_steps, int y_steps,
                                        int tick_radius, COLORREF color) {
    HGDIOBJ old_pen;
    COLORREF old_color;
    int axis_x;
    int axis_y;
    int i;

    if (!hdc || rect_width(plot) <= 0 || rect_height(plot) <= 0) return;
    vertical_percent = clamp_int(vertical_percent, 0, 100);
    horizontal_percent = clamp_int(horizontal_percent, 0, 100);
    x_steps = clamp_int(x_steps, 1, PANEL_WORLDGEN_MAX_GRID_DIVISIONS);
    y_steps = clamp_int(y_steps, 1, PANEL_WORLDGEN_MAX_GRID_DIVISIONS);
    tick_radius = clamp_int(tick_radius, 1, 8);
    axis_x = plot.left + rect_width(plot) * vertical_percent / 100;
    axis_y = plot.bottom - rect_height(plot) * horizontal_percent / 100;
    begin_stock_pen(hdc, color, &old_pen, &old_color);
    for (i = 0; i <= x_steps; i++) {
        int x = plot.left + rect_width(plot) * i / x_steps;
        MoveToEx(hdc, x, axis_y - tick_radius, NULL);
        LineTo(hdc, x, axis_y + tick_radius + 1);
    }
    for (i = 0; i <= y_steps; i++) {
        int y = plot.bottom - rect_height(plot) * i / y_steps;
        MoveToEx(hdc, axis_x - tick_radius, y, NULL);
        LineTo(hdc, axis_x + tick_radius + 1, y);
    }
    end_stock_pen(hdc, old_pen, old_color);
}

void panel_worldgen_controls_draw_continuous_slider(
    HDC hdc, const PanelWorldgenSliderGeometry *geometry,
    const char *label, int value, PanelWorldgenControlFlags flags) {
    UiClayState state;
    char value_text[24];
    int saved;

    if (!hdc || !geometry || !label ||
        geometry->clip.right <= geometry->clip.left ||
        geometry->clip.bottom <= geometry->clip.top) return;
    value = clamp_int(value, 0, 100);
    state = panel_worldgen_controls_clay_state(flags);
    snprintf(value_text, sizeof(value_text), "%d / 0-100", value);
    saved = SaveDC(hdc);
    if (!saved) return;
    IntersectClipRect(hdc, geometry->clip.left, geometry->clip.top,
                      geometry->clip.right, geometry->clip.bottom);
    draw_text_rect(hdc, geometry->label, label,
                   ui_clay_muted_text_color(),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, geometry->value, value_text,
                   ui_clay_text_color(state),
                   DT_SINGLELINE | DT_VCENTER | DT_RIGHT |
                   DT_END_ELLIPSIS);
    ui_clay_draw_slider(hdc, geometry->track, value, state);
    if (flags.focused) {
        panel_worldgen_controls_draw_focus_ring(
            hdc, geometry->track, rect_height(geometry->track) / 2, 1);
    }
    RestoreDC(hdc, saved);
}
