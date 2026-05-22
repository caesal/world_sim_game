#include "render/panel_country_decision.h"

#include "render/render_common.h"
#include "render/ui_format.h"
#include "ui/ui_types.h"
#include "ui/ui_widgets.h"

#include <stdio.h>

static const char *decision_subtab_label(int tab) {
    switch (tab) {
        case COUNTRY_DECISION_EXPANSION: return tr("Expansion", "扩张");
        case COUNTRY_DECISION_WAR: return tr("War", "战争");
        case COUNTRY_DECISION_STABILITY: return tr("Stability", "稳定");
        default: return tr("Overview", "总览");
    }
}

const char *country_decision_stability_mode_label(int mode) {
    switch ((StabilityMode)mode) {
        case STABILITY_MODE_CAUTIOUS: return tr("Cautious", "谨慎");
        case STABILITY_MODE_REORGANIZING: return tr("Reorganizing", "整顿");
        case STABILITY_MODE_CRISIS: return tr("Crisis", "危机");
        case STABILITY_MODE_EMERGENCY: return tr("Emergency", "紧急");
        case STABILITY_MODE_COLLAPSE: return tr("Collapse", "崩溃");
        default: return tr("Normal", "正常");
    }
}

void draw_country_decision_subtabs(HDC hdc, UiCursor *cursor) {
    int i;
    int gap = 4;
    int tab_w = (cursor->width - gap * (COUNTRY_DECISION_SUBTAB_COUNT - 1)) / COUNTRY_DECISION_SUBTAB_COUNT;
    RECT row = ui_take_rect(cursor, 28);
    for (i = 0; i < COUNTRY_DECISION_SUBTAB_COUNT; i++) {
        RECT r = {row.left + i * (tab_w + gap), row.top,
                  i == COUNTRY_DECISION_SUBTAB_COUNT - 1 ? row.right :
                  row.left + i * (tab_w + gap) + tab_w, row.bottom - 2};
        int active = i == country_decision_subtab;
        fill_rect(hdc, r, active ? RGB(87, 93, 78) : RGB(43, 49, 52));
        draw_center_text(hdc, r, decision_subtab_label(i),
                         active ? RGB(255, 238, 190) : ui_theme_color(UI_COLOR_TEXT));
    }
    cursor->y += 6;
}

int country_decision_subtab_hit_test(RECT viewport, int scroll, int mouse_x, int mouse_y) {
    int i;
    int gap = 4;
    int width = viewport.right - viewport.left - 8;
    int tab_w = (width - gap * (COUNTRY_DECISION_SUBTAB_COUNT - 1)) / COUNTRY_DECISION_SUBTAB_COUNT;
    int y = viewport.top - scroll;
    if (mouse_y < y || mouse_y > y + 26) return -1;
    for (i = 0; i < COUNTRY_DECISION_SUBTAB_COUNT; i++) {
        RECT r = {viewport.left + i * (tab_w + gap), y,
                  i == COUNTRY_DECISION_SUBTAB_COUNT - 1 ? viewport.left + width :
                  viewport.left + i * (tab_w + gap) + tab_w, y + 26};
        if (point_in_rect_local(r, mouse_x, mouse_y)) return i;
    }
    return -1;
}

static COLORREF stability_mode_color(int mode) {
    switch ((StabilityMode)mode) {
        case STABILITY_MODE_CAUTIOUS: return RGB(178, 151, 78);
        case STABILITY_MODE_REORGANIZING: return RGB(191, 124, 65);
        case STABILITY_MODE_CRISIS: return RGB(188, 82, 67);
        case STABILITY_MODE_EMERGENCY:
        case STABILITY_MODE_COLLAPSE: return RGB(164, 54, 64);
        default: return RGB(83, 143, 98);
    }
}

static void badge(HDC hdc, RECT r, const char *text, COLORREF color) {
    fill_rect(hdc, r, color);
    draw_center_text(hdc, r, text, RGB(248, 246, 235));
}

static void restriction_card(HDC hdc, RECT r, const char *label, const char *value, COLORREF accent) {
    fill_rect(hdc, r, ui_theme_color(UI_COLOR_PANEL_SOFT));
    fill_rect(hdc, (RECT){r.left, r.top, r.left + 3, r.bottom}, accent);
    draw_text_rect(hdc, (RECT){r.left + 8, r.top + 4, r.right - 6, r.top + 20},
                   label, ui_theme_color(UI_COLOR_TEXT_DIM), DT_SINGLELINE | DT_VCENTER);
    draw_text_rect(hdc, (RECT){r.left + 8, r.top + 21, r.right - 6, r.bottom - 4},
                   value, ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_VCENTER);
}

static void stability_meter(HDC hdc, UiCursor *cursor, const DecisionSnapshot *snap) {
    RECT card = ui_take_rect(cursor, 74);
    RECT bar = {card.left + 10, card.top + 34, card.right - 10, card.top + 48};
    RECT mode_badge = {card.right - 92, card.top + 8, card.right - 10, card.top + 28};
    int ranges[6] = {45, 15, 15, 15, 10, 1};
    COLORREF colors[6] = {RGB(83, 143, 98), RGB(178, 151, 78), RGB(191, 124, 65),
                          RGB(188, 82, 67), RGB(164, 54, 64), RGB(112, 45, 52)};
    const char *labels[6] = {tr("Normal", "正常"), tr("Cautious", "谨慎"), tr("Reorg", "整顿"),
                             tr("Crisis", "危机"), tr("Emergency", "紧急"), tr("Collapse", "崩溃")};
    int x = bar.left, i;
    int marker_x = bar.left + clamp(snap->stability_pressure, 0, 100) * (bar.right - bar.left) / 100;
    char text[48];
    fill_rect(hdc, card, ui_theme_color(UI_COLOR_PANEL));
    snprintf(text, sizeof(text), "%s %d / 100", tr("Disorder", "混乱"), snap->stability_pressure);
    draw_text_rect(hdc, (RECT){card.left + 10, card.top + 7, mode_badge.left - 8, card.top + 29},
                   text, ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_VCENTER);
    badge(hdc, mode_badge, country_decision_stability_mode_label(snap->stability_mode),
          stability_mode_color(snap->stability_mode));
    for (i = 0; i < 6; i++) {
        int w = max(2, ranges[i] * (bar.right - bar.left) / 101);
        RECT seg = {x, bar.top, i == 5 ? bar.right : x + w, bar.bottom};
        fill_rect(hdc, seg, colors[i]);
        if (seg.right - seg.left >= 38) {
            draw_center_text(hdc, (RECT){seg.left, bar.bottom + 3, seg.right, bar.bottom + 18},
                             labels[i], ui_theme_color(UI_COLOR_TEXT_DIM));
        }
        x = seg.right;
    }
    fill_rect(hdc, (RECT){marker_x - 2, bar.top - 5, marker_x + 2, bar.bottom + 5}, RGB(250, 244, 205));
    cursor->y += 8;
}

static void exit_condition(HDC hdc, UiCursor *cursor, const DecisionSnapshot *snap) {
    char text[96], span[48];
    if (snap->stability_mode == STABILITY_MODE_NORMAL) {
        ui_row_text(hdc, cursor, tr("Exit", "退出条件"), tr("Already normal", "已正常"));
        return;
    }
    ui_format_months(span, sizeof(span), snap->stability_recovery_months, UI_MONTH_ZERO_NOW);
    snprintf(text, sizeof(text), tr("Below threshold for %s", "低于阈值持续%s"), span);
    ui_row_text(hdc, cursor, tr("Exit", "退出条件"), text);
}

static void restriction_grid(HDC hdc, UiCursor *cursor, const DecisionSnapshot *snap) {
    int mode = snap->stability_mode;
    int gap = 6, cols = cursor->width >= 360 ? 4 : 2;
    int card_w = (cursor->width - gap * (cols - 1)) / cols;
    int card_h = 44;
    RECT area = ui_take_rect(cursor, cols == 4 ? 48 : 96);
    const char *war = mode >= STABILITY_MODE_REORGANIZING ? tr("Blocked", "禁止") :
                      mode == STABILITY_MODE_CAUTIOUS ? tr("Core only", "核心连接") : tr("Normal", "正常");
    const char *expand = mode >= STABILITY_MODE_CRISIS ? tr("Paused", "暂停") :
                         mode >= STABILITY_MODE_REORGANIZING ? tr("Core only", "仅核心") :
                         mode == STABILITY_MODE_CAUTIOUS ? tr("Reduced", "降低") : tr("Normal", "正常");
    const char *peace = snap->stability_peace_bonus > 0 ? tr("Higher", "提高") : tr("Normal", "正常");
    const char *self = mode >= STABILITY_MODE_EMERGENCY ? tr("Stopped", "停止") :
                       mode >= STABILITY_MODE_CRISIS ? tr("Defensive", "防御") : tr("Normal", "正常");
    const char *labels[4] = {tr("War limit", "宣战限制"), tr("Expansion", "扩张限制"),
                             tr("Peace", "议和倾向"), tr("Actions", "自发行为")};
    const char *values[4] = {war, expand, peace, self};
    int i;
    for (i = 0; i < 4; i++) {
        int col = i % cols, row = i / cols;
        RECT r = {area.left + col * (card_w + gap), area.top + row * (card_h + gap),
                  area.left + col * (card_w + gap) + card_w, area.top + row * (card_h + gap) + card_h};
        restriction_card(hdc, r, labels[i], values[i], stability_mode_color(mode));
    }
    cursor->y += 8;
}

static void gate_flow(HDC hdc, UiCursor *cursor, const char *label,
                      int raw, int penalty, int final, COLORREF accent) {
    RECT row = ui_take_rect(cursor, 46);
    RECT title = {row.left, row.top, row.left + 86, row.bottom};
    RECT a = {row.left + 92, row.top + 7, row.left + 148, row.bottom - 7};
    RECT b = {a.right + 8, a.top, a.right + 64, a.bottom};
    RECT c = {b.right + 8, a.top, row.right, a.bottom};
    char text[32];
    draw_text_rect(hdc, title, label, ui_theme_color(UI_COLOR_TEXT_DIM), DT_SINGLELINE | DT_VCENTER);
    snprintf(text, sizeof(text), "%d", raw);
    badge(hdc, a, text, RGB(84, 103, 128));
    snprintf(text, sizeof(text), "-%d", penalty);
    badge(hdc, b, text, penalty > 0 ? RGB(167, 88, 72) : RGB(92, 116, 88));
    snprintf(text, sizeof(text), "%d", final);
    badge(hdc, c, text, accent);
}

void draw_country_decision_stability_tab(HDC hdc, UiCursor *cursor, const DecisionSnapshot *snap) {
    char span[48], text[48];
    ui_section(hdc, cursor, tr("Stability", "稳定"));
    stability_meter(hdc, cursor, snap);
    exit_condition(hdc, cursor, snap);
    ui_format_months(span, sizeof(span), snap->stability_mode_months, UI_MONTH_ZERO_NOW);
    ui_row_text(hdc, cursor, tr("Duration", "持续"), span);
    ui_section(hdc, cursor, tr("Active Limits", "启用限制"));
    restriction_grid(hdc, cursor, snap);
    ui_section(hdc, cursor, tr("Gate Result", "闸门结果"));
    gate_flow(hdc, cursor, tr("War", "战争"), snap->war_pre_stability_desire,
              snap->war_stability_penalty, snap->war_raw_desire, RGB(172, 86, 72));
    gate_flow(hdc, cursor, tr("Expansion", "扩张"), snap->expansion.raw_expansion_desire,
              snap->expansion.stability_expansion_penalty, snap->expansion.expansion_desire,
              RGB(103, 148, 91));
    snprintf(text, sizeof(text), "%+d", snap->stability_peace_bonus);
    ui_row_text(hdc, cursor, tr("Peace pressure", "议和压力"), text);
}

static const char *war_gate_badge_text(const DecisionSnapshot *snap) {
    if (!snap->stability_allows_war) return tr("War blocked", "禁止宣战");
    if (snap->war_stability_penalty > 0) return tr("Limited war", "限制宣战");
    if (snap->stability_mode == STABILITY_MODE_CAUTIOUS) return tr("Core only", "只许核心连接");
    return tr("War allowed", "允许宣战");
}

void draw_country_decision_war_stability_summary(HDC hdc, UiCursor *cursor,
                                                 const DecisionSnapshot *snap) {
    int gap = 6;
    int card_w = (cursor->width - gap * 3) / 4;
    RECT area;
    ui_section(hdc, cursor, tr("Stability Gate", "稳定阀门"));
    gate_flow(hdc, cursor, tr("War desire", "战争意愿"), snap->war_pre_stability_desire,
              snap->war_stability_penalty, snap->war_raw_desire, RGB(172, 86, 72));
    area = ui_take_rect(cursor, 30);
    badge(hdc, (RECT){area.left, area.top + 3, area.right, area.bottom - 3},
          war_gate_badge_text(snap), stability_mode_color(snap->stability_mode));
    area = ui_take_rect(cursor, 48);
    restriction_card(hdc, (RECT){area.left, area.top, area.left + card_w, area.bottom - 4},
                     tr("Disorder", "混乱等级"),
                     country_decision_stability_mode_label(snap->stability_mode),
                     stability_mode_color(snap->stability_mode));
    restriction_card(hdc, (RECT){area.left + card_w + gap, area.top,
                     area.left + (card_w + gap) * 2 - gap, area.bottom - 4},
                     tr("Fronts", "已有战线"),
                     snap->stability_peace_bonus > 0 ? tr("Pressure", "求和") : tr("Normal", "正常"),
                     RGB(176, 137, 65));
    restriction_card(hdc, (RECT){area.left + (card_w + gap) * 2, area.top,
                     area.left + (card_w + gap) * 3 - gap, area.bottom - 4},
                     tr("Core link", "核心连接"),
                     snap->stability_mode == STABILITY_MODE_CAUTIOUS ? tr("Required", "需要") : tr("Any", "不限"),
                     RGB(93, 134, 178));
    restriction_card(hdc, (RECT){area.left + (card_w + gap) * 3, area.top, area.right, area.bottom - 4},
                     tr("Crisis", "危机限制"),
                     snap->stability_mode >= STABILITY_MODE_CRISIS ? tr("Active", "启用") : tr("Off", "未启用"),
                     RGB(188, 82, 67));
}
