#include "panel_country_decision.h"
#include "render/render_common.h"
#include "render/snapshot_ui.h"
#include "render/ui_format.h"
#include "ui/ui_types.h"
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
    if (!intent || !intent[0]) return tr("Unknown", "未知");
    if (strcmp(intent, "Waiting") == 0) return tr("Waiting", "等待");
    if (strcmp(intent, "Unknown") == 0) return tr("Unknown", "未知");
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
    ui_stat_chip(hdc, r, tr("Land", "陆接"), snap->expansion.land_adjacent_unowned_regions, RGB(118, 143, 95));
    r.left += w + 4; r.right += w + 4;
    ui_stat_chip(hdc, r, tr("Reach", "陆达"), snap->expansion.land_nearby_unowned_regions, RGB(108, 132, 91));
    r.left += w + 4; r.right += w + 4;
    ui_stat_chip(hdc, r, tr("Shallow", "浅海"), snap->expansion.shallow_sea_reachable_regions, RGB(85, 142, 154));
    cursor->y += 34;
    r = (RECT){cursor->x, cursor->y, cursor->x + w, cursor->y + 28};
    ui_stat_chip(hdc, r, tr("Route", "航道"), snap->expansion.maritime_reachable_regions, RGB(95, 126, 172));
    r.left += w + 4; r.right += w + 4;
    ui_stat_chip(hdc, r, tr("Deep", "深海"), snap->expansion.deep_sea_reachable_regions, RGB(98, 96, 152));
    r.left += w + 4; r.right += w + 4;
    ui_stat_chip(hdc, r, tr("Open", "无主"), snap->expansion.global_unowned_regions, RGB(156, 142, 92));
    cursor->y += 36;
}
static void format_maritime_blockers(const MaritimeExpansionDiagnostics *m, char *out, size_t out_size) {
    if (!out || out_size == 0 || !m) return;
    if (m->blocked_no_port) snprintf(out, out_size, "%s", tr("No port", "无港口"));
    else if (m->suppressed_by_land_targets) snprintf(out, out_size, "%s", tr("Land first", "本土优先"));
    else if (m->blocked_no_shallow_path > 0) snprintf(out, out_size, "%s", tr("Route broken", "航道断链"));
    else if (m->blocked_city_cap > 0) snprintf(out, out_size, "%s", tr("No capacity", "容量不足"));
    else if (m->blocked_low_score > 0) snprintf(out, out_size, "%s", tr("Low score", "分数不足"));
    else if (m->blocked_path_budget > 0) snprintf(out, out_size, "%s", tr("No budget", "预算不足"));
    else if (m->blocked_deep_locked > 0) snprintf(out, out_size, "%s", tr("Deep locked", "深海锁定"));
    else if (m->blocked_no_sea_entry > 0) snprintf(out, out_size, "%s", tr("No entry", "无入口"));
    else if (m->blocked_no_capital_or_port_site > 0) snprintf(out, out_size, "%s", tr("No hub", "缺首府/港"));
    else if (m->blocked_no_port_site > 0) snprintf(out, out_size, "%s", tr("No port site", "无港址"));
    else snprintf(out, out_size, "%s", tr("Clear", "无阻塞"));
}
static const char *status_text(FunnelStatus status) {
    switch (status) {
        case FUNNEL_PASS: return tr("PASS", "通过");
        case FUNNEL_WAIT: return tr("WAIT", "等待");
        case FUNNEL_SKIP: return tr("SKIP", "跳过");
        case FUNNEL_CAPPED: return tr("BLOCK", "阻塞");
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
static void draw_gate_chip(HDC hdc, RECT rect, const char *label, FunnelStatus status,
                           const char *detail) {
    RECT badge = {rect.left + 7, rect.top + 6, rect.left + 58, rect.top + 22};
    RECT label_rect = {rect.left + 64, rect.top + 4, rect.right - 8, rect.top + 21};
    RECT detail_rect = {rect.left + 8, rect.top + 24, rect.right - 8, rect.bottom - 4};
    fill_rect(hdc, rect, ui_theme_color(UI_COLOR_PANEL_SOFT));
    fill_rect(hdc, badge, status_color(status));
    draw_center_text(hdc, badge, status_text(status), RGB(248, 248, 244));
    draw_text_rect(hdc, label_rect, label, ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, detail_rect, detail ? detail : "", ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}
static const char *integrity_status_text(const DecisionSnapshot *snap) {
    if (snap->disconnected_components <= 0) return tr("Fully linked", "完整联通");
    if (snap->longest_disconnected_months > 0) return tr("Detaching", "距脱离");
    return tr("Broken link", "断链中");
}
static COLORREF integrity_status_color(const DecisionSnapshot *snap) {
    if (snap->disconnected_components <= 0) return RGB(83, 143, 98);
    if (snap->longest_disconnected_months <= 0) return RGB(176, 137, 65);
    return RGB(180, 94, 74);
}
static void draw_integrity_card(HDC hdc, RECT card, const char *label, const char *value,
                                const char *detail, COLORREF accent) {
    fill_rect(hdc, card, ui_theme_color(UI_COLOR_PANEL_SOFT));
    fill_rect(hdc, (RECT){card.left, card.top, card.left + 3, card.bottom}, accent);
    draw_text_rect(hdc, (RECT){card.left + 8, card.top + 4, card.right - 8, card.top + 18},
                   label, ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, (RECT){card.left + 8, card.top + 18, card.right - 8, card.top + 37},
                   value, ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, (RECT){card.left + 8, card.top + 36, card.right - 8, card.bottom - 4},
                   detail ? detail : "", ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}
static void countdown_chip(HDC hdc, UiCursor *cursor, const char *label,
                           int months_value, int max_months) {
    RECT row = ui_take_rect(cursor, 27);
    int row_w = row.right - row.left;
    int label_w = row_w >= 330 ? 116 : 96;
    int time_w = row_w >= 330 ? 86 : 74;
    RECT label_rect = {row.left, row.top, row.left + label_w, row.bottom};
    RECT time_rect = {row.right - time_w, row.top, row.right, row.bottom};
    RECT bar = {label_rect.right + 8, row.top + 9, time_rect.left - 8, row.bottom - 8};
    int valid = months_value >= 0 && months_value <= 9000;
    int denom = max(max_months, 1);
    int progress = valid ? clamp(denom - min(months_value, denom), 0, denom) : 0;
    char text[56];
    draw_text_rect(hdc, label_rect, label,
                   ui_theme_color(UI_COLOR_TEXT_DIM), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    if (bar.right > bar.left + 16) {
        ui_progress_bar(hdc, bar, progress, denom,
                        valid ? RGB(104, 139, 172) : ui_theme_color(UI_COLOR_PANEL_LINE));
    }
    if (!valid) snprintf(text, sizeof(text), "%s", tr("None", "无"));
    else ui_format_months(text, sizeof(text), months_value, UI_MONTH_ZERO_NOW);
    draw_text_rect(hdc, time_rect, text,
                   ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_VCENTER | DT_RIGHT);
}
static const char *integrity_link_reason(const DecisionSnapshot *snap) {
    if (snap->disconnected_components <= 0) {
        if (snap->capital_core_network_count > 0) return tr("Linked", "完整联通");
        return tr("Linked", "完整联通");
    }
    if (!snap->disconnected_has_port) return tr("No port", "无港口");
    if (!snap->disconnected_has_network) return tr("Port offline", "港口未入网");
    if (!snap->disconnected_network_matches_capital) return tr("Route broken", "航道断链");
    return tr("Sea linked", "海路维持");
}
static const char *current_state_text(const DecisionSnapshot *snap, int reachable,
                                      int desire_ready, int cooldown_ready, int budget_ready) {
    const char *intent = snap->main_intent ? snap->main_intent : "";
    if (reachable <= 0) return tr("No target", "无目标");
    if (!desire_ready) return tr("Waiting", "等待");
    if (!cooldown_ready) return tr("Cooldown", "冷却中");
    if (!budget_ready) return tr("No budget", "预算不足");
    if (snapshot_ui_city_count() >= MAX_CITIES) return tr("No capacity", "容量不足");
    if (strcmp(intent, "War") == 0) return tr("At war", "战争中");
    if (strcmp(intent, "Stability") == 0) return tr("Stability", "稳定");
    return tr("Ready", "可执行");
}
static int has_real_expansion_result(const DecisionSnapshot *snap) {
    const char *reason = snap->expansion_reason ? snap->expansion_reason : "";
    return reason[0] && !strstr(reason, "No expansion decision");
}
static int has_last_result(const DecisionSnapshot *snap) {
    return has_real_expansion_result(snap) || snap->war_result != WAR_DESIRE_RESULT_NONE;
}
static const char *last_expansion_text(const DecisionSnapshot *snap) {
    const char *reason = snap->expansion_reason ? snap->expansion_reason : "";
    if (strstr(reason, "cooldown")) return tr("Cooldown", "冷却中");
    if (strstr(reason, "Claimed")) return tr("Expanded", "已扩张");
    if (strstr(reason, "city cap")) return tr("No capacity", "容量不足");
    if (strstr(reason, "No adjacent") || strstr(reason, "no adjacent")) return tr("No target", "无目标");
    if (strstr(reason, "blocked") || strstr(reason, "0 reachable")) return tr("Route broken", "航道断链");
    return tr("Waiting", "等待");
}
static const char *war_result_text(const DecisionSnapshot *snap);
static const char *last_result_text(const DecisionSnapshot *snap) {
    if (has_real_expansion_result(snap)) return last_expansion_text(snap);
    if (snap->war_result != WAR_DESIRE_RESULT_NONE) return war_result_text(snap);
    return NULL;
}
static void draw_integrity_dashboard(HDC hdc, UiCursor *cursor, const DecisionSnapshot *snap) {
    char value[64], detail[96], span[48];
    int cols = cursor->width >= 380 ? 4 : 2;
    int gap = 6;
    int card_h = 52;
    int rows = (4 + cols - 1) / cols;
    int card_w = (cursor->width - gap * (cols - 1)) / cols;
    RECT panel = ui_take_rect(cursor, 58 + rows * (card_h + gap));
    RECT status = {panel.left, panel.top, panel.right, panel.top + 50};
    COLORREF accent = integrity_status_color(snap);
    const int enclave_limit_months = 300;
    int i;
    fill_rect(hdc, status, ui_theme_color(UI_COLOR_PANEL));
    fill_rect(hdc, (RECT){status.left, status.top, status.left + 4, status.bottom}, accent);
    if (snap->disconnected_components > 0 && snap->longest_disconnected_months > 0) {
        ui_format_months(value, sizeof(value),
                         max(0, enclave_limit_months - snap->longest_disconnected_months),
                         UI_MONTH_ZERO_NOW);
        snprintf(detail, sizeof(detail), "%s %s", integrity_status_text(snap), value);
    } else {
        snprintf(detail, sizeof(detail), "%s", integrity_status_text(snap));
    }
    draw_text_rect(hdc, (RECT){status.left + 12, status.top + 6, status.right - 12, status.top + 28},
                   detail, accent, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, (RECT){status.left + 12, status.top + 28, status.right - 12, status.bottom - 5},
                   snap->disconnected_components <= 0 ? tr("No break", "无断链") :
                   snap->longest_disconnected_months > 0 ? tr("Longest break", "最长断联") :
                                                           tr("Waiting timer", "等待计时"),
                   ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    for (i = 0; i < 4; i++) {
        int col = i % cols;
        int row = i / cols;
        RECT card = {panel.left + col * (card_w + gap), panel.top + 58 + row * (card_h + gap),
                     panel.left + col * (card_w + gap) + card_w, panel.top + 58 + row * (card_h + gap) + card_h};
        if (i == 0) {
            snprintf(value, sizeof(value), "%d/%d %d%%", snap->capital_connected_regions,
                     snap->owned_regions, snap->capital_connected_percent);
            draw_integrity_card(hdc, card, tr("Core link", "核心连通"), value,
                                tr("core", "核心区"), RGB(83, 143, 98));
        } else if (i == 1) {
            snprintf(value, sizeof(value), "%d", snap->disconnected_components);
            draw_integrity_card(hdc, card, tr("Enclaves", "断联飞地"), value,
                                snap->disconnected_components > 0 ? integrity_link_reason(snap) : tr("none", "无"),
                                snap->disconnected_components > 0 ? RGB(180, 94, 74) : RGB(93, 134, 178));
        } else if (i == 2) {
            snprintf(value, sizeof(value), "%d/%d", snap->capital_core_network_count,
                     snap->capital_core_port_count);
            draw_integrity_card(hdc, card, tr("Port access", "港口接入"), value,
                                tr("Port access", "港口接入"), RGB(93, 134, 178));
        } else {
            if (snap->longest_disconnected_months > 0) {
                ui_format_months(span, sizeof(span), snap->longest_disconnected_months, UI_MONTH_ZERO_NOW);
                snprintf(value, sizeof(value), "%s", span);
            } else {
                snprintf(value, sizeof(value), "%s", tr("Fully linked", "完整联通"));
            }
            snprintf(detail, sizeof(detail), "%s", tr("longest break", "最长断联"));
            draw_integrity_card(hdc, card, tr("Longest", "最长断联"), value, detail, RGB(176, 137, 65));
        }
    }
    cursor->y += 8;
}
static void draw_decision_funnel_grid(HDC hdc, UiCursor *cursor, const DecisionSnapshot *snap,
                                      int reachable, int desire_ready, int cooldown_ready,
                                      int budget_ready, int capacity_ready) {
    char detail[6][96], span[48];
    const char *labels[6];
    FunnelStatus statuses[6];
    int outcome_ready = reachable > 0 && desire_ready && cooldown_ready && budget_ready && capacity_ready;
    int cols = cursor->width >= 380 ? 3 : 2;
    int gap = 6;
    int chip_h = 46;
    int rows = (6 + cols - 1) / cols;
    int chip_w = (cursor->width - gap * (cols - 1)) / cols;
    int i;
    labels[0] = tr("Reachable", "有可达地");
    labels[1] = tr("Desire", "意愿达标");
    labels[2] = tr("Cooldown", "冷却完成");
    labels[3] = tr("Budget", "预算可用");
    labels[4] = tr("Capacity", "容量可用");
    labels[5] = tr("Result", "结果");
    statuses[0] = reachable > 0 ? FUNNEL_PASS : FUNNEL_BLOCK;
    statuses[1] = desire_ready ? FUNNEL_PASS : FUNNEL_WAIT;
    statuses[2] = cooldown_ready ? FUNNEL_PASS : FUNNEL_WAIT;
    statuses[3] = budget_ready ? FUNNEL_PASS : FUNNEL_WAIT;
    statuses[4] = capacity_ready ? FUNNEL_PASS : FUNNEL_BLOCK;
    statuses[5] = outcome_ready ? FUNNEL_PASS : FUNNEL_BLOCK;
    if (snap->expansion_reason && strstr(snap->expansion_reason, "chance")) statuses[5] = FUNNEL_SKIP;
    snprintf(detail[0], sizeof(detail[0]), "%d%s", reachable, tr("signals", "信号"));
    snprintf(detail[1], sizeof(detail[1]), "%d / %d",
             snap->expansion.expansion_desire, snap->expansion.expansion_threshold);
    ui_format_months(span, sizeof(span), snap->expansion.months_until_next_claim, UI_MONTH_ZERO_NOW);
    snprintf(detail[2], sizeof(detail[2]), "%s", span);
    snprintf(detail[3], sizeof(detail[3]), "%d", snap->expansion.claim_budget);
    snprintf(detail[4], sizeof(detail[4]), "%s",
             capacity_ready ? tr("Ready", "可用") : tr("Blocked", "不足"));
    snprintf(detail[5], sizeof(detail[5]), "%s",
             current_state_text(snap, reachable, desire_ready, cooldown_ready, budget_ready));
    for (i = 0; i < 6; i++) {
        int col = i % cols;
        int row = i / cols;
        RECT chip = {cursor->x + col * (chip_w + gap), cursor->y + row * (chip_h + gap),
                     cursor->x + col * (chip_w + gap) + chip_w, cursor->y + row * (chip_h + gap) + chip_h};
        draw_gate_chip(hdc, chip, labels[i], statuses[i], detail[i]);
    }
    cursor->y += rows * (chip_h + gap) + 8;
}
static const char *war_result_text(const DecisionSnapshot *snap) {
    switch ((WarDesireResult)snap->war_result) {
        case WAR_DESIRE_RESULT_NO_FRONT: return tr("No target", "无目标");
        case WAR_DESIRE_RESULT_FRONTIER: return tr("Land first", "本土优先");
        case WAR_DESIRE_RESULT_TRUCE: return tr("Truce", "停战中");
        case WAR_DESIRE_RESULT_LOW_READINESS: return tr("Low troops", "兵力不足");
        case WAR_DESIRE_RESULT_STABILITY: return tr("Stability", "稳定闸门");
        case WAR_DESIRE_RESULT_READY: return tr("Ready", "可宣战");
        case WAR_DESIRE_RESULT_BELOW_THRESHOLD: return tr("Below threshold", "低于门槛");
        default: return tr("None", "无");
    }
}
static const char *war_result_short_text(const DecisionSnapshot *snap) {
    switch ((WarDesireResult)snap->war_result) {
        case WAR_DESIRE_RESULT_NO_FRONT: return tr("No target", "无目标");
        case WAR_DESIRE_RESULT_FRONTIER: return tr("Open land", "空地优先");
        case WAR_DESIRE_RESULT_TRUCE: return tr("Truce", "停战中");
        case WAR_DESIRE_RESULT_LOW_READINESS: return tr("Low troops", "兵力不足");
        case WAR_DESIRE_RESULT_STABILITY: return tr("Stability gate", "稳定闸门");
        case WAR_DESIRE_RESULT_READY: return tr("Can declare", "可以开战");
        case WAR_DESIRE_RESULT_BELOW_THRESHOLD: return tr("Below threshold", "低于门槛");
        default: return tr("No decision", "暂无");
    }
}
static void format_signed_value(char *out, size_t out_size, int value, int sign) {
    if (sign > 0) snprintf(out, out_size, "+%d", value);
    else if (sign < 0) snprintf(out, out_size, "-%d", value);
    else snprintf(out, out_size, "%d", value);
}
static void draw_desire_chip(HDC hdc, RECT rect, const char *label, const char *value,
                             COLORREF accent, int dimmed) {
    RECT stripe = rect;
    RECT label_rect = {rect.left + 8, rect.top + 3, rect.right - 6, rect.top + 15};
    RECT value_rect = {rect.left + 8, rect.top + 14, rect.right - 6, rect.bottom - 2};
    COLORREF text = dimmed ? ui_theme_color(UI_COLOR_TEXT_DIM) : ui_theme_color(UI_COLOR_TEXT);
    fill_rect(hdc, rect, dimmed ? RGB(31, 36, 38) : ui_theme_color(UI_COLOR_PANEL_SOFT));
    stripe.right = stripe.left + 3;
    fill_rect(hdc, stripe, accent);
    draw_text_rect(hdc, label_rect, label, ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, value_rect, value, text,
                   DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_END_ELLIPSIS);
}
static void desire_chip_signed(HDC hdc, RECT rect, const char *label, int value, int sign) {
    char text[24];
    COLORREF accent = sign > 0 ? RGB(92, 145, 88) : RGB(154, 78, 68);
    format_signed_value(text, sizeof(text), value, sign);
    draw_desire_chip(hdc, rect, label, text, accent, value == 0);
}
static void desire_chip_plain(HDC hdc, RECT rect, const char *label, const char *value,
                              COLORREF accent, int dimmed) {
    draw_desire_chip(hdc, rect, label, value, accent, dimmed);
}
static void draw_war_desire_breakdown(HDC hdc, UiCursor *cursor, const DecisionSnapshot *snap) {
    char text[128], own[32], enemy[32], ratio[24], cap_text[24];
    int threshold = snap->war_threshold > 0 ? snap->war_threshold : 70;
    int cap = snap->war_readiness_cap > 0 ? snap->war_readiness_cap : 100;
    int cols = cursor->width >= 360 ? 4 : 3;
    int gap = 6;
    int chip_h = 28;
    int rows = (13 + cols - 1) / cols;
    int card_h = 72 + rows * (chip_h + gap) + 4;
    RECT card = ui_take_rect(cursor, card_h);
    RECT inner = {card.left + 10, card.top + 8, card.right - 10, card.bottom - 8};
    RECT title_rect = {inner.left, inner.top, inner.right - 86, inner.top + 22};
    RECT score_rect = {inner.right - 82, inner.top, inner.right, inner.top + 22};
    RECT bar = {inner.left, inner.top + 27, inner.right, inner.top + 38};
    RECT result_rect = {inner.left, inner.top + 42, inner.right, inner.top + 62};
    int grid_y = inner.top + 66;
    int chip_w = (inner.right - inner.left - gap * (cols - 1)) / cols;
    int i = 0;
    fill_rect(hdc, card, ui_theme_color(UI_COLOR_PANEL));
    fill_rect(hdc, (RECT){card.left, card.top, card.left + 3, card.bottom},
              snap->war_desire >= threshold ? RGB(169, 83, 72) : RGB(96, 105, 103));
    draw_text_rect(hdc, title_rect, tr("Declaration Desire", "宣战意愿"),
                   ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    snprintf(text, sizeof(text), "%d / %d", snap->war_desire, threshold);
    draw_text_rect(hdc, score_rect, text, ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_END_ELLIPSIS);
    ui_progress_bar(hdc, bar, snap->war_desire, threshold,
                    snap->war_desire >= threshold ? RGB(174, 84, 70) : RGB(103, 121, 111));
    draw_text_rect(hdc, result_rect, war_result_short_text(snap),
                   ui_theme_color(UI_COLOR_TEXT_MUTED), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    #define CHIP_RECT() (RECT){inner.left + (i % cols) * (chip_w + gap), \
        grid_y + (i / cols) * (chip_h + gap), \
        inner.left + (i % cols) * (chip_w + gap) + chip_w, \
        grid_y + (i / cols) * (chip_h + gap) + chip_h}
    snprintf(text, sizeof(text), "%d", snap->war_pre_stability_desire);
    desire_chip_plain(hdc, CHIP_RECT(), tr("Raw", "原始"), text, RGB(176, 137, 65), snap->war_raw_desire == 0); i++;
    desire_chip_signed(hdc, CHIP_RECT(), tr("Agg", "侵略"), snap->war_aggression_score, 1); i++;
    desire_chip_signed(hdc, CHIP_RECT(), tr("Border", "边界"), snap->war_border_score, 1); i++;
    desire_chip_signed(hdc, CHIP_RECT(), tr("Res", "资源"), snap->war_resource_score, 1); i++;
    desire_chip_signed(hdc, CHIP_RECT(), tr("Power", "兵力"), snap->war_strength_score, 1); i++;
    desire_chip_signed(hdc, CHIP_RECT(), tr("Trade", "贸易"), snap->war_trade_penalty, -1); i++;
    desire_chip_signed(hdc, CHIP_RECT(), tr("Truce", "停战"), snap->war_truce_penalty, -1); i++;
    desire_chip_signed(hdc, CHIP_RECT(), tr("Disorder", "混乱"), snap->war_disorder_penalty, -1); i++;
    desire_chip_signed(hdc, CHIP_RECT(), tr("Open", "空地"), snap->war_frontier_penalty, -1); i++;
    desire_chip_signed(hdc, CHIP_RECT(), tr("Heritage", "文明"), snap->war_heritage_affinity_penalty, -1); i++;
    desire_chip_signed(hdc, CHIP_RECT(), tr("Stability", "稳定"), snap->war_stability_penalty, -1); i++;
    format_metric_value(snap->war_own_soldiers, own, sizeof(own));
    format_metric_value(snap->war_enemy_soldiers, enemy, sizeof(enemy));
    snprintf(ratio, sizeof(ratio), "%d%%", snap->war_readiness_percent);
    desire_chip_plain(hdc, CHIP_RECT(), tr("Ratio", "兵力比"), ratio, RGB(181, 144, 67),
                      snap->war_readiness_percent <= 0); i++;
    if (snap->war_readiness_cap_applied) snprintf(cap_text, sizeof(cap_text), "%d", cap);
    else snprintf(cap_text, sizeof(cap_text), "%s", "-");
    desire_chip_plain(hdc, CHIP_RECT(), tr("Cap", "上限"), cap_text, RGB(190, 151, 67),
                      !snap->war_readiness_cap_applied); i++;
    #undef CHIP_RECT
    cursor->y += 8;
}
int country_decision_tab_height(int civ_id) {
    (void)civ_id;
    return 860;
}
void draw_country_decision_tab(HDC hdc, UiCursor *cursor, int civ_id) {
    const SnapshotCiv *civ = snapshot_ui_civ(civ_id);
    DecisionSnapshot snap = civ ? civ->decision : (DecisionSnapshot){0};
    char text[256];
    int reachable;
    int desire_ready;
    int cooldown_ready;
    int budget_ready;
    int capacity_ready;
    reachable = snap.expansion.land_adjacent_unowned_regions + snap.expansion.land_nearby_unowned_regions +
                snap.expansion.shallow_sea_reachable_regions + snap.expansion.maritime_reachable_regions +
                snap.expansion.deep_sea_reachable_regions;
    desire_ready = snap.expansion.expansion_desire >= snap.expansion.expansion_threshold;
    cooldown_ready = snap.expansion.months_until_next_claim <= 0;
    budget_ready = snap.expansion.claim_budget > 0;
    capacity_ready = snapshot_ui_city_count() < MAX_CITIES;
    draw_country_decision_subtabs(hdc, cursor);
    if (country_decision_subtab == COUNTRY_DECISION_STABILITY) {
        draw_country_decision_stability_tab(hdc, cursor, &snap);
        return;
    }
    if (country_decision_subtab == COUNTRY_DECISION_WAR) {
        draw_war_desire_breakdown(hdc, cursor, &snap);
        draw_country_decision_war_stability_summary(hdc, cursor, &snap);
    } else if (country_decision_subtab == COUNTRY_DECISION_OVERVIEW) {
        ui_section(hdc, cursor, tr("Decision", "决策"));
        ui_row_text(hdc, cursor, tr("Stability", "稳定"),
                    country_decision_stability_mode_label(snap.stability_mode));
        ui_row_text(hdc, cursor, tr("Current State", "当前状态"),
                    current_state_text(&snap, reachable, desire_ready, cooldown_ready, budget_ready));
        ui_row_text(hdc, cursor, tr("Main Intent", "主要意图"), intent_label(snap.main_intent));
        draw_weight_bar(hdc, cursor, tr("Expansion", "扩张"), snap.expansion_weight, RGB(116, 158, 91));
        draw_weight_bar(hdc, cursor, tr("War", "战争"), snap.war_weight, RGB(180, 84, 74));
        draw_weight_bar(hdc, cursor, tr("Stability", "稳定"), snap.stability_weight, RGB(92, 130, 162));
    }
    if (country_decision_subtab == COUNTRY_DECISION_EXPANSION) {
        ui_section(hdc, cursor, tr("Reachability", "可达性"));
        draw_reachability_grid(hdc, cursor, &snap);
        snprintf(text, sizeof(text), tr("ports %d / candidates %d / shallow path %d / fail %d",
                                        "港口 %d / 候选 %d / 浅海路径 %d / 失败 %d"),
                 snap.expansion.maritime.own_port_count, snap.expansion.port_candidate_regions,
                 snap.expansion.shallow_sea_reachable_regions, snap.expansion.maritime.blocked_no_shallow_path);
        ui_row_text(hdc, cursor, tr("Sea targets", "海路目标"), text);
        format_maritime_blockers(&snap.expansion.maritime, text, sizeof(text));
        ui_row_text(hdc, cursor, tr("Main sea blocker", "主要海路阻塞"), text);
        ui_section(hdc, cursor, tr("Territory Integrity", "领土完整性"));
        draw_integrity_dashboard(hdc, cursor, &snap);
    } else if (country_decision_subtab == COUNTRY_DECISION_OVERVIEW) {
        ui_section(hdc, cursor, tr("Decision Funnel", "决策漏斗"));
        draw_decision_funnel_grid(hdc, cursor, &snap, reachable, desire_ready,
                                  cooldown_ready, budget_ready, capacity_ready);
        ui_section(hdc, cursor, tr("Countdowns", "倒计时"));
        countdown_chip(hdc, cursor, tr("Expansion attempt", "扩张尝试"), snap.next_expansion_months, 36);
        countdown_chip(hdc, cursor, tr("Diplomacy eval", "外交评估"), snap.next_diplomacy_months, 12);
        countdown_chip(hdc, cursor, tr("Next battle", "下次战斗"), snap.next_battle_months, 36);
        countdown_chip(hdc, cursor, tr("Collapse check", "崩溃判定"), snap.next_collapse_years * 12, 300);
    }
    if (country_decision_subtab != COUNTRY_DECISION_EXPANSION && has_last_result(&snap)) {
        const char *last = last_result_text(&snap);
        if (last && last[0]) {
            ui_section(hdc, cursor, tr("Last Result", "上次结果"));
            ui_row_text(hdc, cursor, tr("Last", "上次"), last);
        }
    }
}
