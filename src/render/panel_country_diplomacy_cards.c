#include "render/panel_country_diplomacy_cards.h"

#include "render/panel_country_diplomacy_score.h"
#include "render/panel_war_compare_bar.h"
#include "render/snapshot_ui.h"
#include "render/ui_format.h"
#include "sim/diplomacy.h"
#include "sim/war.h"
#include "sim/war_front.h"
#include "ui/ui_clay_primitives.h"

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

static const AllianceSnapshotRecord *card_alliance_record(int alliance_id) {
    const RenderSnapshot *snapshot = cards_snapshot();
    int i;
    if (!snapshot || alliance_id < 0) return NULL;
    for (i = 0; i < snapshot->alliance_count && i < ALLIANCE_MAX; i++)
        if (snapshot->alliances[i].active && snapshot->alliances[i].id == alliance_id) return &snapshot->alliances[i];
    return NULL;
}

static COLORREF card_alliance_color_for(int civ_id, COLORREF fallback) {
    const SnapshotCiv *civ = card_civ(civ_id);
    const AllianceSnapshotRecord *record = civ ? card_alliance_record(civ->alliance_display_id) : NULL;
    return record ? (COLORREF)record->color : fallback;
}

static int card_side_vassal_support(int civ_id, COLORREF *color_out) {
    const RenderSnapshot *snapshot = cards_snapshot();
    int i, total = 0;
    if (color_out) *color_out = RGB(154, 105, 178);
    for (i = 0; snapshot && i < snapshot->civ_count && i < MAX_CIVS; i++) {
        const SnapshotCiv *v = &snapshot->civs[i];
        if (!v->alive || v->overlord != civ_id || v->vassal_support_used <= 0) continue;
        total += v->vassal_support_used;
        if (color_out && total == v->vassal_support_used) *color_out = v->color;
    }
    return total;
}

static const char *last_war_result_text(int civ_id, SnapshotDiplomacyRelation relation) {
    if (relation.last_war_result == DIP_LAST_WAR_INTERRUPTED ||
        relation.last_war_result == DIP_LAST_WAR_FRONT_SEVERED) return tr("Front Severed", "战线中断");
    if (relation.last_war_result == DIP_LAST_WAR_NEGOTIATED_TRUCE) return tr("Negotiated Truce", "议和停战");
    if (relation.last_war_result == DIP_LAST_WAR_OFFENSIVE_HALTED) {
        if (relation.last_war_winner == civ_id) return tr("Offensive Halted", "攻势中止");
        if (relation.last_war_loser == civ_id) return tr("Enemy Offensive Halted", "对方攻势中止");
        return tr("Offensive Halted", "攻势中止");
    }
    if (relation.last_war_result == DIP_LAST_WAR_SURRENDER) {
        if (relation.last_war_winner == civ_id) return tr("Surrender Win", "受降胜利");
        if (relation.last_war_loser == civ_id) return tr("Surrender", "投降战败");
    }
    if (relation.last_war_result == DIP_LAST_WAR_DECISIVE ||
        relation.last_war_result == DIP_LAST_WAR_MILITARY) {
        if (relation.last_war_winner == civ_id) return tr("Military Win", "军事胜利");
        if (relation.last_war_loser == civ_id) return tr("Military Defeat", "军事战败");
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
        case DIPLOMACY_ALLIANCE: return tr("Alliance", "同盟");
        case DIPLOMACY_TENSE: return tr("Tense", "紧张");
        case DIPLOMACY_TRUCE: return tr("Truce", "停战");
        case DIPLOMACY_WAR: return tr("War", "战争");
        case DIPLOMACY_VASSAL: return tr("Vassal", "附庸");
        default: return tr("Unknown", "未知");
    }
}

static UiClaySemanticStyle status_style(int civ_id, int other_id, SnapshotDiplomacyRelation relation) {
    if (card_is_direct_vassal(civ_id, other_id)) return ui_clay_semantic_style(UI_CLAY_TONE_VASSAL);
    if (card_is_direct_vassal(other_id, civ_id)) return ui_clay_semantic_style(UI_CLAY_TONE_TRIBUTE);
    if ((card_overlord(civ_id) >= 0 && other_id != card_overlord(civ_id)) ||
        (card_overlord(other_id) >= 0 && civ_id != card_overlord(other_id))) {
        return ui_clay_semantic_style(UI_CLAY_TONE_MUTED);
    }
    switch (relation.state) {
        case DIPLOMACY_PEACE: return ui_clay_semantic_style(UI_CLAY_TONE_PEACE);
        case DIPLOMACY_ALLIANCE: return ui_clay_semantic_style(UI_CLAY_TONE_ALLIANCE);
        case DIPLOMACY_TENSE: return ui_clay_semantic_style(UI_CLAY_TONE_TENSE);
        case DIPLOMACY_TRUCE: return ui_clay_semantic_style(UI_CLAY_TONE_TRUCE);
        case DIPLOMACY_WAR: return ui_clay_semantic_style(UI_CLAY_TONE_WAR);
        case DIPLOMACY_VASSAL: return ui_clay_semantic_style(UI_CLAY_TONE_VASSAL);
        default: return ui_clay_semantic_style(UI_CLAY_TONE_NEUTRAL);
    }
}

static void draw_header(HDC hdc, UiCursor *cursor, int civ_id, int other_id,
                        SnapshotDiplomacyRelation relation) {
    const SnapshotCiv *other = card_civ(other_id);
    UiClaySemanticStyle style = status_style(civ_id, other_id, relation);
    RECT row = ui_take_rect(cursor, 28);
    RECT swatch = {row.left, row.top + 7, row.left + 14, row.top + 21};
    RECT name_rect = {row.left + 22, row.top, row.right - 90, row.bottom};
    RECT tag = {row.right - 82, row.top + 2, row.right, row.bottom - 2};
    char title[160];
    fill_rect(hdc, swatch, other ? other->color : RGB(96, 100, 104));
    snprintf(title, sizeof(title), "%c %.80s", other ? other->symbol : '?', snapshot_ui_civ_name(other_id));
    draw_text_rect(hdc, name_rect, title, ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    fill_rect(hdc, tag, style.tag_fill);
    fill_rect(hdc, (RECT){tag.left, tag.top, tag.left + 4, tag.bottom}, style.accent);
    draw_center_text(hdc, tag, status_label(civ_id, other_id, relation), style.tag_text);
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
    int mid = rect.top + (rect.bottom - rect.top) / 2;
    RECT label_rect = {rect.left + 27, rect.top + 3, rect.right - 6, mid + 1};
    RECT value_rect = {rect.left + 27, mid - 1, rect.right - 6, rect.bottom - 3};
    ui_clay_draw_card(hdc, rect, UI_CLAY_STATE_NORMAL);
    fill_rect(hdc, (RECT){rect.left, rect.top, rect.left + 4, rect.bottom}, accent);
    draw_icon(hdc, icon, icon_rect, accent);
    draw_text_rect(hdc, label_rect, label, ui_theme_color(UI_COLOR_TEXT_DIM), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, value_rect, value, ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_RIGHT | DT_VCENTER | DT_END_ELLIPSIS);
}

static void chip_row3(HDC hdc, UiCursor *cursor,
                      IconId ia, const char *a, const char *av,
                      IconId ib, const char *b, const char *bv,
                      IconId ic, const char *c, const char *cv) {
    RECT row = ui_take_rect(cursor, 34);
    int w = (row.right - row.left - 12) / 3;
    metric_chip(hdc, (RECT){row.left, row.top, row.left + w, row.bottom}, ia, a, av, RGB(88, 134, 190));
    metric_chip(hdc, (RECT){row.left + w + 6, row.top, row.left + 2 * w + 6, row.bottom}, ib, b, bv, RGB(196, 154, 72));
    metric_chip(hdc, (RECT){row.left + 2 * (w + 6), row.top, row.right, row.bottom}, ic, c, cv, RGB(132, 148, 126));
}

static void chip_row2(HDC hdc, UiCursor *cursor,
                      IconId ia, const char *a, const char *av,
                      IconId ib, const char *b, const char *bv) {
    RECT row = ui_take_rect(cursor, 34);
    int w = (row.right - row.left - 6) / 2;
    metric_chip(hdc, (RECT){row.left, row.top, row.left + w, row.bottom}, ia, a, av, RGB(88, 134, 190));
    metric_chip(hdc, (RECT){row.left + w + 6, row.top, row.right, row.bottom}, ib, b, bv, RGB(132, 148, 126));
}

static const char *annex_status_text(int remaining_years) {
    static char text[64];
    if (remaining_years <= 0) return tr("Ready", "可吞并");
    snprintf(text, sizeof(text), ui_language ? "还差%d年" : "%dy left", remaining_years);
    return text;
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

static const char *peace_status_text(int own, int enemy, int own_is_attacker) {
    if (own >= 70 && enemy >= 70) return tr("Both willing: 25y truce at next battle.", "双方愿和：下次结算停战。");
    if (own >= 70) {
        return own_is_attacker ? tr("Attacker willing: offensive halts next battle.", "进攻方愿和：下次结算攻势中止。") :
               tr("Defender willing: attacker wins next battle.", "防御方愿和：下次结算进攻方胜。");
    }
    if (enemy >= 70) {
        return own_is_attacker ? tr("Defender willing: attacker wins next battle.", "防御方愿和：下次结算进攻方胜。") :
               tr("Attacker willing: offensive halts next battle.", "进攻方愿和：下次结算攻势中止。");
    }
    return tr("War can end when both sides reach 70.", "等双方达到 70 门槛。");
}

static int battle_months_remaining(SnapshotWar war) {
    const RenderSnapshot *snapshot = cards_snapshot();
    int current_month = snapshot ? snapshot->month : 1;
    int ticks_left = WAR_BATTLE_INTERVAL_YEARS - (war.years % WAR_BATTLE_INTERVAL_YEARS);
    int months_to_year_tick = 13 - clamp(current_month, 1, 12);
    if (ticks_left <= 0) ticks_left = WAR_BATTLE_INTERVAL_YEARS;
    return clamp((ticks_left - 1) * 12 + months_to_year_tick, 1, WAR_BATTLE_INTERVAL_MONTHS);
}

static int battle_progress_percent(int remaining_months) {
    return clamp(100 - remaining_months * 100 / WAR_BATTLE_INTERVAL_MONTHS, 0, 100);
}

static void draw_peace_tense(HDC hdc, UiCursor *cursor, int civ_id, int other_id,
                             SnapshotDiplomacyRelation relation) {
    char border[32];
    draw_diplomacy_relation_score_block(hdc, cursor, civ_id, other_id);
    if (relation.state == DIPLOMACY_TENSE) {
        snprintf(border, sizeof(border), "%d", relation.border_length);
        chip_row3(hdc, cursor, ICON_TERRITORY, tr("Border", "边界"), border,
                  ICON_ATTACK, tr("Conflict", "冲突"), level_label(relation.resource_conflict),
                  ICON_BATTLE, tr("History", "历史"), last_war_result_text(civ_id, relation));
    } else {
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
    UiClaySemanticStyle war_style = ui_clay_semantic_style(UI_CLAY_TONE_WAR);
    UiClaySemanticStyle truce_style = ui_clay_semantic_style(UI_CLAY_TONE_TRUCE);
    char a[48], b[48], c[48], span[48], losses[128], front[80], disorder[48];
    int flags = card_front_flags(civ_id, other_id);
    if (war.active) {
        int own_is_attacker = civ_id == war.attacker;
        int own = max(0, own_is_attacker ? war.soldiers_a : war.soldiers_b);
        int enemy = max(0, own_is_attacker ? war.soldiers_b : war.soldiers_a);
        int own_merc = max(0, own_is_attacker ? war.temporary_soldiers_a : war.temporary_soldiers_b);
        int enemy_merc = max(0, own_is_attacker ? war.temporary_soldiers_b : war.temporary_soldiers_a);
        int own_alliance = max(0, own_is_attacker ? war.alliance_reinforcements_a : war.alliance_reinforcements_b);
        int enemy_alliance = max(0, own_is_attacker ? war.alliance_reinforcements_b : war.alliance_reinforcements_a);
        COLORREF own_vassal_color, enemy_vassal_color;
        int own_vassal = min(max(0, card_side_vassal_support(civ_id, &own_vassal_color)), own);
        int enemy_vassal = min(max(0, card_side_vassal_support(other_id, &enemy_vassal_color)), enemy);
        WarCompareBarModel war_bar;
        int own_loss = own_is_attacker ? war.casualties_a + war.support_casualties_a :
                       war.casualties_b + war.support_casualties_b;
        int enemy_loss = own_is_attacker ? war.casualties_b + war.support_casualties_b :
                         war.casualties_a + war.support_casualties_a;
        int own_wins = own_is_attacker ? war.wins_a : war.wins_b;
        int enemy_wins = own_is_attacker ? war.wins_b : war.wins_a;
        int own_disorder = own_civ ? own_civ->disorder : 0;
        int enemy_disorder = enemy_civ ? enemy_civ->disorder : 0;
        int own_peace = card_peace_pressure(civ_id, other_id);
        int enemy_peace = card_peace_pressure(other_id, civ_id);
        int battle_left = battle_months_remaining(war);
        memset(&war_bar, 0, sizeof(war_bar));
        war_bar.left_name = snapshot_ui_civ_name(civ_id);
        war_bar.right_name = snapshot_ui_civ_name(other_id);
        war_bar.left_role = own_is_attacker ? tr("Attacker", "进攻方") : tr("Defender", "防御方");
        war_bar.right_role = own_is_attacker ? tr("Defender", "防御方") : tr("Attacker", "进攻方");
        war_bar.left_regular = max(0, own - own_vassal);
        war_bar.right_regular = max(0, enemy - enemy_vassal);
        war_bar.left_vassal = own_vassal;
        war_bar.right_vassal = enemy_vassal;
        war_bar.left_mercenary = own_merc;
        war_bar.right_mercenary = enemy_merc;
        war_bar.left_alliance = own_alliance;
        war_bar.right_alliance = enemy_alliance;
        war_bar.left_regular_color = own_civ ? own_civ->color : RGB(120, 140, 160);
        war_bar.right_regular_color = enemy_civ ? enemy_civ->color : RGB(160, 120, 120);
        war_bar.left_vassal_color = own_vassal_color;
        war_bar.right_vassal_color = enemy_vassal_color;
        war_bar.left_alliance_color = card_alliance_color_for(civ_id, RGB(86, 152, 218));
        war_bar.right_alliance_color = card_alliance_color_for(other_id, RGB(86, 152, 218));
        panel_war_compare_bar_draw(hdc, cursor, &war_bar);
        ui_format_months(span, sizeof(span), battle_left, UI_MONTH_ZERO_NOW);
        bar_row(hdc, cursor, tr("Next battle", "下次战斗"), span, battle_progress_percent(battle_left), war_style.accent);
        draw_peace_compare(hdc, cursor, own_peace, enemy_peace);
        draw_text_rect(hdc, ui_take_rect(cursor, 20), peace_status_text(own_peace, enemy_peace, own_is_attacker),
                       ui_theme_color(UI_COLOR_TEXT_MUTED), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        format_metric_value(own_loss, a, sizeof(a));
        format_metric_value(enemy_loss, b, sizeof(b));
        snprintf(losses, sizeof(losses), "%s / %s", a, b);
        snprintf(c, sizeof(c), "%d - %d", own_wins, enemy_wins);
        chip_row2(hdc, cursor, ICON_BATTLE, tr("Casualties", "阵亡"), losses,
                  ICON_ATTACK, tr("Wins", "胜场"), c);
        snprintf(front, sizeof(front), "%s", ui_language ? war_front_reason_zh(flags) : war_front_reason_en(flags));
        snprintf(disorder, sizeof(disorder), "%d / %d", own_disorder, enemy_disorder);
        chip_row2(hdc, cursor, ICON_HARBOR, tr("Front", "战线"), front,
                  ICON_DISORDER, tr("Disorder", "混乱"), disorder);
    } else {
        int truce_denom = relation.truce_initial_years > 0 ? relation.truce_initial_years : relation.truce_years_left;
        RECT chip_row;
        int chip_w;
        draw_diplomacy_relation_score_block(hdc, cursor, civ_id, other_id);
        ui_format_months(span, sizeof(span), relation.truce_years_left * 12, UI_MONTH_ZERO_DONE);
        bar_row(hdc, cursor, tr("Truce left", "停战剩余"), span,
                clamp(relation.truce_years_left * 100 / max(1, truce_denom), 0, 100), truce_style.accent);
        chip_row = ui_take_rect(cursor, 34);
        chip_w = (chip_row.right - chip_row.left - 8) / 2;
        metric_chip(hdc, (RECT){chip_row.left, chip_row.top, chip_row.left + chip_w, chip_row.bottom},
                    ICON_DISORDER, tr("Risk", "再战风险"), level_label(relation.border_tension), RGB(88, 134, 190));
        metric_chip(hdc, (RECT){chip_row.left + chip_w + 8, chip_row.top, chip_row.right, chip_row.bottom},
                    ICON_GOVERNANCE, tr("Status", "停战状态"), truce_after_text(relation), RGB(132, 148, 126));
        cursor->y += 5;
        metric_chip(hdc, ui_take_rect(cursor, 34), ICON_BATTLE, tr("History", "历史"),
                    last_war_result_text(civ_id, relation), RGB(132, 148, 126));
    }
}

static void format_signed_metric(int total, int sign, char *out, size_t size) {
    char compact[32];
    format_metric_value(total, compact, sizeof(compact));
    snprintf(out, size, "%c%s", sign < 0 ? '-' : '+', compact);
}

static void format_metric_pair(int a, int b, char *out, size_t size) {
    char left[32], right[32];
    format_metric_value(a, left, sizeof(left));
    format_metric_value(b, right, sizeof(right));
    snprintf(out, size, "%s/%s", left, right);
}

static void draw_vassal_card(HDC hdc, UiCursor *cursor, int civ_id, int other_id) {
    const SnapshotCiv *selected = card_civ(civ_id);
    const SnapshotCiv *other = card_civ(other_id);
    UiClaySemanticStyle vassal_style = ui_clay_semantic_style(UI_CLAY_TONE_VASSAL);
    UiClaySemanticStyle tribute_style = ui_clay_semantic_style(UI_CLAY_TONE_TRIBUTE);
    char a[96], b[96], c[96];
    if (card_is_direct_vassal(civ_id, other_id)) {
        int tribute = other ? other->vassal_resource_tribute : 0;
        int callable = other ? other->vassal_callable_soldiers : 0;
        int total = other ? other->current_soldiers : 0;
        format_signed_metric(tribute, 1, a, sizeof(a));
        format_signed_metric(tribute, -1, b, sizeof(b));
        snprintf(c, sizeof(c), "+%d", selected ? selected->vassal_governance_disorder : 0);
        bar_row(hdc, cursor, tr("Tribute", "贡赋"), "40%", 40, tribute_style.accent);
        chip_row3(hdc, cursor, ICON_FOOD, tr("Gain", "获得"), a,
                  ICON_COMMERCE, tr("Paid", "上缴"), b,
                  ICON_GOVERNANCE, tr("Burden", "负担"), c);
        format_metric_pair(callable, total, a, sizeof(a));
        bar_row(hdc, cursor, tr("Call-up", "军调"), "70%", 70, vassal_style.accent);
        chip_row2(hdc, cursor, ICON_MILITARY, tr("Callable", "可调"), a,
                  ICON_GOVERNANCE, tr("Annex", "吞并"),
                  annex_status_text(other ? other->vassal_annex_remaining_years : 0));
    } else if (card_is_direct_vassal(other_id, civ_id)) {
        int callable = selected ? selected->vassal_callable_soldiers : 0;
        int total = selected ? selected->current_soldiers : 0;
        int tribute = selected ? selected->vassal_resource_tribute : 0;
        format_signed_metric(tribute, -1, a, sizeof(a));
        format_signed_metric(tribute, 1, b, sizeof(b));
        bar_row(hdc, cursor, tr("Tribute", "上缴"), "40%", 40, tribute_style.accent);
        chip_row3(hdc, cursor, ICON_FOOD, tr("Paid", "上缴"), a,
                  ICON_COMMERCE, tr("Overlord", "宗主"), b,
                  ICON_GOVERNANCE, tr("Autonomy", "自主"), tr("None", "无"));
        format_metric_pair(callable, total, a, sizeof(a));
        bar_row(hdc, cursor, tr("Call-up", "军调"), "70%", 70, vassal_style.accent);
        chip_row2(hdc, cursor, ICON_MILITARY, tr("Callable", "可调"), a,
                  ICON_GOVERNANCE, tr("Annex", "吞并"),
                  annex_status_text(selected ? selected->vassal_annex_remaining_years : 0));
    } else {
        int shown_vassal = card_overlord(other_id) >= 0 ? other_id : civ_id;
        int over = card_overlord(shown_vassal);
        snprintf(a, sizeof(a), "%.24s", over >= 0 ? snapshot_ui_civ_name(over) : tr("None", "无"));
        chip_row3(hdc, cursor, ICON_GOVERNANCE, tr("Autonomy", "自主"), tr("None", "无"),
                  ICON_COUNTRY_DEFENSE, tr("Overlord", "宗主"), a,
                  ICON_TERRITORY, tr("Status", "状态"), tr("Vassal", "附庸"));
    }
}

int diplomacy_relation_card_height(int civ_id, int other_id, DiplomacyView view) {
    SnapshotDiplomacyRelation relation = card_relation(civ_id, other_id);
    int vassal_like = view == DIPLOMACY_VIEW_VASSAL ||
                      card_is_direct_vassal(civ_id, other_id) || card_is_direct_vassal(other_id, civ_id) ||
                      card_overlord(civ_id) >= 0 || card_overlord(other_id) >= 0;
    int direct_vassal = card_is_direct_vassal(civ_id, other_id) || card_is_direct_vassal(other_id, civ_id);
    if (relation.state == DIPLOMACY_WAR) {
        return 330;
    }
    if (relation.state == DIPLOMACY_TRUCE) return 150 + diplomacy_relation_score_block_height(civ_id, other_id);
    if (direct_vassal) return 162;
    if (vassal_like) return 110;
    return 78 + diplomacy_relation_score_block_height(civ_id, other_id);
}

void draw_diplomacy_relation_card(HDC hdc, UiCursor *cursor, int civ_id,
                                  int other_id, DiplomacyView view) {
    SnapshotDiplomacyRelation relation = card_relation(civ_id, other_id);
    int vassal_like = view == DIPLOMACY_VIEW_VASSAL ||
                      card_is_direct_vassal(civ_id, other_id) || card_is_direct_vassal(other_id, civ_id) ||
                      card_overlord(civ_id) >= 0 || card_overlord(other_id) >= 0;
    UiClaySemanticStyle style = status_style(civ_id, other_id, relation);
    RECT card = {cursor->x, cursor->y, cursor->x + cursor->width,
                 cursor->y + diplomacy_relation_card_height(civ_id, other_id, view)};
    UiCursor inner = ui_cursor(card.left + 10, card.top + 8, card.right - card.left - 20, card.bottom - 8);
    ui_clay_draw_card(hdc, card, UI_CLAY_STATE_NORMAL);
    fill_rect(hdc, (RECT){card.left, card.top + 8, card.left + 4, card.bottom - 8}, style.accent);
    fill_rect(hdc, (RECT){card.left + 8, card.top, card.right - 8, card.top + 2}, style.border);
    draw_header(hdc, &inner, civ_id, other_id, relation);
    if (view == DIPLOMACY_VIEW_WAR || relation.state == DIPLOMACY_WAR || relation.state == DIPLOMACY_TRUCE) {
        draw_war_truce(hdc, &inner, civ_id, other_id, relation);
    } else if (vassal_like) {
        draw_vassal_card(hdc, &inner, civ_id, other_id);
    } else {
        draw_peace_tense(hdc, &inner, civ_id, other_id, relation);
    }
    cursor->y = card.bottom + (vassal_like ? 5 : 8);
}
