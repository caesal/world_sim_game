#include "render/panel_alliance_detail.h"

#include "render/icons.h"
#include "render/panel_alliance_council.h"
#include "render/panel_alliance_history.h"
#include "render/panel_alliance_sections.h"
#include "render/panel_alliance_votes.h"
#include "render/render_common.h"
#include "ui/ui_clay_primitives.h"
#include "ui/ui_clay_widgets.h"
#include "ui/ui_theme.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    int icon;
    const char *label;
    char value[96];
    COLORREF accent;
} OverviewCard;

static const char *civ_name(const RenderSnapshot *snapshot, int civ_id) {
    if (!snapshot || civ_id < 0 || civ_id >= snapshot->civ_count) return "-";
    return ui_language == UI_LANG_ZH ? snapshot->civs[civ_id].name_zh : snapshot->civs[civ_id].name_en;
}

static const char *status_label(int war_count) {
    return war_count > 0 ? tr("War", "战争") : tr("Peace", "和平");
}

static const char *alliance_type_label(int type) {
    return type == ALLIANCE_TYPE_MILITARY ? tr("Military Alliance", "军事同盟") :
                                            tr("Defensive Alliance", "防御同盟");
}

static COLORREF alliance_type_color(int type) {
    return type == ALLIANCE_TYPE_MILITARY ? RGB(76, 64, 128) : RGB(104, 142, 174);
}

const char *alliance_detail_vote_type_label(int type) {
    switch (type) {
        case ALLIANCE_VOTE_CREATE: return tr("Creation vote", "创建投票");
        case ALLIANCE_VOTE_JOIN: return tr("Join vote", "加入投票");
        case ALLIANCE_VOTE_REMOVAL: return tr("Removal vote", "清退投票");
        case ALLIANCE_VOTE_MILITARY_UPGRADE: return tr("Upgrade Vote", "升级投票");
        case ALLIANCE_VOTE_UNION: return tr("Union Vote", "联合投票");
        default: return tr("Vote", "投票");
    }
}

const char *alliance_detail_reason_label(int reason) {
    switch (reason) {
        case ALLIANCE_REJECT_VOTE_FAILED: return tr("vote failed", "投票未通过");
        case ALLIANCE_REJECT_RELATION_BELOW_THRESHOLD: return tr("relation below threshold", "关系不足");
        case ALLIANCE_REJECT_COOLDOWN_ACTIVE: return tr("cooldown active", "冷却中");
        case ALLIANCE_REJECT_HARD_BLOCKER: return tr("hard blocker", "硬性阻止");
        case ALLIANCE_REJECT_BECAME_VASSAL: return tr("became vassal", "成为附庸");
        case ALLIANCE_REJECT_JOINED_ANOTHER_ALLIANCE: return tr("joined another alliance", "已加入其他同盟");
        case ALLIANCE_REJECT_ALLIANCE_DISSOLVED: return tr("alliance dissolved", "同盟已解散");
        default: return tr("none", "无");
    }
}

const char *alliance_detail_tab_label(int tab) {
    static const char *en[ALLIANCE_DETAIL_TAB_COUNT] = {"Overview", "Members", "Votes", "History", "Union"};
    static const char *zh[ALLIANCE_DETAIL_TAB_COUNT] = {"总览", "成员", "投票", "历史", "联合"};
    tab = clamp(tab, 0, ALLIANCE_DETAIL_TAB_COUNT - 1);
    return tr(en[tab], zh[tab]);
}

static void metric_text(int value, char *out, int out_size) {
    format_metric_value(value, out, out_size);
}

static RECT grid3_rect(RECT row, int col) {
    int gap = 8;
    int w = (row.right - row.left - gap * 2) / 3;
    RECT r = {row.left + col * (w + gap), row.top,
              row.left + col * (w + gap) + w, row.bottom};
    if (col == 2) r.right = row.right;
    return r;
}

static void draw_overview_card(HDC hdc, RECT rect, const OverviewCard *card) {
    RECT stripe = rect;
    RECT icon = {rect.left + 8, rect.top + (rect.bottom - rect.top - 18) / 2,
                 rect.left + 26, rect.top + (rect.bottom - rect.top + 18) / 2};
    RECT label = {rect.left + 34, rect.top + 5, rect.right - 8, rect.top + 24};
    RECT value = {rect.left + 34, rect.top + 24, rect.right - 8, rect.bottom - 5};
    ui_clay_draw_card(hdc, rect, UI_CLAY_STATE_NORMAL);
    stripe.right = stripe.left + 3;
    fill_rect(hdc, stripe, card->accent);
    draw_icon_fit(hdc, (IconId)card->icon, icon, card->accent);
    draw_text_rect(hdc, label, card->label, ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, value, card->value, ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_END_ELLIPSIS);
}

static void draw_overview(HDC hdc, UiCursor *cursor, const RenderSnapshot *snapshot,
                          const AlliancePanelRow *row) {
    const SnapshotCiv *leader = row->leader_civ >= 0 && row->leader_civ < snapshot->civ_count ?
                                &snapshot->civs[row->leader_civ] : NULL;
    OverviewCard cards[9];
    int i;
    memset(cards, 0, sizeof(cards));
    cards[0] = (OverviewCard){ICON_COUNTRY_DEFENSE, tr("Type", "类型"), "", alliance_type_color(row->type)};
    snprintf(cards[0].value, sizeof(cards[0].value), "%s", alliance_type_label(row->type));
    cards[1] = (OverviewCard){ICON_BATTLE, tr("Status", "状态"), "", row->war_count ? RGB(170, 82, 74) : RGB(90, 150, 96)};
    snprintf(cards[1].value, sizeof(cards[1].value), "%s", status_label(row->war_count));
    cards[2] = (OverviewCard){ICON_TERRITORY, tr("Founded year", "成立年份"), "", RGB(154, 128, 74)};
    snprintf(cards[2].value, sizeof(cards[2].value), "%d", row->founded_year);
    cards[3] = (OverviewCard){ICON_MIGRATION, tr("Age", "年龄"), "", RGB(116, 145, 94)};
    snprintf(cards[3].value, sizeof(cards[3].value), "%d", max(0, snapshot->year - row->founded_year));
    cards[4] = (OverviewCard){ICON_CITY_CAPITAL, tr("Leader", "领袖"), "", RGB(184, 156, 86)};
    snprintf(cards[4].value, sizeof(cards[4].value), "%.90s", leader ? civ_name(snapshot, row->leader_civ) : "-");
    cards[5] = (OverviewCard){ICON_POPULATION, tr("Members count", "成员数量"), "", RGB(74, 112, 160)};
    snprintf(cards[5].value, sizeof(cards[5].value), "%d", row->member_count);
    cards[6] = (OverviewCard){ICON_POPULATION, tr("Population", "人口"), "", RGB(74, 112, 160)};
    metric_text(row->population, cards[6].value, sizeof(cards[6].value));
    cards[7] = (OverviewCard){ICON_MILITARY, tr("Military", "军力"), "", RGB(204, 172, 112)};
    metric_text(row->military, cards[7].value, sizeof(cards[7].value));
    cards[8] = (OverviewCard){ICON_MONEY, tr("Money", "金钱"), "", RGB(169, 134, 54)};
    metric_text(row->treasury, cards[8].value, sizeof(cards[8].value));
    ui_section(hdc, cursor, tr("Overview", "总览"));
    for (i = 0; i < 9; i += 3) {
        RECT row_rect = ui_take_rect(cursor, 54);
        draw_overview_card(hdc, grid3_rect(row_rect, 0), &cards[i]);
        draw_overview_card(hdc, grid3_rect(row_rect, 1), &cards[i + 1]);
        draw_overview_card(hdc, grid3_rect(row_rect, 2), &cards[i + 2]);
        cursor->y += 8;
    }
    alliance_council_draw_overview(hdc, cursor, snapshot, row);
}

static void draw_union(HDC hdc, UiCursor *cursor, const RenderSnapshot *snapshot,
                       const AlliancePanelRow *row) {
    int latest = row->latest_join_year > 0 ? row->latest_join_year : row->founded_year;
    int elapsed = max(0, snapshot->year - latest);
    int target = alliance_union_required_years_for_type(row->type);
    int remaining = max(0, target - elapsed);
    int progress = clamp(elapsed * 100 / max(1, target), 0, 100);
    char text[128];
    ui_section(hdc, cursor, tr("Union Eligibility", "联合资格"));
    snprintf(text, sizeof(text), "%d", latest);
    ui_row_text(hdc, cursor, tr("Latest current member join year", "当前成员最晚加入年份"), text);
    snprintf(text, sizeof(text), "%d", elapsed);
    ui_row_text(hdc, cursor, tr("Elapsed years", "已过年份"), text);
    snprintf(text, sizeof(text), "%d", remaining);
    ui_row_text(hdc, cursor, tr("Years remaining", "剩余年份"), text);
    snprintf(text, sizeof(text), "%d%%", progress);
    ui_row_text(hdc, cursor, tr("Progress", "进度"), text);
    ui_clay_draw_progress_bar(hdc, ui_take_rect(cursor, 14), progress, 100, RGB(104, 158, 186));
    cursor->y += 8;
    ui_row_text(hdc, cursor, tr("Status", "状态"),
                remaining == 0 ? tr("Eligible for proposer-led union votes.", "已符合成员发起联合投票资格。") :
                tr("Not yet eligible.", "尚未符合资格。"));
    ui_row_text(hdc, cursor, tr("Scope", "范围"),
                row->type == ALLIANCE_TYPE_MILITARY ?
                tr("At 500 stable years, formal members can vote for one proposer to absorb the others.",
                   "稳定满500年后，正式成员可投票让一个发起国吞并其他成员。") :
                tr("At 800 stable years, formal members can vote for one proposer to absorb the others.",
                   "稳定满800年后，正式成员可投票让一个发起国吞并其他成员。"));
    ui_row_text(hdc, cursor, tr("Vote threshold", "投票门槛"),
                tr("Strictly greater than 3/4 council votes.", "必须严格超过3/4议会票数。"));
}

int alliance_detail_content_height(const RenderSnapshot *snapshot, const AlliancePanelRow *row) {
    if (!snapshot || !row) return 0;
    switch (clamp(alliance_detail_subtab, 0, ALLIANCE_DETAIL_TAB_COUNT - 1)) {
        case ALLIANCE_DETAIL_MEMBERS: return alliance_sections_members_height(snapshot, row);
        case ALLIANCE_DETAIL_VOTES: return alliance_votes_content_height(snapshot, row);
        case ALLIANCE_DETAIL_HISTORY: return alliance_history_content_height(snapshot, row);
        case ALLIANCE_DETAIL_UNION: return 250;
        default: return 580;
    }
}

void alliance_detail_draw_content(HDC hdc, UiCursor *cursor, const RenderSnapshot *snapshot,
                                  const AlliancePanelRow *row) {
    switch (clamp(alliance_detail_subtab, 0, ALLIANCE_DETAIL_TAB_COUNT - 1)) {
        case ALLIANCE_DETAIL_MEMBERS: alliance_sections_draw_members(hdc, cursor, snapshot, row); break;
        case ALLIANCE_DETAIL_VOTES: alliance_votes_draw_content(hdc, cursor, snapshot, row); break;
        case ALLIANCE_DETAIL_HISTORY: alliance_history_draw_content(hdc, cursor, snapshot, row); break;
        case ALLIANCE_DETAIL_UNION: draw_union(hdc, cursor, snapshot, row); break;
        default: draw_overview(hdc, cursor, snapshot, row); break;
    }
}
