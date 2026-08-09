#include "render/panel_country_decision.h"

#include "render/render_common.h"
#include "render/ui_format.h"
#include "ui/ui_clay_primitives.h"
#include "ui/ui_clay_theme.h"
#include "ui/ui_types.h"
#include "ui/ui_widgets.h"

#include <stdio.h>

#define STABILITY_INTENT_H 62
#define STABILITY_STATUS_H 30
#define STABILITY_COMPOSITION_H 31
#define STABILITY_FACTOR_ROW_H 50
#define STABILITY_TOTAL_H 36
#define STABILITY_FACTOR_ACCENT_W 3

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

static COLORREF stability_intent_accent(void) {
    return RGB(92, 130, 162);
}

void country_decision_stability_layout(
    int x, int y, int width, CountryDecisionStabilityLayout *layout) {
    int split;
    int matrix_top;
    int i;
    if (!layout) return;
    split = x + width / 2;
    layout->intent = (RECT){x, y, x + width, y + STABILITY_INTENT_H};
    y = layout->intent.bottom;
    layout->status = (RECT){x, y, x + width, y + STABILITY_STATUS_H};
    y = layout->status.bottom;
    layout->composition =
        (RECT){x, y, x + width, y + STABILITY_COMPOSITION_H};
    matrix_top = layout->composition.bottom;
    for (i = 0; i < 6; i++) {
        int col = i % 2;
        int row = i / 2;
        RECT cell = {
            col ? split : x,
            matrix_top + row * STABILITY_FACTOR_ROW_H,
            col ? x + width : split,
            matrix_top + (row + 1) * STABILITY_FACTOR_ROW_H
        };
        layout->factors[i] = cell;
        layout->factor_accents[i] = (RECT){
            cell.left, cell.top, cell.left + STABILITY_FACTOR_ACCENT_W,
            cell.bottom
        };
        layout->factor_values[i] = (RECT){
            cell.right - 60, cell.top + 2, cell.right - 7, cell.bottom - 2
        };
        layout->factor_labels[i] = (RECT){
            cell.left + 9, cell.top + 2, layout->factor_values[i].left - 4,
            cell.bottom - 2
        };
    }
    y = matrix_top + STABILITY_FACTOR_ROW_H * 3;
    layout->total = (RECT){x, y, x + width, y + STABILITY_TOTAL_H};
    layout->total_label = (RECT){x + 4, y + 2, split, layout->total.bottom - 2};
    layout->total_value =
        (RECT){split, y + 2, x + width - 4, layout->total.bottom - 2};
    layout->bottom = layout->total.bottom;
}

static void draw_stability_intent_meter(HDC hdc, RECT area,
                                        const DecisionSnapshot *snap) {
    RECT label = {area.left + 2, area.top + 2, area.right - 92, area.top + 25};
    RECT value = {area.right - 90, area.top + 2, area.right - 2, area.top + 25};
    RECT bar = {area.left + 2, area.top + 31, area.right - 2, area.top + 49};
    char text[32];

    snprintf(text, sizeof(text), "%d / 100", snap->stability_weight);
    draw_text_rect(hdc, label, tr("Stability Intent", "稳定倾向"),
                   ui_clay_text_color(UI_CLAY_STATE_NORMAL),
                   DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    draw_text_rect(hdc, value, text, ui_clay_text_color(UI_CLAY_STATE_NORMAL),
                   DT_SINGLELINE | DT_RIGHT | DT_VCENTER | DT_NOPREFIX);
    ui_progress_bar(hdc, bar, snap->stability_weight, 100,
                    stability_intent_accent());
}

static void draw_stability_compact_status(HDC hdc, RECT area,
                                          const DecisionSnapshot *snap) {
    char duration[48];
    char text[192];
    const char *mode = country_decision_stability_mode_label(snap->stability_mode);

    ui_format_months(duration, sizeof(duration), snap->stability_mode_months,
                     UI_MONTH_ZERO_NOW);
    snprintf(text, sizeof(text),
             tr("Stability Mode: %s    Duration: %s",
                "稳定模式：%s    持续：%s"),
             mode, duration);
    draw_text_rect(hdc, (RECT){area.left + 2, area.top,
                               area.right - 2, area.bottom},
                   text, ui_clay_muted_text_color(),
                   DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
}

static void format_stability_factor(char *out, size_t out_size, int value) {
    if (value > 0) snprintf(out, out_size, "+%d", value);
    else snprintf(out, out_size, "%d", value);
}

static void draw_stability_factor_label(HDC hdc, RECT rect,
                                        const char *label) {
    WCHAR wide[128];
    RECT measured = {0, 0, rect.right - rect.left, 0};
    int length = MultiByteToWideChar(
        CP_UTF8, 0, label, -1, wide,
        (int)(sizeof(wide) / sizeof(wide[0])));
    if (length <= 0) return;
    DrawTextW(hdc, wide, -1, &measured,
              DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    if (measured.bottom < rect.bottom - rect.top) {
        rect.top += ((rect.bottom - rect.top) - measured.bottom) / 2;
    }
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, ui_clay_text_color(UI_CLAY_STATE_NORMAL));
    DrawTextW(hdc, wide, -1, &rect, DT_WORDBREAK | DT_NOPREFIX);
}

static void draw_stability_factor_cell(
    HDC hdc, const CountryDecisionStabilityLayout *layout, int index,
    const char *label, int value) {
    COLORREF value_color = value > 0 ? RGB(104, 184, 224) :
                           value < 0 ? ui_theme_color(UI_COLOR_DANGER) :
                           ui_clay_muted_text_color();
    char text[32];

    fill_rect(hdc, layout->factors[index],
              ui_theme_color(UI_COLOR_PANEL_SOFT));
    fill_rect(hdc, layout->factor_accents[index], stability_intent_accent());
    draw_stability_factor_label(hdc, layout->factor_labels[index], label);
    format_stability_factor(text, sizeof(text), value);
    draw_text_rect(hdc, layout->factor_values[index], text, value_color,
                   DT_SINGLELINE | DT_RIGHT | DT_VCENTER | DT_NOPREFIX);
}

static void draw_stability_factor_grid(
    HDC hdc, const CountryDecisionStabilityLayout *layout,
                                       const DecisionSnapshot *snap) {
    const DecisionStabilityBreakdown *breakdown = &snap->stability_breakdown;
    const char *labels[6] = {
        tr("Base Pressure", "基础压力"), tr("War Status", "战争状态"),
        tr("Territory Fragmentation", "领土断裂"),
        tr("Capital Connectivity", "首都连通"),
        tr("Vassal Governance", "附庸治理"), tr("High Disorder", "高混乱")
    };
    int values[6] = {
        breakdown->base_contribution, breakdown->war_status_contribution,
        breakdown->territory_fragmentation_contribution,
        breakdown->capital_connectivity_contribution,
        breakdown->vassal_governance_contribution,
        breakdown->high_disorder_contribution
    };
    int i;

    for (i = 0; i < 6; i++) {
        draw_stability_factor_cell(hdc, layout, i, labels[i], values[i]);
    }
    fill_rect(hdc, (RECT){layout->factors[0].right - 1,
                          layout->factors[0].top,
                          layout->factors[0].right,
                          layout->factors[5].bottom},
              ui_theme_color(UI_COLOR_PANEL_LINE));
    fill_rect(hdc, (RECT){layout->factors[0].left,
                          layout->factors[2].top,
                          layout->factors[1].right,
                          layout->factors[2].top + 1},
              ui_theme_color(UI_COLOR_PANEL_LINE));
    fill_rect(hdc, (RECT){layout->factors[0].left,
                          layout->factors[4].top,
                          layout->factors[1].right,
                          layout->factors[4].top + 1},
              ui_theme_color(UI_COLOR_PANEL_LINE));
}

static void draw_stability_monthly_total(
    HDC hdc, const CountryDecisionStabilityLayout *layout,
    const DecisionSnapshot *snap) {
    const DecisionStabilityBreakdown *breakdown = &snap->stability_breakdown;
    char value[96];

    if (breakdown->raw_total == breakdown->final_intent) {
        snprintf(value, sizeof(value), "%d", breakdown->final_intent);
    } else {
        snprintf(value, sizeof(value), "%d → %d",
                 breakdown->raw_total, breakdown->final_intent);
    }
    fill_rect(hdc, layout->total, ui_theme_color(UI_COLOR_PANEL_SOFT));
    fill_rect(hdc, (RECT){layout->total.left, layout->total.top,
                          layout->total.right, layout->total.top + 1},
              ui_theme_color(UI_COLOR_PANEL_LINE));
    draw_text_rect(hdc, layout->total_label,
                   tr("Monthly Total", "本月合计"),
                   ui_clay_text_color(UI_CLAY_STATE_NORMAL),
                   DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    draw_text_rect(hdc, layout->total_value,
                   value, ui_clay_text_color(UI_CLAY_STATE_NORMAL),
                   DT_SINGLELINE | DT_RIGHT | DT_VCENTER | DT_NOPREFIX);
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
    CountryDecisionStabilityLayout layout;
    UiCursor section;
    char text[96];
    country_decision_stability_layout(
        cursor->x, cursor->y, cursor->width, &layout);
    draw_stability_intent_meter(hdc, layout.intent, snap);
    draw_stability_compact_status(hdc, layout.status, snap);
    section = ui_cursor(layout.composition.left, layout.composition.top,
                        cursor->width, layout.composition.bottom);
    ui_section(hdc, &section, tr("Composition", "倾向构成"));
    draw_stability_factor_grid(hdc, &layout, snap);
    draw_stability_monthly_total(hdc, &layout, snap);
    cursor->y = layout.bottom;
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
