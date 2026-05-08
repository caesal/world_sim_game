#include "render/panel_country_detail.h"

#include "render_panel_internal.h"
#include "sim/collapse.h"
#include "sim/diplomacy.h"
#include "sim/disorder.h"
#include "sim/expansion.h"
#include "sim/plague.h"
#include "sim/population.h"
#include "sim/regions.h"
#include "sim/technology.h"
#include "ui/ui_widgets.h"

#include <stdio.h>

static RECT last_civil_unrest_button;
static int last_civil_unrest_enabled;

static int province_count_for_civ(int civ_id) {
    int i;
    int count = 0;
    for (i = 0; i < region_count; i++) {
        if (natural_regions[i].alive && natural_regions[i].owner_civ == civ_id) count++;
    }
    return count > 0 ? count : summarize_country(civ_id).cities;
}

int country_detail_content_height(int civ_id) {
    int owned_cities = 0;
    int i;
    int height = 49 + 3 * 36 + 31 + 10 * 21 + 78 + 21 + 31 + 3 * 36 + 21 +
                 31 + 3 * 36 + 31 + 2 * 36 + 84 + 5 * 21;

    for (i = 0; i < city_count; i++) {
        if (cities[i].alive && cities[i].owner == civ_id) owned_cities++;
    }
    if (plague_civ_active_count(civ_id) > 0) height += 31 + 36;
    height += 31 + owned_cities * 22;
    return height + 28;
}

void country_detail_reset_hit(void) {
    last_civil_unrest_enabled = 0;
    SetRectEmpty(&last_civil_unrest_button);
}

int country_detail_civil_unrest_hit(RECT viewport, int mouse_x, int mouse_y) {
    return last_civil_unrest_enabled &&
           point_in_rect_local(viewport, mouse_x, mouse_y) &&
           point_in_rect_local(last_civil_unrest_button, mouse_x, mouse_y);
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

static void format_percent_components(char *buffer, size_t buffer_size,
                                      int total, int tech, int disorder) {
    snprintf(buffer, buffer_size, "%d%% (%s %d%%, %s %d%%)", total,
             tr("Tech", "科技"), tech, tr("Disorder", "混乱"), disorder);
}

static void format_disorder_percent(char *buffer, size_t buffer_size, int total) {
    snprintf(buffer, buffer_size, "%d%% (%s %d%%)", total, tr("Disorder", "混乱"), total);
}

static void draw_country_header(HDC hdc, UiCursor *cursor, int civ_id,
                                HFONT title_font, HFONT body_font) {
    RECT swatch = {cursor->x, cursor->y + 4, cursor->x + 18, cursor->y + 22};
    char text[128];

    SelectObject(hdc, title_font);
    fill_rect(hdc, swatch, civs[civ_id].color);
    snprintf(text, sizeof(text), "%c  %.63s", civs[civ_id].symbol, civilization_display_name(civ_id));
    draw_text_line(hdc, cursor->x + 28, cursor->y, text, ui_theme_color(UI_COLOR_TEXT));
    cursor->y += 28;
    SelectObject(hdc, body_font);
    ui_row_text(hdc, cursor, tr("Capital", "首都"), capital_name_for_civ(civ_id));
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

static void draw_ai_decisions(HDC hdc, UiCursor *cursor, int civ_id) {
    int resource_score = expansion_resource_score_for_civ(civ_id);
    ExpansionAIDiagnostics ai = expansion_ai_diagnostics(civ_id, resource_score);
    char text[160];

    ui_section(hdc, cursor, tr("AI Decisions", "AI 决策"));
    snprintf(text, sizeof(text), "%s %d   %s %d   %s %d",
             tr("Land Adj", "陆接"), ai.land_adjacent_unowned_regions,
             tr("Land Reach", "陆达"), ai.land_nearby_unowned_regions,
             tr("Shallow", "浅海"), ai.shallow_sea_reachable_regions);
    ui_row_text(hdc, cursor, tr("Reachability", "可达性"), text);
    snprintf(text, sizeof(text), "%s %d   %s %d   %s %d",
             tr("Route", "航道"), ai.maritime_reachable_regions,
             tr("Deep", "深海"), ai.deep_sea_reachable_regions,
             tr("Port Cand", "潜在港口"), ai.port_candidate_regions);
    ui_row_text(hdc, cursor, tr("Sea Targets", "海路目标"), text);
    snprintf(text, sizeof(text), "%d   %d%%", ai.global_unowned_regions, ai.global_unowned_percent);
    ui_row_text(hdc, cursor, tr("Global Open Land", "全图无主地"), text);
    snprintf(text, sizeof(text), "%d / %d", ai.population_pressure, ai.resource_pressure);
    ui_row_text(hdc, cursor, tr("Pop / Resource Pressure", "人口/资源压力"), text);
    snprintf(text, sizeof(text), "%d / %d   x%d%%", ai.expansion_desire,
             ai.expansion_threshold, ai.tech_expansion_percent);
    ui_row_text(hdc, cursor, tr("Expansion Desire", "扩张意愿"), text);
    snprintf(text, sizeof(text), "%d mo   %s %d", ai.claim_cooldown_months,
             tr("Wait", "等待"), ai.months_until_next_claim);
    ui_row_text(hdc, cursor, tr("Claim Cooldown", "占领冷却"), text);
    snprintf(text, sizeof(text), "%d %s", ai.claim_budget, tr("region this window", "个区域"));
    ui_row_text(hdc, cursor, tr("Claim Budget", "占领预算"), text);
    snprintf(text, sizeof(text), "%d", diplomacy_last_war_desire(civ_id));
    ui_row_text(hdc, cursor, tr("War Desire", "战争意愿"), text);
    ui_row_text(hdc, cursor, tr("Expansion Reason", "扩张原因"), expansion_last_reason(civ_id));
    ui_row_text(hdc, cursor, tr("War Reason", "战争原因"), diplomacy_last_war_reason(civ_id));
}

static void draw_technology_block(HDC hdc, UiCursor *cursor, int civ_id) {
    char text[96];
    int stage = clamp(civs[civ_id].tech_stage, 0, 10);

    ui_section(hdc, cursor, tr("Technology", "科技阶段"));
    ui_row_text(hdc, cursor, tr("Stage", "阶段"), technology_stage_name(stage, ui_language));
    draw_ai_decisions(hdc, cursor, civ_id);
    snprintf(text, sizeof(text), "%d/10   %s %d", stage,
             tr("Years to next", "距下阶段年数"), technology_years_to_next(civ_id));
    ui_row_text(hdc, cursor, tr("Progress", "进度"), text);
    ui_row_text(hdc, cursor, tr("Effect", "效果"), technology_stage_effect(stage, ui_language));
    ui_row_text(hdc, cursor, tr("Pressure", "压力"),
                tr("Population and resource saturation; high values affect disorder and expansion.",
                   "人口与资源饱和压力；高压力会影响混乱和扩张。"));
}

static void draw_disorder_block(HDC hdc, UiCursor *cursor, int civ_id) {
    char risk[80];
    char text[96];
    int can_trigger = collapse_can_trigger(civ_id);
    const char *reason = collapse_trigger_block_reason(civ_id);
    int chance = collapse_decade_chance_for_disorder(civs[civ_id].disorder);
    RECT button;
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
                     ICON_COUNTRY_DEFENSE, ICON_DISORDER, metric_label("Stability", "稳定"),
                     metric_label("Total", "总计"));
    format_percent_components(text, sizeof(text), tech_resource * disorder_resource / 100,
                              tech_resource, disorder_resource);
    ui_row_text(hdc, cursor, tr("Resource Output", "资源产出"), text);
    format_disorder_percent(text, sizeof(text), disorder_population_growth_percent(civs[civ_id].disorder));
    ui_row_text(hdc, cursor, tr("Population Growth", "人口增长"), text);
    format_percent_components(text, sizeof(text), tech_progress * disorder_progress / 100,
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
                 tr("per 10 years", "每 10 年"));
    }
    ui_row_text(hdc, cursor, tr("Collapse Risk", "崩溃风险"), risk);
    ui_row_text(hdc, cursor, tr("Collapse Check", "崩溃判定"), collapse_last_reason(civ_id));
    if (!can_trigger) ui_row_text(hdc, cursor, tr("Cannot collapse", "无法崩溃"), reason);
    if (civs[civ_id].disorder >= 100) {
        snprintf(text, sizeof(text), "%s", tr("Immediate path", "立即路径"));
    } else if (chance > 0) {
        int years_left = 10 - (year % 10);
        if (years_left <= 0) years_left = 10;
        snprintf(text, sizeof(text), "%d %s", years_left, tr("years", "年"));
    } else {
        snprintf(text, sizeof(text), "%s", tr("No scheduled roll", "无定期判定"));
    }
    ui_row_text(hdc, cursor, tr("Next Check", "下次判定"), text);
    button = (RECT){cursor->x, cursor->y + 4, cursor->x + cursor->width, cursor->y + 34};
        last_civil_unrest_button = button;
        last_civil_unrest_enabled = can_trigger;
        fill_rect(hdc, button, can_trigger ? RGB(112, 45, 45) : RGB(65, 55, 55));
        draw_center_text(hdc, button, tr("Civil Unrest", "内乱"),
                         can_trigger ? RGB(255, 236, 226) : ui_theme_color(UI_COLOR_TEXT_DIM));
        cursor->y += 42;
}

void draw_country_detail_content(HDC hdc, UiCursor *cursor, int civ_id,
                                 HFONT title_font, HFONT body_font) {
    CountrySummary country = summarize_country(civ_id);

    draw_country_header(hdc, cursor, civ_id, title_font, body_font);
    draw_metric_row(hdc, cursor, country.population, province_count_for_civ(civ_id), country.cities,
                    ICON_POPULATION, ICON_TERRITORY, ICON_CITY_VILLAGE,
                    metric_label("Pop", "人口"), metric_label("Provinces", "省份"), metric_label("Cities", "城市"));
    draw_metric_row(hdc, cursor, war_estimated_soldiers(civ_id), country.money, civs[civ_id].disorder,
                    ICON_MILITARY, ICON_MONEY, ICON_DISORDER,
                    metric_label("Army", "军队"), metric_label("Money", "金钱"), metric_label("Disorder", "混乱"));
    draw_metric_row(hdc, cursor, country.ports, country.territory, population_pressure_for_civ(civ_id),
                    ICON_HARBOR, ICON_TERRITORY, ICON_POPULATION,
                    metric_label("Ports", "港口"), metric_label("Land", "土地"), metric_label("Pressure", "压力"));
    draw_disorder_block(hdc, cursor, civ_id);
    draw_technology_block(hdc, cursor, civ_id);
    ui_section(hdc, cursor, tr("Civilization Metrics", "文明指标"));
    draw_metric_row(hdc, cursor, civs[civ_id].governance, civs[civ_id].cohesion, civs[civ_id].production,
                    ICON_GOVERNANCE, ICON_COHESION, ICON_PRODUCTION,
                    metric_label("Gov", "治理"), metric_label("Coh", "凝聚"), metric_label("Prod", "生产"));
    draw_metric_row(hdc, cursor, civs[civ_id].military, civs[civ_id].commerce, civs[civ_id].logistics,
                    ICON_MILITARY, ICON_COMMERCE, ICON_LOGISTICS,
                    metric_label("Mil", "军备"), metric_label("Trade", "贸易"), metric_label("Log", "后勤"));
    draw_metric_pair(hdc, cursor, civs[civ_id].innovation, civs[civ_id].adaptation,
                     ICON_INNOVATION, ICON_ADAPTATION, metric_label("Tech", "技术"), metric_label("Adapt", "适应"));
    ui_row_text(hdc, cursor, tr("Adaptation", "适应力"),
                tr("Dynamic state from environment and pressure.", "由环境与压力派生的动态状态。"));
    ui_section(hdc, cursor, tr("Resources", "资源"));
    draw_metric_row(hdc, cursor, country.food, country.water, country.pop_capacity,
                    ICON_FOOD, ICON_WATER, ICON_POPULATION,
                    metric_label("Food", "粮食"), metric_label("Water", "水源"), metric_label("Cap", "承载"));
    draw_metric_row(hdc, cursor, country.livestock, country.wood, country.stone,
                    ICON_LIVESTOCK, ICON_WOOD, ICON_STONE,
                    metric_label("Herd", "畜牧"), metric_label("Wood", "木材"), metric_label("Stone", "石料"));
    draw_metric_row(hdc, cursor, country.minerals, country.money, country.habitability,
                    ICON_ORE, ICON_MONEY, ICON_HABITABILITY,
                    metric_label("Ore", "矿石"), metric_label("Money", "金钱"), metric_label("Live", "宜居"));
    if (plague_civ_active_count(civ_id) > 0) {
        ui_section(hdc, cursor, tr("Plague", "瘟疫"));
        draw_metric_row(hdc, cursor, plague_civ_active_count(civ_id), plague_civ_peak_severity(civ_id),
                        plague_civ_deaths_total(civ_id), ICON_CITY_VILLAGE, ICON_DISORDER, ICON_POPULATION,
                        metric_label("Cities", "城市"), metric_label("Severity", "烈度"), metric_label("Deaths", "死亡"));
    }
    draw_city_list(hdc, cursor, civ_id);
}
