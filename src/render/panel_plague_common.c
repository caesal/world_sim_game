#include "render/panel_plague_common.h"

#include "render/render_common.h"
#include "ui/ui_clay_widgets.h"
#include "ui/ui_types.h"

#include <stdio.h>
#include <string.h>

const char *plague_panel_size_label(PlagueSize size) {
    if (size == PLAGUE_SIZE_SMALL) return tr("Small", "小型");
    if (size == PLAGUE_SIZE_MEDIUM) return tr("Medium", "中型");
    if (size == PLAGUE_SIZE_LARGE) return tr("Large", "大型");
    return tr("None", "无");
}

void plague_panel_format_absolute_month(int absolute_month, char *out,
                                        size_t out_size) {
    int safe = max(0, absolute_month);
    int display_year = safe / 12;
    int display_month = safe % 12 + 1;
    if (ui_language == UI_LANG_ZH) {
        snprintf(out, out_size, "%d年%d月", display_year, display_month);
    } else {
        snprintf(out, out_size, "Year %d Month %d", display_year, display_month);
    }
}

void plague_panel_format_duration(int months, char *out, size_t out_size) {
    int safe = max(0, months);
    int years = safe / 12;
    int remainder = safe % 12;
    if (ui_language == UI_LANG_ZH) {
        snprintf(out, out_size, "%d个月（%d年%d个月）", safe, years, remainder);
    } else {
        snprintf(out, out_size, "%d months (%dy %dm)", safe, years, remainder);
    }
}

void plague_panel_format_count64(int64_t value, char *out, size_t out_size) {
    char raw[32];
    size_t digits;
    size_t source;
    size_t target = 0;
    if (!out || out_size == 0) return;
    snprintf(raw, sizeof(raw), "%lld", (long long)max(INT64_C(0), value));
    digits = strlen(raw);
    for (source = 0; source < digits && target + 1 < out_size; source++) {
        if (source > 0 && (digits - source) % 3 == 0 && target + 1 < out_size) {
            out[target++] = ',';
        }
        if (target + 1 < out_size) out[target++] = raw[source];
    }
    out[target] = '\0';
}

void plague_panel_format_annual_mortality(int severity, char *out,
                                          size_t out_size) {
    snprintf(out, out_size, "%d%%", clamp(severity + 5, 6, 15));
}

void plague_panel_row(HDC hdc, UiCursor *cursor, const char *label,
                      const char *value) {
    RECT row = ui_take_rect(cursor, 22);
    RECT left = row;
    RECT right = row;
    int label_width = clamp(cursor->width * 43 / 100, 94, 190);
    left.right = min(row.right, left.left + label_width);
    right.left = min(row.right, left.right + 6);
    draw_text_rect(hdc, left, label, ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    draw_text_rect(hdc, right, value && value[0] ? value : "—",
                   ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_END_ELLIPSIS |
                       DT_NOPREFIX);
}

void plague_panel_pair(HDC hdc, UiCursor *cursor,
                       const char *left_label, const char *left_value,
                       const char *right_label, const char *right_value) {
    char left_text[160];
    char right_text[160];
    RECT row = ui_take_rect(cursor, 22);
    RECT left = row;
    RECT right = row;
    int gap = 10;
    int half = (cursor->width - gap) / 2;
    snprintf(left_text, sizeof(left_text), "%s: %s", left_label, left_value);
    snprintf(right_text, sizeof(right_text), "%s: %s", right_label, right_value);
    left.right = left.left + half;
    right.left = left.right + gap;
    draw_text_rect(hdc, left, left_text, ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    draw_text_rect(hdc, right, right_text, ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
}

void plague_panel_progress(HDC hdc, UiCursor *cursor, const char *label,
                           const char *value, int amount, int maximum,
                           COLORREF color) {
    RECT row = ui_take_rect(cursor, 20);
    RECT label_rect = row;
    RECT value_rect = row;
    RECT bar = ui_take_rect(cursor, 12);
    label_rect.right = row.left + cursor->width * 62 / 100;
    value_rect.left = label_rect.right + 4;
    draw_text_rect(hdc, label_rect, label, ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    draw_text_rect(hdc, value_rect, value, ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_END_ELLIPSIS |
                       DT_NOPREFIX);
    InflateRect(&bar, 0, -1);
    ui_clay_draw_progress_bar(hdc, bar, amount, maximum, color);
    cursor->y += 4;
}

void plague_panel_draw_immunity(HDC hdc, UiCursor *cursor,
                                const PlagueStateView *state) {
    char a[24], b[24], c[24], d[24];
    snprintf(a, sizeof(a), "%d", state->immunity_city_count[0]);
    snprintf(b, sizeof(b), "%d", state->immunity_city_count[1]);
    snprintf(c, sizeof(c), "%d", state->immunity_city_count[2]);
    snprintf(d, sizeof(d), "%d", state->immunity_city_count[3]);
    ui_section(hdc, cursor, tr("City Immunity", "城市免疫"));
    plague_panel_pair(hdc, cursor, "30%", a, "50%", b);
    plague_panel_pair(hdc, cursor, "80%", c, "100%", d);
}

void plague_panel_draw_schedule(HDC hdc, UiCursor *cursor,
                                const PlagueStateView *state) {
    char value[128];
    ui_section(hdc, cursor, tr("Plague Schedule", "瘟疫日程"));
    plague_panel_format_absolute_month(state->next_scheduled_check_month,
                                       value, sizeof(value));
    plague_panel_row(hdc, cursor, tr("Next check", "下次检查"), value);
    snprintf(value, sizeof(value), "%d / %d", state->starts_in_rolling_window,
             PLAGUE_ROLLING_START_CAP);
    plague_panel_row(hdc, cursor,
                     tr("Starts / 100y window", "滚动100年开始"), value);
    if (state->episode.active) {
        plague_panel_row(hdc, cursor, tr("Checks while active", "活跃期检查"),
                         tr("Skipped; never postponed", "跳过；绝不顺延"));
    }
}
