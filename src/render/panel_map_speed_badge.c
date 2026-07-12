#include "render/panel_map_speed_badge.h"

#include "render/render_common.h"
#include "ui/ui_layout.h"
#include "ui/ui_types.h"

#include <stdio.h>
#include <string.h>

static COLORREF render_ms_color(int render_ms) {
    if (render_ms <= 50) return RGB(82, 224, 132);
    if (render_ms <= 100) return RGB(238, 207, 86);
    return RGB(232, 104, 96);
}

static void rounded_panel(HDC hdc, RECT rect, int radius, COLORREF fill, COLORREF border) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ old_brush = SelectObject(hdc, brush);
    HGDIOBJ old_pen = SelectObject(hdc, pen);
    RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    DeleteObject(pen);
    DeleteObject(brush);
}

static void draw_chip_text_pair(HDC hdc, RECT rect, const char *label,
                                const char *value, COLORREF value_color) {
    SIZE label_size = {0}, value_size = {0};
    int gap = 6;
    RECT inner = {rect.left + 14, rect.top, rect.right - 12, rect.bottom};
    int available = max(0, inner.right - inner.left);
    int value_w, label_w;
    measure_text_utf8(hdc, label, &label_size);
    measure_text_utf8(hdc, value, &value_size);
    value_w = min(value_size.cx, available);
    label_w = min(label_size.cx, max(0, available - gap - value_w));
    if (label_w <= 0 && available > value_w) gap = 0;
    if (value_w + label_w + gap > available) value_w = max(0, available - label_w - gap);
    RECT label_rect = {inner.left, rect.top, inner.left + label_w, rect.bottom};
    RECT value_rect = {label_rect.right + gap, rect.top, inner.right, rect.bottom};
    draw_text_rect(hdc, label_rect, label, RGB(222, 226, 228),
                   DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
    draw_text_rect(hdc, value_rect, value, value_color,
                   DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
}

void panel_map_bottom_status_build(PanelMapBottomStatus *out, RECT client, int panel_w,
                                   int language, int render_ms, int pending,
                                   int auto_running, int overloaded) {
    RECT row = get_bottom_control_row_rect(client);
    int x = 398;
    int right_limit = client.right - panel_w - 12;
    int render_w = language == UI_LANG_ZH ? 132 : 150;
    int queue_w = language == UI_LANG_ZH ? 118 : 108;
    int status_w = language == UI_LANG_ZH ? 102 : 140;
    int gap = 10;
    int total_w = render_w + queue_w + status_w + gap * 2;
    if (!out) return;
    memset(out, 0, sizeof(*out));
    snprintf(out->render_label, sizeof(out->render_label), "%s",
             language == UI_LANG_ZH ? "渲染" : "Render");
    snprintf(out->render_value, sizeof(out->render_value), "%dms", render_ms);
    snprintf(out->queue_label, sizeof(out->queue_label), "%s",
             language == UI_LANG_ZH ? "队列" : "Queue");
    snprintf(out->queue_value, sizeof(out->queue_value), "%d", pending);
    snprintf(out->status_text, sizeof(out->status_text), "%s",
             !auto_running ? (language == UI_LANG_ZH ? "暂停" : "Paused") :
             overloaded ? (language == UI_LANG_ZH ? "过载" : "Overloaded") :
             (language == UI_LANG_ZH ? "稳定" : "Stable"));
    out->render_value_color = render_ms_color(render_ms);
    out->status_color = !auto_running ? RGB(176, 184, 190) :
                        overloaded ? RGB(232, 104, 96) : RGB(82, 224, 132);
    if (right_limit - x < total_w) {
        gap = right_limit - x >= total_w - 6 ? 7 : 6;
        total_w = render_w + queue_w + status_w + gap * 2;
    }
    if (right_limit - x < total_w) x = right_limit - total_w;
    if (x < 12) x = 12;
    out->render_rect = (RECT){x, row.top, x + render_w, row.bottom};
    out->queue_rect = (RECT){out->render_rect.right + gap, row.top,
                             out->render_rect.right + gap + queue_w, row.bottom};
    out->status_rect = (RECT){out->queue_rect.right + gap, row.top,
                              out->queue_rect.right + gap + status_w, row.bottom};
}

RECT panel_map_bottom_status_dot_rect(const PanelMapBottomStatus *status) {
    int center_y;
    if (!status) return (RECT){0, 0, 0, 0};
    center_y = (status->status_rect.top + status->status_rect.bottom) / 2;
    return (RECT){status->status_rect.right - 28, center_y - 5,
                  status->status_rect.right - 17, center_y + 6};
}

void panel_map_draw_bottom_status_chips(HDC hdc, RECT client, int panel_w,
                                        int language, int render_ms, int pending,
                                        int auto_running, int overloaded) {
    PanelMapBottomStatus status;
    RECT dot;
    panel_map_bottom_status_build(&status, client, panel_w, language, render_ms,
                                  pending, auto_running, overloaded);
    rounded_panel(hdc, status.render_rect, 11, RGB(27, 36, 40), RGB(73, 87, 92));
    rounded_panel(hdc, status.queue_rect, 11, RGB(27, 36, 40), RGB(73, 87, 92));
    rounded_panel(hdc, status.status_rect, 11, RGB(27, 36, 40), RGB(73, 87, 92));
    draw_chip_text_pair(hdc, status.render_rect, status.render_label,
                        status.render_value, status.render_value_color);
    draw_chip_text_pair(hdc, status.queue_rect, status.queue_label,
                        status.queue_value, RGB(238, 207, 86));
    draw_text_rect(hdc, (RECT){status.status_rect.left + 18, status.status_rect.top,
                   status.status_rect.right - 28, status.status_rect.bottom},
                   status.status_text, status.status_color,
                   DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
    dot = panel_map_bottom_status_dot_rect(&status);
    rounded_panel(hdc, dot, 9, status.status_color, RGB(70, 86, 72));
}

void panel_map_format_actual_speed_badge(char *out, size_t size, int actual_ms) {
    if (!out || size == 0) return;
    if (actual_ms > 0) snprintf(out, size, "%dms", actual_ms);
    else snprintf(out, size, "--ms");
}

RECT panel_map_actual_speed_badge_rect(RECT client) {
    return get_map_actual_speed_badge_rect(client);
}

int panel_map_actual_speed_badge_inside_frame(RECT client) {
    RECT frame = get_map_viewport_rect(client);
    RECT badge = panel_map_actual_speed_badge_rect(client);
    return badge.left >= frame.left && badge.top >= frame.top &&
           badge.right <= frame.right && badge.bottom <= frame.bottom;
}

void panel_map_draw_actual_speed_badge(HDC hdc, RECT client, int actual_ms) {
    char text[24];
    SIZE size;
    RECT badge = panel_map_actual_speed_badge_rect(client);
    int width;
    panel_map_format_actual_speed_badge(text, sizeof(text), actual_ms);
    measure_text_utf8(hdc, text, &size);
    width = size.cx + 18;
    if (width < 48) width = 48;
    if (width > badge.right - badge.left) width = badge.right - badge.left;
    badge.right = badge.left + width;
    rounded_panel(hdc, badge, 8, RGB(17, 24, 28), RGB(70, 78, 82));
    draw_text_rect(hdc, badge, text, RGB(242, 244, 246),
                   DT_SINGLELINE | DT_CENTER | DT_VCENTER);
}
