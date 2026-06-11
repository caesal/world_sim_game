#include "render/panel_country_population.h"

#include "render/panel_country_population_cards.h"
#include "render/snapshot_ui.h"
#include "render_panel_internal.h"
#include "ui/ui_clay_widgets.h"
#include "ui/ui_theme.h"

#include <stdio.h>
#include <string.h>

#define POPULATION_TAB_FIXED_H 920

static const int display_row_first[] = {
    75, 70, 65, 60, 55, 50, 45, 40, 35, 30, 25, 20, 15, 10, 5, 0
};
static const int display_row_last[] = {
    100, 74, 69, 64, 59, 54, 49, 44, 39, 34, 29, 24, 19, 14, 9, 4
};
#define DISPLAY_ROW_COUNT ((int)(sizeof(display_row_first) / sizeof(display_row_first[0])))

static COLORREF pressure_color(int pressure) {
    if (pressure < 50) return RGB(83, 143, 98);
    if (pressure <= 80) return RGB(106, 158, 186);
    if (pressure <= 105) return RGB(196, 154, 72);
    return RGB(188, 78, 68);
}

static int percent_of(int value, int max_value) {
    if (max_value <= 0) return 0;
    return (int)((long long)value * 100 / max_value);
}

static void format_signed(char *out, size_t out_size, int value) {
    snprintf(out, out_size, "%+d", value);
}

static void format_percent(char *out, size_t out_size, int value) {
    snprintf(out, out_size, "%d%%", value);
}

static void draw_bar_fill(HDC hdc, RECT rect, int value, int max_value, COLORREF color) {
    RECT inner = rect;
    RECT fill;
    ui_clay_draw_progress_bar(hdc, rect, 0, 100, color);
    InflateRect(&inner, -3, -3);
    if (inner.right <= inner.left || inner.bottom <= inner.top) return;
    fill = inner;
    value = clamp(value, 0, max(1, max_value));
    fill.right = fill.left + (fill.right - fill.left) * value / max(1, max_value);
    fill_rect(hdc, inner, RGB(47, 58, 63));
    if (fill.right > fill.left) fill_rect(hdc, fill, color);
}

static int draw_capacity_overview(HDC hdc, UiCursor *cursor, PopulationSummary summary) {
    RECT area = ui_take_rect(cursor, 58);
    RECT text = {area.left, area.top, area.right, area.top + 22};
    RECT bar = {area.left, area.top + 27, area.right, area.top + 41};
    RECT note = {area.left, area.top + 42, area.right, area.bottom};
    char pop[32];
    char cap[32];
    char line[144];
    int usage = percent_of(summary.total, summary.carrying_capacity);
    int delta = summary.carrying_capacity - summary.total;
    format_metric_value(summary.total, pop, sizeof(pop));
    format_metric_value(summary.carrying_capacity, cap, sizeof(cap));
    snprintf(line, sizeof(line), "%s / %s  %d%%", pop, cap, usage);
    draw_text_rect(hdc, text, line, ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_bar_fill(hdc, bar, min(usage, 140), 140, pressure_color(usage));
    format_metric_value(delta >= 0 ? delta : -delta, pop, sizeof(pop));
    snprintf(line, sizeof(line), "%s %s", delta >= 0 ? tr("Remaining capacity", "剩余承载") :
             tr("Over capacity by", "超出承载"), pop);
    draw_text_rect(hdc, note, line, ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    return usage;
}

static const char *display_row_label(int row) {
    static const char *labels[] = {
        "75+", "70-74", "65-69", "60-64", "55-59", "50-54",
        "45-49", "40-44", "35-39", "30-34", "25-29", "20-24",
        "15-19", "10-14", "5-9", "0-4"
    };
    return labels[row];
}

static int display_row_value(const int *ages, int row) {
    int first = display_row_first[row];
    int last = display_row_last[row];
    int total = 0;
    int age;
    for (age = first; age <= last && age < POP_DISPLAY_AGE_COUNT; age++) total += ages[age];
    return total;
}

static int display_row_density_value(const int *ages, int row) {
    int width = row == 0 ? 12 : 5;
    return (display_row_value(ages, row) + width / 2) / width;
}

static int max_population_side(const PopulationDisplayCohorts *display) {
    int max_value = 1;
    int row;
    for (row = 0; row < DISPLAY_ROW_COUNT; row++) {
        int male = display_row_density_value(display->male, row);
        int female = display_row_density_value(display->female, row);
        if (male > max_value) max_value = male;
        if (female > max_value) max_value = female;
    }
    return max_value;
}

static void draw_population_side_bar(HDC hdc, RECT rect, int value, int max_value,
                                     int left_side, COLORREF color) {
    RECT bar = rect;
    int width = (rect.right - rect.left) * value / max(1, max_value);
    fill_rect(hdc, rect, RGB(34, 42, 50));
    if (left_side) bar.left = rect.right - width;
    else bar.right = rect.left + width;
    fill_rect(hdc, bar, color);
}

static void draw_compact_pyramid(HDC hdc, UiCursor *cursor, PopulationSummary summary,
                                 const PopulationDisplayCohorts *display) {
    PopulationDisplayCohorts fallback;
    const PopulationDisplayCohorts *source = display;
    RECT area = ui_take_rect(cursor, 188);
    int center = area.left + (area.right - area.left) / 2;
    int bar_w = max(32, (area.right - area.left - 112) / 2);
    int row_h = 10;
    int bar_h = 7;
    int max_value;
    int y = area.top;
    int row;

    if (!source || population_display_total(source) <= 0) {
        population_display_uniform_from_summary(&fallback, summary);
        source = &fallback;
    }
    max_value = max_population_side(source);

    fill_rect(hdc, area, ui_theme_color(UI_COLOR_PANEL));
    draw_text_rect(hdc, (RECT){center - bar_w - 34, y, center - 36, y + 16}, tr("Male", "男"),
                   ui_theme_color(UI_COLOR_TEXT_MUTED), DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
    draw_text_rect(hdc, (RECT){center + 36, y, center + bar_w + 34, y + 16}, tr("Female", "女"),
                   ui_theme_color(UI_COLOR_TEXT_MUTED), DT_SINGLELINE | DT_LEFT | DT_VCENTER);
    y += 18;
    for (row = 0; row < DISPLAY_ROW_COUNT; row++) {
        RECT male = {center - bar_w - 34, y + 1, center - 34, y + 1 + bar_h};
        RECT female = {center + 34, y + 1, center + bar_w + 34, y + 1 + bar_h};
        RECT label = {center - 31, y, center + 31, y + row_h + 1};
        draw_population_side_bar(hdc, male, display_row_density_value(source->male, row),
                                 max_value, 1, RGB(83, 123, 166));
        draw_population_side_bar(hdc, female, display_row_density_value(source->female, row),
                                 max_value, 0, RGB(164, 102, 141));
        draw_center_text(hdc, label, display_row_label(row), RGB(218, 224, 230));
        y += row_h;
    }
    draw_text_rect(hdc, (RECT){area.left, y + 1, area.right, area.bottom},
                   tr("Age bands", "年龄段"),
                   ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS);
}

static void draw_pressure_bar(HDC hdc, UiCursor *cursor, const char *label, int value,
                              COLORREF color, int important) {
    RECT row = ui_take_rect(cursor, important ? 30 : 24);
    RECT label_rect = {row.left, row.top, row.left + 168, row.bottom};
    RECT bar = {row.left + 174, row.top + 8, row.right - 42, row.bottom - 7};
    RECT value_rect = {row.right - 38, row.top, row.right, row.bottom};
    char text[24];
    draw_text_rect(hdc, label_rect, label,
                   important ? ui_theme_color(UI_COLOR_TEXT) : ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_bar_fill(hdc, bar, value, 100, color);
    format_percent(text, sizeof(text), value);
    draw_text_rect(hdc, value_rect, text, ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
}

static void draw_pressure_section(HDC hdc, UiCursor *cursor, PopulationDiagnostics diag) {
    RECT formula = ui_take_rect(cursor, 22);
    draw_text_rect(hdc, formula,
                   tr("Actual = max(National Resource, Capacity Overload)",
                      "实际人口压力 = max(国家资源压力, 承载超载压力)"),
                   ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_pressure_bar(hdc, cursor, tr("Actual Population Pressure", "实际人口压力"),
                      diag.effective_pressure, pressure_color(diag.effective_pressure), 1);
    draw_pressure_bar(hdc, cursor, tr("National Resource Pressure", "国家资源压力"),
                      diag.national_resource_pressure, RGB(106, 158, 186), 0);
    draw_pressure_bar(hdc, cursor, tr("Capacity Overload Pressure", "承载超载压力"),
                      diag.local_overcapacity_pressure, RGB(196, 154, 72), 0);
}

static void draw_centered_net_bar(HDC hdc, UiCursor *cursor, int net_x100, int population) {
    RECT area = ui_take_rect(cursor, 34);
    RECT track = {area.left, area.top + 15, area.right, area.top + 27};
    RECT fill = track;
    int center = track.left + (track.right - track.left) / 2;
    int magnitude = net_x100 < 0 ? -net_x100 : net_x100;
    int limit = max(100, max(magnitude, max(10, population / 200) * 100));
    int width = (track.right - track.left) / 2 * magnitude / limit;
    char text[64];

    ui_clay_draw_progress_bar(hdc, track, 0, 100, RGB(83, 143, 98));
    InflateRect(&track, -3, -3);
    fill_rect(hdc, track, RGB(47, 58, 63));
    fill_rect(hdc, (RECT){center - 1, track.top - 5, center + 1, track.bottom + 5},
              RGB(220, 224, 210));
    if (net_x100 >= 0) {
        fill.left = center;
        fill.right = min(track.right, center + width);
        fill.top = track.top;
        fill.bottom = track.bottom;
        if (fill.right > fill.left) fill_rect(hdc, fill, RGB(83, 143, 98));
    } else {
        fill.left = max(track.left, center - width);
        fill.right = center;
        fill.top = track.top;
        fill.bottom = track.bottom;
        if (fill.right > fill.left) fill_rect(hdc, fill, RGB(188, 78, 68));
    }
    snprintf(text, sizeof(text), "%s%d.%02d", net_x100 < 0 ? "-" : "+",
             magnitude / 100, magnitude % 100);
    snprintf(text + strlen(text), sizeof(text) - strlen(text), "%s", tr(" / month", " / 月"));
    draw_text_rect(hdc, (RECT){area.left, area.top, area.right, area.top + 14}, text,
                   net_x100 >= 0 ? RGB(154, 210, 160) : RGB(224, 132, 124),
                   DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
}

static void draw_month_card(HDC hdc, RECT rect, int icon, const char *label,
                            int value, const char *suffix, COLORREF accent,
                            int signed_value) {
    char number[32];
    char text[48];

    if (signed_value) format_signed(number, sizeof(number), value);
    else format_metric_value(value, number, sizeof(number));
    snprintf(text, sizeof(text), "%s%s", number, suffix);
    ui_clay_draw_metric_chip_text(hdc, rect, icon, label, text, accent);
}

static void draw_month_card_x100(HDC hdc, RECT rect, int icon, const char *label,
                                 int value_x100, const char *suffix, COLORREF accent) {
    char text[48];
    int value = value_x100 < 0 ? -value_x100 : value_x100;
    snprintf(text, sizeof(text), "%s%d.%02d%s", value_x100 < 0 ? "-" : "",
             value / 100, value % 100, suffix);
    ui_clay_draw_metric_chip_text(hdc, rect, icon, label, text, accent);
}

static void draw_monthly_change_cards(HDC hdc, UiCursor *cursor,
                                      PopulationDiagnostics diag) {
    int gap = 6;
    int w = (cursor->width - gap * 2) / 3;
    int h = 34;
    const char *per_month = tr(" /mo", " /月");
    RECT area = ui_take_rect(cursor, h * 2 + gap + 4);
    RECT r = {area.left, area.top, area.left + w, area.top + h};

    draw_month_card_x100(hdc, r, ICON_POPULATION, metric_label("Estimated Births", "估算出生"),
                         diag.estimated_monthly_births_x100, per_month, RGB(83, 143, 98));
    r.left += w + gap; r.right += w + gap;
    draw_month_card_x100(hdc, r, ICON_DISORDER, metric_label("Total Deaths", "总死亡"),
                         -diag.estimated_total_deaths_x100, per_month, RGB(188, 78, 68));
    r.left += w + gap; r.right = area.right;
    draw_month_card(hdc, r, ICON_HABITABILITY, metric_label("Birth Multiplier", "出生系数"),
                    diag.birth_multiplier_percent, "%", pressure_color(100 - diag.birth_multiplier_percent), 0);

    r = (RECT){area.left, area.top + h + gap, area.left + w, area.top + h * 2 + gap};
    draw_month_card(hdc, r, ICON_DISORDER, metric_label("Pressure Deaths", "压力死亡"),
                    -diag.estimated_pressure_deaths, per_month, RGB(196, 154, 72), 1);
    r.left += w + gap; r.right += w + gap;
    draw_month_card_x100(hdc, r, ICON_HABITABILITY, metric_label("Natural/Age Deaths", "自然年龄死亡"),
                         -diag.estimated_natural_age_deaths_x100, per_month, RGB(188, 118, 82));
    r.left += w + gap; r.right = area.right;
    draw_month_card_x100(hdc, r, ICON_POPULATION, metric_label("Child Accidental Deaths", "儿童意外死亡"),
                         -diag.estimated_child_accidental_deaths_x100, per_month, RGB(164, 102, 141));
}

static void draw_monthly_change(HDC hdc, UiCursor *cursor, PopulationSummary summary,
                                PopulationDiagnostics diag) {
    draw_monthly_change_cards(hdc, cursor, diag);
    draw_centered_net_bar(hdc, cursor, diag.estimated_net_monthly_change_x100, summary.total);
}

static void draw_treasury_buffer(HDC hdc, UiCursor *cursor, const SnapshotCiv *civ) {
    char current[32];
    char cap[32];
    char balance[32];
    char pair[72];
    char years[24];
    int gap = 4;
    int w = (cursor->width - gap) / 2;
    int h = 30;
    RECT area;
    RECT r;
    if (!civ) return;
    format_metric_value(civ->treasury, current, sizeof(current));
    format_metric_value(civ->treasury_cap, cap, sizeof(cap));
    format_signed(balance, sizeof(balance), civ->treasury_last_annual_balance);
    snprintf(pair, sizeof(pair), "%s / %s", current, cap);
    snprintf(years, sizeof(years), "%d", civ->treasury_deficit_years);
    area = ui_take_rect(cursor, h * 2 + gap + 4);
    r = (RECT){area.left, area.top, area.left + w, area.top + h};
    ui_clay_draw_metric_chip_text(hdc, r, ICON_MONEY, metric_label("Treasury / Cap", "国库 / 上限"),
                                  pair, RGB(106, 158, 186));
    r.left += w + gap; r.right = area.right;
    ui_clay_draw_metric_chip_text(hdc, r, ICON_MONEY, metric_label("Last Year", "上年盈亏"),
                                  balance, civ->treasury_last_annual_balance < 0 ?
                                  RGB(188, 78, 68) : RGB(83, 143, 98));
    r = (RECT){area.left, area.top + h + gap, area.left + w, area.top + h * 2 + gap};
    ui_clay_draw_metric_chip_text(hdc, r, ICON_DISORDER, metric_label("Deficit Years", "赤字年数"),
                                  years, RGB(196, 154, 72));
    r.left += w + gap; r.right = area.right;
    ui_clay_draw_metric_chip_text(hdc, r, ICON_HABITABILITY, metric_label("Buffer", "缓冲状态"),
                                  civ->treasury_last_deficit > 0 && civ->treasury > 0 ?
                                  tr("Buffering", "正在缓冲") :
                                  (civ->treasury_last_deficit > 0 ? tr("No buffer", "无缓冲") :
                                   tr("No deficit", "无赤字")),
                                  civ->treasury_last_deficit > 0 ? RGB(188, 118, 82) :
                                  RGB(83, 143, 98));
}

static void draw_city_header(HDC hdc, RECT row) {
    int total = row.right - row.left;
    int name_w = total * 22 / 100;
    int pop_w = total * 13 / 100;
    int cap_w = total * 13 / 100;
    int usage_w = total * 12 / 100;
    int type_w = total * 24 / 100;
    int x = row.left;
    draw_text_rect(hdc, (RECT){x, row.top, x + name_w, row.bottom},
                   tr("City", "城市"), ui_theme_color(UI_COLOR_TEXT_MUTED), DT_SINGLELINE | DT_VCENTER);
    x += name_w;
    draw_text_rect(hdc, (RECT){x, row.top, x + pop_w, row.bottom},
                   tr("Population", "人口"), ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
    x += pop_w;
    draw_text_rect(hdc, (RECT){x, row.top, x + cap_w, row.bottom},
                   tr("Capacity", "承载"), ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
    x += cap_w;
    draw_text_rect(hdc, (RECT){x, row.top, x + usage_w - 6, row.bottom},
                   tr("Usage", "使用率"), ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
    x += usage_w;
    draw_text_rect(hdc, (RECT){x + 8, row.top, x + type_w, row.bottom},
                   tr("Type", "类型"), ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    x += type_w;
    draw_text_rect(hdc, (RECT){x + 3, row.top, row.right, row.bottom},
                   tr("Status", "状态"), ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}

static const char *city_type_text(const SnapshotCity *city) {
    if (city->capital && city->port) return tr("Harbor Capital", "港口首都");
    if (city->capital) return tr("City Capital", "城市首都");
    if (city->port) return tr("Harbor", "港口");
    return tr("City", "城市");
}

static const char *city_status_text(int usage) {
    if (usage < 80) return tr("Loose", "宽松");
    if (usage <= 100) return tr("Normal", "正常");
    if (usage <= 125) return tr("Overloaded", "超载");
    return tr("Severe", "严重超载");
}

static void draw_city_row(HDC hdc, RECT row, const SnapshotCity *city) {
    int total = row.right - row.left;
    int name_w = total * 22 / 100;
    int pop_w = total * 13 / 100;
    int cap_w = total * 13 / 100;
    int usage_w = total * 12 / 100;
    int type_w = total * 24 / 100;
    int x = row.left;
    int usage = percent_of(city->population_summary.total,
                           city->population_summary.carrying_capacity);
    char pop[32];
    char cap[32];
    char pct[24];
    char name[96];
    fill_rect(hdc, row, RGB(31, 37, 40));
    snapshot_ui_city_display_name(snapshot_ui_current(), city, name, sizeof(name));
    format_metric_value(city->population_summary.total, pop, sizeof(pop));
    format_metric_value(city->population_summary.carrying_capacity, cap, sizeof(cap));
    format_percent(pct, sizeof(pct), usage);
    draw_text_rect(hdc, (RECT){x + 4, row.top, x + name_w - 4, row.bottom},
                   name, ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    x += name_w;
    draw_text_rect(hdc, (RECT){x, row.top, x + pop_w, row.bottom},
                   pop, ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
    x += pop_w;
    draw_text_rect(hdc, (RECT){x, row.top, x + cap_w, row.bottom},
                   cap, ui_theme_color(UI_COLOR_TEXT_MUTED), DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
    x += cap_w;
    draw_text_rect(hdc, (RECT){x, row.top, x + usage_w, row.bottom},
                   city->population_summary.carrying_capacity > 0 ? pct : "--",
                   pressure_color(usage), DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
    x += usage_w;
    draw_text_rect(hdc, (RECT){x + 3, row.top, x + type_w, row.bottom},
                   city_type_text(city), ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    x += type_w;
    draw_text_rect(hdc, (RECT){x + 3, row.top, row.right - 4, row.bottom},
                   city_status_text(usage), ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}

static void draw_top_cities(HDC hdc, UiCursor *cursor, const SnapshotCiv *civ) {
    int i;
    int shown = 0;
    char text[96];
    RECT header = ui_take_rect(cursor, 18);
    draw_city_header(hdc, header);
    for (i = 0; civ && i < POPULATION_TOP_CITY_COUNT; i++) {
        int city_id = civ->population_top_city_ids[i];
        const SnapshotCity *city = snapshot_ui_city(city_id);
        RECT row;
        if (!city || !city->alive || city->owner != civ->id) continue;
        row = ui_take_rect(cursor, 18);
        draw_city_row(hdc, row, city);
        cursor->y += 1;
        shown++;
    }
    if (civ && civ->population_city_count > shown) {
        snprintf(text, sizeof(text), "%s %d %s", tr("Other", "其余"),
                 civ->population_city_count - shown, tr("cities", "座城市"));
        draw_text_rect(hdc, ui_take_rect(cursor, 16), text, ui_theme_color(UI_COLOR_TEXT_MUTED),
                       DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }
}

int country_population_tab_height(int civ_id) {
    (void)civ_id;
    return POPULATION_TAB_FIXED_H;
}

void draw_country_population_tab(HDC hdc, RECT client, UiCursor *cursor,
                                 int civ_id, HFONT body_font) {
    const SnapshotCiv *civ = snapshot_ui_civ(civ_id);
    PopulationSummary summary = civ ? civ->population_summary : (PopulationSummary){0};
    PopulationDiagnostics diag = civ ? civ->population_diagnostics : (PopulationDiagnostics){0};

    (void)client;
    SelectObject(hdc, body_font);
    ui_section(hdc, cursor, tr("Population Capacity", "人口承载总览"));
    draw_capacity_overview(hdc, cursor, summary);
    ui_section(hdc, cursor, tr("Population Structure", "人口结构"));
    draw_compact_pyramid(hdc, cursor, summary, civ ? &civ->population_display : NULL);
    draw_population_structure_cards(hdc, cursor, summary,
                                    civ ? civ->current_soldiers : 0);
    ui_section(hdc, cursor, tr("Population Pressure Sources", "人口压力来源"));
    draw_pressure_section(hdc, cursor, diag);
    ui_section(hdc, cursor, tr("Monthly Population Change", "月度人口变化"));
    draw_monthly_change(hdc, cursor, summary, diag);
    ui_section(hdc, cursor, tr("Treasury Buffer", "国库缓冲"));
    draw_treasury_buffer(hdc, cursor, civ);
    ui_section(hdc, cursor, tr("City Population Top 6", "城市人口 Top 6"));
    draw_top_cities(hdc, cursor, civ);
}
