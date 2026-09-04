#include "render/panel_plague_chart.h"

#include "render/panel_plague_common.h"
#include "render/render_common.h"
#include "ui/ui_clay_primitives.h"
#include "ui/ui_clay_theme.h"
#include "ui/ui_types.h"

#include <stdio.h>

static COLORREF size_color(PlagueSize size) {
    UiClaySemanticTone tone = size == PLAGUE_SIZE_SMALL ? UI_CLAY_TONE_PEACE :
                              size == PLAGUE_SIZE_MEDIUM ? UI_CLAY_TONE_TENSE :
                              UI_CLAY_TONE_WAR;
    return ui_clay_semantic_style(tone).accent;
}

static int history_value(const PlagueEpisodeHistory *history,
                         PlaguePanelChartMetric metric, int64_t *out) {
    if (!history || !out) return 0;
    switch (metric) {
        case PLAGUE_CHART_SEVERITY: *out = history->severity; return 1;
        case PLAGUE_CHART_DURATION: *out = history->duration_months; return 1;
        case PLAGUE_CHART_DEATHS: *out = history->total_deaths; return 1;
        case PLAGUE_CHART_CITIES: *out = history->infected_city_count; return 1;
        case PLAGUE_CHART_COUNTRIES: *out = history->affected_country_count; return 1;
        case PLAGUE_CHART_SPORES: *out = history->spores_initial; return 1;
        default: *out = 0; return 0;
    }
}

int64_t plague_panel_chart_rounded_max(int64_t maximum) {
    int64_t unit = 1;
    int64_t normalized;
    int64_t multiplier;
    if (maximum <= 0) return 1;
    while (unit <= INT64_MAX / 10 && maximum > unit * 10) unit *= 10;
    normalized = (maximum + unit - 1) / unit;
    if (normalized <= 1) multiplier = 1;
    else if (normalized <= 2) multiplier = 2;
    else if (normalized <= 5) multiplier = 5;
    else multiplier = 10;
    if (multiplier > 0 && unit > INT64_MAX / multiplier) return INT64_MAX;
    return multiplier * unit;
}

int plague_panel_chart_duration_max(const PlagueEpisodeHistory *history,
                                    int count) {
    int maximum = 300;
    int i;
    for (i = 0; history && i < count; i++) {
        maximum = max(maximum, history[i].duration_months);
    }
    return ((maximum + 59) / 60) * 60;
}

int plague_panel_chart_value_height(int64_t value, int64_t maximum,
                                    int plot_height) {
    long double scaled;
    int64_t height;
    if (value <= 0 || maximum <= 0 || plot_height <= 0) return 0;
    scaled = (long double)value * (long double)plot_height /
             (long double)maximum;
    height = value >= maximum ? plot_height : (int64_t)scaled;
    return (int)clamp(height, INT64_C(1), (int64_t)plot_height);
}

int plague_panel_chart_type_category_y(PlagueSize size, int plot_top,
                                       int plot_bottom) {
    int category = clamp((int)size - (int)PLAGUE_SIZE_SMALL, 0, 2);
    return plot_bottom - (category * 2 + 1) *
           max(0, plot_bottom - plot_top) / 6;
}

int plague_panel_chart_spore_fill_height(int capacity_height, int spores_used,
                                         int spores_initial) {
    int used;
    if (capacity_height <= 0 || spores_initial <= 0) return 0;
    used = clamp(spores_used, 0, spores_initial);
    return (int)((int64_t)capacity_height * used / spores_initial);
}

static void compact_value(int64_t value, char *out, size_t out_size) {
    if (value >= INT64_C(1000000)) {
        int64_t whole = value / INT64_C(1000000);
        int64_t hundredths = value % INT64_C(1000000) / INT64_C(10000);
        snprintf(out, out_size, hundredths ? "%lld.%02lldM" : "%lldM",
                 (long long)whole, (long long)hundredths);
    } else if (value >= 1000) {
        int64_t whole = value / 1000;
        int64_t tenths = value % 1000 / 100;
        snprintf(out, out_size, tenths ? "%lld.%lldK" : "%lldK",
                 (long long)whole, (long long)tenths);
    } else {
        snprintf(out, out_size, "%lld", (long long)max(INT64_C(0), value));
    }
}

static void draw_guide(HDC hdc, RECT chart, RECT plot, int y,
                       const char *label, COLORREF color) {
    RECT line = {plot.left, y, plot.right, y + 1};
    RECT text = {chart.left, y - 9, plot.left - 5, y + 9};
    fill_rect(hdc, line, color);
    draw_text_rect(hdc, text, label, ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_SINGLELINE | DT_RIGHT | DT_VCENTER | DT_END_ELLIPSIS |
                       DT_NOPREFIX);
}

static int value_y(RECT plot, int64_t value, int64_t maximum) {
    int height = plague_panel_chart_value_height(
        value, maximum, max(1, plot.bottom - plot.top));
    return plot.bottom - height;
}

static int64_t axis_max_for(const PlagueEpisodeHistory *history, int count,
                            PlaguePanelChartMetric metric) {
    int i;
    int64_t maximum = 0;
    if (metric == PLAGUE_CHART_SEVERITY) return 10;
    if (metric == PLAGUE_CHART_DURATION) {
        return plague_panel_chart_duration_max(history, count);
    }
    for (i = 0; history && i < count; i++) {
        int64_t value;
        if (history_value(&history[i], metric, &value)) maximum = max(maximum, value);
    }
    maximum = plague_panel_chart_rounded_max(maximum);
    return maximum;
}

static void draw_linear_guides(HDC hdc, RECT chart, RECT plot, int64_t maximum) {
    char label[32];
    compact_value(maximum, label, sizeof(label));
    draw_guide(hdc, chart, plot, plot.top, label,
               ui_theme_color(UI_COLOR_PANEL_LINE));
    compact_value(maximum / 2, label, sizeof(label));
    draw_guide(hdc, chart, plot, (plot.top + plot.bottom) / 2,
               label, ui_theme_color(UI_COLOR_PANEL_LINE));
    draw_guide(hdc, chart, plot, plot.bottom - 1, "0",
               ui_theme_color(UI_COLOR_PANEL_LINE));
}

static void draw_duration_background(HDC hdc, RECT chart, RECT plot,
                                     int maximum) {
    static const int tier_start[] = {0, 80, 140, 200};
    static const int tier_end[] = {80, 140, 200, INT_MAX};
    static const int tier_percent[] = {30, 50, 80, 100};
    static const UiClaySemanticTone tones[] = {
        UI_CLAY_TONE_NEUTRAL, UI_CLAY_TONE_PEACE,
        UI_CLAY_TONE_TENSE, UI_CLAY_TONE_WAR
    };
    int i;
    for (i = 0; i < 4; i++) {
        int low = min(maximum, tier_start[i]);
        int high = min(maximum, tier_end[i]);
        RECT band;
        char label[16];
        if (high <= low) continue;
        band.left = plot.left;
        band.right = plot.right;
        band.top = value_y(plot, high, maximum);
        band.bottom = value_y(plot, low, maximum);
        fill_rect(hdc, band, ui_clay_semantic_style(tones[i]).soft_fill);
        snprintf(label, sizeof(label), "%d%%", tier_percent[i]);
        draw_text_rect(hdc, (RECT){chart.left, band.top, plot.left - 5,
                                   band.bottom},
                       label, ui_theme_color(UI_COLOR_TEXT_DIM),
                       DT_SINGLELINE | DT_RIGHT | DT_VCENTER | DT_NOPREFIX);
    }
    for (i = 0; i < 3; i++) {
        static const int guides[] = {80, 140, 200};
        char label[16];
        int y;
        if (guides[i] > maximum) continue;
        y = value_y(plot, guides[i], maximum);
        snprintf(label, sizeof(label), "%d", guides[i]);
        draw_guide(hdc, chart, plot, y, label,
                   ui_theme_color(UI_COLOR_PANEL_LINE));
    }
}

static void draw_type_chart(HDC hdc, RECT chart, RECT plot,
                            const RECT hits[PLAGUE_VIEW_HISTORY_COUNT],
                            const PlagueEpisodeHistory *history, int count,
                            int selected, int hovered) {
    const char *labels_en[] = {"Small", "Medium", "Large"};
    const char *labels_zh[] = {"小型", "中型", "大型"};
    int category;
    int i;
    for (category = 0; category < 3; category++) {
        int y = plague_panel_chart_type_category_y(
            (PlagueSize)(PLAGUE_SIZE_SMALL + category),
            plot.top, plot.bottom);
        draw_guide(hdc, chart, plot, y,
                   ui_language == UI_LANG_ZH ? labels_zh[category] :
                                               labels_en[category],
                   ui_theme_color(UI_COLOR_PANEL_LINE));
    }
    for (i = 0; i < count; i++) {
        int newest_offset = count - 1 - i;
        const PlagueEpisodeHistory *item = &history[newest_offset];
        int x = (hits[i].left + hits[i].right) / 2;
        int y = plague_panel_chart_type_category_y(
            item->size, plot.top, plot.bottom);
        RECT post = {x - 1, y + 8, x + 1, plot.bottom};
        RECT marker = {x - 9, y - 9, x + 9, y + 9};
        UiClayState state = newest_offset == hovered ? UI_CLAY_STATE_HOVER :
                            newest_offset == selected ? UI_CLAY_STATE_SELECTED :
                                                        UI_CLAY_STATE_NORMAL;
        fill_rect(hdc, post, size_color(item->size));
        ui_clay_draw_pill(hdc, marker, state);
        draw_center_text(hdc, marker,
                         item->size == PLAGUE_SIZE_SMALL ? tr("S", "小") :
                         item->size == PLAGUE_SIZE_MEDIUM ? tr("M", "中") :
                                                           tr("L", "大"),
                         size_color(item->size));
    }
}

static void draw_columns(HDC hdc, RECT chart, RECT plot,
                         const RECT hits[PLAGUE_VIEW_HISTORY_COUNT],
                         const PlagueEpisodeHistory *history, int count,
                         PlaguePanelChartMetric metric, int selected,
                         int hovered) {
    int64_t maximum = axis_max_for(history, count, metric);
    int i;
    if (metric == PLAGUE_CHART_DURATION) {
        draw_duration_background(hdc, chart, plot, (int)maximum);
    } else {
        draw_linear_guides(hdc, chart, plot, maximum);
    }
    for (i = 0; i < count; i++) {
        int newest_offset = count - 1 - i;
        const PlagueEpisodeHistory *item = &history[newest_offset];
        int64_t value = 0;
        int width = max(5, min(26, (hits[i].right - hits[i].left) * 3 / 5));
        int x = (hits[i].left + hits[i].right) / 2;
        int height;
        RECT bar;
        RECT outline;
        char label[24];
        history_value(item, metric, &value);
        height = plague_panel_chart_value_height(value, maximum,
                                                  plot.bottom - plot.top);
        bar = (RECT){x - width / 2, plot.bottom - height,
                     x + (width + 1) / 2, plot.bottom};
        outline = bar;
        if (newest_offset == selected || newest_offset == hovered) {
            InflateRect(&outline, 2, 2);
            fill_rect(hdc, outline, newest_offset == hovered ?
                      ui_theme_color(UI_COLOR_TEXT_MUTED) :
                      ui_theme_color(UI_COLOR_ACCENT));
        }
        fill_rect(hdc, bar, size_color(item->size));
        compact_value(value, label, sizeof(label));
        draw_text_rect(hdc, (RECT){hits[i].left, max(chart.top, bar.top - 18),
                                   hits[i].right, bar.top},
                       label, ui_theme_color(UI_COLOR_TEXT_MUTED),
                       DT_SINGLELINE | DT_CENTER | DT_BOTTOM | DT_END_ELLIPSIS |
                           DT_NOPREFIX);
    }
}

static void draw_spores(HDC hdc, RECT chart, RECT plot,
                        const RECT hits[PLAGUE_VIEW_HISTORY_COUNT],
                        const PlagueEpisodeHistory *history, int count,
                        int selected, int hovered) {
    int64_t maximum = axis_max_for(history, count, PLAGUE_CHART_SPORES);
    RECT legend = {chart.left + 4, chart.top, chart.right - 4,
                   min(plot.top, chart.top + 18)};
    int i;
    draw_text_rect(hdc, legend,
                   tr("Fill: used; outline: initial budget.",
                      "填充：已用；轮廓：初始预算。"),
                   ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS |
                       DT_NOPREFIX);
    draw_linear_guides(hdc, chart, plot, maximum);
    for (i = 0; i < count; i++) {
        int newest_offset = count - 1 - i;
        const PlagueEpisodeHistory *item = &history[newest_offset];
        int width = max(7, min(28, (hits[i].right - hits[i].left) * 3 / 5));
        int x = (hits[i].left + hits[i].right) / 2;
        int capacity_h = plague_panel_chart_value_height(
            item->spores_initial, maximum, plot.bottom - plot.top);
        int fill_h = plague_panel_chart_spore_fill_height(
            capacity_h, item->spores_used, item->spores_initial);
        RECT capacity = {x - width / 2, plot.bottom - capacity_h,
                         x + (width + 1) / 2, plot.bottom};
        RECT inner = capacity;
        RECT fill;
        COLORREF border = newest_offset == hovered ?
                          ui_theme_color(UI_COLOR_TEXT) :
                          newest_offset == selected ?
                          ui_theme_color(UI_COLOR_ACCENT) :
                          ui_theme_color(UI_COLOR_TEXT_DIM);
        if (capacity_h <= 0) {
            fill_rect(hdc, (RECT){capacity.left, plot.bottom - 1,
                                  capacity.right, plot.bottom}, border);
            continue;
        }
        fill_rect(hdc, capacity, border);
        InflateRect(&inner, -2, -2);
        if (inner.right > inner.left && inner.bottom > inner.top) {
            fill_rect(hdc, inner, ui_theme_color(UI_COLOR_PANEL));
            fill = inner;
            fill.top = max(inner.top, inner.bottom - max(0, fill_h - 4));
            if (fill.bottom > fill.top) fill_rect(hdc, fill, size_color(item->size));
        }
    }
}

static void draw_episode_labels(HDC hdc, RECT chart,
                                const RECT hits[PLAGUE_VIEW_HISTORY_COUNT],
                                const PlagueEpisodeHistory *history, int count) {
    int i;
    for (i = 0; i < count; i++) {
        int newest_offset = count - 1 - i;
        const PlagueEpisodeHistory *item = &history[newest_offset];
        char label[32];
        if (newest_offset == 0) {
            snprintf(label, sizeof(label), "%s", tr("Latest", "最近"));
        } else {
            snprintf(label, sizeof(label), ui_language == UI_LANG_ZH ? "%d年" : "Y%d",
                     max(0, item->start_month) / 12);
        }
        draw_text_rect(hdc, (RECT){hits[i].left, chart.bottom - 20,
                                   hits[i].right, chart.bottom},
                       label, newest_offset == 0 ?
                                                  ui_theme_color(UI_COLOR_ACCENT) :
                                                  ui_theme_color(UI_COLOR_TEXT_DIM),
                       DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS |
                           DT_NOPREFIX);
    }
}

void plague_panel_chart_draw(HDC hdc, RECT chart, RECT plot,
                             const RECT episode_hits[PLAGUE_VIEW_HISTORY_COUNT],
                             const PlagueEpisodeHistory *newest_first,
                             int count, PlaguePanelChartMetric metric,
                             int selected_newest_offset,
                             int hovered_newest_offset) {
    count = clamp(count, 0, PLAGUE_VIEW_HISTORY_COUNT);
    if (!newest_first || count <= 0) return;
    if (metric == PLAGUE_CHART_TYPE) {
        draw_type_chart(hdc, chart, plot, episode_hits, newest_first, count,
                        selected_newest_offset, hovered_newest_offset);
    } else if (metric == PLAGUE_CHART_SPORES) {
        draw_spores(hdc, chart, plot, episode_hits, newest_first, count,
                    selected_newest_offset, hovered_newest_offset);
    } else {
        draw_columns(hdc, chart, plot, episode_hits, newest_first, count,
                     metric, selected_newest_offset, hovered_newest_offset);
    }
    draw_episode_labels(hdc, chart, episode_hits, newest_first, count);
}
