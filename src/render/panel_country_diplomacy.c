#include "render/panel_country_diplomacy.h"

#include "render/panel_country.h"
#include "render_panel_internal.h"
#include "sim/diplomacy.h"
#include "sim/vassal.h"
#include "sim/war.h"
#include "ui/ui_types.h"
#include "ui/ui_widgets.h"

#define DIPLOMACY_VIEW_TAB_H 28
#define DIPLOMACY_VIEW_TAB_GAP 6
typedef DiplomacyView DiplomacyDisplayGroup;
#define DIP_DISPLAY_PEACE_TENSE DIPLOMACY_VIEW_PEACE_TENSE
#define DIP_DISPLAY_WAR_TRUCE DIPLOMACY_VIEW_WAR_TRUCE
#define DIP_DISPLAY_VASSAL DIPLOMACY_VIEW_TRIBUTE_VASSAL
#define DIP_DISPLAY_OTHER DIPLOMACY_VIEW_OTHER

static const char *status_label(DiplomacyStatus status) {
    switch (status) {
        case DIPLOMACY_PEACE: return tr("Peace", "和平");
        case DIPLOMACY_ALLIANCE: return tr("Alliance", "联盟");
        case DIPLOMACY_TENSE: return tr("Tense", "紧张");
        case DIPLOMACY_TRUCE: return tr("Truce", "停战");
        case DIPLOMACY_WAR: return tr("War", "战争");
        case DIPLOMACY_VASSAL: return tr("Vassal", "附庸");
        default: return tr("Unknown", "未知");
    }
}

static COLORREF status_color(DiplomacyStatus status) {
    switch (status) {
        case DIPLOMACY_PEACE: return RGB(86, 150, 96);
        case DIPLOMACY_ALLIANCE: return RGB(88, 134, 190);
        case DIPLOMACY_TENSE: return RGB(196, 154, 72);
        case DIPLOMACY_TRUCE: return RGB(128, 148, 170);
        case DIPLOMACY_WAR: return RGB(176, 72, 62);
        case DIPLOMACY_VASSAL: return RGB(154, 105, 178);
        default: return RGB(104, 112, 120);
    }
}

static const char *status_label_for_pair(int civ_id, int other_id, DiplomacyRelation relation) {
    int own_overlord = vassal_overlord(civ_id);
    int other_overlord = vassal_overlord(other_id);

    if (vassal_is_direct(civ_id, other_id)) return tr("Vassal", "附庸");
    if (vassal_is_direct(other_id, civ_id)) return tr("Tribute", "朝贡");
    if (own_overlord >= 0 && other_id != own_overlord) return tr("No autonomy", "无自主权");
    if (other_overlord >= 0 && civ_id != other_overlord) return tr("No autonomy", "无自主权");
    return status_label(relation.state);
}

static COLORREF status_color_for_pair(int civ_id, int other_id, DiplomacyRelation relation) {
    if (vassal_is_direct(civ_id, other_id) || vassal_is_direct(other_id, civ_id)) {
        return RGB(154, 105, 178);
    }
    if ((vassal_overlord(civ_id) >= 0 && other_id != vassal_overlord(civ_id)) ||
        (vassal_overlord(other_id) >= 0 && civ_id != vassal_overlord(other_id))) {
        return RGB(104, 112, 120);
    }
    return status_color(relation.state);
}

static int status_rank(DiplomacyStatus status) {
    switch (status) {
        case DIPLOMACY_WAR: return 0;
        case DIPLOMACY_TENSE: return 1;
        case DIPLOMACY_TRUCE: return 2;
        case DIPLOMACY_PEACE: return 3;
        default: return 4;
    }
}

static const char *display_group_label(int selected_is_vassal, DiplomacyDisplayGroup group) {
    if (selected_is_vassal) {
        return group == DIP_DISPLAY_VASSAL ? tr("Tribute & Vassal", "朝贡 & 附庸") :
                                             tr("Other", "其他");
    }
    switch (group) {
        case DIP_DISPLAY_PEACE_TENSE: return tr("Peace & Tense", "和平 & 紧张");
        case DIP_DISPLAY_WAR_TRUCE: return tr("War & Truce", "战争 & 停战");
        case DIP_DISPLAY_VASSAL: return tr("Tribute & Vassal", "朝贡 & 附庸");
        default: return tr("Other", "其他");
    }
}

static DiplomacyDisplayGroup display_group_for_pair(int civ_id, int other_id) {
    DiplomacyRelation relation = diplomacy_relation(civ_id, other_id);

    if (vassal_overlord(civ_id) >= 0) {
        return other_id == vassal_overlord(civ_id) ? DIP_DISPLAY_VASSAL : DIP_DISPLAY_OTHER;
    }
    if (vassal_is_direct(civ_id, other_id) || vassal_is_direct(other_id, civ_id)) {
        return DIP_DISPLAY_VASSAL;
    }
    if (relation.state == DIPLOMACY_WAR || relation.state == DIPLOMACY_TRUCE) return DIP_DISPLAY_WAR_TRUCE;
    if (relation.state == DIPLOMACY_PEACE || relation.state == DIPLOMACY_TENSE ||
        relation.state == DIPLOMACY_ALLIANCE) {
        return DIP_DISPLAY_PEACE_TENSE;
    }
    return DIP_DISPLAY_OTHER;
}

static DiplomacyView normalize_diplomacy_view(int civ_id) {
    int selected_is_vassal = vassal_overlord(civ_id) >= 0;
    DiplomacyView view = (DiplomacyView)country_diplomacy_view;

    if (selected_is_vassal) {
        if (view != DIPLOMACY_VIEW_TRIBUTE_VASSAL) view = DIPLOMACY_VIEW_OTHER;
    } else if (view == DIPLOMACY_VIEW_OTHER ||
               view < DIPLOMACY_VIEW_PEACE_TENSE || view > DIPLOMACY_VIEW_TRIBUTE_VASSAL) {
        view = DIPLOMACY_VIEW_PEACE_TENSE;
    }
    country_diplomacy_view = view;
    return view;
}

static int diplomacy_view_count(int selected_is_vassal) {
    return selected_is_vassal ? 2 : 3;
}

static DiplomacyView diplomacy_view_at(int selected_is_vassal, int index) {
    if (selected_is_vassal) return index == 0 ? DIPLOMACY_VIEW_TRIBUTE_VASSAL : DIPLOMACY_VIEW_OTHER;
    if (index == 1) return DIPLOMACY_VIEW_WAR_TRUCE;
    if (index == 2) return DIPLOMACY_VIEW_TRIBUTE_VASSAL;
    return DIPLOMACY_VIEW_PEACE_TENSE;
}

static int direct_relation_visible(int civ_id, int other_id) {
    if (other_id < 0 || other_id >= civ_count || !civs[other_id].alive || other_id == civ_id) return 0;
    if (vassal_overlord(civ_id) >= 0) return 1;
    if (vassal_is_direct(civ_id, other_id)) return 1;
    if (vassal_overlord(other_id) >= 0) return 0;
    return diplomacy_relation(civ_id, other_id).state != DIPLOMACY_NONE;
}

static void metric3(HDC hdc, UiCursor *cursor, int a, int b, int c,
                    IconId ia, IconId ib, IconId ic,
                    const char *la, const char *lb, const char *lc) {
    int w = (cursor->width - 16) / 3;
    RECT r = {cursor->x, cursor->y, cursor->x + w, cursor->y + 28};
    ui_metric_chip(hdc, r, ia, la, a, RGB(118, 143, 95));
    r.left += w + 8; r.right += w + 8;
    ui_metric_chip(hdc, r, ib, lb, b, RGB(188, 154, 88));
    r.left += w + 8; r.right += w + 8;
    ui_metric_chip(hdc, r, ic, lc, c, RGB(83, 123, 166));
    cursor->y += 36;
}

static void draw_war_strength_bar(HDC hdc, UiCursor *cursor, int own, int enemy,
                                  COLORREF own_color, COLORREF enemy_color) {
    RECT bar = ui_take_rect(cursor, 14);
    int total = max(1, own + enemy);
    int split = bar.left + (bar.right - bar.left) * own / total;
    RECT left = {bar.left, bar.top + 2, split, bar.bottom - 2};
    RECT right = {split, bar.top + 2, bar.right, bar.bottom - 2};
    fill_rect(hdc, (RECT){bar.left, bar.top + 2, bar.right, bar.bottom - 2}, RGB(45, 50, 52));
    fill_rect(hdc, left, own_color);
    fill_rect(hdc, right, enemy_color);
    cursor->y += 4;
}

static void draw_army_pool(HDC hdc, UiCursor *cursor, int civ_id) {
    char text[160];
    char total[32];
    char deployed[32];
    char reserve[32];
    char support[32];
    char tribute[32];

    format_metric_value(war_current_soldiers_for_civ(civ_id), total, sizeof(total));
    format_metric_value(war_deployed_soldiers_for_civ(civ_id), deployed, sizeof(deployed));
    format_metric_value(war_available_reserve_for_civ(civ_id), reserve, sizeof(reserve));
    format_metric_value(vassal_total_callable_soldiers(civ_id), support, sizeof(support));
    format_metric_value(vassal_estimated_resource_tribute_total(civ_id), tribute, sizeof(tribute));
    ui_section(hdc, cursor, tr("National Army", "全国军队"));
    metric3(hdc, cursor, war_current_soldiers_for_civ(civ_id), war_deployed_soldiers_for_civ(civ_id),
            war_available_reserve_for_civ(civ_id), ICON_MILITARY, ICON_MILITARY, ICON_POPULATION,
            metric_label("Total", "总量"), metric_label("Deployed", "已部署"), metric_label("Reserve", "预备队"));
    snprintf(text, sizeof(text), "%s %s   %s %s   %s %s   %s %d",
             tr("Total", "总量"), total, tr("Deployed", "已部署"), deployed,
             tr("Reserve", "预备队"), reserve, tr("Fronts", "战线"), war_front_count_for_civ(civ_id));
    ui_row_text(hdc, cursor, tr("Pool", "军队池"), text);
    if (vassal_direct_count(civ_id) > 0) {
        snprintf(text, sizeof(text), "%s %s   %s %s   %s %d",
                 tr("Callable vassal army", "可调用附庸军队"), support,
                 tr("Resource tribute", "资源贡赋"), tribute,
                 tr("Governance burden", "治理负担"), vassal_governance_disorder(civ_id));
        ui_row_text(hdc, cursor, tr("Vassals", "附庸"), text);
    }
}

static void draw_war_card(HDC hdc, UiCursor *cursor, int civ_id, int other_id, ActiveWar war) {
    char text[192];
    int own = civ_id == war.attacker ? war.soldiers_a : war.soldiers_b;
    int enemy = civ_id == war.attacker ? war.soldiers_b : war.soldiers_a;
    int own_losses = civ_id == war.attacker ? war.casualties_a + war.support_casualties_a :
                                             war.casualties_b + war.support_casualties_b;
    int enemy_losses = civ_id == war.attacker ? war.casualties_b + war.support_casualties_b :
                                               war.casualties_a + war.support_casualties_a;
    int own_wins = civ_id == war.attacker ? war.wins_a : war.wins_b;
    int enemy_wins = civ_id == war.attacker ? war.wins_b : war.wins_a;
    char own_text[32];
    char enemy_text[32];
    char own_loss_text[32];
    char enemy_loss_text[32];

    snprintf(text, sizeof(text), "%s %s", tr("Front vs", "战线对手"), civilization_display_name(other_id));
    ui_row_text(hdc, cursor, tr("Active War", "战争中"), text);
    draw_war_strength_bar(hdc, cursor, own, enemy, civs[civ_id].color, civs[other_id].color);
    format_metric_value(own, own_text, sizeof(own_text));
    format_metric_value(enemy, enemy_text, sizeof(enemy_text));
    format_metric_value(own_losses, own_loss_text, sizeof(own_loss_text));
    format_metric_value(enemy_losses, enemy_loss_text, sizeof(enemy_loss_text));
    snprintf(text, sizeof(text), "%s / %s", own_text, enemy_text);
    ui_row_text(hdc, cursor, tr("Soldiers", "兵力"), text);
    snprintf(text, sizeof(text), "%s / %s", own_loss_text, enemy_loss_text);
    ui_row_text(hdc, cursor, tr("Losses", "损失"), text);
    snprintf(text, sizeof(text), "%d-%d", own_wins, enemy_wins);
    ui_row_text(hdc, cursor, tr("Wins", "胜场"), text);
}

static void draw_vassal_subrow(HDC hdc, UiCursor *cursor, int selected_id, int vassal_id, int overlord_id) {
    RECT row = ui_take_rect(cursor, 46);
    RECT swatch = {row.left + 22, row.top + 6, row.left + 34, row.top + 18};
    RECT name_rect = {row.left + 40, row.top, row.right - 92, row.top + 22};
    RECT tag_rect = {row.right - 86, row.top, row.right, row.top + 22};
    RECT note_rect = {row.left + 40, row.top + 22, row.right, row.bottom};
    char title[160];
    char note[192];
    int own_vassal = vassal_is_direct(selected_id, vassal_id);

    fill_rect(hdc, row, RGB(34, 40, 43));
    fill_rect(hdc, swatch, civs[vassal_id].color);
    snprintf(title, sizeof(title), "sub %c %.80s", civs[vassal_id].symbol, civilization_display_name(vassal_id));
    draw_text_rect(hdc, name_rect, title, ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    fill_rect(hdc, tag_rect, own_vassal ? RGB(154, 105, 178) : RGB(104, 112, 120));
    draw_center_text(hdc, tag_rect, own_vassal ? tr("Vassal", "附庸") : tr("No autonomy", "无自主权"),
                     RGB(246, 248, 250));
    if (own_vassal) {
        snprintf(note, sizeof(note), "%s %d   %s %d",
                 tr("Callable army", "可调用军队"), vassal_callable_soldiers(vassal_id),
                 tr("Resource tribute", "资源贡赋"), vassal_estimated_resource_tribute_from(vassal_id));
    } else {
        snprintf(note, sizeof(note), "%s %.64s",
                 tr("Diplomacy represented by", "外交由"),
                 overlord_id >= 0 ? civilization_display_name(overlord_id) : tr("overlord", "宗主"));
    }
    draw_text_rect(hdc, note_rect, note, ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    cursor->y += 6;
}

static void draw_relation(HDC hdc, UiCursor *cursor, int civ_id, int other_id) {
    DiplomacyRelation relation = diplomacy_relation(civ_id, other_id);
    ActiveWar war = war_state_between(civ_id, other_id);
    RECT swatch = {cursor->x, cursor->y + 5, cursor->x + 14, cursor->y + 19};
    char title[160];
    char text[192];

    fill_rect(hdc, swatch, civs[other_id].color);
    snprintf(title, sizeof(title), "%c %.80s", civs[other_id].symbol, civilization_display_name(other_id));
    draw_text_rect(hdc, (RECT){cursor->x + 22, cursor->y, cursor->x + cursor->width - 90, cursor->y + 24},
                   title, ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    fill_rect(hdc, (RECT){cursor->x + cursor->width - 82, cursor->y, cursor->x + cursor->width, cursor->y + 24},
              status_color_for_pair(civ_id, other_id, relation));
    draw_center_text(hdc, (RECT){cursor->x + cursor->width - 82, cursor->y, cursor->x + cursor->width, cursor->y + 24},
                     status_label_for_pair(civ_id, other_id, relation), RGB(246, 248, 250));
    cursor->y += 30;
    metric3(hdc, cursor, relation.relation_score, relation.border_tension, relation.trade_fit,
            ICON_CULTURE, ICON_ATTACK, ICON_ECONOMY,
            metric_label("Relation", "关系"), metric_label("Tension", "紧张"), metric_label("Trade", "贸易"));
    metric3(hdc, cursor, relation.resource_conflict, relation.border_length, relation.natural_barrier,
            ICON_DISORDER, ICON_TERRITORY, ICON_GEOGRAPHY,
            metric_label("Conflict", "冲突"), metric_label("Border", "边界"), metric_label("Barrier", "屏障"));
    snprintf(text, sizeof(text), "%s %d   %s %d",
             tr("Years known", "已知年数"), relation.years_known,
             tr("Truce left", "停战剩余"), relation.truce_years_left);
    ui_row_text(hdc, cursor, tr("Contact", "接触"), text);
    if (relation.state == DIPLOMACY_VASSAL) {
        snprintf(text, sizeof(text), "%s: %s   %s: %s",
                 tr("Overlord", "宗主"),
                 relation.overlord >= 0 ? civilization_display_name(relation.overlord) : tr("None", "无"),
                 tr("Vassal", "附庸"),
                 relation.vassal >= 0 ? civilization_display_name(relation.vassal) : tr("None", "无"));
        ui_row_text(hdc, cursor, tr("Vassalage", "附庸关系"), text);
    }
    if (vassal_is_direct(civ_id, other_id)) {
        snprintf(text, sizeof(text), "%s %d   %s %d",
                 tr("Callable army", "可调用军队"), vassal_callable_soldiers(other_id),
                 tr("Resource tribute", "资源贡赋"), vassal_estimated_resource_tribute_from(other_id));
        ui_row_text(hdc, cursor, tr("Overlord rights", "宗主权利"), text);
    } else if (vassal_is_direct(other_id, civ_id)) {
        ui_row_text(hdc, cursor, tr("Diplomacy", "外交"),
                    tr("Foreign policy is represented by the overlord.", "外交由主国代表。"));
    } else if (vassal_overlord(civ_id) >= 0 || vassal_overlord(other_id) >= 0) {
        ui_row_text(hdc, cursor, tr("Diplomacy", "外交"),
                    tr("This vassal has no autonomous diplomacy.", "附庸国无自主外交权。"));
    }
    if (war.active) draw_war_card(hdc, cursor, civ_id, other_id, war);
    cursor->y += 8;
}

static void draw_diplomacy_view_tabs(HDC hdc, UiCursor *cursor, int civ_id) {
    int selected_is_vassal = vassal_overlord(civ_id) >= 0;
    int count = diplomacy_view_count(selected_is_vassal);
    int width = (cursor->width - (count - 1) * DIPLOMACY_VIEW_TAB_GAP) / count;
    DiplomacyView active = normalize_diplomacy_view(civ_id);
    int i;

    for (i = 0; i < count; i++) {
        DiplomacyView view = diplomacy_view_at(selected_is_vassal, i);
        RECT tab = {cursor->x + i * (width + DIPLOMACY_VIEW_TAB_GAP), cursor->y,
                    cursor->x + i * (width + DIPLOMACY_VIEW_TAB_GAP) + width,
                    cursor->y + DIPLOMACY_VIEW_TAB_H};
        fill_rect(hdc, tab, view == active ? RGB(87, 93, 78) : RGB(43, 49, 52));
        draw_center_text(hdc, tab, display_group_label(selected_is_vassal, view),
                         view == active ? RGB(255, 238, 190) : ui_theme_color(UI_COLOR_TEXT));
    }
    cursor->y += DIPLOMACY_VIEW_TAB_H + 8;
}

int country_diplomacy_view_hit_test(int civ_id, RECT viewport, int scroll, int mouse_x, int mouse_y) {
    int selected_is_vassal;
    int count;
    int width;
    int i;
    int y;

    if (civ_id < 0 || civ_id >= civ_count || country_detail_subtab != COUNTRY_DETAIL_DIPLOMACY) return -1;
    selected_is_vassal = vassal_overlord(civ_id) >= 0;
    count = diplomacy_view_count(selected_is_vassal);
    width = ((viewport.right - viewport.left - 8) - (count - 1) * DIPLOMACY_VIEW_TAB_GAP) / count;
    y = viewport.top - scroll + 31;
    for (i = 0; i < count; i++) {
        RECT tab = {viewport.left + i * (width + DIPLOMACY_VIEW_TAB_GAP), y,
                    viewport.left + i * (width + DIPLOMACY_VIEW_TAB_GAP) + width,
                    y + DIPLOMACY_VIEW_TAB_H};
        if (point_in_rect(tab, mouse_x, mouse_y)) {
            return COUNTRY_PANEL_HIT_DIPLOMACY_VIEW_BASE - diplomacy_view_at(selected_is_vassal, i);
        }
    }
    return -1;
}

int country_diplomacy_tab_height(int civ_id) {
    int visible = 0;
    int i;
    DiplomacyView view = normalize_diplomacy_view(civ_id);

    for (i = 0; i < civ_count; i++) {
        if (!direct_relation_visible(civ_id, i)) continue;
        if (display_group_for_pair(civ_id, i) != view) continue;
        visible++;
        if (vassal_overlord(civ_id) < 0 && vassal_overlord(i) < 0) {
            visible += vassal_direct_count(i);
        }
    }
    return 220 + max(1, visible) * 205 + war_front_count_for_civ(civ_id) * 110;
}

static int draw_diplomacy_group(HDC hdc, UiCursor *cursor, int civ_id,
                                DiplomacyDisplayGroup group, int selected_is_vassal) {
    int shown_group = 0;
    int i;

    ui_section(hdc, cursor, display_group_label(selected_is_vassal, group));
    if (selected_is_vassal) {
        int overlord = vassal_overlord(civ_id);
        if (group == DIP_DISPLAY_VASSAL && overlord >= 0) {
            draw_relation(hdc, cursor, civ_id, overlord);
            shown_group++;
        } else if (group == DIP_DISPLAY_OTHER) {
            for (i = 0; i < civ_count; i++) {
                if (!direct_relation_visible(civ_id, i) || i == overlord) continue;
                draw_vassal_subrow(hdc, cursor, civ_id, i, overlord);
                shown_group++;
            }
        }
    } else {
        for (i = 0; i < civ_count; i++) {
            int children[MAX_CIVS];
            int child_count;
            int c;
            if (!direct_relation_visible(civ_id, i)) continue;
            if (display_group_for_pair(civ_id, i) != group) continue;
            draw_relation(hdc, cursor, civ_id, i);
            shown_group++;
            child_count = vassal_collect_direct(i, children, MAX_CIVS);
            for (c = 0; c < child_count; c++) {
                draw_vassal_subrow(hdc, cursor, civ_id, children[c], i);
                shown_group++;
            }
        }
    }
    if (shown_group == 0) {
        ui_row_text(hdc, cursor, tr("Entries", "条目"), tr("No entries in this group.", "本组暂无条目。"));
    }
    return shown_group;
}

static void draw_country_diplomacy_tab_grouped(HDC hdc, UiCursor *cursor, int civ_id) {
    int selected_is_vassal = vassal_overlord(civ_id) >= 0;
    int groups = 1;
    int group_index;
    int shown_total = 0;
    DiplomacyView view = normalize_diplomacy_view(civ_id);

    ui_section(hdc, cursor, tr("Diplomacy", "外交"));
    draw_diplomacy_view_tabs(hdc, cursor, civ_id);
    draw_army_pool(hdc, cursor, civ_id);
    for (group_index = 0; group_index < groups; group_index++) {
        DiplomacyDisplayGroup group = view;
        shown_total += draw_diplomacy_group(hdc, cursor, civ_id, group, selected_is_vassal);
    }
    if (shown_total == 0) {
        ui_row_text(hdc, cursor, tr("Known Countries", "已知国家"),
                    tr("No contacted civilizations yet.", "还没有已接触文明。"));
    }
}

void draw_country_diplomacy_tab(HDC hdc, UiCursor *cursor, int civ_id) {
    int i;
    int relations = 0;
    int ids[MAX_CIVS];

    (void)i;
    (void)relations;
    (void)ids;
    draw_country_diplomacy_tab_grouped(hdc, cursor, civ_id);
    return;

    draw_army_pool(hdc, cursor, civ_id);
    ui_section(hdc, cursor, tr("Diplomacy", "外交"));
    for (i = 0; i < civ_count; i++) {
        if (i == civ_id || !civs[i].alive) continue;
        if (diplomacy_relation(civ_id, i).state == DIPLOMACY_NONE) continue;
        ids[relations++] = i;
    }
    for (i = 1; i < relations; i++) {
        int key = ids[i];
        int rank = status_rank(diplomacy_relation(civ_id, key).state);
        int j = i - 1;
        while (j >= 0 && status_rank(diplomacy_relation(civ_id, ids[j]).state) > rank) {
            ids[j + 1] = ids[j];
            j--;
        }
        ids[j + 1] = key;
    }
    for (i = 0; i < relations; i++) {
        draw_relation(hdc, cursor, civ_id, ids[i]);
    }
    if (relations == 0) {
        ui_row_text(hdc, cursor, tr("Known Countries", "已知国家"),
                    tr("No contacted civilizations yet.", "还没有已接触文明。"));
    }
}
