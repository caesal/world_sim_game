#include "render/panel_alliance_votes.h"

#include "render/panel_alliance_detail.h"
#include "render/panel_alliance_vote_state.h"
#include "render/panel_country_diplomacy_tooltip.h"
#include "render/render_common.h"
#include "sim/alliance_contact.h"
#include "ui/ui_clay_widgets.h"
#include "ui/ui_theme.h"

#include <stdio.h>

static const char *civ_name(const RenderSnapshot *snapshot, int civ_id) {
    if (!snapshot || civ_id < 0 || civ_id >= snapshot->civ_count) return "-";
    return ui_language == UI_LANG_ZH ? snapshot->civs[civ_id].name_zh : snapshot->civs[civ_id].name_en;
}

static const char *alliance_name(const RenderSnapshot *snapshot, int alliance_id) {
    const AllianceSnapshotRecord *record = alliance_panel_snapshot_record(snapshot, alliance_id);
    if (!record) return tr("Unknown Alliance", "未知联盟");
    return ui_language == UI_LANG_ZH ? record->name_zh : record->name_en;
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
    if (initiator == ALLIANCE_CANDIDATE_INITIATOR_CANDIDATE)
        return tr("Candidate application", "候选国主动申请");
    if (initiator == ALLIANCE_CANDIDATE_INITIATOR_ALLIANCE)
        return tr("Alliance initiated", "联盟发起");
    return tr("System", "系统");
}

static int elapsed_candidate_years(const RenderSnapshot *snapshot,
                                   const AllianceCandidateRecord *candidate) {
    int from_progress = clamp(candidate->qualification_progress, 0, 100) * 30 / 100;
    int from_year = snapshot ? max(0, snapshot->year - candidate->candidate_year + 1) : 0;
    return max(from_progress, min(from_year, 30));
}

static const char *candidate_status_label(const RenderSnapshot *snapshot,
                                          const AllianceCandidateRecord *candidate,
                                          const AllianceVoteRecord *last_vote) {
    switch (alliance_vote_state_phase(snapshot, candidate, last_vote)) {
        case ALLIANCE_CANDIDATE_PHASE_WAIT_RESULT: return tr("Waiting for vote result", "等待投票结果");
        case ALLIANCE_CANDIDATE_PHASE_PASSED: return tr("Passed", "通过");
        case ALLIANCE_CANDIDATE_PHASE_NOT_PASSED: return tr("Not passed", "未通过");
        case ALLIANCE_CANDIDATE_PHASE_WAIT_RETRY: return tr("Waiting for next vote", "等待下次投票");
        case ALLIANCE_CANDIDATE_PHASE_BLOCKED: return tr("Blocked", "阻塞");
        default: return tr("First vote countdown", "首次投票倒计时");
    }
}

static COLORREF vote_color(int vote) {
    if (vote == ALLIANCE_MEMBER_VOTE_YES) return RGB(58, 124, 72);
    if (vote == ALLIANCE_MEMBER_VOTE_NO) return RGB(142, 62, 58);
    if (vote == ALLIANCE_MEMBER_VOTE_ABSTAIN) return RGB(144, 112, 54);
    return RGB(82, 88, 94);
}

static const char *vote_symbol(int vote) {
    if (vote == ALLIANCE_MEMBER_VOTE_YES) return "✓";
    if (vote == ALLIANCE_MEMBER_VOTE_NO) return "X";
    if (vote == ALLIANCE_MEMBER_VOTE_ABSTAIN) return "•";
    return "-";
}

static COLORREF status_color(int passed, int retryable, int terminal) {
    if (passed) return RGB(56, 112, 68);
    if (retryable) return RGB(126, 104, 54);
    return terminal ? RGB(132, 58, 52) : RGB(62, 82, 108);
}

static void draw_badge(HDC hdc, RECT rect, COLORREF color, const char *text) {
    fill_rect(hdc, rect, color);
    draw_text_rect(hdc, rect, text, readable_text_color(color),
                   DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS);
}

static void draw_vote_box(HDC hdc, RECT rect, int vote) {
    COLORREF color = vote_color(vote);
    fill_rect(hdc, rect, color);
    draw_text_rect(hdc, rect, vote_symbol(vote), readable_text_color(color),
                   DT_SINGLELINE | DT_CENTER | DT_VCENTER);
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

static int relation_score(const RenderSnapshot *snapshot, int a, int b) {
    if (!snapshot || a < 0 || b < 0 || a >= snapshot->civ_count || b >= snapshot->civ_count) return 0;
    return snapshot->relations[a][b].relation_score;
}

static int direct_contact(const RenderSnapshot *snapshot, int a, int b) {
    if (!snapshot || a < 0 || b < 0 || a >= snapshot->civ_count || b >= snapshot->civ_count) return 0;
    return snapshot->relations[a][b].contact_kind != DIP_CONTACT_NONE;
}

static AllianceDiplomaticContactSource contact_source(const RenderSnapshot *snapshot,
                                                      const AllianceSnapshotRecord *record,
                                                      int candidate, int member) {
    int i;
    if (direct_contact(snapshot, candidate, member)) return ALLIANCE_DIP_CONTACT_DIRECT;
    for (i = 0; record && i < record->member_count && i < MAX_CIVS; i++) {
        int other = record->members[i];
        if (other == member) continue;
        if (direct_contact(snapshot, candidate, other)) return ALLIANCE_DIP_CONTACT_ALLIANCE;
    }
    return ALLIANCE_DIP_CONTACT_NONE;
}

static const char *contact_label(AllianceDiplomaticContactSource source) {
    if (source == ALLIANCE_DIP_CONTACT_DIRECT) return tr("Direct contact", "直接接触");
    if (source == ALLIANCE_DIP_CONTACT_ALLIANCE || source == ALLIANCE_DIP_CONTACT_VASSAL_PROXY)
        return tr("Alliance diplomatic contact", "联盟外交接触");
    return tr("No contact", "无接触");
}

static const char *intent_label(int score, AllianceDiplomaticContactSource source, int blocked) {
    if (blocked || source == ALLIANCE_DIP_CONTACT_NONE) return tr("Blocked", "阻塞");
    if (score >= 75) return tr("Support", "支持");
    if (score >= 60) return tr("Swing", "摇摆");
    return tr("Oppose", "反对");
}

static COLORREF intent_color(int score, AllianceDiplomaticContactSource source, int blocked) {
    if (blocked || source == ALLIANCE_DIP_CONTACT_NONE) return RGB(112, 58, 52);
    if (score >= 75) return RGB(58, 118, 72);
    if (score >= 60) return RGB(130, 112, 62);
    return RGB(136, 72, 64);
}

static void draw_progress(HDC hdc, RECT rect, int value, COLORREF color) {
    RECT fill = rect;
    fill_rect(hdc, rect, RGB(30, 35, 40));
    fill.right = fill.left + (fill.right - fill.left) * clamp(value, 0, 100) / 100;
    fill_rect(hdc, fill, color);
}

static void draw_member_intent_card(HDC hdc, RECT rect, const RenderSnapshot *snapshot,
                                    const AllianceSnapshotRecord *record,
                                    const AllianceCandidateRecord *candidate,
                                    const AllianceVoteRecord *last_vote,
                                    int member) {
    int score = relation_score(snapshot, member, candidate->civ_id);
    AllianceDiplomaticContactSource source = contact_source(snapshot, record, candidate->civ_id, member);
    AllianceCandidatePhase phase = alliance_vote_state_phase(snapshot, candidate, last_vote);
    int blocked = phase == ALLIANCE_CANDIDATE_PHASE_BLOCKED || source == ALLIANCE_DIP_CONTACT_NONE;
    RECT intent = {rect.right - 106, rect.top + 6, rect.right - 8, rect.top + 28};
    RECT chip = {rect.left + 8, rect.top + 6, intent.left - 8, rect.top + 28};
    RECT line = {rect.left + 8, rect.top + 30, rect.right - 8, rect.top + 50};
    RECT bar = {rect.left + 8, rect.top + 52, rect.right - 8, rect.top + 60};
    RECT result = {rect.right - 31, rect.top + 31, rect.right - 8, rect.top + 54};
    char text[192];
    fill_rect(hdc, rect, RGB(36, 42, 48));
    snprintf(text, sizeof(text), "%c %.52s", snapshot->civs[member].symbol, civ_name(snapshot, member));
    draw_badge(hdc, chip, snapshot->civs[member].color, text);
    draw_badge(hdc, intent, intent_color(score, source, blocked),
               intent_label(score, source, blocked));
    snprintf(text, sizeof(text), "%s %d   %s   %s %d/30",
             tr("Relation", "关系"), score, contact_label(source),
             score >= 60 ? tr("Qualifying", "合格年") : tr("Qualifying", "合格年"),
             score >= 60 && source != ALLIANCE_DIP_CONTACT_NONE ? elapsed_candidate_years(snapshot, candidate) : 0);
    if (last_vote) line.right = result.left - 8;
    draw_text_rect(hdc, line, text, ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    if (last_vote) draw_vote_box(hdc, result, last_vote->member_votes[member]);
    draw_progress(hdc, bar, clamp(score + 100, 0, 200) / 2,
                  score >= 60 && source != ALLIANCE_DIP_CONTACT_NONE ? RGB(78, 132, 86) : RGB(132, 76, 68));
    diplomacy_score_tooltip_register_bar(bar, candidate->civ_id, member);
}

static void next_vote_text(char *out, int out_size, const RenderSnapshot *snapshot,
                           const AllianceCandidateRecord *candidate,
                           const AllianceVoteRecord *last_vote) {
    int remaining = 0, total = 30;
    AllianceCandidatePhase phase = alliance_vote_state_phase(snapshot, candidate, last_vote);
    alliance_vote_state_progress(snapshot, candidate, last_vote, &remaining, &total);
    if (phase == ALLIANCE_CANDIDATE_PHASE_PASSED) snprintf(out, out_size, "%s", tr("Completed", "已完成"));
    else if (phase == ALLIANCE_CANDIDATE_PHASE_BLOCKED) snprintf(out, out_size, "%s", alliance_detail_reason_label(candidate->rejection_reason));
    else if (phase == ALLIANCE_CANDIDATE_PHASE_NOT_PASSED) snprintf(out, out_size, "%s", tr("Not passed", "未通过"));
    else if (phase == ALLIANCE_CANDIDATE_PHASE_WAIT_RETRY)
        snprintf(out, out_size, "%s %d/%d", tr("Retry countdown", "下次投票倒计时"), remaining, total);
    else if (phase == ALLIANCE_CANDIDATE_PHASE_WAIT_RESULT) snprintf(out, out_size, "%s", tr("Waiting for vote result", "等待投票结果"));
    else snprintf(out, out_size, "%s %d/%d", tr("First vote countdown", "首次投票倒计时"), remaining, total);
}

static void draw_candidate_card(HDC hdc, UiCursor *cursor, const RenderSnapshot *snapshot,
                                const AllianceSnapshotRecord *record,
                                const AllianceCandidateRecord *candidate) {
    const AllianceVoteRecord *last_vote = alliance_vote_state_previous_vote(record, candidate);
    int rows = alliance_vote_state_candidate_member_count(snapshot, record, candidate, last_vote);
    AllianceCandidatePhase phase = alliance_vote_state_phase(snapshot, candidate, last_vote);
    int retry = phase == ALLIANCE_CANDIDATE_PHASE_WAIT_RETRY ||
                phase == ALLIANCE_CANDIDATE_PHASE_NOT_PASSED;
    int term = phase == ALLIANCE_CANDIDATE_PHASE_BLOCKED;
    int remaining = 0, total = 30;
    int progress_value = alliance_vote_state_progress(snapshot, candidate, last_vote, &remaining, &total);
    int base_h = last_vote ? 142 : 126;
    RECT card = draw_log_card(hdc, cursor, base_h + rows * 68,
                              status_color(candidate->status == ALLIANCE_CANDIDATE_PASSED, retry, term));
    RECT title = {card.left + 10, card.top + 6, card.right - 286, card.top + 29};
    RECT candidate_chip = {card.right - 278, card.top + 6, card.right - 138, card.top + 29};
    RECT chip = {card.right - 130, card.top + 7, card.right - 10, card.top + 28};
    RECT progress = {card.left + 12, card.top + 58, card.right - 12, card.top + 70};
    char text[320], next[96];
    int i, y = card.top + 78;
    int vote_year = alliance_vote_state_candidate_vote_year(snapshot, candidate, last_vote);
    next_vote_text(next, sizeof(next), snapshot, candidate, last_vote);
    snprintf(text, sizeof(text), "[%d] %s", candidate->candidate_year,
             candidate_type_label(candidate->type));
    draw_text_rect(hdc, title, text, ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    if (candidate->civ_id >= 0 && candidate->civ_id < snapshot->civ_count) {
        snprintf(text, sizeof(text), "%c %.52s", snapshot->civs[candidate->civ_id].symbol,
                 civ_name(snapshot, candidate->civ_id));
        draw_badge(hdc, candidate_chip, snapshot->civs[candidate->civ_id].color, text);
    }
    draw_badge(hdc, chip, status_color(candidate->status == ALLIANCE_CANDIDATE_PASSED, retry, term),
               candidate_status_label(snapshot, candidate, last_vote));
    snprintf(text, sizeof(text), "%s: %s   %s: %s   %s: %s",
             tr("Alliance", "联盟"), alliance_name(snapshot, record->id),
             tr("Initiator", "发起者"), candidate_initiator_label(candidate->initiated_by),
             tr("Next", "下一步"), next);
    draw_text_rect(hdc, (RECT){card.left + 10, card.top + 32, card.right - 10, card.top + 54},
                   text, ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_progress(hdc, progress, progress_value,
                  term ? RGB(132, 68, 62) : RGB(82, 134, 96));
    if (last_vote) {
        snprintf(text, sizeof(text), "%s: %s %d/%d, %s",
                 tr("Previous vote", "上次投票"),
                 last_vote->passed ? tr("Passed", "通过") : tr("Not passed", "未通过"),
                 last_vote->yes_count, last_vote->no_count,
                 last_vote->passed ? tr("Result applied", "结果已执行") : tr("Not passed", "未通过"));
        draw_text_rect(hdc, (RECT){card.left + 10, card.top + 72, card.right - 10, card.top + 92},
                       text, last_vote->passed ? RGB(132, 220, 150) : RGB(226, 194, 112),
                       DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        y += 16;
    }
    for (i = 0; record && i < record->member_count && i < MAX_CIVS; i++) {
        int member = record->members[i];
        if (member < 0 || member >= snapshot->civ_count) continue;
        if (!alliance_vote_state_member_can_vote(record, member, candidate->civ_id, vote_year)) continue;
        draw_member_intent_card(hdc, (RECT){card.left + 10, y, card.right - 10, y + 62},
                                snapshot, record, candidate, last_vote, member);
        y += 68;
    }
}

static int ordered_candidate_indices(const RenderSnapshot *snapshot, const AllianceSnapshotRecord *record,
                                     int *order, int cap) {
    int i, count = 0;
    for (i = 0; record && i < record->candidate_count && i < ALLIANCE_CANDIDATE_RECORD_CAP; i++) {
        int j, idx = ring_index(record->candidate_next, ALLIANCE_CANDIDATE_RECORD_CAP, i);
        const AllianceCandidateRecord *candidate = &record->candidates[idx];
        if (!alliance_vote_state_visible(snapshot, record, candidate) || count >= cap) continue;
        for (j = count; j > 0; j--) {
            const AllianceCandidateRecord *prev = &record->candidates[order[j - 1]];
            if (!alliance_vote_state_candidate_before(snapshot, record, candidate, prev)) break;
            order[j] = order[j - 1];
        }
        order[j] = idx;
        count++;
    }
    return count;
}

static const char *vote_result_label(const AllianceVoteRecord *vote) {
    if (vote->passed) return tr("Passed", "通过");
    if (vote->rejection_reason == ALLIANCE_REJECT_VOTE_FAILED) return tr("Not passed", "未通过");
    return tr("Failure", "失败");
}

static const char *vote_next_action(const AllianceVoteRecord *vote) {
    if (vote->passed) return tr("Apply result", "执行结果");
    if (vote->rejection_reason == ALLIANCE_REJECT_VOTE_FAILED)
        return tr("Waiting for next vote", "等待下次投票");
    return alliance_detail_reason_label(vote->rejection_reason);
}

static const char *vote_initiator(const AllianceSnapshotRecord *record, const AllianceVoteRecord *vote) {
    int i, want = vote->vote_type == ALLIANCE_VOTE_REMOVAL ? ALLIANCE_CANDIDATE_REMOVAL : ALLIANCE_CANDIDATE_JOIN;
    if (!record || vote->vote_type == ALLIANCE_VOTE_CREATE) return tr("Alliance initiated", "联盟发起");
    for (i = 0; i < record->candidate_count && i < ALLIANCE_CANDIDATE_RECORD_CAP; i++) {
        int idx = ring_index(record->candidate_next, ALLIANCE_CANDIDATE_RECORD_CAP, i);
        const AllianceCandidateRecord *c = &record->candidates[idx];
        if (c->active && c->civ_id == vote->target_civ_id && c->type == want)
            return candidate_initiator_label(c->initiated_by);
    }
    return tr("Alliance/System", "联盟/系统");
}

static void draw_vote_member_row(HDC hdc, RECT row, const RenderSnapshot *snapshot, int civ_id, int vote) {
    RECT name = {row.left + 4, row.top, row.right - 34, row.bottom};
    RECT result = {row.right - 27, row.top, row.right - 4, row.bottom};
    char text[128];
    snprintf(text, sizeof(text), "%c %.72s", snapshot->civs[civ_id].symbol, civ_name(snapshot, civ_id));
    draw_badge(hdc, name, snapshot->civs[civ_id].color, text);
    draw_vote_box(hdc, result, vote);
}

static void draw_vote_card(HDC hdc, UiCursor *cursor, const RenderSnapshot *snapshot,
                           const AllianceSnapshotRecord *record, const AllianceVoteRecord *vote) {
    int rows = alliance_vote_state_vote_member_count(record, vote);
    int retry = !vote->passed && vote->rejection_reason == ALLIANCE_REJECT_VOTE_FAILED;
    RECT card = draw_log_card(hdc, cursor, 98 + rows * 24,
                              status_color(vote->passed, retry, !retry));
    RECT title = {card.left + 10, card.top + 5, card.right - 104, card.top + 27};
    RECT chip = {card.right - 98, card.top + 7, card.right - 8, card.top + 25};
    char text[320];
    int i, y = card.top + 57;
    snprintf(text, sizeof(text), "[%d] %s  %s", vote->vote_year,
             alliance_detail_vote_type_label(vote->vote_type), civ_name(snapshot, vote->target_civ_id));
    draw_text_rect(hdc, title, text, ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_badge(hdc, chip, status_color(vote->passed, retry, !retry), vote_result_label(vote));
    snprintf(text, sizeof(text), "%s: %s   %s: %s   %s: %d/%d   %s: %s",
             tr("Alliance", "联盟"), alliance_name(snapshot, record->id),
             tr("Initiator", "发起者"), vote_initiator(record, vote),
             tr("Yes/No", "赞成/反对"), vote->yes_count, vote->no_count,
             tr("Next", "下一步"), vote_next_action(vote));
    draw_text_rect(hdc, (RECT){card.left + 10, card.top + 31, card.right - 10, card.top + 52},
                   text, ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    for (i = 0; record && i < record->member_count && i < MAX_CIVS; i++) {
        int member = record->members[i];
        if (member < 0 || member >= snapshot->civ_count) continue;
        if (!alliance_vote_state_member_can_vote(record, member,
                vote->vote_type == ALLIANCE_VOTE_CREATE ? -1 : vote->target_civ_id,
                vote->vote_year)) continue;
        draw_vote_member_row(hdc, (RECT){card.left + 18, y, card.right - 12, y + 21},
                             snapshot, member, vote->member_votes[member]);
        y += 24;
    }
}

void alliance_votes_draw_content(HDC hdc, UiCursor *cursor, const RenderSnapshot *snapshot,
                                 const AlliancePanelRow *row) {
    const AllianceSnapshotRecord *record = alliance_panel_snapshot_record(snapshot, row->alliance_id);
    int i, shown = 0, order[ALLIANCE_CANDIDATE_RECORD_CAP], candidate_count;
    diplomacy_score_tooltip_begin_scope(SCORE_TOOLTIP_SCOPE_ALLIANCE_VOTES);
    ui_section(hdc, cursor, tr("Candidate Applications", "候选申请"));
    candidate_count = ordered_candidate_indices(snapshot, record, order, ALLIANCE_CANDIDATE_RECORD_CAP);
    for (i = 0; i < candidate_count; i++) {
        draw_candidate_card(hdc, cursor, snapshot, record, &record->candidates[order[i]]);
        shown++;
    }
    if (!shown) ui_row_text(hdc, cursor, tr("Candidates", "候选"),
                            tr("No active or recent applications.", "暂无活跃或近期申请。"));
    shown = 0;
    ui_section(hdc, cursor, tr("Recent Vote Records", "最近投票记录"));
    if (record) for (i = 0; i < record->vote_count && i < ALLIANCE_VOTE_RECORD_CAP; i++) {
        int idx = ring_index(record->vote_next, ALLIANCE_VOTE_RECORD_CAP, i);
        if (record->votes[idx].active) { draw_vote_card(hdc, cursor, snapshot, record, &record->votes[idx]); shown++; }
    }
    if (!shown) ui_row_text(hdc, cursor, tr("Votes", "投票"), tr("No active votes", "暂无投票"));
    diplomacy_score_tooltip_commit_scope(SCORE_TOOLTIP_SCOPE_ALLIANCE_VOTES);
}

int alliance_votes_content_height(const RenderSnapshot *snapshot, const AlliancePanelRow *row) {
    const AllianceSnapshotRecord *record = row ? alliance_panel_snapshot_record(snapshot, row->alliance_id) : NULL;
    int i, h = 110;
    if (!record) return h + 60;
    for (i = 0; i < record->candidate_count && i < ALLIANCE_CANDIDATE_RECORD_CAP; i++) {
        int idx = ring_index(record->candidate_next, ALLIANCE_CANDIDATE_RECORD_CAP, i);
        const AllianceCandidateRecord *c = &record->candidates[idx];
        if (alliance_vote_state_visible(snapshot, record, c))
            h += 134 + max(0, alliance_vote_state_candidate_member_count(
                            snapshot, record, c, alliance_vote_state_previous_vote(record, c))) * 68 +
                 (alliance_vote_state_previous_vote(record, c) ? 16 : 0);
    }
    for (i = 0; i < record->vote_count && i < ALLIANCE_VOTE_RECORD_CAP; i++) {
        int idx = ring_index(record->vote_next, ALLIANCE_VOTE_RECORD_CAP, i);
        const AllianceVoteRecord *v = &record->votes[idx];
        if (v->active) h += 106 + max(0, alliance_vote_state_vote_member_count(record, v)) * 24;
    }
    if (record->candidate_count == 0) h += 38;
    if (record->vote_count == 0) h += 38;
    return h;
}

const char *alliance_votes_probe_candidate_status_label(const RenderSnapshot *snapshot,
                                                        const AllianceCandidateRecord *candidate,
                                                        const AllianceVoteRecord *last_vote) {
    return candidate_status_label(snapshot, candidate, last_vote);
}

int alliance_votes_probe_phase_progress(const RenderSnapshot *snapshot,
                                        const AllianceCandidateRecord *candidate,
                                        const AllianceVoteRecord *last_vote,
                                        int *remaining_out, int *total_out) {
    return alliance_vote_state_progress(snapshot, candidate, last_vote, remaining_out, total_out);
}

int alliance_votes_probe_candidate_visible(const RenderSnapshot *snapshot,
                                           const AllianceCandidateRecord *candidate) {
    return alliance_vote_state_visible(snapshot, NULL, candidate);
}

const char *alliance_votes_probe_vote_symbol(int vote) {
    return vote_symbol(vote);
}
