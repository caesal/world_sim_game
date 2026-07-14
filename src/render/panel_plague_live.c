#include "render/panel_plague_live.h"

#include "render/panel_plague_common.h"
#include "render/render_common.h"
#include "sim/plague_rules.h"
#include "ui/ui_clay_theme.h"
#include "ui/ui_clay_widgets.h"
#include "ui/ui_types.h"

#include <stdio.h>

static COLORREF plague_size_accent(PlagueSize size) {
    UiClaySemanticTone tone = UI_CLAY_TONE_MUTED;
    if (size == PLAGUE_SIZE_SMALL) tone = UI_CLAY_TONE_PEACE;
    else if (size == PLAGUE_SIZE_MEDIUM) tone = UI_CLAY_TONE_TENSE;
    else if (size == PLAGUE_SIZE_LARGE) tone = UI_CLAY_TONE_WAR;
    return ui_clay_semantic_style(tone).accent;
}

PlaguePanelLiveMode plague_panel_live_mode(const RenderSnapshot *snapshot) {
    if (!snapshot || !snapshot->plague_state.episode.active) {
        return snapshot && snapshot->plague_state.recent_history_count > 0 ?
               PLAGUE_PANEL_LIVE_COMPLETED : PLAGUE_PANEL_LIVE_EMPTY;
    }
    return PLAGUE_PANEL_LIVE_ACTIVE;
}

static void draw_status(HDC hdc, UiCursor *cursor, const char *text,
                        PlagueSize size) {
    RECT row = ui_take_rect(cursor, 30);
    UiClaySemanticStyle style = ui_clay_semantic_style(
        size == PLAGUE_SIZE_SMALL ? UI_CLAY_TONE_PEACE :
        size == PLAGUE_SIZE_MEDIUM ? UI_CLAY_TONE_TENSE :
        size == PLAGUE_SIZE_LARGE ? UI_CLAY_TONE_WAR : UI_CLAY_TONE_MUTED);
    RECT marker = row;
    marker.right = marker.left + 4;
    fill_rect(hdc, marker, style.accent);
    row.left += 11;
    draw_text_rect(hdc, row, text, ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    cursor->y += 2;
}

static void draw_metric_pair(HDC hdc, UiCursor *cursor,
                             IconId left_icon, const char *left_label,
                             const char *left_value, IconId right_icon,
                             const char *right_label, const char *right_value,
                             COLORREF accent) {
    RECT row = ui_take_rect(cursor, 52);
    RECT left = row;
    RECT right = row;
    int gap = 7;
    int half = (cursor->width - gap) / 2;
    left.right = left.left + half;
    right.left = left.right + gap;
    ui_clay_draw_metric_chip_text(hdc, left, left_icon, left_label,
                                  left_value, accent);
    ui_clay_draw_metric_chip_text(hdc, right, right_icon, right_label,
                                  right_value, accent);
    cursor->y += 6;
}

static void draw_episode_rules(HDC hdc, UiCursor *cursor,
                               const PlagueEpisodeState *episode) {
    PlagueSizeRules rules;
    char left[80];
    char right[80];
    if (!plague_rules_size_values(episode->size, &rules)) return;
    ui_section(hdc, cursor, tr("Episode Rules", "本次规则"));
    snprintf(left, sizeof(left), "%d", rules.maximum_generation);
    snprintf(right, sizeof(right), ui_language == UI_LANG_ZH ? "%d月" : "%dmo",
             rules.continuous_city_cap_months);
    plague_panel_pair(hdc, cursor, tr("Maximum generation", "最大传播代际"), left,
                      tr("Continuous city cap", "城市连续感染上限"), right);
    plague_panel_row(hdc, cursor, tr("Severity", "烈度"),
                     tr("Fixed for the whole episode", "全程固定"));
    plague_panel_row(hdc, cursor, tr("Spores", "孢子"),
                     tr("Finite pool; no regeneration", "有限总量；不会再生"));
}

static void draw_active(HDC hdc, UiCursor *cursor,
                        const RenderSnapshot *snapshot) {
    const PlagueStateView *state = &snapshot->plague_state;
    const PlagueEpisodeState *episode = &state->episode;
    COLORREF accent = plague_size_accent(episode->size);
    int absolute_month = snapshot->year * 12 + snapshot->month - 1;
    int age = max(0, absolute_month - episode->start_month);
    int spores_remaining = clamp(episode->spores_remaining, 0,
                                 episode->spores_initial);
    int spore_percent = episode->spores_initial > 0 ? (int)(
        (int64_t)spores_remaining * 100 /
        episode->spores_initial) : 0;
    int next_threshold = state->months_to_next_immunity > 0 ?
                         age + state->months_to_next_immunity : max(1, age);
    char a[160], b[160], c[160], d[336];

    draw_status(hdc, cursor, tr("Active now", "正在发生"), episode->size);
    plague_panel_row(hdc, cursor, tr("Chinese name", "中文名"),
                     snapshot->plague_names.active_zh);
    plague_panel_row(hdc, cursor, tr("English name", "英文名"),
                     snapshot->plague_names.active_en);
    snprintf(a, sizeof(a), "%s", plague_panel_size_label(episode->size));
    snprintf(b, sizeof(b), "%d/10", episode->severity);
    draw_metric_pair(hdc, cursor, ICON_ADAPTATION, tr("Type", "类型"), a,
                     ICON_DISORDER, tr("Severity", "烈度"), b, accent);
    plague_panel_format_annual_mortality(episode->severity, a, sizeof(a));
    plague_panel_format_duration(age, b, sizeof(b));
    draw_metric_pair(hdc, cursor, ICON_POPULATION,
                     tr("Annual mortality", "年死亡率"), a,
                     ICON_INNOVATION, tr("Age", "持续时间"), b, accent);

    ui_section(hdc, cursor, tr("Origin and Timing", "起源与时间"));
    plague_panel_format_absolute_month(episode->start_month, a, sizeof(a));
    plague_panel_row(hdc, cursor, tr("Started", "开始"), a);
    plague_panel_row(hdc, cursor, tr("Origin city", "起源城市"),
                     episode->origin_city_name);
    plague_panel_row(hdc, cursor, tr("Origin country", "起源国家"),
                     ui_language == UI_LANG_ZH ? episode->origin_civ_name_zh :
                                                 episode->origin_civ_name_en);
    snprintf(a, sizeof(a), "%d", episode->frozen_occupied_cities);
    plague_panel_row(hdc, cursor, tr("Frozen occupied cities", "冻结占用城市数"), a);

    ui_section(hdc, cursor, tr("Spread Capacity", "传播容量"));
    snprintf(a, sizeof(a), "%d / %d (%d%%)",
             episode->spores_remaining, episode->spores_initial,
             spore_percent);
    plague_panel_progress(hdc, cursor,
                          tr("Remaining / Initial", "剩余 / 初始"),
                           a, spores_remaining, episode->spores_initial, accent);
    snprintf(a, sizeof(a), "%d / %d", episode->active_city_count,
             episode->ever_infected_city_count);
    snprintf(b, sizeof(b), "%d / %d", episode->current_affected_country_count,
             episode->ever_affected_country_count);
    plague_panel_pair(hdc, cursor, tr("Cities active / ever", "城市当前/累计"), a,
                      tr("Countries current / ever", "国家当前/累计"), b);
    snprintf(a, sizeof(a), "%d / %d", episode->current_generation,
             episode->maximum_generation_reached);
    snprintf(b, sizeof(b), "%d / %d", state->maximum_generation_index,
             state->reachable_generation_layers);
    plague_panel_pair(hdc, cursor,
                      tr("Generation current / highest", "代际当前/最高"), a,
                      tr("Max index / total layers", "最大索引/总层数"), b);
    plague_panel_row(hdc, cursor, tr("Generation origin", "代际起点"),
                     tr("Origin city is generation 0", "起源城市为第0代"));

    ui_section(hdc, cursor, tr("Deaths and Disorder", "死亡与混乱"));
    plague_panel_format_count64(episode->current_month_deaths, a, sizeof(a));
    plague_panel_format_count64(state->rolling_12_month_deaths, b, sizeof(b));
    plague_panel_format_count64(episode->total_deaths, c, sizeof(c));
    plague_panel_pair(hdc, cursor, tr("Current month", "本月死亡"), a,
                      tr("Rolling 12 months", "滚动12个月"), b);
    plague_panel_row(hdc, cursor, tr("Episode total deaths", "本次总死亡"), c);
    snprintf(a, sizeof(a), "%d / %d", state->disorder_current,
             state->disorder_target);
    plague_panel_progress(hdc, cursor, tr("Disorder current / target", "混乱当前/目标"),
                          a, state->disorder_current, 100,
                          ui_clay_semantic_style(UI_CLAY_TONE_TENSE).accent);

    snprintf(a, sizeof(a), "%d%%", state->projected_immunity_percent);
    if (state->next_immunity_percent > 0 && state->months_to_next_immunity > 0) {
        snprintf(b, sizeof(b), ui_language == UI_LANG_ZH ?
                 "%d月→%d%%" : "%d%% in %dmo",
                 ui_language == UI_LANG_ZH ? state->months_to_next_immunity :
                                             state->next_immunity_percent,
                 ui_language == UI_LANG_ZH ? state->next_immunity_percent :
                                             state->months_to_next_immunity);
    } else {
        snprintf(b, sizeof(b), "%s", tr("Final tier", "最终档位"));
    }
    snprintf(d, sizeof(d), "%s · %s", a, b);
    plague_panel_progress(hdc, cursor, tr("Projected immunity", "预计免疫"), d,
                          age, next_threshold,
                          ui_clay_semantic_style(UI_CLAY_TONE_PEACE).accent);
    draw_episode_rules(hdc, cursor, episode);
    plague_panel_draw_immunity(hdc, cursor, state);
    plague_panel_draw_schedule(hdc, cursor, state);
}

static void draw_completed(HDC hdc, UiCursor *cursor,
                           const RenderSnapshot *snapshot) {
    const PlagueStateView *state = &snapshot->plague_state;
    const PlagueEpisodeHistory *history = &state->recent_history[0];
    COLORREF accent = plague_size_accent(history->size);
    int immunity = plague_rules_immunity_percent_for_duration(history->duration_months);
    int spore_percent = history->spores_initial > 0 ? (int)(
        (int64_t)clamp(history->spores_used, 0, history->spores_initial) * 100 /
        history->spores_initial) : 0;
    char a[160], b[160];

    draw_status(hdc, cursor, tr("Last completed", "上次已结束"), history->size);
    plague_panel_row(hdc, cursor, tr("Chinese name", "中文名"),
                     snapshot->plague_names.history_zh[0]);
    plague_panel_row(hdc, cursor, tr("English name", "英文名"),
                     snapshot->plague_names.history_en[0]);
    snprintf(a, sizeof(a), "%s", plague_panel_size_label(history->size));
    snprintf(b, sizeof(b), "%d/10", history->severity);
    draw_metric_pair(hdc, cursor, ICON_ADAPTATION, tr("Type", "类型"), a,
                     ICON_DISORDER, tr("Severity", "烈度"), b, accent);
    plague_panel_format_annual_mortality(history->severity, a, sizeof(a));
    plague_panel_format_duration(history->duration_months, b, sizeof(b));
    draw_metric_pair(hdc, cursor, ICON_POPULATION,
                     tr("Annual mortality", "年死亡率"), a,
                     ICON_INNOVATION, tr("Duration", "完整时长"), b, accent);
    ui_section(hdc, cursor, tr("Completed Summary", "结束摘要"));
    plague_panel_format_absolute_month(history->start_month, a, sizeof(a));
    plague_panel_format_absolute_month(history->end_month, b, sizeof(b));
    plague_panel_pair(hdc, cursor, tr("Started", "开始"), a,
                      tr("Ended", "结束"), b);
    plague_panel_row(hdc, cursor, tr("Origin city", "起源城市"),
                     history->origin_city_name);
    plague_panel_row(hdc, cursor, tr("Origin country", "起源国家"),
                     ui_language == UI_LANG_ZH ? history->origin_civ_name_zh :
                                                 history->origin_civ_name_en);
    plague_panel_format_count64(history->total_deaths, a, sizeof(a));
    plague_panel_row(hdc, cursor, tr("Total deaths", "总死亡"), a);
    snprintf(a, sizeof(a), "%d", history->infected_city_count);
    snprintf(b, sizeof(b), "%d", history->affected_country_count);
    plague_panel_pair(hdc, cursor, tr("Infected cities", "感染城市"), a,
                      tr("Affected countries", "受影响国家"), b);
    snprintf(a, sizeof(a), "%d / %d (%d%%)", history->spores_used,
             history->spores_initial, spore_percent);
    snprintf(b, sizeof(b), "%d%%", immunity);
    plague_panel_row(hdc, cursor,
                     tr("Used / Initial budget", "已用 / 初始预算"), a);
    plague_panel_row(hdc, cursor, tr("Final immunity", "最终免疫"), b);
    plague_panel_draw_immunity(hdc, cursor, state);
    plague_panel_draw_schedule(hdc, cursor, state);
}

static void draw_empty(HDC hdc, UiCursor *cursor,
                       const RenderSnapshot *snapshot) {
    draw_status(hdc, cursor, tr("No plague records", "暂无瘟疫记录"),
                PLAGUE_SIZE_NONE);
    plague_panel_row(hdc, cursor, tr("History", "历史"),
                     tr("No completed episodes.", "尚无已结束瘟疫。"));
    plague_panel_draw_immunity(hdc, cursor, &snapshot->plague_state);
    plague_panel_draw_schedule(hdc, cursor, &snapshot->plague_state);
}

void plague_panel_live_draw(HDC hdc, UiCursor *cursor,
                            const RenderSnapshot *snapshot) {
    PlaguePanelLiveMode mode = plague_panel_live_mode(snapshot);
    if (mode == PLAGUE_PANEL_LIVE_ACTIVE) draw_active(hdc, cursor, snapshot);
    else if (mode == PLAGUE_PANEL_LIVE_COMPLETED) {
        draw_completed(hdc, cursor, snapshot);
    } else {
        draw_empty(hdc, cursor, snapshot);
    }
}
