#include "render/panel_country_diplomacy.h"

#include "render/panel_country.h"
#include "render/panel_country_diplomacy_cards.h"
#include "render/panel_country_diplomacy_hits.h"
#include "render/snapshot_ui.h"
#include "render/ui_format.h"
#include "render_panel_internal.h"
#include "sim/diplomacy.h"
#include "ui/ui_types.h"
#include "ui/ui_widgets.h"

#include <stdio.h>

#define DIPLOMACY_VIEW_TAB_H 28
#define DIPLOMACY_VIEW_TAB_GAP 6
typedef DiplomacyView DiplomacyDisplayGroup;
#define DIP_DISPLAY_PEACE_TENSE DIPLOMACY_VIEW_PEACE_TENSE
#define DIP_DISPLAY_WAR_TRUCE DIPLOMACY_VIEW_WAR_TRUCE
#define DIP_DISPLAY_VASSAL DIPLOMACY_VIEW_TRIBUTE_VASSAL
#define DIP_DISPLAY_OTHER DIPLOMACY_VIEW_OTHER

typedef struct {
    int id;
    int primary;
    int secondary;
    int tertiary;
} DiplomacyEntry;

static const RenderSnapshot *dip_snapshot(void) {
    return snapshot_ui_current();
}

static const SnapshotCiv *dip_civ(int civ_id) {
    return snapshot_ui_civ(civ_id);
}

static int dip_civ_alive(int civ_id) {
    const SnapshotCiv *civ = dip_civ(civ_id);
    return civ && civ->alive;
}

static int dip_civ_count(void) {
    const RenderSnapshot *snapshot = dip_snapshot();
    return snapshot ? snapshot->civ_count : 0;
}

static int dip_overlord(int civ_id) {
    const SnapshotCiv *civ = dip_civ(civ_id);
    return civ ? civ->overlord : -1;
}

static int dip_is_direct_vassal(int overlord, int vassal) {
    return dip_civ_alive(overlord) && dip_civ_alive(vassal) && dip_overlord(vassal) == overlord;
}

static int dip_direct_count(int overlord) {
    const RenderSnapshot *snapshot = dip_snapshot();
    int i, count = 0;
    if (!snapshot) return 0;
    for (i = 0; i < snapshot->civ_count; i++) {
        if (snapshot->civs[i].alive && snapshot->civs[i].overlord == overlord) count++;
    }
    return count;
}

static int dip_collect_direct(int overlord, int *out, int max_out) {
    const RenderSnapshot *snapshot = dip_snapshot();
    int i, count = 0;
    if (!snapshot) return 0;
    for (i = 0; i < snapshot->civ_count; i++) {
        if (!snapshot->civs[i].alive || snapshot->civs[i].overlord != overlord) continue;
        if (count < max_out) out[count] = i;
        count++;
    }
    return min(count, max_out);
}

static int dip_total_callable_soldiers(int overlord) {
    const RenderSnapshot *snapshot = dip_snapshot();
    int i, total = 0;
    if (!snapshot) return 0;
    for (i = 0; i < snapshot->civ_count; i++) {
        if (snapshot->civs[i].alive && snapshot->civs[i].overlord == overlord) {
            total += snapshot->civs[i].vassal_callable_soldiers;
        }
    }
    return total;
}

static int dip_total_tribute(int overlord) {
    const RenderSnapshot *snapshot = dip_snapshot();
    int i, total = 0;
    if (!snapshot) return 0;
    for (i = 0; i < snapshot->civ_count; i++) {
        if (snapshot->civs[i].alive && snapshot->civs[i].overlord == overlord) {
            total += snapshot->civs[i].vassal_resource_tribute;
        }
    }
    return total;
}

static SnapshotDiplomacyRelation dip_relation(int civ_id, int other_id) {
    const RenderSnapshot *snapshot = dip_snapshot();
    SnapshotDiplomacyRelation relation = {0};
    relation.state = DIPLOMACY_NONE;
    if (!snapshot || civ_id < 0 || other_id < 0 ||
        civ_id >= MAX_CIVS || other_id >= MAX_CIVS) return relation;
    return snapshot->relations[civ_id][other_id];
}

static SnapshotWar dip_war(int a, int b) {
    const RenderSnapshot *snapshot = dip_snapshot();
    SnapshotWar war = {0};
    if (!snapshot || a < 0 || b < 0 || a >= MAX_CIVS || b >= MAX_CIVS) return war;
    return snapshot->wars[a][b];
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
    SnapshotDiplomacyRelation relation = dip_relation(civ_id, other_id);
    if (dip_overlord(civ_id) >= 0) return other_id == dip_overlord(civ_id) ? DIP_DISPLAY_VASSAL : DIP_DISPLAY_OTHER;
    if (dip_is_direct_vassal(civ_id, other_id) || dip_is_direct_vassal(other_id, civ_id)) return DIP_DISPLAY_VASSAL;
    if (relation.state == DIPLOMACY_WAR || relation.state == DIPLOMACY_TRUCE) return DIP_DISPLAY_WAR_TRUCE;
    if (relation.state == DIPLOMACY_PEACE || relation.state == DIPLOMACY_TENSE || relation.state == DIPLOMACY_ALLIANCE) {
        return DIP_DISPLAY_PEACE_TENSE;
    }
    return DIP_DISPLAY_OTHER;
}

static DiplomacyView normalize_diplomacy_view(int civ_id) {
    int selected_is_vassal = dip_overlord(civ_id) >= 0;
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

static int diplomacy_view_count(int selected_is_vassal) { return selected_is_vassal ? 2 : 3; }

static DiplomacyView diplomacy_view_at(int selected_is_vassal, int index) {
    if (selected_is_vassal) return index == 0 ? DIPLOMACY_VIEW_TRIBUTE_VASSAL : DIPLOMACY_VIEW_OTHER;
    if (index == 1) return DIPLOMACY_VIEW_WAR_TRUCE;
    if (index == 2) return DIPLOMACY_VIEW_TRIBUTE_VASSAL;
    return DIPLOMACY_VIEW_PEACE_TENSE;
}

static int sovereign_id(int civ_id) {
    int over = dip_overlord(civ_id);
    return over >= 0 ? over : civ_id;
}

static int direct_relation_visible(int civ_id, int other_id) {
    if (!dip_civ_alive(other_id) || other_id == civ_id) return 0;
    if (dip_overlord(civ_id) >= 0) return 1;
    if (dip_is_direct_vassal(civ_id, other_id)) return 1;
    if (dip_overlord(other_id) >= 0) return 0;
    return dip_relation(civ_id, other_id).state != DIPLOMACY_NONE;
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

static void draw_army_pool(HDC hdc, UiCursor *cursor, int civ_id) {
    const SnapshotCiv *civ = dip_civ(civ_id);
    char text[192], total[32], deployed[32], reserve[32], support[32], tribute[32];
    int current = civ ? civ->current_soldiers : 0;
    int deployed_soldiers = civ ? civ->war_deployed_soldiers : 0;
    int reserve_soldiers = civ ? civ->war_available_reserve : 0;
    format_metric_value(current, total, sizeof(total));
    format_metric_value(deployed_soldiers, deployed, sizeof(deployed));
    format_metric_value(reserve_soldiers, reserve, sizeof(reserve));
    format_metric_value(dip_total_callable_soldiers(civ_id), support, sizeof(support));
    format_metric_value(dip_total_tribute(civ_id), tribute, sizeof(tribute));
    ui_section(hdc, cursor, tr("National Army", "全国军队"));
    metric3(hdc, cursor, current, deployed_soldiers, reserve_soldiers,
            ICON_MILITARY, ICON_MILITARY, ICON_POPULATION,
            metric_label("Total", "总量"), metric_label("Deployed", "已部署"), metric_label("Reserve", "预备队"));
    snprintf(text, sizeof(text), "%s %s   %s %s   %s %s   %s %d",
             tr("Total", "总量"), total, tr("Deployed", "已部署"), deployed,
             tr("Reserve", "预备队"), reserve, tr("Fronts", "战线"), civ ? civ->war_front_count : 0);
    ui_row_text(hdc, cursor, tr("Pool", "军队池"), text);
    if (dip_direct_count(civ_id) > 0) {
        snprintf(text, sizeof(text), "%s %s   %s %s   %s %d",
                 tr("Callable vassal army", "可调用附庸军队"), support,
                 tr("Resource tribute", "资源贡赋"), tribute,
                 tr("Governance burden", "治理负担"), civ ? civ->vassal_governance_disorder : 0);
        ui_row_text(hdc, cursor, tr("Vassals", "附庸"), text);
    }
}

static void sort_ids_by_army_desc(int *ids, int count) {
    int i;
    for (i = 1; i < count; i++) {
        int key = ids[i], j = i - 1;
        int key_army = dip_civ(key) ? dip_civ(key)->current_soldiers : 0;
        while (j >= 0) {
            int army = dip_civ(ids[j]) ? dip_civ(ids[j])->current_soldiers : 0;
            if (army > key_army || (army == key_army && ids[j] <= key)) break;
            ids[j + 1] = ids[j];
            j--;
        }
        ids[j + 1] = key;
    }
}

static void draw_vassal_subrow(HDC hdc, UiCursor *cursor, int selected_id, int vassal_id, int overlord_id) {
    const SnapshotCiv *vassal = dip_civ(vassal_id);
    RECT row = ui_take_rect(cursor, 46);
    RECT swatch = {row.left + 22, row.top + 6, row.left + 34, row.top + 18};
    RECT name_rect = {row.left + 40, row.top, row.right - 92, row.top + 22};
    RECT tag_rect = {row.right - 86, row.top, row.right, row.top + 22};
    RECT note_rect = {row.left + 40, row.top + 22, row.right, row.bottom};
    char title[160], note[192];
    int own_vassal = dip_is_direct_vassal(selected_id, vassal_id);
    fill_rect(hdc, row, RGB(34, 40, 43));
    country_diplomacy_hit_add(row, vassal_id);
    fill_rect(hdc, swatch, vassal ? vassal->color : RGB(96, 100, 104));
    snprintf(title, sizeof(title), "sub %c %.80s", vassal ? vassal->symbol : '?', snapshot_ui_civ_name(vassal_id));
    draw_text_rect(hdc, name_rect, title, ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    fill_rect(hdc, tag_rect, own_vassal ? RGB(154, 105, 178) : RGB(104, 112, 120));
    draw_center_text(hdc, tag_rect, own_vassal ? tr("Vassal", "附庸") : tr("No autonomy", "无自主权"),
                     RGB(246, 248, 250));
    if (own_vassal) {
        snprintf(note, sizeof(note), "%s %d   %s %d",
                 tr("Callable army", "可调用军队"), vassal ? vassal->vassal_callable_soldiers : 0,
                 tr("Resource tribute", "资源贡赋"), vassal ? vassal->vassal_resource_tribute : 0);
    } else {
        snprintf(note, sizeof(note), "%s %.64s",
                 tr("Diplomacy represented by", "外交由"),
                 overlord_id >= 0 ? snapshot_ui_civ_name(overlord_id) : tr("overlord", "宗主"));
    }
    draw_text_rect(hdc, note_rect, note, ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    cursor->y += 6;
}

static void draw_relation(HDC hdc, UiCursor *cursor, int civ_id, int other_id) {
    int start_y = cursor->y;
    draw_diplomacy_relation_card(hdc, cursor, civ_id, other_id, normalize_diplomacy_view(civ_id));
    country_diplomacy_hit_add((RECT){cursor->x, start_y, cursor->x + cursor->width, cursor->y}, other_id);
}

static int relation_war_scale(int a, int b) {
    SnapshotWar war = dip_war(a, b);
    return war.active ? war.soldiers_a + war.soldiers_b : 0;
}

static int status_order_for_vassal_other(int status) {
    if (status == DIPLOMACY_WAR) return 0;
    if (status == DIPLOMACY_TRUCE) return 1;
    if (status == DIPLOMACY_TENSE) return 2;
    if (status == DIPLOMACY_PEACE || status == DIPLOMACY_ALLIANCE) return 3;
    return 4;
}

static DiplomacyEntry make_entry(int civ_id, int other_id, DiplomacyDisplayGroup group, int selected_is_vassal) {
    int sort_a = civ_id;
    int sort_b = other_id;
    SnapshotDiplomacyRelation rel;
    DiplomacyEntry e = {other_id, 0, 0, other_id};
    if (selected_is_vassal && group == DIP_DISPLAY_OTHER) {
        sort_a = sovereign_id(civ_id);
        sort_b = sovereign_id(other_id);
    }
    rel = dip_relation(sort_a, sort_b);
    if (group == DIP_DISPLAY_PEACE_TENSE) {
        e.primary = rel.state == DIPLOMACY_TENSE ? 1 : 0;
        e.secondary = rel.state == DIPLOMACY_TENSE ? -rel.border_tension : -rel.relation_score;
    } else if (group == DIP_DISPLAY_WAR_TRUCE) {
        e.primary = rel.state == DIPLOMACY_WAR ? 0 : 1;
        e.secondary = rel.state == DIPLOMACY_WAR ? -relation_war_scale(civ_id, other_id) : rel.truce_years_left;
    } else if (group == DIP_DISPLAY_VASSAL) {
        e.primary = dip_is_direct_vassal(civ_id, other_id) ? 0 : 1;
        e.secondary = -(dip_civ(other_id) ? dip_civ(other_id)->current_soldiers : 0);
    } else {
        e.primary = status_order_for_vassal_other(rel.state);
        e.secondary = rel.state == DIPLOMACY_TENSE ? -rel.border_tension :
                      rel.state == DIPLOMACY_TRUCE ? rel.truce_years_left :
                      rel.state == DIPLOMACY_WAR ? -relation_war_scale(sort_a, sort_b) : -rel.relation_score;
    }
    return e;
}

static void sort_entries(DiplomacyEntry *entries, int count) {
    int i;
    for (i = 1; i < count; i++) {
        DiplomacyEntry key = entries[i];
        int j = i - 1;
        while (j >= 0 && (entries[j].primary > key.primary ||
               (entries[j].primary == key.primary && entries[j].secondary > key.secondary) ||
               (entries[j].primary == key.primary && entries[j].secondary == key.secondary &&
                entries[j].tertiary > key.tertiary))) {
            entries[j + 1] = entries[j];
            j--;
        }
        entries[j + 1] = key;
    }
}

static int collect_entries(int civ_id, DiplomacyDisplayGroup group, int selected_is_vassal,
                           DiplomacyEntry *entries, int max_entries) {
    int count = 0;
    int i;
    if (selected_is_vassal && group == DIP_DISPLAY_VASSAL) {
        int overlord = dip_overlord(civ_id);
        if (overlord >= 0 && count < max_entries) entries[count++] = make_entry(civ_id, overlord, group, selected_is_vassal);
        return count;
    }
    for (i = 0; i < dip_civ_count(); i++) {
        if (!direct_relation_visible(civ_id, i)) continue;
        if (!selected_is_vassal && display_group_for_pair(civ_id, i) != group) continue;
        if (selected_is_vassal && group == DIP_DISPLAY_OTHER && i == dip_overlord(civ_id)) continue;
        if (count < max_entries) entries[count++] = make_entry(civ_id, i, group, selected_is_vassal);
    }
    sort_entries(entries, count);
    return count;
}

static void draw_diplomacy_view_tabs(HDC hdc, UiCursor *cursor, int civ_id) {
    int selected_is_vassal = dip_overlord(civ_id) >= 0;
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
    int selected_is_vassal, count, width, i, y;
    if (!dip_civ_alive(civ_id) || country_detail_subtab != COUNTRY_DETAIL_DIPLOMACY) return -1;
    selected_is_vassal = dip_overlord(civ_id) >= 0;
    count = diplomacy_view_count(selected_is_vassal);
    width = ((viewport.right - viewport.left - 8) - (count - 1) * DIPLOMACY_VIEW_TAB_GAP) / count;
    y = viewport.top - scroll + 31;
    for (i = 0; i < count; i++) {
        RECT tab = {viewport.left + i * (width + DIPLOMACY_VIEW_TAB_GAP), y,
                    viewport.left + i * (width + DIPLOMACY_VIEW_TAB_GAP) + width,
                    y + DIPLOMACY_VIEW_TAB_H};
        if (point_in_rect(tab, mouse_x, mouse_y)) return COUNTRY_PANEL_HIT_DIPLOMACY_VIEW_BASE - diplomacy_view_at(selected_is_vassal, i);
    }
    return -1;
}

int country_diplomacy_tab_height(int civ_id) {
    const SnapshotCiv *civ = dip_civ(civ_id);
    int visible = 0;
    int i;
    DiplomacyView view = normalize_diplomacy_view(civ_id);
    for (i = 0; i < dip_civ_count(); i++) {
        if (!direct_relation_visible(civ_id, i)) continue;
        if (display_group_for_pair(civ_id, i) != view && dip_overlord(civ_id) < 0) continue;
        visible++;
        if (dip_overlord(civ_id) < 0 && dip_overlord(i) < 0) visible += dip_direct_count(i);
    }
    return 220 + max(1, visible) * 205 + (civ ? civ->war_front_count : 0) * 110;
}

static int draw_diplomacy_group(HDC hdc, UiCursor *cursor, int civ_id,
                                DiplomacyDisplayGroup group, int selected_is_vassal) {
    DiplomacyEntry entries[MAX_CIVS];
    int shown = 0;
    int count = collect_entries(civ_id, group, selected_is_vassal, entries, MAX_CIVS);
    int i;
    ui_section(hdc, cursor, display_group_label(selected_is_vassal, group));
    for (i = 0; i < count; i++) {
        int id = entries[i].id;
        if (selected_is_vassal && group == DIP_DISPLAY_OTHER) {
            draw_vassal_subrow(hdc, cursor, civ_id, id, dip_overlord(civ_id));
        } else {
            draw_relation(hdc, cursor, civ_id, id);
            if (!selected_is_vassal && dip_overlord(id) < 0) {
                int children[MAX_CIVS];
                int child_count = dip_collect_direct(id, children, MAX_CIVS);
                int c;
                sort_ids_by_army_desc(children, child_count);
                for (c = 0; c < child_count; c++) draw_vassal_subrow(hdc, cursor, civ_id, children[c], id);
            }
        }
        shown++;
    }
    if (shown == 0) ui_row_text(hdc, cursor, tr("Entries", "条目"), tr("No entries in this group.", "本组暂无条目。"));
    return shown;
}

void draw_country_diplomacy_tab(HDC hdc, UiCursor *cursor, int civ_id) {
    int selected_is_vassal = dip_overlord(civ_id) >= 0;
    DiplomacyView view = normalize_diplomacy_view(civ_id);
    int shown_total;
    country_diplomacy_hit_reset();
    ui_section(hdc, cursor, tr("Diplomacy", "外交"));
    draw_diplomacy_view_tabs(hdc, cursor, civ_id);
    draw_army_pool(hdc, cursor, civ_id);
    shown_total = draw_diplomacy_group(hdc, cursor, civ_id, view, selected_is_vassal);
    if (shown_total == 0) {
        ui_row_text(hdc, cursor, tr("Known Countries", "已知国家"),
                    tr("No contacted civilizations yet.", "还没有已接触文明。"));
    }
}
