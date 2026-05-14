#include "render/panel_country_diplomacy_cards.h"

#include "render/ui_format.h"
#include "sim/diplomacy.h"
#include "sim/simulation.h"
#include "sim/vassal.h"
#include "sim/war.h"
#include "sim/war_front.h"

#include <stdio.h>

static const char *level_label(int value) {
    if (value >= 67) return tr("High", "高");
    if (value >= 34) return tr("Medium", "中");
    return tr("Low", "低");
}

static const char *status_label(int civ_id, int other_id, DiplomacyRelation relation) {
    int own_overlord = vassal_overlord(civ_id);
    int other_overlord = vassal_overlord(other_id);
    if (vassal_is_direct(civ_id, other_id)) return tr("Vassal", "附庸");
    if (vassal_is_direct(other_id, civ_id)) return tr("Tribute", "朝贡");
    if (own_overlord >= 0 && other_id != own_overlord) return tr("No autonomy", "无自主权");
    if (other_overlord >= 0 && civ_id != other_overlord) return tr("No autonomy", "无自主权");
    switch (relation.state) {
        case DIPLOMACY_PEACE: return tr("Peace", "和平");
        case DIPLOMACY_TENSE: return tr("Tense", "紧张");
        case DIPLOMACY_TRUCE: return tr("Truce", "停战");
        case DIPLOMACY_WAR: return tr("War", "战争");
        case DIPLOMACY_VASSAL: return tr("Vassal", "附庸");
        default: return tr("Unknown", "未知");
    }
}

static COLORREF status_color(int civ_id, int other_id, DiplomacyRelation relation) {
    if (vassal_is_direct(civ_id, other_id) || vassal_is_direct(other_id, civ_id)) return RGB(154, 105, 178);
    if ((vassal_overlord(civ_id) >= 0 && other_id != vassal_overlord(civ_id)) ||
        (vassal_overlord(other_id) >= 0 && civ_id != vassal_overlord(other_id))) return RGB(104, 112, 120);
    switch (relation.state) {
        case DIPLOMACY_PEACE: return RGB(86, 150, 96);
        case DIPLOMACY_TENSE: return RGB(196, 154, 72);
        case DIPLOMACY_TRUCE: return RGB(196, 172, 92);
        case DIPLOMACY_WAR: return RGB(176, 72, 62);
        case DIPLOMACY_VASSAL: return RGB(154, 105, 178);
        default: return RGB(104, 112, 120);
    }
}

static void draw_header(HDC hdc, UiCursor *cursor, int civ_id, int other_id, DiplomacyRelation relation) {
    RECT row = ui_take_rect(cursor, 28);
    RECT swatch = {row.left, row.top + 7, row.left + 14, row.top + 21};
    RECT name_rect = {row.left + 22, row.top, row.right - 90, row.bottom};
    RECT tag = {row.right - 82, row.top + 2, row.right, row.bottom - 2};
    char title[160];
    fill_rect(hdc, swatch, civs[other_id].color);
    snprintf(title, sizeof(title), "%c %.80s", civs[other_id].symbol, civilization_display_name(other_id));
    draw_text_rect(hdc, name_rect, title, ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    fill_rect(hdc, tag, status_color(civ_id, other_id, relation));
    draw_center_text(hdc, tag, status_label(civ_id, other_id, relation), RGB(246, 248, 250));
}

static void bar_row(HDC hdc, UiCursor *cursor, const char *label, const char *value,
                    int score, COLORREF color) {
    RECT row = ui_take_rect(cursor, 24);
    RECT label_rect = {row.left, row.top, row.left + 106, row.bottom};
    RECT value_rect = {row.left + 108, row.top, row.left + 160, row.bottom};
    RECT bar = {row.left + 164, row.top + 7, row.right, row.bottom - 7};
    draw_text_rect(hdc, label_rect, label, ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, value_rect, value, ui_theme_color(UI_COLOR_TEXT_MUTED), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    fill_rect(hdc, bar, RGB(43, 48, 50));
    bar.right = bar.left + (bar.right - bar.left) * clamp(score, 0, 100) / 100;
    fill_rect(hdc, bar, color);
}

static void metric_chip(HDC hdc, RECT rect, IconId icon, const char *label, const char *value, COLORREF accent) {
    RECT icon_rect = {rect.left + 6, rect.top + 5, rect.left + 22, rect.bottom - 5};
    RECT label_rect = {rect.left + 27, rect.top, rect.left + (rect.right - rect.left) / 2 + 4, rect.bottom};
    RECT sep = {label_rect.right + 2, rect.top + 6, label_rect.right + 3, rect.bottom - 6};
    RECT value_rect = {sep.right + 6, rect.top, rect.right - 5, rect.bottom};
    fill_rect(hdc, rect, RGB(42, 47, 50));
    fill_rect(hdc, (RECT){rect.left, rect.top, rect.left + 4, rect.bottom}, accent);
    draw_icon(hdc, icon, icon_rect, accent);
    draw_text_rect(hdc, label_rect, label, ui_theme_color(UI_COLOR_TEXT_DIM), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    fill_rect(hdc, sep, RGB(88, 94, 96));
    draw_text_rect(hdc, value_rect, value, ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}

static void chip_row3(HDC hdc, UiCursor *cursor,
                      IconId ia, const char *a, const char *av,
                      IconId ib, const char *b, const char *bv,
                      IconId ic, const char *c, const char *cv) {
    RECT row = ui_take_rect(cursor, 28);
    int w = (row.right - row.left - 12) / 3;
    metric_chip(hdc, (RECT){row.left, row.top, row.left + w, row.bottom}, ia, a, av, RGB(88, 134, 190));
    metric_chip(hdc, (RECT){row.left + w + 6, row.top, row.left + 2 * w + 6, row.bottom}, ib, b, bv, RGB(196, 154, 72));
    metric_chip(hdc, (RECT){row.left + 2 * (w + 6), row.top, row.right, row.bottom}, ic, c, cv, RGB(132, 148, 126));
}

static void draw_strength_compare(HDC hdc, UiCursor *cursor, int own, int enemy, COLORREF own_color, COLORREF enemy_color) {
    RECT row = ui_take_rect(cursor, 28);
    RECT bar = {row.left + 58, row.top + 8, row.right - 58, row.bottom - 8};
    int total = max(1, own + enemy);
    int split = bar.left + (bar.right - bar.left) * own / total;
    char left[32], right[32];
    format_metric_value(own, left, sizeof(left));
    format_metric_value(enemy, right, sizeof(right));
    draw_text_rect(hdc, (RECT){row.left, row.top, row.left + 54, row.bottom}, left,
                   ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_VCENTER | DT_RIGHT);
    draw_text_rect(hdc, (RECT){row.right - 54, row.top, row.right, row.bottom}, right,
                   ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_VCENTER);
    fill_rect(hdc, bar, RGB(45, 50, 52));
    fill_rect(hdc, (RECT){bar.left, bar.top, split, bar.bottom}, own_color);
    fill_rect(hdc, (RECT){split, bar.top, bar.right, bar.bottom}, enemy_color);
}

static void draw_peace_tense(HDC hdc, UiCursor *cursor, int civ_id, int other_id, DiplomacyRelation relation) {
    char num[32], border[32], gap[32];
    int flags = war_front_flags(civ_id, other_id);
    int spark = clamp((relation.border_tension + relation.resource_conflict) / 2, 0, 100);
    if (relation.state == DIPLOMACY_TENSE) {
        snprintf(num, sizeof(num), "%d", relation.border_tension);
        bar_row(hdc, cursor, tr("Tension risk", "紧张风险"), num, relation.border_tension, RGB(206, 126, 64));
        snprintf(num, sizeof(num), "%d", spark);
        bar_row(hdc, cursor, tr("War spark", "战争火花"), num, spark, RGB(188, 76, 66));
        snprintf(border, sizeof(border), "%d", relation.border_length);
        snprintf(gap, sizeof(gap), "%s %d", tr("gap", "缺口"),
                 max(0, relation.border_tension - 34) + max(0, 46 - relation.relation_score));
        chip_row3(hdc, cursor, ICON_TERRITORY, tr("Border", "边界"), border,
                  ICON_ATTACK, tr("Conflict", "冲突"), level_label(relation.resource_conflict),
                  ICON_COHESION, tr("Peace", "回和平"), gap);
    } else {
        bar_row(hdc, cursor, tr("Relation temp", "关系温度"), level_label(relation.relation_score),
                relation.relation_score, RGB(92, 156, 108));
        snprintf(num, sizeof(num), "%d", relation.border_tension);
        bar_row(hdc, cursor, tr("Tension risk", "紧张风险"), num, relation.border_tension, RGB(196, 154, 72));
        snprintf(border, sizeof(border), "%d", relation.border_length);
        chip_row3(hdc, cursor, ICON_COMMERCE, tr("Trade", "贸易"), level_label(relation.trade_fit),
                  ICON_TERRITORY, tr("Border", "边界"), border,
                  ICON_HARBOR, tr("Contact", "接触"), ui_language ? war_front_reason_zh(flags) : war_front_reason_en(flags));
    }
}

static int peace_pressure_for_side(int civ_id, ActiveWar war) {
    int casualties = civ_id == war.attacker ? war.casualties_a + war.support_casualties_a :
                     war.casualties_b + war.support_casualties_b;
    int initial = civ_id == war.attacker ? war.initial_soldiers_a : war.initial_soldiers_b;
    return clamp(civs[civ_id].disorder + (initial > 0 ? casualties * 70 / initial : 0), 0, 100);
}

static void draw_war_truce(HDC hdc, UiCursor *cursor, int civ_id, int other_id, DiplomacyRelation relation) {
    ActiveWar war = war_state_between(civ_id, other_id);
    char a[48], b[48], c[48], span[48], losses[128];
    int flags = war_front_flags(civ_id, other_id);
    if (war.active) {
        int own = civ_id == war.attacker ? war.soldiers_a : war.soldiers_b;
        int enemy = civ_id == war.attacker ? war.soldiers_b : war.soldiers_a;
        int own_loss = civ_id == war.attacker ? war.casualties_a + war.support_casualties_a :
                       war.casualties_b + war.support_casualties_b;
        int enemy_loss = civ_id == war.attacker ? war.casualties_b + war.support_casualties_b :
                         war.casualties_a + war.support_casualties_a;
        int own_wins = civ_id == war.attacker ? war.wins_a : war.wins_b;
        int enemy_wins = civ_id == war.attacker ? war.wins_b : war.wins_a;
        draw_strength_compare(hdc, cursor, own, enemy, civs[civ_id].color, civs[other_id].color);
        ui_format_months(span, sizeof(span), max(0, 3 - (war.years % 3)) * 12, UI_MONTH_ZERO_NOW);
        bar_row(hdc, cursor, tr("Next battle", "下次战斗"), span, 55, RGB(188, 84, 74));
        format_metric_value(own_loss, a, sizeof(a));
        format_metric_value(enemy_loss, b, sizeof(b));
        snprintf(losses, sizeof(losses), "%s / %s", a, b);
        snprintf(c, sizeof(c), "%d-%d", own_wins, enemy_wins);
        snprintf(a, sizeof(a), "%d/%d", peace_pressure_for_side(civ_id, war), peace_pressure_for_side(other_id, war));
        chip_row3(hdc, cursor, ICON_BATTLE, tr("Losses", "战损"), losses,
                  ICON_COHESION, tr("Peace", "议和压力"), a,
                  ICON_HARBOR, tr("Front", "战线"), ui_language ? war_front_reason_zh(flags) : war_front_reason_en(flags));
        (void)c;
    } else {
        ui_format_months(span, sizeof(span), relation.truce_years_left * 12, UI_MONTH_ZERO_DONE);
        bar_row(hdc, cursor, tr("Truce left", "停战剩余"), span,
                clamp(relation.truce_years_left * 100 / 45, 0, 100), RGB(196, 154, 72));
        chip_row3(hdc, cursor, ICON_DISORDER, tr("Risk", "再战风险"), level_label(relation.border_tension),
                  ICON_TERRITORY, tr("Border", "边界"), relation.border_length > 0 ? tr("Adjacent", "接壤") : tr("Separated", "隔绝"),
                  ICON_GOVERNANCE, tr("Trend", "趋势"), relation.border_tension >= 55 ? tr("Tense", "回紧张") : tr("Peace", "回和平"));
    }
}

static void format_tribute_total(VassalTributeBreakdown br, int sign, char *out, size_t size) {
    snprintf(out, size, "%c%d", sign < 0 ? '-' : '+', br.total);
}

static void draw_vassal_card(HDC hdc, UiCursor *cursor, int civ_id, int other_id) {
    char a[48], b[48], c[48];
    if (vassal_is_direct(civ_id, other_id)) {
        VassalTributeBreakdown br = vassal_resource_tribute_breakdown_from(other_id);
        bar_row(hdc, cursor, tr("Resource extraction", "资源抽取"), "40%", 40, RGB(154, 105, 178));
        bar_row(hdc, cursor, tr("Army call-up", "军队调用"), "70%", 70, RGB(188, 88, 154));
        format_tribute_total(br, 1, a, sizeof(a));
        snprintf(b, sizeof(b), "%d / %d", vassal_callable_soldiers(other_id), vassal_total_soldiers(other_id));
        snprintf(c, sizeof(c), "+%d", vassal_governance_disorder(civ_id));
        chip_row3(hdc, cursor, ICON_FOOD, tr("Tribute", "资源贡献"), a,
                  ICON_MILITARY, tr("Callable", "可调用"), b,
                  ICON_GOVERNANCE, tr("Burden", "治理负担"), c);
    } else if (vassal_is_direct(other_id, civ_id)) {
        VassalTributeBreakdown br = vassal_resource_tribute_breakdown_from(civ_id);
        bar_row(hdc, cursor, tr("Resource extracted", "资源被抽取"), "40%", 40, RGB(154, 105, 178));
        bar_row(hdc, cursor, tr("Army callable", "军队被调用"), "70%", 70, RGB(188, 88, 154));
        format_tribute_total(br, -1, a, sizeof(a));
        snprintf(b, sizeof(b), "%d / %d", vassal_callable_soldiers(civ_id), vassal_total_soldiers(civ_id));
        snprintf(c, sizeof(c), "%d", vassal_garrison_soldiers(civ_id));
        chip_row3(hdc, cursor, ICON_FOOD, tr("Extracted", "被抽取"), a,
                  ICON_MILITARY, tr("Called", "被调用"), b,
                  ICON_COUNTRY_DEFENSE, tr("Home guard", "留守"), c);
    } else {
        int over = vassal_overlord(other_id);
        snprintf(a, sizeof(a), "%.24s", over >= 0 ? civilization_display_name(over) : tr("None", "无"));
        chip_row3(hdc, cursor, ICON_GOVERNANCE, tr("Autonomy", "自主权"), tr("None", "无"),
                  ICON_COUNTRY_DEFENSE, tr("Represented", "外交代表"), a,
                  ICON_TERRITORY, tr("Status", "状态"), tr("Subordinate", "从属"));
    }
}

void draw_diplomacy_relation_card(HDC hdc, UiCursor *cursor, int civ_id,
                                  int other_id, DiplomacyView view) {
    DiplomacyRelation relation = diplomacy_relation(civ_id, other_id);
    RECT card = {cursor->x, cursor->y, cursor->x + cursor->width, cursor->y + 158};
    UiCursor inner = ui_cursor(card.left + 10, card.top + 8, card.right - card.left - 20, card.bottom - 8);
    fill_rect(hdc, card, RGB(34, 39, 42));
    draw_header(hdc, &inner, civ_id, other_id, relation);
    if (view == DIPLOMACY_VIEW_WAR_TRUCE || relation.state == DIPLOMACY_WAR || relation.state == DIPLOMACY_TRUCE) {
        draw_war_truce(hdc, &inner, civ_id, other_id, relation);
    } else if (view == DIPLOMACY_VIEW_TRIBUTE_VASSAL ||
               vassal_is_direct(civ_id, other_id) || vassal_is_direct(other_id, civ_id) ||
               vassal_overlord(civ_id) >= 0 || vassal_overlord(other_id) >= 0) {
        draw_vassal_card(hdc, &inner, civ_id, other_id);
    } else {
        draw_peace_tense(hdc, &inner, civ_id, other_id, relation);
    }
    cursor->y = card.bottom + 8;
}
