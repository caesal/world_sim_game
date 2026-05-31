#include "ui/ui_clay_primitives.h"

static int clay_rect_width(RECT rect) {
    return rect.right - rect.left;
}

static int clay_rect_height(RECT rect) {
    return rect.bottom - rect.top;
}

static int clay_radius_for(RECT rect, int radius) {
    int max_radius = min(clay_rect_width(rect), clay_rect_height(rect));
    if (max_radius < 2) return 2;
    return min(radius, max_radius);
}

static void clay_round_rect(HDC hdc, RECT rect, int radius, COLORREF fill, COLORREF border) {
    HBRUSH brush;
    HPEN pen;
    HGDIOBJ old_brush;
    HGDIOBJ old_pen;
    int r;

    if (clay_rect_width(rect) <= 0 || clay_rect_height(rect) <= 0) return;
    r = clay_radius_for(rect, radius);
    brush = CreateSolidBrush(fill);
    pen = CreatePen(PS_SOLID, 1, border);
    if (!brush || !pen) {
        if (brush) DeleteObject(brush);
        if (pen) DeleteObject(pen);
        return;
    }
    old_brush = SelectObject(hdc, brush);
    old_pen = SelectObject(hdc, pen);
    RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, r, r);
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    DeleteObject(pen);
    DeleteObject(brush);
}

static void clay_highlight(HDC hdc, RECT rect, const UiClayStyle *style) {
    HPEN pen;
    HGDIOBJ old_pen;
    int inset = max(2, style->radius / 4);

    if (clay_rect_width(rect) <= 4 || clay_rect_height(rect) <= 4) return;
    pen = CreatePen(PS_SOLID, 1, style->highlight);
    if (!pen) return;
    old_pen = SelectObject(hdc, pen);
    MoveToEx(hdc, rect.left + inset, rect.top + 1, NULL);
    LineTo(hdc, rect.right - inset, rect.top + 1);
    MoveToEx(hdc, rect.left + 1, rect.top + inset, NULL);
    LineTo(hdc, rect.left + 1, rect.bottom - inset);
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
}

static void clay_inner_shadow(HDC hdc, RECT rect, const UiClayStyle *style) {
    HPEN pen;
    HGDIOBJ old_pen;
    int inset = max(2, style->radius / 4);

    if (clay_rect_width(rect) <= 4 || clay_rect_height(rect) <= 4) return;
    pen = CreatePen(PS_SOLID, 1, style->shadow);
    if (!pen) return;
    old_pen = SelectObject(hdc, pen);
    MoveToEx(hdc, rect.left + inset, rect.bottom - 2, NULL);
    LineTo(hdc, rect.right - inset, rect.bottom - 2);
    MoveToEx(hdc, rect.right - 2, rect.top + inset, NULL);
    LineTo(hdc, rect.right - 2, rect.bottom - inset);
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
}

static void clay_draw_surface(HDC hdc, RECT rect, UiClaySurface surface, UiClayState state) {
    UiClayStyle style = ui_clay_style(surface, state);
    RECT shadow = rect;
    RECT shadow_soft = rect;

    if (clay_rect_width(rect) <= 0 || clay_rect_height(rect) <= 0) return;
    OffsetRect(&shadow_soft, style.shadow_offset + 2, style.shadow_offset + 2);
    OffsetRect(&shadow, style.shadow_offset, style.shadow_offset);
    clay_round_rect(hdc, shadow_soft, style.radius, RGB(31, 36, 38), RGB(31, 36, 38));
    clay_round_rect(hdc, shadow, style.radius, style.shadow, style.shadow);
    clay_round_rect(hdc, rect, style.radius, style.fill, style.border);
    clay_highlight(hdc, rect, &style);
}

static void clay_draw_inset_surface(HDC hdc, RECT rect, UiClaySurface surface, UiClayState state) {
    UiClayStyle style = ui_clay_style(surface, state);

    if (clay_rect_width(rect) <= 0 || clay_rect_height(rect) <= 0) return;
    clay_round_rect(hdc, rect, style.radius, style.fill, style.border);
    clay_highlight(hdc, rect, &style);
    clay_inner_shadow(hdc, rect, &style);
}

void ui_clay_draw_shell(HDC hdc, RECT rect) {
    clay_draw_surface(hdc, rect, UI_CLAY_SURFACE_SHELL, UI_CLAY_STATE_NORMAL);
}

void ui_clay_draw_panel(HDC hdc, RECT rect, UiClayState state) {
    clay_draw_surface(hdc, rect, UI_CLAY_SURFACE_PANEL, state);
}

void ui_clay_draw_card(HDC hdc, RECT rect, UiClayState state) {
    clay_draw_surface(hdc, rect, UI_CLAY_SURFACE_CARD, state);
}

void ui_clay_draw_pill(HDC hdc, RECT rect, UiClayState state) {
    clay_draw_surface(hdc, rect, UI_CLAY_SURFACE_PILL, state);
}

void ui_clay_draw_pill_inset(HDC hdc, RECT rect, UiClayState state) {
    clay_draw_inset_surface(hdc, rect, UI_CLAY_SURFACE_PILL, state);
}

void ui_clay_draw_tab(HDC hdc, RECT rect, UiClayState state) {
    clay_draw_surface(hdc, rect, UI_CLAY_SURFACE_TAB, state);
}
