#include "panel_country_decision.h"

#include "core/game_state.h"
#include "render/render_common.h"
#include "sim/decision_snapshot.h"
#include "ui/ui_widgets.h"

#include <stdio.h>
#include <string.h>

typedef enum {
    FUNNEL_PASS,
    FUNNEL_WAIT,
    FUNNEL_BLOCK,
    FUNNEL_SKIP,
    FUNNEL_CAPPED
} FunnelStatus;

static const char *intent_label(const char *intent) {
    if (!intent) return tr("Unknown", "未知");
    if (strcmp(intent, "War") == 0) return tr("War", "战争");
    if (strcmp(intent, "Stability") == 0) return tr("Stability", "稳定");
    return tr("Expansion", "扩张");
}

static void draw_weight_bar(HDC hdc, UiCursor *cursor, const char *label,
                            int value, COLORREF color) {
    RECT row = ui_take_rect(cursor, 24);
    RECT label_rect = {row.left, row.top, row.left + 82, row.bottom};
    RECT bar = {row.left + 88, row.top + 6, row.right - 36, row.bottom - 6};
    char text[24];
    draw_text_rect(hdc, label_rect, label, ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    ui_progress_bar(hdc, bar, value, 100, color);
    snprintf(text, sizeof(text), "%d", value);
    draw_text_rect(hdc, (RECT){bar.right + 6, row.top, row.right, row.bottom}, text,
                   ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_VCENTER | DT_RIGHT);
}

static void draw_reachability_grid(HDC hdc, UiCursor *cursor, const DecisionSnapshot *snap) {
    int w = (cursor->width - 8) / 3;
    RECT r = {cursor->x, cursor->y, cursor->x + w, cursor->y + 28};
    ui_stat_chip(hdc, r, tr("Land Adj", "陆接"), snap->expansion.land_adjacent_unowned_regions, RGB(118, 143, 95));
    r.left += w + 4; r.right += w + 4;
    ui_stat_chip(hdc, r, tr("Land Reach", "陆达"), snap->expansion.land_nearby_unowned_regions, RGB(108, 132, 91));
    r.left += w + 4; r.right += w + 4;
    ui_stat_chip(hdc, r, tr("Shallow", "浅海"), snap->expansion.shallow_sea_reachable_regions, RGB(85, 142, 154));
    cursor->y += 34;
    r = (RECT){cursor->x, cursor->y, cursor->x + w, cursor->y + 28};
    ui_stat_chip(hdc, r, tr("Route", "航道"), snap->expansion.maritime_reachable_regions, RGB(95, 126, 172));
    r.left += w + 4; r.right += w + 4;
    ui_stat_chip(hdc, r, tr("Deep", "深海"), snap->expansion.deep_sea_reachable_regions, RGB(98, 96, 152));
    r.left += w + 4; r.right += w + 4;
    ui_stat_chip(hdc, r, tr("Global Open", "全图无主"), snap->expansion.global_unowned_regions, RGB(156, 142, 92));
    cursor->y += 36;
}

static const char *status_text(FunnelStatus status) {
    switch (status) {
        case FUNNEL_PASS: return tr("PASS", "通过");
        case FUNNEL_WAIT: return tr("WAIT", "等待");
        case FUNNEL_SKIP: return tr("SKIP", "跳过");
        case FUNNEL_CAPPED: return tr("CAP", "上限");
        default: return tr("BLOCK", "阻塞");
    }
}

static COLORREF status_color(FunnelStatus status) {
    switch (status) {
        case FUNNEL_PASS: return RGB(83, 143, 98);
        case FUNNEL_WAIT: return RGB(176, 137, 65);
        case FUNNEL_SKIP: return RGB(136, 112, 71);
        case FUNNEL_CAPPED: return RGB(151, 78, 68);
        default: return RGB(150, 74, 68);
    }
}

static void funnel_step(HDC hdc, UiCursor *cursor, const char *label,
                        FunnelStatus status, const char *detail) {
    RECT row = ui_take_rect(cursor, 32);
    RECT badge = {row.left, row.top + 5, row.left + 56, row.bottom - 5};
    RECT label_rect = {row.left + 64, row.top, row.right, row.top + 17};
    RECT detail_rect = {row.left + 64, row.top + 16, row.right, row.bottom};
    fill_rect(hdc, badge, status_color(status));
    draw_center_text(hdc, badge, status_text(status), RGB(248, 248, 244));
    draw_text_rect(hdc, label_rect, label, ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_END_ELLIPSIS);
    if (detail && detail[0]) {
        draw_text_rect(hdc, detail_rect, detail, ui_theme_color(UI_COLOR_TEXT_DIM),
                       DT_SINGLELINE | DT_END_ELLIPSIS);
    }
}

static void countdown_chip(HDC hdc, UiCursor *cursor, const char *label,
                           int value, const char *unit, int max_value) {
    RECT row = ui_take_rect(cursor, 26);
    RECT bar = {row.left + 112, row.top + 7, row.right - 60, row.bottom - 7};
    char text[56];
    draw_text_rect(hdc, (RECT){row.left, row.top, row.left + 106, row.bottom}, label,
                   ui_theme_color(UI_COLOR_TEXT_DIM), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    ui_progress_bar(hdc, bar, max_value - min(value, max_value), max_value, RGB(106, 158, 186));
    snprintf(text, sizeof(text), "%d %s", value, unit);
    draw_text_rect(hdc, (RECT){bar.right + 6, row.top, row.right, row.bottom}, text,
                   ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_VCENTER | DT_RIGHT);
}

static const char *current_state_text(const DecisionSnapshot *snap, int reachable,
                                      int desire_ready, int cooldown_ready, int budget_ready) {
    const char *intent = snap->main_intent ? snap->main_intent : "";
    if (reachable <= 0) return tr("No reachable target", "没有可达目标");
    if (!desire_ready) return tr("Waiting for expansion desire", "等待扩张意愿");
    if (!cooldown_ready) return tr("Waiting for cooldown", "等待冷却");
    if (!budget_ready) return tr("No claim budget", "没有占领预算");
    if (city_count >= MAX_CITIES) return tr("City limit reached", "城市数量已达上限");
    if (strcmp(intent, "War") == 0) return tr("War preferred", "战争优先");
    if (strcmp(intent, "Stability") == 0) return tr("Stability preferred", "稳定优先");
    return tr("Can expand now", "现在可以扩张");
}

int country_decision_tab_height(int civ_id) {
    (void)civ_id;
    return 560;
}

void draw_country_decision_tab(HDC hdc, UiCursor *cursor, int civ_id) {
    DecisionSnapshot snap;
    char text[256];
    int reachable;
    int desire_ready;
    int cooldown_ready;
    int budget_ready;
    int capacity_ready;
    FunnelStatus outcome_status;

    decision_snapshot_for_civ(civ_id, &snap);
    reachable = snap.expansion.land_adjacent_unowned_regions + snap.expansion.land_nearby_unowned_regions +
                snap.expansion.shallow_sea_reachable_regions + snap.expansion.maritime_reachable_regions +
                snap.expansion.deep_sea_reachable_regions;
    desire_ready = snap.expansion.expansion_desire >= snap.expansion.expansion_threshold;
    cooldown_ready = snap.expansion.months_until_next_claim <= 0;
    budget_ready = snap.expansion.claim_budget > 0;
    capacity_ready = city_count < MAX_CITIES;

    ui_section(hdc, cursor, tr("Decision", "决策"));
    ui_row_text(hdc, cursor, tr("Current State", "当前状态"),
                current_state_text(&snap, reachable, desire_ready, cooldown_ready, budget_ready));
    ui_row_text(hdc, cursor, tr("Main Intent", "主要意图"), intent_label(snap.main_intent));
    draw_weight_bar(hdc, cursor, tr("Expansion", "扩张"), snap.expansion_weight, RGB(116, 158, 91));
    draw_weight_bar(hdc, cursor, tr("War", "战争"), snap.war_weight, RGB(180, 84, 74));
    draw_weight_bar(hdc, cursor, tr("Internal", "内部"), snap.stability_weight, RGB(92, 130, 162));

    ui_section(hdc, cursor, tr("Reachability", "可达性"));
    draw_reachability_grid(hdc, cursor, &snap);

    ui_section(hdc, cursor, tr("Decision Funnel", "决策漏斗"));
    snprintf(text, sizeof(text), "%d %s", reachable, tr("land/sea signals", "陆海信号"));
    funnel_step(hdc, cursor, tr("Reachable land or sea exists", "存在可达陆地或海路"),
                reachable > 0 ? FUNNEL_PASS : FUNNEL_BLOCK, text);
    snprintf(text, sizeof(text), "%d / %d", snap.expansion.expansion_desire, snap.expansion.expansion_threshold);
    funnel_step(hdc, cursor, tr("Desire reaches threshold", "意愿达到门槛"),
                desire_ready ? FUNNEL_PASS : FUNNEL_WAIT, text);
    snprintf(text, sizeof(text), "%d %s %s", snap.expansion.months_until_next_claim,
             tr("months", "月"), tr("until expansion attempt", "后尝试扩张"));
    funnel_step(hdc, cursor, tr("Cooldown ready", "冷却完成"),
                cooldown_ready ? FUNNEL_PASS : FUNNEL_WAIT, text);
    snprintf(text, sizeof(text), "%d", snap.expansion.claim_budget);
    funnel_step(hdc, cursor, tr("Claim budget available", "占领预算可用"),
                budget_ready ? FUNNEL_PASS : FUNNEL_WAIT, text);
    funnel_step(hdc, cursor, tr("Capacity available", "容量可用"),
                capacity_ready ? FUNNEL_PASS : FUNNEL_CAPPED,
                capacity_ready ? tr("City capacity available", "城市容量可用") :
                                 tr("City limit blocks new admin city", "城市上限阻止新行政城市"));
    outcome_status = reachable > 0 && desire_ready && cooldown_ready && budget_ready && capacity_ready ?
                     FUNNEL_PASS : FUNNEL_BLOCK;
    if (snap.expansion_reason && strstr(snap.expansion_reason, "chance")) outcome_status = FUNNEL_SKIP;
    funnel_step(hdc, cursor, tr("Current outcome", "当前结果"), outcome_status,
                current_state_text(&snap, reachable, desire_ready, cooldown_ready, budget_ready));

    ui_section(hdc, cursor, tr("Countdowns", "倒计时"));
    countdown_chip(hdc, cursor, tr("Expansion attempt", "扩张尝试"), snap.next_expansion_months, tr("mo", "月"), 36);
    countdown_chip(hdc, cursor, tr("Diplomacy eval", "外交评估"), snap.next_diplomacy_months, tr("mo", "月"), 12);
    countdown_chip(hdc, cursor, tr("Next battle", "下次战斗"), snap.next_battle_months, tr("mo", "月"), 36);
    countdown_chip(hdc, cursor, tr("Collapse check", "崩溃判定"), snap.next_collapse_years, tr("yr", "年"), 10);

    ui_section(hdc, cursor, tr("Last Attempt Reasons", "上次尝试原因"));
    ui_row_text(hdc, cursor, tr("Expansion", "扩张"),
                snap.expansion_reason ? snap.expansion_reason : tr("No expansion decision yet.", "暂无扩张决策。"));
    ui_row_text(hdc, cursor, tr("War", "战争"),
                snap.war_reason ? snap.war_reason : tr("No war decision yet.", "暂无战争决策。"));
}
