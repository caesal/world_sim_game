#include "render/panel_plague_history.h"

#include "render/panel_plague_chart.h"
#include "render/panel_plague_common.h"
#include "render/render_common.h"
#include "sim/plague_rules.h"
#include "ui/ui_clay_theme.h"
#include "ui/ui_clay_widgets.h"
#include "ui/ui_pressed_state.h"
#include "ui/ui_types.h"

#include <stdio.h>

static const char *history_metric_label(int metric) {
    static const char *en[] = {
        "Type", "Severity", "Duration", "Deaths", "Cities", "Countries", "Spores"
    };
    static const char *zh[] = {
        "类型", "烈度", "时长", "死亡", "城市", "国家", "孢子"
    };
    metric = clamp(metric, 0, PLAGUE_HISTORY_METRIC_COUNT - 1);
    return ui_language == UI_LANG_ZH ? zh[metric] : en[metric];
}

static PlaguePanelChartMetric chart_metric(PlagueHistoryMetric metric) {
    switch (metric) {
        case PLAGUE_HISTORY_METRIC_SEVERITY: return PLAGUE_CHART_SEVERITY;
        case PLAGUE_HISTORY_METRIC_DURATION: return PLAGUE_CHART_DURATION;
        case PLAGUE_HISTORY_METRIC_DEATHS: return PLAGUE_CHART_DEATHS;
        case PLAGUE_HISTORY_METRIC_CITIES: return PLAGUE_CHART_CITIES;
        case PLAGUE_HISTORY_METRIC_COUNTRIES: return PLAGUE_CHART_COUNTRIES;
        case PLAGUE_HISTORY_METRIC_SPORES: return PLAGUE_CHART_SPORES;
        default: return PLAGUE_CHART_TYPE;
    }
}

static int newest_offset_for_episode(const PlagueStateView *state,
                                     int episode_id) {
    int i;
    if (!state || episode_id < 0) return -1;
    for (i = 0; i < state->recent_history_count; i++) {
        if (state->recent_history[i].episode_id == episode_id) return i;
    }
    return -1;
}

static void sync_history_slots(const PlagueStateView *state) {
    int ids[PLAGUE_VIEW_HISTORY_COUNT];
    int count = clamp(state->recent_history_count, 0, PLAGUE_VIEW_HISTORY_COUNT);
    int i;
    for (i = 0; i < count; i++) {
        ids[i] = state->recent_history[count - 1 - i].episode_id;
    }
    ui_plague_panel_set_history_episode_slots(ids, count);
}

static void draw_metric_tabs(HDC hdc, const UiPlagueHistoryLayout *layout) {
    PlagueHistoryMetric selected = ui_plague_panel_history_metric();
    int hover = ui_plague_panel_hover_target();
    int i;
    for (i = 0; i < PLAGUE_HISTORY_METRIC_COUNT; i++) {
        int hovered = hover == UI_PLAGUE_PANEL_HIT_HISTORY_METRIC_BASE + i;
        UiClayState state = ui_clay_state_from_flags(
            hovered,
            ui_pressed_control_is_active(
                UI_PRESSED_PLAGUE_HISTORY_METRIC, i),
            i == (int)selected, 0);
        ui_clay_draw_button(hdc, layout->metric_tabs[i],
                            history_metric_label(i), state);
    }
}

static COLORREF detail_accent(PlagueSize size) {
    UiClaySemanticTone tone = size == PLAGUE_SIZE_SMALL ? UI_CLAY_TONE_PEACE :
                              size == PLAGUE_SIZE_MEDIUM ? UI_CLAY_TONE_TENSE :
                                                          UI_CLAY_TONE_WAR;
    return ui_clay_semantic_style(tone).accent;
}

static void draw_detail(HDC hdc, RECT rect,
                        const PlagueEpisodeHistory *history,
                        const char *name_en, const char *name_zh) {
    RECT accent = rect;
    RECT line1 = rect;
    RECT line2 = rect;
    RECT line3 = rect;
    RECT line4 = rect;
    char duration[96];
    char deaths[48];
    char text[320];
    int immunity;
    int percent;
    fill_rect(hdc, rect, ui_theme_color(UI_COLOR_PANEL_SOFT));
    accent.right = accent.left + 4;
    fill_rect(hdc, accent, detail_accent(history->size));
    line1.left += 12;
    line1.right -= 8;
    line1.top += 6;
    line1.bottom = line1.top + 23;
    line2 = (RECT){line1.left, line1.bottom, line1.right, line1.bottom + 22};
    line3 = (RECT){line1.left, line2.bottom, line1.right, line2.bottom + 22};
    line4 = (RECT){line1.left, line3.bottom, line1.right, rect.bottom - 5};
    snprintf(text, sizeof(text), "%s / %s",
             name_zh && name_zh[0] ? name_zh : "—",
             name_en && name_en[0] ? name_en : "—");
    draw_text_rect(hdc, line1, text, ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    plague_panel_format_duration(history->duration_months, duration, sizeof(duration));
    snprintf(text, sizeof(text), "%s · %s %d/10 · %s",
             plague_panel_size_label(history->size), tr("Severity", "烈度"),
             history->severity, duration);
    draw_text_rect(hdc, line2, text, ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    plague_panel_format_count64(history->total_deaths, deaths, sizeof(deaths));
    snprintf(text, sizeof(text), "%s %s · %s %d · %s %d",
             tr("Deaths", "死亡"), deaths, tr("Cities", "城市"),
             history->infected_city_count, tr("Countries", "国家"),
             history->affected_country_count);
    draw_text_rect(hdc, line3, text, ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    percent = history->spores_initial > 0 ? (int)(
              (int64_t)clamp(history->spores_used, 0, history->spores_initial) *
              100 / history->spores_initial) : 0;
    immunity = plague_rules_immunity_percent_for_duration(history->duration_months);
    snprintf(text, sizeof(text), "%s %d/%d (%d%%) · %s %d%%",
             tr("Used / Initial budget", "已用 / 初始预算"),
             history->spores_used, history->spores_initial,
             percent, tr("Final immunity", "最终免疫"), immunity);
    draw_text_rect(hdc, line4, text, ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_LEFT | DT_VCENTER | DT_WORDBREAK | DT_NOPREFIX);
}

static void draw_empty(HDC hdc, const UiPlagueHistoryLayout *layout) {
    RECT message = layout->chart;
    fill_rect(hdc, message, ui_theme_color(UI_COLOR_PANEL_SOFT));
    draw_center_text(hdc, message,
                     tr("No completed plague episodes", "暂无已结束的瘟疫"),
                     ui_theme_color(UI_COLOR_TEXT_MUTED));
}

void plague_panel_history_draw(HDC hdc, const UiPlagueHistoryLayout *layout,
                               const RenderSnapshot *snapshot) {
    const PlagueStateView *state = &snapshot->plague_state;
    int count = clamp(state->recent_history_count, 0, PLAGUE_VIEW_HISTORY_COUNT);
    int selected;
    int hovered;
    int detail;
    sync_history_slots(state);
    draw_metric_tabs(hdc, layout);
    if (count <= 0) {
        draw_empty(hdc, layout);
        return;
    }
    selected = newest_offset_for_episode(
        state, ui_plague_panel_selected_history_episode_id());
    if (selected < 0) selected = 0;
    hovered = newest_offset_for_episode(
        state, ui_plague_panel_hovered_history_episode_id());
    plague_panel_chart_draw(hdc, layout->chart, layout->plot,
                            layout->episode_hits, state->recent_history, count,
                            chart_metric(ui_plague_panel_history_metric()),
                            selected, hovered);
    detail = hovered >= 0 ? hovered : selected;
    draw_detail(hdc, layout->detail_strip, &state->recent_history[detail],
                snapshot->plague_names.history_en[detail],
                snapshot->plague_names.history_zh[detail]);
}
