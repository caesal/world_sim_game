#include "render/panel_alliance_sections.h"

#include "render/render_common.h"
#include "ui/ui_clay_primitives.h"
#include "ui/ui_clay_widgets.h"
#include "ui/ui_theme.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    int civ_id;
    int join_year;
} MemberView;

static const char *civ_name(const RenderSnapshot *snapshot, int civ_id) {
    if (!snapshot || civ_id < 0 || civ_id >= snapshot->civ_count) return "-";
    return ui_language == UI_LANG_ZH ? snapshot->civs[civ_id].name_zh : snapshot->civs[civ_id].name_en;
}

static const char *alliance_name(const RenderSnapshot *snapshot, int alliance_id) {
    const AllianceSnapshotRecord *record = alliance_panel_snapshot_record(snapshot, alliance_id);
    if (!record) return tr("Unknown Alliance", "未知联盟");
    return ui_language == UI_LANG_ZH ? record->name_zh : record->name_en;
}

static const SnapshotCiv *snap_civ(const RenderSnapshot *snapshot, int civ_id) {
    if (!snapshot || civ_id < 0 || civ_id >= snapshot->civ_count) return NULL;
    return &snapshot->civs[civ_id];
}

static int province_count(const RenderSnapshot *snapshot, int civ_id) {
    int i, count = 0;
    if (!snapshot || civ_id < 0) return 0;
    for (i = 0; i < snapshot->region_count; i++)
        if (snapshot->regions[i].alive && snapshot->regions[i].owner == civ_id) count++;
    return count;
}

static void metric_text(int value, char *out, int out_size) {
    format_metric_value(value, out, out_size);
}

static const char *sovereignty_label(const SnapshotCiv *civ) {
    if (!civ) return tr("Unknown", "未知");
    if (civ->overlord >= 0) return tr("Vassal", "附庸");
    if (civ->vassal_count > 0) return tr("Overlord", "宗主");
    return tr("Independent", "独立");
}

static const char *diplomacy_label(const SnapshotCiv *civ) {
    if (!civ) return tr("Unknown", "未知");
    return civ->war_active ? tr("War", "战争") : tr("Peace", "和平");
}

static COLORREF sovereignty_color(const SnapshotCiv *civ) {
    if (!civ) return RGB(72, 78, 82);
    if (civ->overlord >= 0) return RGB(78, 66, 96);
    if (civ->vassal_count > 0) return RGB(104, 86, 48);
    return RGB(56, 75, 66);
}

static COLORREF diplomacy_color(const SnapshotCiv *civ) {
    if (!civ) return RGB(72, 78, 82);
    return civ->war_active ? RGB(112, 58, 52) : RGB(56, 88, 64);
}

static void draw_badge(HDC hdc, RECT rect, COLORREF color, const char *text) {
    fill_rect(hdc, rect, color);
    draw_text_rect(hdc, rect, text, ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS);
}

static void draw_metric_text(HDC hdc, RECT rect, const char *label, const char *value, COLORREF color) {
    draw_text_rect(hdc, (RECT){rect.left, rect.top, rect.right, rect.top + 14},
                   label, ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_SINGLELINE | DT_CENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, (RECT){rect.left, rect.top + 15, rect.right, rect.bottom},
                   value, color, DT_SINGLELINE | DT_CENTER | DT_END_ELLIPSIS);
}

static void draw_metric_int(HDC hdc, RECT rect, const char *label, int value, COLORREF color) {
    char text[32];
    metric_text(value, text, sizeof(text));
    draw_metric_text(hdc, rect, label, text, color);
}

static int member_sort_value(const RenderSnapshot *snapshot, const MemberView *m, int column) {
    const SnapshotCiv *civ = snap_civ(snapshot, m->civ_id);
    if (!civ) return 0;
    switch (column) {
        case ALLIANCE_MEMBER_SORT_PROVINCES: return province_count(snapshot, m->civ_id);
        case ALLIANCE_MEMBER_SORT_ARMY: return civ->current_soldiers;
        case ALLIANCE_MEMBER_SORT_TECHNOLOGY:
            return civ->tech_stage * 100 + clamp(civ->tech_stage_progress_percent, 0, 99);
        case ALLIANCE_MEMBER_SORT_JOINED: return m->join_year;
        default: return civ->summary.population;
    }
}

static int member_before(const RenderSnapshot *snapshot, const MemberView *a, const MemberView *b) {
    int av = member_sort_value(snapshot, a, alliance_member_sort_column);
    int bv = member_sort_value(snapshot, b, alliance_member_sort_column);
    if (av != bv) return av > bv;
    if (a->join_year != b->join_year) return a->join_year > b->join_year;
    return a->civ_id < b->civ_id;
}

static void sort_members(const RenderSnapshot *snapshot, MemberView *members, int count) {
    int i;
    for (i = 1; i < count; i++) {
        MemberView v = members[i];
        int j = i - 1;
        while (j >= 0 && member_before(snapshot, &v, &members[j])) {
            members[j + 1] = members[j];
            j--;
        }
        members[j + 1] = v;
    }
}

void alliance_detail_draw_member_sort(HDC hdc, const AllianceMemberSortLayout *layout) {
    static const char *en[ALLIANCE_MEMBER_SORT_COUNT] = {
        "Population", "Provinces", "Army", "Technology", "Joined"
    };
    static const char *zh[ALLIANCE_MEMBER_SORT_COUNT] = {
        "人口", "省份", "军队", "科技", "加入"
    };
    int i;
    if (!layout) return;
    for (i = 0; i < ALLIANCE_MEMBER_SORT_COUNT; i++) {
        UiClayState state = ui_clay_state_for_rect(layout->buttons[i], hover_x, hover_y,
                                                   i == alliance_member_sort_column, 0);
        ui_clay_draw_tab(hdc, layout->buttons[i], state);
        draw_text_rect(hdc, layout->buttons[i], tr(en[i], zh[i]), ui_clay_text_color(state),
                       DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS);
    }
}

static void draw_member_card(HDC hdc, UiCursor *cursor, const RenderSnapshot *snapshot,
                             const MemberView *member, int leader) {
    const SnapshotCiv *civ = snap_civ(snapshot, member->civ_id);
    RECT card = ui_take_rect(cursor, 72);
    RECT swatch = {card.left + 8, card.top + 8, card.left + 24, card.top + 24};
    RECT name_rect = {card.left + 32, card.top + 5, card.right - 176, card.top + 27};
    RECT sov_rect = {card.right - 168, card.top + 6, card.right - 82, card.top + 25};
    RECT dip_rect = {card.right - 78, card.top + 6, card.right - 8, card.top + 25};
    int metric_w = (card.right - card.left - 16) / 5;
    RECT metric = {card.left + 8, card.top + 31, card.left + 8 + metric_w - 4, card.top + 68};
    char title[160], text[32];
    if (!civ) return;
    ui_clay_draw_card(hdc, card, leader ? UI_CLAY_STATE_SELECTED : UI_CLAY_STATE_NORMAL);
    fill_rect(hdc, swatch, civ->color);
    snprintf(title, sizeof(title), "%c  %.80s", civ->symbol, civ_name(snapshot, member->civ_id));
    draw_text_rect(hdc, name_rect, title, ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    if (leader) draw_badge(hdc, sov_rect, RGB(110, 86, 46), tr("Leader", "领袖"));
    else draw_badge(hdc, sov_rect, sovereignty_color(civ), sovereignty_label(civ));
    draw_badge(hdc, dip_rect, diplomacy_color(civ), diplomacy_label(civ));
    draw_metric_int(hdc, metric, metric_label("Population", "人口"),
                    civ->summary.population, ui_theme_color(UI_COLOR_TEXT));
    metric.left += metric_w; metric.right += metric_w;
    draw_metric_int(hdc, metric, metric_label("Provinces", "省份"),
                    province_count(snapshot, member->civ_id), RGB(190, 204, 216));
    metric.left += metric_w; metric.right += metric_w;
    draw_metric_int(hdc, metric, metric_label("Army", "军队"),
                    civ->current_soldiers, RGB(204, 172, 112));
    metric.left += metric_w; metric.right += metric_w;
    snprintf(text, sizeof(text), "%d.%02d", clamp(civ->tech_stage, 0, 99),
             clamp(civ->tech_stage_progress_percent, 0, 99));
    draw_metric_text(hdc, metric, metric_label("Technology", "科技"), text, RGB(147, 176, 214));
    metric.left += metric_w; metric.right += metric_w;
    snprintf(text, sizeof(text), "%d", member->join_year);
    draw_metric_text(hdc, metric, metric_label("Joined year", "加入年份"), text, RGB(184, 156, 86));
    cursor->y += 8;
}

void alliance_sections_draw_members(HDC hdc, UiCursor *cursor, const RenderSnapshot *snapshot,
                                    const AlliancePanelRow *row) {
    const AllianceSnapshotRecord *record = alliance_panel_snapshot_record(snapshot, row->alliance_id);
    MemberView leader = {-1, -1};
    MemberView members[MAX_CIVS];
    int i, count = 0;
    ui_section(hdc, cursor, tr("Formal Members", "正式成员"));
    if (!record || record->member_count <= 0) {
        ui_row_text(hdc, cursor, tr("Members", "成员"), tr("No members", "暂无成员"));
        return;
    }
    for (i = 0; i < record->member_count && i < MAX_CIVS; i++) {
        int civ_id = record->members[i];
        if (!snap_civ(snapshot, civ_id) || !snapshot->civs[civ_id].alive) continue;
        if (civ_id == row->leader_civ) leader = (MemberView){civ_id, record->joined_year_by_civ[civ_id]};
        else members[count++] = (MemberView){civ_id, record->joined_year_by_civ[civ_id]};
    }
    sort_members(snapshot, members, count);
    if (leader.civ_id >= 0) draw_member_card(hdc, cursor, snapshot, &leader, 1);
    for (i = 0; i < count; i++) draw_member_card(hdc, cursor, snapshot, &members[i], 0);
}

static int ring_index(int next, int cap, int newest_offset) {
    int index = next - 1 - newest_offset;
    while (index < 0) index += cap;
    return index % cap;
}

static const char *candidate_type_label(int type) {
    return type == ALLIANCE_CANDIDATE_REMOVAL ? tr("Removal target", "清退目标") :
                                                tr("Join candidate", "加入候选");
}

static const char *candidate_initiator_label(int initiator) {
    switch (initiator) {
        case ALLIANCE_CANDIDATE_INITIATOR_CANDIDATE:
            return tr("Candidate application", "候选国主动申请");
        case ALLIANCE_CANDIDATE_INITIATOR_ALLIANCE:
            return tr("Alliance initiated", "联盟发起");
        default:
            return tr("System", "系统");
    }
}

static const char *candidate_status_label(int status) {
    switch (status) {
        case ALLIANCE_CANDIDATE_PASSED: return tr("Passed", "通过");
        case ALLIANCE_CANDIDATE_REJECTED: return tr("Rejected", "拒绝");
        case ALLIANCE_CANDIDATE_BLOCKED: return tr("Blocked", "阻止");
        default: return tr("Active", "进行中");
    }
}

static const char *member_vote_label(int vote) {
    if (vote == ALLIANCE_MEMBER_VOTE_YES) return tr("Yes", "赞成");
    if (vote == ALLIANCE_MEMBER_VOTE_NO) return tr("No", "反对");
    if (vote == ALLIANCE_MEMBER_VOTE_ABSTAIN) return tr("Abstain", "弃权");
    return NULL;
}

static int vote_member_rows(const RenderSnapshot *snapshot, const AllianceVoteRecord *vote) {
    int i, rows = 0;
    for (i = 0; snapshot && i < snapshot->civ_count; i++)
        if (member_vote_label(vote->member_votes[i])) rows++;
    return rows;
}

static const char *vote_initiator(const AllianceSnapshotRecord *record, const AllianceVoteRecord *vote) {
    int i, want = vote->vote_type == ALLIANCE_VOTE_REMOVAL ? ALLIANCE_CANDIDATE_REMOVAL :
                  ALLIANCE_CANDIDATE_JOIN;
    if (!record || vote->vote_type == ALLIANCE_VOTE_CREATE) return tr("Alliance initiated", "联盟发起");
    for (i = 0; i < record->candidate_count && i < ALLIANCE_CANDIDATE_RECORD_CAP; i++) {
        int idx = ring_index(record->candidate_next, ALLIANCE_CANDIDATE_RECORD_CAP, i);
        const AllianceCandidateRecord *c = &record->candidates[idx];
        if (c->active && c->civ_id == vote->target_civ_id && c->type == want)
            return candidate_initiator_label(c->initiated_by);
    }
    return tr("Alliance/System", "联盟/系统");
}

static RECT draw_log_card(HDC hdc, UiCursor *cursor, int height, COLORREF accent) {
    RECT card = ui_take_rect(cursor, height);
    RECT stripe = card;
    fill_rect(hdc, card, ui_theme_color(UI_COLOR_PANEL_SOFT));
    stripe.right = stripe.left + 4;
    fill_rect(hdc, stripe, accent);
    cursor->y += 7;
    return card;
}

static void draw_candidate_card(HDC hdc, UiCursor *cursor, const RenderSnapshot *snapshot,
                                const AllianceSnapshotRecord *record,
                                const AllianceCandidateRecord *candidate) {
    RECT card = draw_log_card(hdc, cursor, 74, RGB(92, 145, 175));
    RECT title = {card.left + 10, card.top + 5, card.right - 82, card.top + 26};
    RECT chip = {card.right - 76, card.top + 7, card.right - 8, card.top + 25};
    RECT body = {card.left + 10, card.top + 29, card.right - 10, card.top + 50};
    RECT reason = {card.left + 10, card.top + 51, card.right - 10, card.bottom - 4};
    char text[256];
    snprintf(text, sizeof(text), "%d  %s  %s", candidate->candidate_year,
             candidate_type_label(candidate->type), civ_name(snapshot, candidate->civ_id));
    draw_text_rect(hdc, title, text, ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_badge(hdc, chip, RGB(62, 72, 94), candidate_status_label(candidate->status));
    snprintf(text, sizeof(text), "%s: %s   %s: %d%%   %s: %s",
             tr("Alliance", "联盟"), alliance_name(snapshot, record->id),
             tr("Progress", "进度"), candidate->qualification_progress,
             tr("Initiator", "发起者"), candidate_initiator_label(candidate->initiated_by));
    draw_text_rect(hdc, body, text, ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    if (candidate->rejection_reason != ALLIANCE_REJECT_NONE) {
        snprintf(text, sizeof(text), "%s: %s", tr("Reason", "原因"),
                 alliance_detail_reason_label(candidate->rejection_reason));
        draw_text_rect(hdc, reason, text, ui_theme_color(UI_COLOR_TEXT_MUTED),
                       DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }
}

static void draw_vote_card(HDC hdc, UiCursor *cursor, const RenderSnapshot *snapshot,
                           const AllianceSnapshotRecord *record, const AllianceVoteRecord *vote) {
    int rows = vote_member_rows(snapshot, vote);
    int h = 88 + rows * 20 + (vote->rejection_reason != ALLIANCE_REJECT_NONE ? 18 : 0);
    RECT card = draw_log_card(hdc, cursor, h, vote->passed ? RGB(92, 145, 98) : RGB(178, 94, 84));
    RECT title = {card.left + 10, card.top + 5, card.right - 74, card.top + 26};
    RECT chip = {card.right - 68, card.top + 7, card.right - 8, card.top + 25};
    RECT line = {card.left + 10, card.top + 29, card.right - 10, card.top + 48};
    char text[320];
    int i, y = card.top + 50;
    snprintf(text, sizeof(text), "%d  %s  %s", vote->vote_year,
             alliance_detail_vote_type_label(vote->vote_type),
             civ_name(snapshot, vote->target_civ_id));
    draw_text_rect(hdc, title, text, ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_badge(hdc, chip, vote->passed ? RGB(56, 88, 64) : RGB(112, 58, 52),
               vote->passed ? tr("Passed", "通过") : tr("Failed", "失败"));
    snprintf(text, sizeof(text), "%s: %s   %s: %s   %s: %d/%d",
             tr("Alliance", "联盟"), alliance_name(snapshot, record->id),
             tr("Initiator", "发起者"), vote_initiator(record, vote),
             tr("Yes/No", "赞成/反对"), vote->yes_count, vote->no_count);
    draw_text_rect(hdc, line, text, ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    for (i = 0; snapshot && i < snapshot->civ_count; i++) {
        const char *choice = member_vote_label(vote->member_votes[i]);
        if (!choice) continue;
        snprintf(text, sizeof(text), "%c  %.70s    %s", snapshot->civs[i].symbol,
                 civ_name(snapshot, i), choice);
        draw_text_rect(hdc, (RECT){card.left + 22, y, card.right - 12, y + 18},
                       text, ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        y += 20;
    }
    if (vote->rejection_reason != ALLIANCE_REJECT_NONE) {
        snprintf(text, sizeof(text), "%s: %s", tr("Reason", "原因"),
                 alliance_detail_reason_label(vote->rejection_reason));
        draw_text_rect(hdc, (RECT){card.left + 10, y, card.right - 10, y + 18},
                       text, ui_theme_color(UI_COLOR_TEXT_MUTED),
                       DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }
}

void alliance_sections_draw_votes(HDC hdc, UiCursor *cursor, const RenderSnapshot *snapshot,
                                  const AlliancePanelRow *row) {
    const AllianceSnapshotRecord *record = alliance_panel_snapshot_record(snapshot, row->alliance_id);
    int i, shown = 0;
    ui_section(hdc, cursor, tr("Candidate Applications", "候选申请"));
    if (record) {
        for (i = 0; i < record->candidate_count && i < ALLIANCE_CANDIDATE_RECORD_CAP; i++) {
            int idx = ring_index(record->candidate_next, ALLIANCE_CANDIDATE_RECORD_CAP, i);
            if (record->candidates[idx].active) {
                draw_candidate_card(hdc, cursor, snapshot, record, &record->candidates[idx]);
                shown++;
            }
        }
    }
    if (!shown) ui_row_text(hdc, cursor, tr("Candidates", "候选"),
                            tr("No candidates or removal targets.", "暂无候选或清退目标。"));
    shown = 0;
    ui_section(hdc, cursor, tr("Recent Vote Records", "最近投票记录"));
    if (record) {
        for (i = 0; i < record->vote_count && i < ALLIANCE_VOTE_RECORD_CAP; i++) {
            int idx = ring_index(record->vote_next, ALLIANCE_VOTE_RECORD_CAP, i);
            if (record->votes[idx].active) {
                draw_vote_card(hdc, cursor, snapshot, record, &record->votes[idx]);
                shown++;
            }
        }
    }
    if (!shown) ui_row_text(hdc, cursor, tr("Votes", "投票"), tr("No active votes", "暂无投票"));
}

static const char *history_label(int type) {
    switch (type) {
        case ALLIANCE_HISTORY_CREATED: return tr("Alliance created", "联盟创建");
        case ALLIANCE_HISTORY_CANDIDATE_APPEARED: return tr("Candidate appeared", "候选出现");
        case ALLIANCE_HISTORY_VOTE_RESOLVED: return tr("Vote resolved", "投票结算");
        case ALLIANCE_HISTORY_VOTE_PASSED: return tr("Vote passed", "投票通过");
        case ALLIANCE_HISTORY_VOTE_FAILED: return tr("Vote failed", "投票未通过");
        case ALLIANCE_HISTORY_MEMBER_JOINED: return tr("Member joined", "成员加入");
        case ALLIANCE_HISTORY_MEMBER_LEFT: return tr("Member left", "成员离开");
        case ALLIANCE_HISTORY_MEMBER_REMOVED: return tr("Member removed", "成员被清退");
        case ALLIANCE_HISTORY_LEADER_CHANGED: return tr("Leader changed", "领袖变更");
        case ALLIANCE_HISTORY_DISSOLVED: return tr("Alliance dissolved", "联盟解散");
        default: return tr("History", "历史");
    }
}

static void history_sentence(char *out, int out_size, const RenderSnapshot *snapshot,
                             const AllianceSnapshotRecord *record, const AllianceHistoryRecord *h) {
    const char *a = civ_name(snapshot, h->civ_id);
    const char *b = h->target_civ_id >= 0 ? civ_name(snapshot, h->target_civ_id) : "-";
    const char *name = alliance_name(snapshot, record->id);
    if (ui_language == UI_LANG_ZH) {
        snprintf(out, out_size, "%s：%s，%s / %s", name, history_label(h->event_type), a, b);
    } else {
        snprintf(out, out_size, "%s: %s, %s / %s", name, history_label(h->event_type), a, b);
    }
}

static void draw_history_card(HDC hdc, UiCursor *cursor, const RenderSnapshot *snapshot,
                              const AllianceSnapshotRecord *record, const AllianceHistoryRecord *h) {
    int has_reason = h->rejection_reason != ALLIANCE_REJECT_NONE;
    RECT card = draw_log_card(hdc, cursor, has_reason ? 82 : 64,
                              h->event_type == ALLIANCE_HISTORY_VOTE_FAILED ? RGB(178, 94, 84) : RGB(92, 145, 175));
    RECT title = {card.left + 10, card.top + 5, card.right - 10, card.top + 26};
    RECT body = {card.left + 10, card.top + 29, card.right - 10, card.top + 50};
    char text[320];
    snprintf(text, sizeof(text), "%d  %s", h->event_year, history_label(h->event_type));
    draw_text_rect(hdc, title, text, ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    history_sentence(text, sizeof(text), snapshot, record, h);
    draw_text_rect(hdc, body, text, ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    if (has_reason) {
        snprintf(text, sizeof(text), "%s: %s", tr("Reason", "原因"),
                 alliance_detail_reason_label(h->rejection_reason));
        draw_text_rect(hdc, (RECT){card.left + 10, card.top + 51, card.right - 10, card.bottom - 4},
                       text, ui_theme_color(UI_COLOR_TEXT_MUTED),
                       DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }
}

void alliance_sections_draw_history(HDC hdc, UiCursor *cursor, const RenderSnapshot *snapshot,
                                    const AlliancePanelRow *row) {
    const AllianceSnapshotRecord *record = alliance_panel_snapshot_record(snapshot, row->alliance_id);
    int i, shown = 0;
    ui_section(hdc, cursor, tr("Alliance History", "联盟历史"));
    if (record) {
        for (i = 0; i < record->history_count && i < ALLIANCE_HISTORY_RECORD_CAP; i++) {
            int idx = ring_index(record->history_next, ALLIANCE_HISTORY_RECORD_CAP, i);
            if (record->history[idx].active) {
                draw_history_card(hdc, cursor, snapshot, record, &record->history[idx]);
                shown++;
            }
        }
    }
    if (!shown) ui_row_text(hdc, cursor, tr("History", "历史"),
                            tr("No structured alliance history yet.", "暂无结构化联盟历史。"));
}

int alliance_sections_members_height(const RenderSnapshot *snapshot, const AlliancePanelRow *row) {
    const AllianceSnapshotRecord *record = row ? alliance_panel_snapshot_record(snapshot, row->alliance_id) : NULL;
    return 50 + (record ? record->member_count * 80 : 40);
}

int alliance_sections_votes_height(const RenderSnapshot *snapshot, const AlliancePanelRow *row) {
    const AllianceSnapshotRecord *record = row ? alliance_panel_snapshot_record(snapshot, row->alliance_id) : NULL;
    int i, h = 110;
    if (!record) return h + 60;
    h += max(1, record->candidate_count) * 82;
    for (i = 0; i < record->vote_count && i < ALLIANCE_VOTE_RECORD_CAP; i++) {
        int idx = ring_index(record->vote_next, ALLIANCE_VOTE_RECORD_CAP, i);
        const AllianceVoteRecord *v = &record->votes[idx];
        if (v->active) h += 96 + vote_member_rows(snapshot, v) * 20 +
                          (v->rejection_reason != ALLIANCE_REJECT_NONE ? 18 : 0);
    }
    if (record->vote_count == 0) h += 38;
    return h;
}

int alliance_sections_history_height(const RenderSnapshot *snapshot, const AlliancePanelRow *row) {
    const AllianceSnapshotRecord *record = row ? alliance_panel_snapshot_record(snapshot, row->alliance_id) : NULL;
    int i, h = 50;
    (void)snapshot;
    if (!record) return h + 60;
    for (i = 0; i < record->history_count && i < ALLIANCE_HISTORY_RECORD_CAP; i++) {
        int idx = ring_index(record->history_next, ALLIANCE_HISTORY_RECORD_CAP, i);
        const AllianceHistoryRecord *entry = &record->history[idx];
        if (entry->active) h += entry->rejection_reason != ALLIANCE_REJECT_NONE ? 90 : 72;
    }
    if (record->history_count == 0) h += 38;
    return h;
}
