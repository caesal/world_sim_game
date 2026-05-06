#include "ui_widgets.h"

UiCursor ui_cursor(int x, int y, int width, int bottom) {
    UiCursor cursor;
    cursor.x = x;
    cursor.y = y;
    cursor.width = width;
    cursor.bottom = bottom;
    return cursor;
}

RECT ui_take_rect(UiCursor *cursor, int height) {
    RECT rect = {cursor->x, cursor->y, cursor->x + cursor->width, cursor->y + height};
    cursor->y += height;
    return rect;
}

void ui_section(HDC hdc, UiCursor *cursor, const char *title) {
    RECT rule;
    if (cursor->y > cursor->bottom - 24) return;
    draw_text_line(hdc, cursor->x, cursor->y, title, ui_theme_color(UI_COLOR_TEXT));
    rule.left = cursor->x;
    rule.top = cursor->y + 22;
    rule.right = cursor->x + cursor->width;
    rule.bottom = rule.top + 1;
    fill_rect(hdc, rule, ui_theme_color(UI_COLOR_PANEL_LINE));
    cursor->y += 31;
}

void ui_row_text(HDC hdc, UiCursor *cursor, const char *label, const char *value) {
    char text[256];
    if (cursor->y > cursor->bottom - 20) return;
    snprintf(text, sizeof(text), "%s: %s", label, value);
    draw_text_line(hdc, cursor->x, cursor->y, text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    cursor->y += 21;
}

void ui_row_int(HDC hdc, UiCursor *cursor, const char *label, int value) {
    char text[64];
    format_metric_value(value, text, sizeof(text));
    ui_row_text(hdc, cursor, label, text);
}

void ui_stat_chip(HDC hdc, RECT rect, const char *label, int value, COLORREF accent) {
    RECT stripe = rect;
    char text[64];

    fill_rect(hdc, rect, ui_theme_color(UI_COLOR_PANEL_SOFT));
    stripe.right = stripe.left + 3;
    fill_rect(hdc, stripe, accent);
    draw_text_line(hdc, rect.left + 9, rect.top + 5, label, ui_theme_color(UI_COLOR_TEXT_DIM));
    format_metric_value(value, text, sizeof(text));
    draw_center_text(hdc, rect, text, ui_theme_color(UI_COLOR_TEXT));
}

void ui_metric_chip(HDC hdc, RECT rect, IconId icon, const char *label, int value, COLORREF accent) {
    RECT stripe = rect;
    RECT icon_rect = {rect.left + 6, rect.top + 5, rect.left + 24, rect.top + 23};
    RECT label_rect = {rect.left + 29, rect.top + 3, rect.right - 30, rect.top + 15};
    RECT value_rect = {rect.left + 29, rect.top + 14, rect.right - 8, rect.bottom - 2};
    char text[64];

    fill_rect(hdc, rect, ui_theme_color(UI_COLOR_PANEL_SOFT));
    stripe.right = stripe.left + 3;
    fill_rect(hdc, stripe, accent);
    draw_icon(hdc, icon, icon_rect, accent);
    draw_text_rect(hdc, label_rect, label, ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    format_metric_value(value, text, sizeof(text));
    draw_text_rect(hdc, value_rect, text, ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_END_ELLIPSIS);
}

void ui_progress_bar(HDC hdc, RECT rect, int value, int max_value, COLORREF color) {
    RECT fill = rect;
    if (max_value <= 0) max_value = 1;
    value = clamp(value, 0, max_value);
    fill_rect(hdc, rect, RGB(30, 35, 38));
    fill.right = rect.left + (rect.right - rect.left) * value / max_value;
    fill_rect(hdc, fill, color);
}
