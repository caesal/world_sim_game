#include "render/panel_country_diplomacy_cards.h"

#include "render/snapshot_ui.h"
#include "render/ui_format.h"
#include "sim/diplomacy.h"
#include "sim/war_front.h"

#include <stdio.h>
#include <string.h>

static const RenderSnapshot *cards_snapshot(void) {
    return snapshot_ui_current();
}

static const SnapshotCiv *card_civ(int civ_id) {
    return snapshot_ui_civ(civ_id);
}

static int card_civ_alive(int civ_id) {
    const SnapshotCiv *civ = card_civ(civ_id);
    return civ && civ->alive;
}

static int card_overlord(int civ_id) {
    const SnapshotCiv *civ = card_civ(civ_id);
    return civ ? civ->overlord : -1;
}

static int card_is_direct_vassal(int overlord, int vassal) {
    return card_civ_alive(overlord) && card_civ_alive(vassal) && card_overlord(vassal) == overlord;
}

static SnapshotDiplomacyRelation card_relation(int civ_id, int other_id) {
    const RenderSnapshot *snapshot = cards_snapshot();
    SnapshotDiplomacyRelation relation;
    memset(&relation, 0, sizeof(relation));
    relation.state = DIPLOMACY_NONE;
    if (!snapshot || civ_id < 0 || other_id < 0 ||
        civ_id >= MAX_CIVS || other_id >= MAX_CIVS) return relation;
    return snapshot->relations[civ_id][other_id];
}

static SnapshotWar card_war(int civ_id, int other_id) {
    const RenderSnapshot *snapshot = cards_snapshot();
    SnapshotWar war;
    memset(&war, 0, sizeof(war));
    if (!snapshot || civ_id < 0 || other_id < 0 ||
        civ_id >= MAX_CIVS || other_id >= MAX_CIVS) return war;
    return snapshot->wars[civ_id][other_id];
}

static int card_front_flags(int civ_id, int other_id) {
    const RenderSnapshot *snapshot = cards_snapshot();
    if (!snapshot || civ_id < 0 || other_id < 0 ||
        civ_id >= MAX_CIVS || other_id >= MAX_CIVS) return 0;
    return snapshot->war_front_flags[civ_id][other_id];
}

static int card_peace_pressure(int civ_id, int other_id) {
    const RenderSnapshot *snapshot = cards_snapshot();
    if (!snapshot || civ_id < 0 || other_id < 0 ||
        civ_id >= MAX_CIVS || other_id >= MAX_CIVS) return 0;
    return snapshot->war_peace_pressure[civ_id][other_id];
}

static const char *last_war_result_text(int civ_id, SnapshotDiplomacyRelation relation) {
    if (relation.last_war_result == DIP_LAST_WAR_INTERRUPTED) return tr("Interrupted", "中断");
    if (relation.last_war_result == DIP_LAST_WAR_DECISIVE) {
        if (relation.last_war_winner == civ_id) return tr("Won", "胜利");
        if (relation.last_war_loser == civ_id) return tr("Lost", "战败");
    }
    return tr("-", "-");
}

static const char *truce_after_text(SnapshotDiplomacyRelation relation) {
    if (relation.contact_kind == DIP_CONTACT_NONE) return tr("No contact", "无联系");
    return relation.border_tension >= 55 ? tr("Tense", "紧张") : tr("Peace", "和平");
}

static const char *level_label(int value) {
    if (value >= 67) return tr("High", "高");
    if (value >= 34) return tr("Medium", "中");
    return tr("Low", "低");
}

static const char *status_label(int civ_id, int other_id, SnapshotDiplomacyRelation relation) {
    int own_overlord = card_overlord(civ_id);
    int other_overlord = card_overlord(other_id);
    if (card_is_direct_vassal(civ_id, other_id)) return tr("Vassal", "附庸");
    if (card_is_direct_vassal(other_id, civ_id)) return tr("Tribute", "朝贡");
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

static COLORREF status_color(int civ_id, int other_id, SnapshotDiplomacyRelation relation) {
    if (card_is_direct_vassal(civ_id, other_id) || card_is_direct_vassal(other_id, civ_id)) return RGB(154, 105, 178);
    if ((card_overlord(civ_id) >= 0 && other_id != card_overlord(civ_id)) ||
        (card_overlord(other_id) >= 0 && civ_id != card_overlord(other_id))) return RGB(104, 112, 120);
    switch (relation.state) {
        case DIPLOMACY_PEACE: return RGB(86, 150, 96);
        case DIPLOMACY_TENSE: return RGB(196, 154, 72);
        case DIPLOMACY_TRUCE: return RGB(196, 172, 92);
        case DIPLOMACY_WAR: return RGB(176, 72, 62);
        case DIPLOMACY_VASSAL: return RGB(154, 105, 178);
        default: return RGB(104, 112, 120);
    }
}

static void draw_header(HDC hdc, UiCursor *cursor, int civ_id, int other_id,
                        SnapshotDiplomacyRelation relation) {
    const SnapshotCiv *other = card_civ(other_id);
    RECT row = ui_take_rect(cursor, 28);
    RECT swatch = {row.left, row.top + 7, row.left + 14, row.top + 21};
    RECT name_rect = {row.left + 22, row.top, row.right - 90, row.bottom};
    RECT tag = {row.right - 82, row.top + 2, row.right, row.bottom - 2};
    char title[160];
    fill_rect(hdc, swatch, other ? other->color : RGB(96, 100, 104));
    snprintf(title, sizeof(title), "%c %.80s", other ? other->symbol : '?', snapshot_ui_civ_name(other_id));
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
    RECT label_rect = {rect.left + 27, rect.top, rect.left + 68, rect.bottom};
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

static void draw_strength_compare(HDC hdc, UiCursor *cursor, int own, int enemy,
                                  COLORREF own_color, COLORREF enemy_color) {
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

static void draw_peace_compare(HDC hdc, UiCursor *cursor, int own, int enemy) {
    RECT title = ui_take_rect(cursor, 16);
    RECT row = ui_take_rect(cursor, 26);
    int gap = 14;
    int bar_w = (row.right - row.left - gap) / 2;
    RECT left = {row.left, row.top + 5, row.left + bar_w, row.bottom - 5};
    RECT right = {row.right - bar_w, left.top, row.right, left.bottom};
    RECT left_fill = left;
    RECT right_fill = right;
    char text[48];

    draw_center_text(hdc, title, tr("Peace pressure", "议和压力"), ui_theme_color(UI_COLOR_TEXT));
    fill_rect(hdc, left, RGB(43, 48, 50));
    left_fill.right = left.left + (left.right - left.left) * clamp(own, 0, 70) / 70;
    fill_rect(hdc, left_fill, RGB(196, 154, 72));
    snprintf(text, sizeof(text), "%s %d/70", tr("Us", "我方"), own);
    draw_center_text(hdc, left, text, ui_theme_color(UI_COLOR_TEXT));

    fill_rect(hdc, right, RGB(43, 48, 50));
    right_fill.right = right.left + (right.right - right.left) * clamp(enemy, 0, 70) / 70;
    fill_rect(hdc, right_fill, RGB(176, 88, 78));
    snprintf(text, sizeof(text), "%s %d/70", tr("Them", "对方"), enemy);
    draw_center_text(hdc, right, text, ui_theme_color(UI_COLOR_TEXT));
}

static const char *peace_status_text(int own, int enemy) {
    if (own >= 70 && enemy >= 70) return tr("Both willing; peace can resolve at next battle.", "双方愿意，下次战斗结算时议和。");
    if (own >= 70) return tr("We are willing; enemy continues.", "我方愿意，对方继续。");
    if (enemy >= 70) return tr("Enemy is willing; we continue.", "对方愿意，我方继续。");
    return tr("War can end when both sides reach 70.", "等双方达到 70 门槛。");
}

static int battle_months_remaining(SnapshotWar war) {
    const RenderSnapshot *snapshot = cards_snapshot();
    int current_month = snapshot ? snapshot->month : 1;
    int ticks_left = 3 - (war.years % 3);
    int months_to_year_tick = 13 - clamp(current_month, 1, 12);
    if (ticks_left <= 0) ticks_left = 3;
    return clamp((ticks_left - 1) * 12 + months_to_year_tick, 1, 36);
}

static int battle_progress_percent(int remaining_months) {
    return clamp(100 - remaining_months * 100 / 36, 0, 100);
}

static void draw_peace_tense(HDC hdc, UiCursor *cursor, int civ_id, int other_id,
                             SnapshotDiplomacyRelation relation) {
    char num[32], border[32];
    int spark = clamp((relation.border_tension + relation.resource_conflict) / 2, 0, 100);
    (void)other_id;
    if (relation.state == DIPLOMACY_TENSE) {
        snprintf(num, sizeof(num), "%d", relation.border_tension);
        bar_row(hdc, cursor, tr("Tension risk", "紧张风险"), num, relation.border_tension, RGB(206, 126, 64));
        snprintf(num, sizeof(num), "%d", spark);
        bar_row(hdc, cursor, tr("War spark", "战争火花"), num, spark, RGB(188, 76, 66));
        snprintf(border, sizeof(border), "%d", relation.border_length);
        chip_row3(hdc, cursor, ICON_TERRITORY, tr("Border", "边界"), border,
                  ICON_ATTACK, tr("Conflict", "冲突"), level_label(relation.resource_conflict),
                  ICON_BATTLE, tr("History", "历史"), last_war_result_text(civ_id, relation));
    } else {
        bar_row(hdc, cursor, tr("Relation temp", "关系温度"), level_label(relation.relation_score),
                relation.relation_score, RGB(92, 156, 108));
        snprintf(num, sizeof(num), "%d", relation.border_tension);
        bar_row(hdc, cursor, tr("Tension risk", "紧张风险"), num, relation.border_tension, RGB(196, 154, 72));
        snprintf(border, sizeof(border), "%d", relation.border_length);
        chip_row3(hdc, cursor, ICON_COMMERCE, tr("Trade", "贸易"), level_label(relation.trade_fit),
                  ICON_TERRITORY, tr("Border", "边界"), border,
                  ICON_BATTLE, tr("History", "历史"), last_war_result_text(civ_id, relation));
    }
}

static void draw_war_truce(HDC hdc, UiCursor *cursor, int civ_id, int other_id,
                           SnapshotDiplomacyRelation relation) {
    const SnapshotCiv *own_civ = card_civ(civ_id);
    const SnapshotCiv *enemy_civ = card_civ(other_id);
    SnapshotWar war = card_war(civ_id, other_id);
    char a[48], b[48], c[48], span[48], losses[128];
    int flags = card_front_flags(civ_id, other_id);
    if (war.active) {
        int own = civ_id == war.attacker ? war.soldiers_a : war.soldiers_b;
        int enemy = civ_id == war.attacker ? war.soldiers_b : war.soldiers_a;
        int own_loss = civ_id == war.attacker ? war.casualties_a + war.support_casualties_a :
                       war.casualties_b + war.support_casualties_b;
        int enemy_loss = civ_id == war.attacker ? war.casualties_b + war.support_casualties_b :
                         war.casualties_a + war.support_casualties_a;
        int own_wins = civ_id == war.attacker ? war.wins_a : war.wins_b;
        int enemy_wins = civ_id == war.attacker ? war.wins_b : war.wins_a;
        int own_peace = card_peace_pressure(civ_id, other_id);
        int enemy_peace = card_peace_pressure(other_id, civ_id);
        int battle_left = battle_months_remaining(war);
        draw_strength_compare(hdc, cursor, own, enemy,
                              own_civ ? own_civ->color : RGB(120, 140, 160),
                              enemy_civ ? enemy_civ->color : RGB(160, 120, 120));
        ui_format_months(span, sizeof(span), battle_left, UI_MONTH_ZERO_NOW);
        bar_row(hdc, cursor, tr("Next battle", "下次战斗"), span, battle_progress_percent(battle_left), RGB(188, 84, 74));
        draw_peace_compare(hdc, cursor, own_peace, enemy_peace);
        draw_text_rect(hdc, ui_take_rect(cursor, 20), peace_status_text(own_peace, enemy_peace),
                       ui_theme_color(UI_COLOR_TEXT_MUTED), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        format_metric_value(own_loss, a, sizeof(a));
        format_metric_value(enemy_loss, b, sizeof(b));
        snprintf(losses, sizeof(losses), "%s / %s", a, b);
        snprintf(c, sizeof(c), "%d - %d", own_wins, enemy_wins);
        chip_row3(hdc, cursor, ICON_BATTLE, tr("Casualties", "阵亡"), losses,
                  ICON_ATTACK, tr("Wins", "胜场"), c,
                  ICON_HARBOR, tr("Front", "战线"), ui_language ? war_front_reason_zh(flags) : war_front_reason_en(flags));
    } else {
        int truce_denom = relation.truce_initial_years > 0 ? relation.truce_initial_years : relation.truce_years_left;
        ui_format_months(span, sizeof(span), relation.truce_years_left * 12, UI_MONTH_ZERO_DONE);
        bar_row(hdc, cursor, tr("Truce left", "停战剩余"), span,
                clamp(relation.truce_years_left * 100 / max(1, truce_denom), 0, 100), RGB(196, 154, 72));
        chip_row3(hdc, cursor, ICON_DISORDER, tr("Risk", "再战风险"), level_label(relation.border_tension),
                  ICON_GOVERNANCE, tr("After truce", "停战结束后"), truce_after_text(relation),
                  ICON_BATTLE, tr("History", "历史"), last_war_result_text(civ_id, relation));
    }
}

static void format_tribute_total(int total, int sign, char *out, size_t size) {
    snprintf(out, size, "%c%d", sign < 0 ? '-' : '+', total);
}

static void draw_vassal_card(HDC hdc, UiCursor *cursor, int civ_id, int other_id) {
    const SnapshotCiv *selected = card_civ(civ_id);
    const SnapshotCiv *other = card_civ(other_id);
    char a[48], b[48], c[48];
    if (card_is_direct_vassal(civ_id, other_id)) {
        format_tribute_total(other ? other->vassal_resource_tribute : 0, 1, a, sizeof(a));
        snprintf(b, sizeof(b), "%d / %d", other ? other->vassal_callable_soldiers : 0,
                 other ? other->current_soldiers : 0);
        snprintf(c, sizeof(c), "+%d", selected ? selected->vassal_governance_disorder : 0);
    bar_row(hdc, cursor, tr("Resource extraction", "????"), "40%", 40, RGB(154, 105, 178));
    bar_row(hdc, cursor, tr("Army call-up", "????"), "70%", 70, RGB(188, 88, 154));
    chip_row3(hdc, cursor, ICON_FOOD, tr("Tribute", "????"), a,
              ICON_MILITARY, tr("Callable", "???"), b,
              ICON_GOVERNANCE, tr("Burden", "????"), c);
    } else if (card_is_direct_vassal(other_id, civ_id)) {
        int callable = selected ? selected->vassal_callable_soldiers : 0;
        int total = selected ? selected->current_soldiers : 0;
        format_tribute_total(selected ? selected->vassal_resource_tribute : 0, -1, a, sizeof(a));
        snprintf(b, sizeof(b), "%d / %d", callable, total);
        snprintf(c, sizeof(c), "%d", max(0, total - callable));
        bar_row(hdc, cursor, tr("Resource extracted", "?????"), "40%", 40, RGB(154, 105, 178));
        bar_row(hdc, cursor, tr("Army callable", "?????"), "70%", 70, RGB(188, 88, 154));
        chip_row3(hdc, cursor, ICON_FOOD, tr("Extracted", "???"), a,
                  ICON_MILITARY, tr("Called", "???"), b,
                  ICON_COUNTRY_DEFENSE, tr("Home guard", "留守"), c);
    } else {
        int over = card_overlord(other_id);
        snprintf(a, sizeof(a), "%.24s", over >= 0 ? snapshot_ui_civ_name(over) : tr("None", "无"));
        chip_row3(hdc, cursor, ICON_GOVERNANCE, tr("Autonomy", "自主权"), tr("None", "无"),
                  ICON_COUNTRY_DEFENSE, tr("Represented", "外交代表"), a,
                  ICON_TERRITORY, tr("Status", "状态"), tr("Subordinate", "从属"));
    }
}

void draw_diplomacy_relation_card(HDC hdc, UiCursor *cursor, int civ_id,
                                  int other_id, DiplomacyView view) {
    SnapshotDiplomacyRelation relation = card_relation(civ_id, other_id);
    RECT card = {cursor->x, cursor->y, cursor->x + cursor->width, cursor->y + 184};
    UiCursor inner = ui_cursor(card.left + 10, card.top + 8, card.right - card.left - 20, card.bottom - 8);
    fill_rect(hdc, card, RGB(34, 39, 42));
    draw_header(hdc, &inner, civ_id, other_id, relation);
    if (view == DIPLOMACY_VIEW_WAR_TRUCE || relation.state == DIPLOMACY_WAR || relation.state == DIPLOMACY_TRUCE) {
        draw_war_truce(hdc, &inner, civ_id, other_id, relation);
    } else if (view == DIPLOMACY_VIEW_TRIBUTE_VASSAL ||
               card_is_direct_vassal(civ_id, other_id) || card_is_direct_vassal(other_id, civ_id) ||
               card_overlord(civ_id) >= 0 || card_overlord(other_id) >= 0) {
        draw_vassal_card(hdc, &inner, civ_id, other_id);
    } else {
        draw_peace_tense(hdc, &inner, civ_id, other_id, relation);
    }
    cursor->y = card.bottom + 8;
}
