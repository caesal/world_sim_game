#include "render/panel_country_detail.h"

#include "render/panel_country_decision.h"
#include "render/panel_country_diplomacy.h"
#include "render/panel_country_disorder.h"
#include "render/panel_country_actions.h"
#include "render/panel_country_events.h"
#include "render/panel_country_population.h"
#include "render/panel_country_resources.h"
#include "render/panel_country_tech.h"
#include "render/ui_format.h"
#include "render_panel_internal.h"
#include "sim/collapse.h"
#include "sim/civilization_slots.h"
#include "sim/decision_snapshot.h"
#include "sim/diplomacy.h"
#include "sim/disorder.h"
#include "sim/expansion.h"
#include "sim/plague.h"
#include "sim/population.h"
#include "sim/regions.h"
#include "sim/technology.h"
#include "sim/vassal.h"
#include "sim/war.h"
#include "ui/ui_widgets.h"

#include <stdio.h>
#include <string.h>

static RECT last_civil_unrest_button;
static int last_civil_unrest_enabled;

static const char *collapse_block_reason_ui(int civ_id) {
    static char buffers[4][EVENT_LOG_LEN];
    static int index;
    char *buffer = buffers[index++ % 4];
    CollapseBlockReason reason = collapse_block_reason(civ_id);

    switch (reason) {
        case COLLAPSE_BLOCK_NONE:
            snprintf(buffer, EVENT_LOG_LEN, "%s", tr("Ready.", "就绪。"));
            break;
        case COLLAPSE_BLOCK_NOT_ALIVE:
            snprintf(buffer, EVENT_LOG_LEN, "%s", tr("Country is invalid or has fallen.", "国家无效或已经灭亡。"));
            break;
        case COLLAPSE_BLOCK_MAX_CIVS:
            snprintf(buffer, EVENT_LOG_LEN, "%s %d/%d, %s %d, %s %d.",
                     tr("No reusable or free country slots. Used", "没有可复用或空闲国家槽。已用"),
                     civ_count, MAX_CIVS, tr("alive", "存活"), civilization_alive_count(),
                     tr("reusable", "可复用"), civilization_reusable_slot_count());
            break;
        case COLLAPSE_BLOCK_NO_CAPITAL_REGION:
            snprintf(buffer, EVENT_LOG_LEN, "%s", tr("No valid capital region.", "没有有效首都区域。"));
            break;
        case COLLAPSE_BLOCK_NO_SPLITTABLE_REGION:
            snprintf(buffer, EVENT_LOG_LEN, "%s", tr("No splittable non-capital region.", "没有可拆分的非首都区域。"));
            break;
        case COLLAPSE_BLOCK_ONLY_CORE_LEFT:
            snprintf(buffer, EVENT_LOG_LEN, "%s", tr("Only capital/core region remains.", "只剩首都/核心区域。"));
            break;
        case COLLAPSE_BLOCK_CITY_CAP:
            snprintf(buffer, EVENT_LOG_LEN, "%s", tr("City limit reached.", "城市数量已达上限。"));
            break;
        default:
            snprintf(buffer, EVENT_LOG_LEN, "%s", tr("Unknown collapse blocker.", "未知崩溃阻碍。"));
            break;
    }
    return buffer;
}

static int province_count_for_civ(int civ_id) {
    int i;
    int count = 0;
    for (i = 0; i < region_count; i++) {
        if (natural_regions[i].alive && natural_regions[i].owner_civ == civ_id) count++;
    }
    return count > 0 ? count : summarize_country(civ_id).cities;
}

int country_detail_content_height(int civ_id) {
    switch (clamp(country_detail_subtab, 0, COUNTRY_DETAIL_TAB_COUNT - 1)) {
        case COUNTRY_DETAIL_TECHNOLOGY: return country_tech_tab_height(civ_id);
        case COUNTRY_DETAIL_DECISION: return country_decision_tab_height(civ_id);
        case COUNTRY_DETAIL_POPULATION: return country_population_tab_height(civ_id);
        case COUNTRY_DETAIL_RESOURCES: return country_resources_tab_height(civ_id);
        case COUNTRY_DETAIL_DIPLOMACY: return country_diplomacy_tab_height(civ_id);
        case COUNTRY_DETAIL_DISORDER: return country_disorder_tab_height(civ_id);
        default:
            return 620 + country_overview_vassal_actions_height(civ_id) +
                   (plague_civ_active_count(civ_id) > 0 ? 67 : 0);
    }
}

void country_detail_reset_hit(void) {
    last_civil_unrest_enabled = 0;
    SetRectEmpty(&last_civil_unrest_button);
    country_overview_vassal_actions_reset_hit();
    country_recent_events_reset_hit();
}

int country_detail_civil_unrest_hit(RECT viewport, int mouse_x, int mouse_y) {
    return last_civil_unrest_enabled &&
           point_in_rect_local(viewport, mouse_x, mouse_y) &&
           point_in_rect_local(last_civil_unrest_button, mouse_x, mouse_y);
}

int country_detail_vassal_action_hit(RECT viewport, int mouse_x, int mouse_y) {
    return country_overview_vassal_action_hit(viewport, mouse_x, mouse_y);
}

static void draw_metric_row(HDC hdc, UiCursor *cursor, int a, int b, int c,
                            IconId ia, IconId ib, IconId ic,
                            const char *la, const char *lb, const char *lc) {
    int w = (cursor->width - 16) / 3;
    RECT r = {cursor->x, cursor->y, cursor->x + w, cursor->y + 28};
    ui_metric_chip(hdc, r, ia, la, a, RGB(118, 143, 95));
    r.left += w + 8; r.right += w + 8;
    ui_metric_chip(hdc, r, ib, lb, b, RGB(83, 123, 166));
    r.left += w + 8; r.right += w + 8;
    ui_metric_chip(hdc, r, ic, lc, c, RGB(188, 154, 88));
    cursor->y += 36;
}

static void draw_metric_pair(HDC hdc, UiCursor *cursor, int a, int b,
                             IconId ia, IconId ib, const char *la, const char *lb) {
    int w = (cursor->width - 8) / 2;
    RECT r = {cursor->x, cursor->y, cursor->x + w, cursor->y + 28};
    ui_metric_chip(hdc, r, ia, la, a, RGB(118, 143, 95));
    r.left += w + 8; r.right += w + 8;
    ui_metric_chip(hdc, r, ib, lb, b, RGB(83, 123, 166));
    cursor->y += 36;
}

#if 0
static void format_percent_components(char *buffer, size_t buffer_size,
                                      int total, int tech, int disorder) {
    snprintf(buffer, buffer_size, "%d%% (%s %d%%, %s %d%%)", total,
             tr("Tech", "科技"), tech, tr("Disorder", "混乱"), disorder);
}

static void format_disorder_percent(char *buffer, size_t buffer_size, int total) {
    snprintf(buffer, buffer_size, "%d%% (%s %d%%)", total, tr("Disorder", "混乱"), total);
}

#endif

static void format_percent_components_clean(char *buffer, size_t buffer_size,
                                            int total, int tech, int disorder) {
    snprintf(buffer, buffer_size, "%d%% (%s %d%%, %s %d%%)", total,
             tr("Tech", "科技"), tech, tr("Disorder", "混乱"), disorder);
}

static void format_disorder_percent_clean(char *buffer, size_t buffer_size, int total) {
    snprintf(buffer, buffer_size, "%d%% (%s %d%%)", total, tr("Disorder", "混乱"), total);
}

static void format_month_span(char *buffer, size_t buffer_size, int months) {
    if (months < 0) snprintf(buffer, buffer_size, "%s", tr("condition based", "条件决定"));
    else ui_format_months(buffer, buffer_size, months, UI_MONTH_ZERO_NOW);
}

static void draw_pressure_source(HDC hdc, UiCursor *cursor, const char *label,
                                 int value, int contribution, int decay,
                                 int eta_months, const char *status) {
    char text[192];
    char eta[48];

    snprintf(text, sizeof(text), "%d / 100", value);
    ui_row_text(hdc, cursor, label, text);
    format_month_span(eta, sizeof(eta), eta_months);
    snprintf(text, sizeof(text), "%s +%d/mo   %s -%d/mo   %s %s   %s",
             tr("Contrib", "贡献"), contribution, tr("Decay", "衰退"), decay,
             tr("Clear", "清零"), eta, status);
    ui_row_text(hdc, cursor, "", text);
}

static void draw_city_list(HDC hdc, UiCursor *cursor, int civ_id) {
    int i;
    char text[192];

    ui_section(hdc, cursor, tr("Provinces / Cities", "行省 / 城市"));
    for (i = 0; i < city_count; i++) {
        if (!cities[i].alive || cities[i].owner != civ_id) continue;
        snprintf(text, sizeof(text), "%s%s%s  %s %d",
                 cities[i].capital ? "* " : "", cities[i].name,
                 cities[i].port ? tr(" Port", " 港口") : "",
                 tr("Pop", "人口"), cities[i].population);
        draw_icon_text_line(hdc, cursor->x, cursor->y,
                            cities[i].capital ? ICON_CITY_CAPITAL : ICON_TERRITORY,
                            text, ui_theme_color(UI_COLOR_TEXT_MUTED));
        cursor->y += 22;
    }
}

static void draw_civil_unrest_action(HDC hdc, UiCursor *cursor, int civ_id);

static const char *overview_next_action_ui(const DecisionSnapshot *decision) {
    static char buffers[4][96];
    static int index;
    char *buffer = buffers[index++ % 4];
    const char *reason = decision->expansion_reason && decision->expansion_reason[0] ?
                         decision->expansion_reason : decision->main_intent;
    if (!reason) return "";
    if (ui_language != 1) return reason;
    if (strstr(reason, "Claimed adjacent region")) return "已占领相邻区域，进入冷却";
    if (strstr(reason, "Claimed shallow")) return "已占领浅海可达区域，进入冷却";
    if (strstr(reason, "Claimed maritime") || strstr(reason, "Claimed overseas")) return "已通过航道占领新区域";
    if (strstr(reason, "cooldown") || strstr(reason, "Cooldown")) {
        if (decision->next_expansion_months > 0) {
            {
                char span[48];
                ui_format_months(span, sizeof(span), decision->next_expansion_months, UI_MONTH_ZERO_NOW);
                snprintf(buffer, sizeof(buffers[0]), "%s %s", tr("Expansion cooldown", "扩张冷却"), span);
            }
            return buffer;
        }
        return "扩张冷却中";
    }
    if (strstr(reason, "no target") || strstr(reason, "No target") ||
        strstr(reason, "No reachable") || strstr(reason, "no reachable")) return "暂无可执行目标";
    if (strstr(reason, "budget") || strstr(reason, "Budget")) return "等待扩张预算";
    if (strstr(reason, "city cap") || strstr(reason, "City cap")) return "城市数量达到上限";
    if (strstr(reason, "random") || strstr(reason, "chance") || strstr(reason, "Probability")) return "本次扩张机会未触发";
    if (strcmp(reason, "Expansion") == 0) return "倾向扩张";
    if (strcmp(reason, "War") == 0) return "战争倾向上升";
    if (strcmp(reason, "Stability") == 0) return "优先维持稳定";
    return reason;
}

static const char *overview_dominant_intent(const DecisionSnapshot *decision) {
    if (decision->expansion_weight >= decision->war_weight &&
        decision->expansion_weight >= decision->stability_weight) return tr("Expansion", "扩张");
    if (decision->war_weight >= decision->expansion_weight &&
        decision->war_weight >= decision->stability_weight) return tr("War", "战争");
    return tr("Stability", "稳定");
}

static void draw_decision_meter_row(HDC hdc, UiCursor *cursor, const char *label,
                                    int value, COLORREF color) {
    RECT row = ui_take_rect(cursor, 24);
    RECT label_rect = {row.left, row.top, row.left + 54, row.bottom};
    RECT value_rect = {row.left + 56, row.top, row.left + 94, row.bottom};
    RECT bar_rect = {row.left + 102, row.top + 7, row.right, row.bottom - 7};
    char text[24];

    draw_text_rect(hdc, label_rect, label, ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    snprintf(text, sizeof(text), "%d", value);
    draw_text_rect(hdc, value_rect, text, ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_RIGHT | DT_VCENTER | DT_END_ELLIPSIS);
    ui_progress_bar(hdc, bar_rect, value, 100, color);
}

static void draw_overview_mini_blocks(HDC hdc, UiCursor *cursor, int civ_id) {
    DecisionSnapshot decision;
    int progress = technology_stage_progress_percent(civ_id);
    int months_next = technology_months_to_next(civ_id);
    char text[192];
    char span[48];

    decision_snapshot_for_civ(civ_id, &decision);
    ui_section(hdc, cursor, tr("Technology Snapshot", "科技摘要"));
    snprintf(text, sizeof(text), "%s %d: %s", tr("Stage", "阶段"),
             clamp(civs[civ_id].tech_stage, 0, 10),
             technology_stage_name(clamp(civs[civ_id].tech_stage, 0, 10), ui_language));
    ui_row_text(hdc, cursor, tr("Current", "当前"), text);
    ui_progress_bar(hdc, ui_take_rect(cursor, 12), progress, 100, RGB(104, 158, 186));
    ui_format_months(span, sizeof(span), months_next >= 0 ? months_next : 0, UI_MONTH_ZERO_DONE);
    snprintf(text, sizeof(text), "%d%%   %s %s", progress, tr("Next in", "距下阶段"), span);
    ui_row_text(hdc, cursor, tr("Progress", "进度"), text);
    ui_section(hdc, cursor, tr("Decision Tendency", "决策倾向"));
    draw_decision_meter_row(hdc, cursor, tr("Expansion", "扩张"), decision.expansion_weight, RGB(82, 156, 112));
    draw_decision_meter_row(hdc, cursor, tr("War", "战争"), decision.war_weight, RGB(180, 84, 74));
    draw_decision_meter_row(hdc, cursor, tr("Stability", "稳定"), decision.stability_weight, RGB(92, 130, 162));
    ui_row_text(hdc, cursor, tr("Dominant Intent", "主导方向"), overview_dominant_intent(&decision));
    ui_row_text(hdc, cursor, tr("Next Action", "下一步"), overview_next_action_ui(&decision));
    ui_section(hdc, cursor, tr("Actions", "操作"));
    draw_civil_unrest_action(hdc, cursor, civ_id);
    draw_country_overview_vassal_actions(hdc, cursor, civ_id);
    draw_country_recent_events(hdc, cursor, civ_id);
}

static void draw_civil_unrest_action(HDC hdc, UiCursor *cursor, int civ_id) {
    int can_trigger = collapse_can_trigger(civ_id);
    const char *reason = collapse_block_reason_ui(civ_id);
    RECT button = {cursor->x, cursor->y + 4, cursor->x + cursor->width, cursor->y + 34};

    last_civil_unrest_button = button;
    last_civil_unrest_enabled = can_trigger;
    fill_rect(hdc, button, can_trigger ? RGB(112, 45, 45) : RGB(65, 55, 55));
    draw_center_text(hdc, button, tr("Civil Unrest", "内乱"),
                     can_trigger ? RGB(255, 236, 226) : ui_theme_color(UI_COLOR_TEXT_DIM));
    cursor->y += 40;
    if (!can_trigger) ui_row_text(hdc, cursor, tr("Cannot collapse", "无法崩溃"), reason);
}

#if 0
static void draw_disorder_block(HDC hdc, UiCursor *cursor, int civ_id) {
    char risk[80];
    char text[96];
    int can_trigger = collapse_can_trigger(civ_id);
    const char *reason = collapse_block_reason_ui(civ_id);
    int chance = collapse_decade_chance_for_disorder(civs[civ_id].disorder);
    int tech_resource = technology_resource_percent(civ_id);
    int disorder_resource = disorder_productivity_percent(civs[civ_id].disorder);
    int tech_progress = technology_progress_percent(civ_id);
    int disorder_progress = disorder_technology_percent(civs[civ_id].disorder);

    ui_section(hdc, cursor, tr("Disorder", "混乱"));
    draw_metric_row(hdc, cursor, civs[civ_id].disorder_resource, civs[civ_id].disorder_plague,
                    civs[civ_id].disorder_migration, ICON_DISORDER, ICON_DISORDER, ICON_MIGRATION,
                    metric_label("Resource", "资源"), metric_label("Plague", "瘟疫"),
                    metric_label("Migration", "迁徙"));
    draw_metric_pair(hdc, cursor, civs[civ_id].disorder_stability, civs[civ_id].disorder,
                     ICON_COUNTRY_DEFENSE, ICON_DISORDER, metric_label("War/Unrest", "战争/内乱"),
                     metric_label("Total", "总计"));
    snprintf(text, sizeof(text), "%d", vassal_governance_disorder(civ_id));
    ui_row_text(hdc, cursor, tr("Vassal governance burden", "附庸治理负担"), text);
    snprintf(text, sizeof(text), "-%d / month", civs[civ_id].governance / 2 + civs[civ_id].cohesion / 2);
    ui_row_text(hdc, cursor, tr("Monthly recovery", "本月恢复"), text);
    format_percent_components_clean(text, sizeof(text), tech_resource * disorder_resource / 100,
                                    tech_resource, disorder_resource);
    ui_row_text(hdc, cursor, tr("Resource Output", "资源产出"), text);
    format_disorder_percent_clean(text, sizeof(text), disorder_population_growth_percent(civs[civ_id].disorder));
    ui_row_text(hdc, cursor, tr("Population Growth", "人口增长"), text);
    format_percent_components_clean(text, sizeof(text), tech_progress * disorder_progress / 100,
                                    tech_progress, disorder_progress);
    ui_row_text(hdc, cursor, tr("Tech Progress", "科技进度"), text);
    ui_row_text(hdc, cursor, tr("Modifier Basis", "加成说明"),
                tr("Total = tech x disorder.", "总值=科技×混乱。"));
    if (civs[civ_id].disorder >= 100) {
        snprintf(risk, sizeof(risk), "%s", tr("Immediate collapse attempt", "立即尝试崩溃"));
    } else if (chance <= 0) {
        snprintf(risk, sizeof(risk), "%s", tr("No collapse risk", "无崩溃风险"));
    } else {
        snprintf(risk, sizeof(risk), "%d%%  %s", chance,
                 tr("per 25 years", "每 25 年"));
    }
    ui_row_text(hdc, cursor, tr("Collapse Risk", "崩溃风险"), risk);
    ui_row_text(hdc, cursor, tr("Collapse Check", "崩溃判定"), collapse_last_reason(civ_id));
    if (!can_trigger) ui_row_text(hdc, cursor, tr("Cannot collapse", "无法崩溃"), reason);
    if (civs[civ_id].disorder >= 100) {
        snprintf(text, sizeof(text), "%s", tr("Immediate path", "立即路径"));
    } else if (chance > 0) {
        int years_left = 25 - (year % 25);
        if (years_left <= 0) years_left = 25;
        ui_format_months(text, sizeof(text), years_left * 12, UI_MONTH_ZERO_NOW);
    } else {
        snprintf(text, sizeof(text), "%s", tr("No scheduled roll", "无定期判定"));
    }
    ui_row_text(hdc, cursor, tr("Next Check", "下次判定"), text);
}

#endif

static void __attribute__((unused)) draw_disorder_block_v2(HDC hdc, UiCursor *cursor, int civ_id) {
    char risk[96];
    char text[128];
    int can_trigger = collapse_can_trigger(civ_id);
    const char *reason = collapse_block_reason_ui(civ_id);
    int chance = collapse_decade_chance_for_disorder(civs[civ_id].disorder);
    int tech_resource = technology_resource_percent(civ_id);
    int disorder_resource = disorder_productivity_percent(civs[civ_id].disorder);
    int tech_progress = technology_progress_percent(civ_id);
    int disorder_progress = disorder_technology_percent(civs[civ_id].disorder);
    int grace = collapse_grace_months_left(civ_id);
    int plague_decay = disorder_last_plague_decay(civ_id);
    int war_decay = disorder_last_war_decay(civ_id);
    int migration_decay = disorder_last_migration_decay(civ_id);

    ui_section(hdc, cursor, tr("Disorder", "混乱"));
    draw_metric_pair(hdc, cursor, civs[civ_id].disorder, disorder_last_net(civ_id),
                     ICON_DISORDER, ICON_DISORDER, metric_label("Total", "总计"),
                     metric_label("Net", "净变化"));
    snprintf(text, sizeof(text), "+%d / -%d / %+d",
             disorder_last_pressure(civ_id), disorder_last_recovery(civ_id), disorder_last_net(civ_id));
    ui_row_text(hdc, cursor, tr("This month", "本月变化"), text);
    draw_pressure_source(hdc, cursor, tr("Resource pressure", "资源压力"),
                         civs[civ_id].disorder_resource, civs[civ_id].disorder_resource / 18,
                         0, -1, tr("current conditions", "当前条件决定"));
    draw_pressure_source(hdc, cursor, tr("Plague pressure", "瘟疫压力"),
                         civs[civ_id].disorder_plague, civs[civ_id].disorder_plague / 24,
                         plague_decay,
                         disorder_pressure_eta_months(civs[civ_id].disorder_plague, plague_decay),
                         plague_active_for_civ(civ_id) ? tr("plague active", "瘟疫活跃") :
                         tr("post-plague recovery", "瘟疫后恢复"));
    draw_pressure_source(hdc, cursor, tr("War / unrest pressure", "战争/内乱压力"),
                         civs[civ_id].disorder_stability, civs[civ_id].disorder_stability / 28,
                         war_decay,
                         disorder_pressure_eta_months(civs[civ_id].disorder_stability, war_decay),
                         war_active_for_civ(civ_id) ? tr("at war", "战争中") :
                         tr("post-war recovery", "战后恢复"));
    draw_pressure_source(hdc, cursor, tr("Migration pressure", "迁徙/扩张压力"),
                         civs[civ_id].disorder_migration, civs[civ_id].disorder_migration / 26,
                         migration_decay,
                         disorder_pressure_eta_months(civs[civ_id].disorder_migration, migration_decay),
                         tr("temporary friction", "临时摩擦"));
    draw_pressure_source(hdc, cursor, tr("Vassal governance burden", "附庸治理负担"),
                         vassal_governance_disorder(civ_id), 0, 0, -1,
                         tr("hard floor", "硬下限"));
    format_percent_components_clean(text, sizeof(text), tech_resource * disorder_resource / 100,
                              tech_resource, disorder_resource);
    ui_row_text(hdc, cursor, tr("Resource Output", "资源产出"), text);
    format_disorder_percent_clean(text, sizeof(text), disorder_population_growth_percent(civs[civ_id].disorder));
    ui_row_text(hdc, cursor, tr("Population Growth", "人口增长"), text);
    format_percent_components_clean(text, sizeof(text), tech_progress * disorder_progress / 100,
                              tech_progress, disorder_progress);
    ui_row_text(hdc, cursor, tr("Tech Progress", "科技进度"), text);
    if (civs[civ_id].disorder >= 100) {
        snprintf(risk, sizeof(risk), "%s", tr("Immediate collapse attempt", "立即尝试崩溃"));
    } else if (grace > 0) {
        char span[48];
        format_month_span(span, sizeof(span), grace);
        snprintf(risk, sizeof(risk), "%s %s", tr("Collapse grace:", "崩溃保护："), span);
    } else if (chance <= 0) {
        snprintf(risk, sizeof(risk), "%s", tr("No collapse risk", "无崩溃风险"));
    } else {
        snprintf(risk, sizeof(risk), "%d%%  %s", chance, tr("per 25 years", "每 25 年"));
    }
    ui_row_text(hdc, cursor, tr("Collapse Risk", "崩溃风险"), risk);
    ui_row_text(hdc, cursor, tr("Collapse Check", "崩溃判定"), collapse_last_reason(civ_id));
    if (!can_trigger) ui_row_text(hdc, cursor, tr("Cannot collapse", "无法崩溃"), reason);
    if (civs[civ_id].disorder >= 100) {
        snprintf(text, sizeof(text), "%s", tr("Immediate path", "立即路径"));
    } else if (grace > 0) {
        snprintf(text, sizeof(text), "%s", tr("ordinary roll skipped", "跳过普通判定"));
    } else if (chance > 0) {
        int years_left = 25 - (year % 25);
        if (years_left <= 0) years_left = 25;
        ui_format_months(text, sizeof(text), years_left * 12, UI_MONTH_ZERO_NOW);
    } else {
        snprintf(text, sizeof(text), "%s", tr("No scheduled roll", "无定期判定"));
    }
    ui_row_text(hdc, cursor, tr("Next Check", "下次判定"), text);
}

void draw_country_detail_content(HDC hdc, UiCursor *cursor, int civ_id,
                                 HFONT title_font, HFONT body_font) {
    CountrySummary country = summarize_country(civ_id);
    char army_text[160];
    char army_total[32];
    char army_deployed[32];
    char army_reserve[32];

    format_metric_value(war_current_soldiers_for_civ(civ_id), army_total, sizeof(army_total));
    format_metric_value(war_deployed_soldiers_for_civ(civ_id), army_deployed, sizeof(army_deployed));
    format_metric_value(war_available_reserve_for_civ(civ_id), army_reserve, sizeof(army_reserve));
    snprintf(army_text, sizeof(army_text), "%s %s   %s %s   %s %s   %s %d",
             tr("Total", "总量"), army_total, tr("Deployed", "已部署"), army_deployed,
             tr("Reserve", "预备队"), army_reserve, tr("Fronts", "战线"), war_front_count_for_civ(civ_id));

    switch (clamp(country_detail_subtab, 0, COUNTRY_DETAIL_TAB_COUNT - 1)) {
        case COUNTRY_DETAIL_TECHNOLOGY:
            draw_country_tech_tab(hdc, cursor, civ_id);
            break;
        case COUNTRY_DETAIL_DECISION:
            draw_country_decision_tab(hdc, cursor, civ_id);
            break;
        case COUNTRY_DETAIL_POPULATION:
            draw_country_population_tab(hdc, (RECT){0, 0, 0, 0}, cursor, civ_id, body_font);
            break;
            ui_section(hdc, cursor, tr("Population", "人口"));
            draw_metric_row(hdc, cursor, country.population, population_pressure_for_civ(civ_id), country.pop_capacity,
                            ICON_POPULATION, ICON_DISORDER, ICON_POPULATION,
                            metric_label("Pop", "人口"), metric_label("Pressure", "压力"), metric_label("Cap", "承载"));
            draw_city_list(hdc, cursor, civ_id);
            break;
        case COUNTRY_DETAIL_RESOURCES:
            draw_country_resources_tab(hdc, cursor, civ_id);
            break;
        case COUNTRY_DETAIL_DIPLOMACY:
            draw_country_diplomacy_tab(hdc, cursor, civ_id);
            break;
        case COUNTRY_DETAIL_DISORDER:
            draw_country_disorder_tab(hdc, cursor, civ_id);
            break;
        default:
            ui_section(hdc, cursor, tr("Overview", "总览"));
            draw_metric_row(hdc, cursor, country.population, province_count_for_civ(civ_id), country.cities,
                            ICON_POPULATION, ICON_TERRITORY, ICON_CITY_VILLAGE,
                            metric_label("Pop", "人口"), metric_label("Provinces", "省份"), metric_label("Cities", "城市"));
            draw_metric_row(hdc, cursor, war_current_soldiers_for_civ(civ_id), country.ports, civs[civ_id].disorder,
                            ICON_MILITARY, ICON_HARBOR, ICON_DISORDER,
                            metric_label("Army", "军队"), metric_label("Ports", "港口"), metric_label("Disorder", "混乱"));
            ui_row_text(hdc, cursor, tr("Army Pool", "军队池"), army_text);
            draw_overview_mini_blocks(hdc, cursor, civ_id);
            if (plague_civ_active_count(civ_id) > 0) {
                ui_section(hdc, cursor, tr("Plague", "瘟疫"));
                draw_metric_row(hdc, cursor, plague_civ_active_count(civ_id), plague_civ_peak_severity(civ_id),
                                plague_civ_deaths_total(civ_id), ICON_CITY_VILLAGE, ICON_DISORDER, ICON_POPULATION,
                                metric_label("Cities", "城市"), metric_label("Severity", "烈度"), metric_label("Deaths", "死亡"));
            }
            break;
    }
    (void)title_font;
    (void)body_font;
}
