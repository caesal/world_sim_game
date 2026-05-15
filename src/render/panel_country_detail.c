#include "render/panel_country_detail.h"

#include "render/panel_country_decision.h"
#include "render/panel_country_diplomacy.h"
#include "render/panel_country_disorder.h"
#include "render/panel_country_actions.h"
#include "render/panel_country_events.h"
#include "render/panel_country_population.h"
#include "render/panel_country_resources.h"
#include "render/panel_country_tech.h"
#include "render/snapshot_ui.h"
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

static __attribute__((unused)) const char *collapse_block_reason_ui(int civ_id) {
    static char buffers[4][EVENT_LOG_LEN];
    static int index;
    char *buffer = buffers[index++ % 4];
    CollapseBlockReason reason = collapse_block_reason(civ_id);

    switch (reason) {
        case COLLAPSE_BLOCK_NONE:
            snprintf(buffer, EVENT_LOG_LEN, "%s", tr("Ready.", "å°±ç»ªã€‚"));
            break;
        case COLLAPSE_BLOCK_NOT_ALIVE:
            snprintf(buffer, EVENT_LOG_LEN, "%s", tr("Country is invalid or has fallen.", "å›½å®¶æ— æ•ˆæˆ–å·²ç»ç­äº¡ã€‚"));
            break;
        case COLLAPSE_BLOCK_MAX_CIVS:
            snprintf(buffer, EVENT_LOG_LEN, "%s %d/%d, %s %d, %s %d.",
                     tr("No reusable or free country slots. Used", "æ²¡æœ‰å¯å¤ç”¨æˆ–ç©ºé—²å›½å®¶æ§½ã€‚å·²ç”¨"),
                     civ_count, MAX_CIVS, tr("alive", "å­˜æ´»"), civilization_alive_count(),
                     tr("reusable", "å¯å¤ç”¨"), civilization_reusable_slot_count());
            break;
        case COLLAPSE_BLOCK_NO_CAPITAL_REGION:
            snprintf(buffer, EVENT_LOG_LEN, "%s", tr("No valid capital region.", "æ²¡æœ‰æœ‰æ•ˆé¦–éƒ½åŒºåŸŸã€‚"));
            break;
        case COLLAPSE_BLOCK_NO_SPLITTABLE_REGION:
            snprintf(buffer, EVENT_LOG_LEN, "%s", tr("No splittable non-capital region.", "æ²¡æœ‰å¯æ‹†åˆ†çš„éžé¦–éƒ½åŒºåŸŸã€‚"));
            break;
        case COLLAPSE_BLOCK_ONLY_CORE_LEFT:
            snprintf(buffer, EVENT_LOG_LEN, "%s", tr("Only capital/core region remains.", "åªå‰©é¦–éƒ½/æ ¸å¿ƒåŒºåŸŸã€‚"));
            break;
        case COLLAPSE_BLOCK_CITY_CAP:
            snprintf(buffer, EVENT_LOG_LEN, "%s", tr("City limit reached.", "åŸŽå¸‚æ•°é‡å·²è¾¾ä¸Šé™ã€‚"));
            break;
        default:
            snprintf(buffer, EVENT_LOG_LEN, "%s", tr("Unknown collapse blocker.", "æœªçŸ¥å´©æºƒé˜»ç¢ã€‚"));
            break;
    }
    return buffer;
}

static int province_count_for_civ(int civ_id) {
    return snapshot_ui_province_count(civ_id);
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
                   ((snapshot_ui_civ(civ_id) && snapshot_ui_civ(civ_id)->plague_active_count > 0) ? 67 : 0);
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

static __attribute__((unused)) void draw_metric_pair(HDC hdc, UiCursor *cursor, int a, int b,
                                                     IconId ia, IconId ib, const char *la, const char *lb) {
    int w = (cursor->width - 8) / 2;
    RECT r = {cursor->x, cursor->y, cursor->x + w, cursor->y + 28};
    ui_metric_chip(hdc, r, ia, la, a, RGB(118, 143, 95));
    r.left += w + 8; r.right += w + 8;
    ui_metric_chip(hdc, r, ib, lb, b, RGB(83, 123, 166));
    cursor->y += 36;
}


static __attribute__((unused)) void format_percent_components_clean(char *buffer, size_t buffer_size,
                                                                    int total, int tech, int disorder) {
    snprintf(buffer, buffer_size, "%d%% (%s %d%%, %s %d%%)", total,
             tr("Tech", "ç§‘æŠ€"), tech, tr("Disorder", "æ··ä¹±"), disorder);
}

static __attribute__((unused)) void format_disorder_percent_clean(char *buffer, size_t buffer_size, int total) {
    snprintf(buffer, buffer_size, "%d%% (%s %d%%)", total, tr("Disorder", "æ··ä¹±"), total);
}

static void format_month_span(char *buffer, size_t buffer_size, int months) {
    if (months < 0) snprintf(buffer, buffer_size, "%s", tr("condition based", "æ¡ä»¶å†³å®š"));
    else ui_format_months(buffer, buffer_size, months, UI_MONTH_ZERO_NOW);
}

static __attribute__((unused)) void draw_pressure_source(HDC hdc, UiCursor *cursor, const char *label,
                                                         int value, int contribution, int decay,
                                                         int eta_months, const char *status) {
    char text[192];
    char eta[48];

    snprintf(text, sizeof(text), "%d / 100", value);
    ui_row_text(hdc, cursor, label, text);
    format_month_span(eta, sizeof(eta), eta_months);
    snprintf(text, sizeof(text), "%s +%d/mo   %s -%d/mo   %s %s   %s",
             tr("Contrib", "è´¡çŒ®"), contribution, tr("Decay", "è¡°é€€"), decay,
             tr("Clear", "æ¸…é›¶"), eta, status);
    ui_row_text(hdc, cursor, "", text);
}

static __attribute__((unused)) void draw_city_list(HDC hdc, UiCursor *cursor, int civ_id) {
    int i;
    char text[192];
    const RenderSnapshot *snapshot = snapshot_ui_current();

    ui_section(hdc, cursor, tr("Provinces / Cities", "è¡Œçœ / åŸŽå¸‚"));
    for (i = 0; snapshot && i < snapshot->city_count; i++) {
        const SnapshotCity *city = &snapshot->cities[i];
        if (!city->alive || city->owner != civ_id) continue;
        snprintf(text, sizeof(text), "%s%s%s  %s %d",
                 city->capital ? "* " : "", city->name,
                 city->port ? tr(" Port", " æ¸¯å£") : "",
                 tr("Pop", "äººå£"), city->population);
        draw_icon_text_line(hdc, cursor->x, cursor->y,
                            city->capital ? ICON_CITY_CAPITAL : ICON_TERRITORY,
                            text, ui_theme_color(UI_COLOR_TEXT_MUTED));
        cursor->y += 22;
    }
}

static void draw_civil_unrest_action(HDC hdc, UiCursor *cursor, int civ_id);

static const char *collapse_block_reason_snapshot_ui(int reason) {
    switch ((CollapseBlockReason)reason) {
        case COLLAPSE_BLOCK_NONE: return tr("Ready.", "å°±ç»ªã€‚");
        case COLLAPSE_BLOCK_NOT_ALIVE: return tr("Country is invalid or has fallen.", "å›½å®¶æ— æ•ˆæˆ–å·²ç»ç­äº¡ã€‚");
        case COLLAPSE_BLOCK_MAX_CIVS: return tr("No reusable or free country slots.", "æ²¡æœ‰å¯å¤ç”¨æˆ–ç©ºé—²å›½å®¶æ§½ã€‚");
        case COLLAPSE_BLOCK_NO_CAPITAL_REGION: return tr("No valid capital region.", "æ²¡æœ‰æœ‰æ•ˆé¦–éƒ½åŒºåŸŸã€‚");
        case COLLAPSE_BLOCK_NO_SPLITTABLE_REGION: return tr("No splittable non-capital region.", "æ²¡æœ‰å¯æ‹†åˆ†çš„éžé¦–éƒ½åŒºåŸŸã€‚");
        case COLLAPSE_BLOCK_ONLY_CORE_LEFT: return tr("Only capital/core region remains.", "åªå‰©é¦–éƒ½/æ ¸å¿ƒåŒºåŸŸã€‚");
        case COLLAPSE_BLOCK_CITY_CAP: return tr("City limit reached.", "åŸŽå¸‚æ•°é‡å·²è¾¾ä¸Šé™ã€‚");
        default: return tr("Unknown collapse blocker.", "æœªçŸ¥å´©æºƒé˜»ç¢ã€‚");
    }
}

static const char *overview_next_action_ui(const DecisionSnapshot *decision) {
    static char buffers[4][96];
    static int index;
    char *buffer = buffers[index++ % 4];
    const char *reason = decision->expansion_reason && decision->expansion_reason[0] ?
                         decision->expansion_reason : decision->main_intent;
    if (!reason) return "";
    if (ui_language != 1) return reason;
    if (strstr(reason, "Claimed adjacent region")) return "å·²å é¢†ç›¸é‚»åŒºåŸŸï¼Œè¿›å…¥å†·å´";
    if (strstr(reason, "Claimed shallow")) return "å·²å é¢†æµ…æµ·å¯è¾¾åŒºåŸŸï¼Œè¿›å…¥å†·å´";
    if (strstr(reason, "Claimed maritime") || strstr(reason, "Claimed overseas")) return "å·²é€šè¿‡èˆªé“å é¢†æ–°åŒºåŸŸ";
    if (strstr(reason, "cooldown") || strstr(reason, "Cooldown")) {
        if (decision->next_expansion_months > 0) {
            {
                char span[48];
                ui_format_months(span, sizeof(span), decision->next_expansion_months, UI_MONTH_ZERO_NOW);
                snprintf(buffer, sizeof(buffers[0]), "%s %s", tr("Expansion cooldown", "æ‰©å¼ å†·å´"), span);
            }
            return buffer;
        }
        return "æ‰©å¼ å†·å´ä¸­";
    }
    if (strstr(reason, "no target") || strstr(reason, "No target") ||
        strstr(reason, "No reachable") || strstr(reason, "no reachable")) return "æš‚æ— å¯æ‰§è¡Œç›®æ ‡";
    if (strstr(reason, "budget") || strstr(reason, "Budget")) return "ç­‰å¾…æ‰©å¼ é¢„ç®—";
    if (strstr(reason, "city cap") || strstr(reason, "City cap")) return "åŸŽå¸‚æ•°é‡è¾¾åˆ°ä¸Šé™";
    if (strstr(reason, "random") || strstr(reason, "chance") || strstr(reason, "Probability")) return "æœ¬æ¬¡æ‰©å¼ æœºä¼šæœªè§¦å‘";
    if (strcmp(reason, "Expansion") == 0) return "å€¾å‘æ‰©å¼ ";
    if (strcmp(reason, "War") == 0) return "æˆ˜äº‰å€¾å‘ä¸Šå‡";
    if (strcmp(reason, "Stability") == 0) return "ä¼˜å…ˆç»´æŒç¨³å®š";
    return reason;
}

static const char *overview_dominant_intent(const DecisionSnapshot *decision) {
    if (decision->expansion_weight >= decision->war_weight &&
        decision->expansion_weight >= decision->stability_weight) return tr("Expansion", "æ‰©å¼ ");
    if (decision->war_weight >= decision->expansion_weight &&
        decision->war_weight >= decision->stability_weight) return tr("War", "æˆ˜äº‰");
    return tr("Stability", "ç¨³å®š");
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
    const SnapshotCiv *civ = snapshot_ui_civ(civ_id);
    int progress = civ ? civ->tech_stage_progress_percent : 0;
    int months_next = civ ? civ->tech_months_to_next : 0;
    char text[192];
    char span[48];

    memset(&decision, 0, sizeof(decision));
    if (civ) {
        decision.expansion_weight = civ->decision_expansion_weight;
        decision.war_weight = civ->decision_war_weight;
        decision.stability_weight = civ->decision_stability_weight;
        decision.next_expansion_months = civ->decision_next_expansion_months;
        decision.main_intent = civ->main_intent;
        decision.expansion_reason = civ->decision_expansion_reason;
    }
    ui_section(hdc, cursor, tr("Technology Snapshot", "ç§‘æŠ€æ‘˜è¦"));
    snprintf(text, sizeof(text), "%s %d: %s", tr("Stage", "é˜¶æ®µ"),
             civ ? clamp(civ->tech_stage, 0, 10) : 0,
             technology_stage_name(civ ? clamp(civ->tech_stage, 0, 10) : 0, ui_language));
    ui_row_text(hdc, cursor, tr("Current", "å½“å‰"), text);
    ui_progress_bar(hdc, ui_take_rect(cursor, 12), progress, 100, RGB(104, 158, 186));
    ui_format_months(span, sizeof(span), months_next >= 0 ? months_next : 0, UI_MONTH_ZERO_DONE);
    snprintf(text, sizeof(text), "%d%%   %s %s", progress, tr("Next in", "è·ä¸‹é˜¶æ®µ"), span);
    ui_row_text(hdc, cursor, tr("Progress", "è¿›åº¦"), text);
    ui_section(hdc, cursor, tr("Decision Tendency", "å†³ç­–å€¾å‘"));
    draw_decision_meter_row(hdc, cursor, tr("Expansion", "æ‰©å¼ "), decision.expansion_weight, RGB(82, 156, 112));
    draw_decision_meter_row(hdc, cursor, tr("War", "æˆ˜äº‰"), decision.war_weight, RGB(180, 84, 74));
    draw_decision_meter_row(hdc, cursor, tr("Stability", "ç¨³å®š"), decision.stability_weight, RGB(92, 130, 162));
    ui_row_text(hdc, cursor, tr("Dominant Intent", "ä¸»å¯¼æ–¹å‘"), overview_dominant_intent(&decision));
    ui_row_text(hdc, cursor, tr("Next Action", "ä¸‹ä¸€æ­¥"), overview_next_action_ui(&decision));
    ui_section(hdc, cursor, tr("Actions", "æ“ä½œ"));
    draw_civil_unrest_action(hdc, cursor, civ_id);
    draw_country_overview_vassal_actions(hdc, cursor, civ_id);
    draw_country_recent_events(hdc, cursor, civ_id);
}

static void draw_civil_unrest_action(HDC hdc, UiCursor *cursor, int civ_id) {
    const SnapshotCiv *civ = snapshot_ui_civ(civ_id);
    int can_trigger = civ ? civ->collapse_can_trigger : 0;
    const char *reason = collapse_block_reason_snapshot_ui(civ ? civ->collapse_block_reason : COLLAPSE_BLOCK_NOT_ALIVE);
    RECT button = {cursor->x, cursor->y + 4, cursor->x + cursor->width, cursor->y + 34};

    last_civil_unrest_button = button;
    last_civil_unrest_enabled = can_trigger;
    fill_rect(hdc, button, can_trigger ? RGB(112, 45, 45) : RGB(65, 55, 55));
    draw_center_text(hdc, button, tr("Civil Unrest", "å†…ä¹±"),
                     can_trigger ? RGB(255, 236, 226) : ui_theme_color(UI_COLOR_TEXT_DIM));
    cursor->y += 40;
    if (!can_trigger) ui_row_text(hdc, cursor, tr("Cannot collapse", "æ— æ³•å´©æºƒ"), reason);
}



void draw_country_detail_content(HDC hdc, UiCursor *cursor, int civ_id,
                                 HFONT title_font, HFONT body_font) {
    const SnapshotCiv *civ = snapshot_ui_civ(civ_id);
    CountrySummary country = civ ? civ->summary : (CountrySummary){0};
    char army_text[160];
    char army_total[32];
    char army_deployed[32];
    char army_reserve[32];

    format_metric_value(civ ? civ->current_soldiers : 0, army_total, sizeof(army_total));
    format_metric_value(civ ? civ->war_deployed_soldiers : 0, army_deployed, sizeof(army_deployed));
    format_metric_value(civ ? civ->war_available_reserve : 0, army_reserve, sizeof(army_reserve));
    snprintf(army_text, sizeof(army_text), "%s %s   %s %s   %s %s   %s %d",
             tr("Total", "æ€»é‡"), army_total, tr("Deployed", "å·²éƒ¨ç½²"), army_deployed,
             tr("Reserve", "é¢„å¤‡é˜Ÿ"), army_reserve, tr("Fronts", "æˆ˜çº¿"), civ ? civ->war_front_count : 0);

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
            ui_section(hdc, cursor, tr("Overview", "æ€»è§ˆ"));
            draw_metric_row(hdc, cursor, country.population, province_count_for_civ(civ_id), country.cities,
                            ICON_POPULATION, ICON_TERRITORY, ICON_CITY_VILLAGE,
                            metric_label("Pop", "äººå£"), metric_label("Provinces", "çœä»½"), metric_label("Cities", "åŸŽå¸‚"));
            draw_metric_row(hdc, cursor, civ ? civ->current_soldiers : 0, country.ports, civ ? civ->disorder : 0,
                            ICON_MILITARY, ICON_HARBOR, ICON_DISORDER,
                            metric_label("Army", "å†›é˜Ÿ"), metric_label("Ports", "æ¸¯å£"), metric_label("Disorder", "æ··ä¹±"));
            ui_row_text(hdc, cursor, tr("Army Pool", "å†›é˜Ÿæ± "), army_text);
            draw_overview_mini_blocks(hdc, cursor, civ_id);
            if ((civ ? civ->plague_active_count : 0) > 0) {
                ui_section(hdc, cursor, tr("Plague", "ç˜Ÿç–«"));
                draw_metric_row(hdc, cursor, civ ? civ->plague_active_count : 0, civ ? civ->plague_peak_severity : 0,
                                civ ? civ->plague_deaths_total : 0, ICON_CITY_VILLAGE, ICON_DISORDER, ICON_POPULATION,
                                metric_label("Cities", "åŸŽå¸‚"), metric_label("Severity", "çƒˆåº¦"), metric_label("Deaths", "æ­»äº¡"));
            }
            break;
    }
    (void)title_font;
    (void)body_font;
}
