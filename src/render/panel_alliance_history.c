#include "render/panel_alliance_history.h"

#include "render/panel_alliance_detail.h"
#include "render/render_common.h"
#include "ui/ui_theme.h"

#include <stdio.h>

static const char *civ_name(const RenderSnapshot *snapshot, int civ_id) {
    if (!snapshot || civ_id < 0 || civ_id >= snapshot->civ_count) return "-";
    return ui_language == UI_LANG_ZH ? snapshot->civs[civ_id].name_zh : snapshot->civs[civ_id].name_en;
}

static int valid_civ(const RenderSnapshot *snapshot, int civ_id) {
    return snapshot && civ_id >= 0 && civ_id < snapshot->civ_count;
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

static const char *history_label(int type) {
    switch (type) {
        case ALLIANCE_HISTORY_CREATED: return tr("Alliance created", "联盟创建");
        case ALLIANCE_HISTORY_CANDIDATE_APPEARED: return tr("Candidate appeared", "候选出现");
        case ALLIANCE_HISTORY_VOTE_RESOLVED: return tr("Vote resolved", "投票结算");
        case ALLIANCE_HISTORY_VOTE_PASSED: return tr("Vote passed", "投票通过");
        case ALLIANCE_HISTORY_VOTE_FAILED: return tr("Vote not passed", "投票未通过");
        case ALLIANCE_HISTORY_MEMBER_JOINED: return tr("Member joined", "成员加入");
        case ALLIANCE_HISTORY_MEMBER_LEFT: return tr("Member left", "成员离开");
        case ALLIANCE_HISTORY_MEMBER_REMOVED: return tr("Member removed", "成员被清退");
        case ALLIANCE_HISTORY_MEMBER_REMOVED_BY_WAR_DEFEAT: return tr("War removal", "战败清退");
        case ALLIANCE_HISTORY_LEADER_CHANGED: return tr("Leader changed", "领袖变更");
        case ALLIANCE_HISTORY_DISSOLVED: return tr("Alliance dissolved", "联盟解散");
        case ALLIANCE_HISTORY_UNION_FORMED: return tr("Union formed", "联合形成");
        case ALLIANCE_HISTORY_MILITARY_UPGRADE_INITIATED: return tr("Upgrade vote", "升级投票");
        case ALLIANCE_HISTORY_MILITARY_UPGRADE_PASSED: return tr("Upgrade passed", "升级通过");
        case ALLIANCE_HISTORY_MILITARY_UPGRADE_FAILED: return tr("Upgrade not passed", "升级未通过");
        case ALLIANCE_HISTORY_UPGRADED_TO_MILITARY: return tr("Military Alliance", "军事同盟");
        case ALLIANCE_HISTORY_DOWNGRADED_LEADER_COLLAPSED: return tr("Leader collapsed", "领袖崩溃");
        case ALLIANCE_HISTORY_DOWNGRADED_LEADER_FELL: return tr("Leader fell", "领袖灭亡");
        case ALLIANCE_HISTORY_DOWNGRADED_LEADER_TRANSFERRED: return tr("Leadership transfer", "领袖转移");
        case ALLIANCE_HISTORY_COUNCIL_REDISTRIBUTED: return tr("Alliance Council", "联盟议会");
        case ALLIANCE_HISTORY_UNION_VOTE_INITIATED: return tr("Union Vote", "联合投票");
        case ALLIANCE_HISTORY_UNION_VOTE_PASSED: return tr("Union passed", "联合通过");
        case ALLIANCE_HISTORY_UNION_VOTE_FAILED: return tr("Union failed", "联合未通过");
        case ALLIANCE_HISTORY_UNION_ABSORBED: return tr("Union completed", "联合完成");
        default: return tr("History", "历史");
    }
}

static COLORREF history_color(const AllianceHistoryRecord *h) {
    if (h->event_type == ALLIANCE_HISTORY_VOTE_PASSED ||
        h->event_type == ALLIANCE_HISTORY_MEMBER_JOINED ||
        h->event_type == ALLIANCE_HISTORY_CREATED ||
        h->event_type == ALLIANCE_HISTORY_UNION_FORMED ||
        h->event_type == ALLIANCE_HISTORY_UNION_VOTE_PASSED ||
        h->event_type == ALLIANCE_HISTORY_UNION_ABSORBED ||
        h->event_type == ALLIANCE_HISTORY_MILITARY_UPGRADE_PASSED ||
        h->event_type == ALLIANCE_HISTORY_UPGRADED_TO_MILITARY ||
        h->event_type == ALLIANCE_HISTORY_COUNCIL_REDISTRIBUTED) return RGB(64, 128, 78);
    if (h->event_type == ALLIANCE_HISTORY_VOTE_FAILED &&
        h->rejection_reason == ALLIANCE_REJECT_VOTE_FAILED) return RGB(132, 112, 58);
    if (h->event_type == ALLIANCE_HISTORY_VOTE_FAILED ||
        h->event_type == ALLIANCE_HISTORY_MEMBER_REMOVED ||
        h->event_type == ALLIANCE_HISTORY_MEMBER_REMOVED_BY_WAR_DEFEAT ||
        h->event_type == ALLIANCE_HISTORY_UNION_VOTE_FAILED ||
        h->event_type == ALLIANCE_HISTORY_MILITARY_UPGRADE_FAILED ||
        h->event_type == ALLIANCE_HISTORY_DOWNGRADED_LEADER_COLLAPSED ||
        h->event_type == ALLIANCE_HISTORY_DOWNGRADED_LEADER_FELL ||
        h->event_type == ALLIANCE_HISTORY_DOWNGRADED_LEADER_TRANSFERRED ||
        h->event_type == ALLIANCE_HISTORY_DISSOLVED) return RGB(148, 68, 62);
    return RGB(92, 145, 175);
}

static void draw_chip(HDC hdc, RECT *line, COLORREF color, const char *text) {
    RECT swatch;
    RECT label;
    if (!line || line->left >= line->right - 24) return;
    swatch = (RECT){line->left, line->top + 4, line->left + 12, line->bottom - 4};
    label = (RECT){swatch.right + 4, line->top, min(line->right, swatch.right + 112), line->bottom};
    fill_rect(hdc, swatch, color);
    draw_text_rect(hdc, label, text, ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    line->left = label.right + 8;
}

static void history_sentence(char *out, int out_size, const RenderSnapshot *snapshot,
                             const AllianceHistoryRecord *h) {
    int has_a = valid_civ(snapshot, h->civ_id);
    int has_b = valid_civ(snapshot, h->target_civ_id);
    const char *a = has_a ? civ_name(snapshot, h->civ_id) : alliance_name(snapshot, h->alliance_id);
    const char *b = has_b ? civ_name(snapshot, h->target_civ_id) : "";
    const char *vote = alliance_detail_vote_type_label(h->vote_type);
    const char *alliance = alliance_name(snapshot, h->alliance_id);
    if (ui_language == UI_LANG_ZH) {
        if (h->event_type == ALLIANCE_HISTORY_CANDIDATE_APPEARED)
            snprintf(out, out_size, "%s 成为加入候选。", a);
        else if (h->event_type == ALLIANCE_HISTORY_VOTE_PASSED)
            snprintf(out, out_size, "%s 的%s通过。", a, vote);
        else if (h->event_type == ALLIANCE_HISTORY_VOTE_FAILED &&
                 h->rejection_reason == ALLIANCE_REJECT_VOTE_FAILED)
            snprintf(out, out_size, "%s 的%s未通过，等待下次投票。", a, vote);
        else if (h->event_type == ALLIANCE_HISTORY_VOTE_RESOLVED)
            snprintf(out, out_size, "%s 的%s完成计票。", a, vote);
        else if (h->event_type == ALLIANCE_HISTORY_MEMBER_JOINED)
            snprintf(out, out_size, "%s 加入联盟。", a);
        else if (h->event_type == ALLIANCE_HISTORY_MEMBER_LEFT)
            snprintf(out, out_size, "%s 离开联盟。", a);
        else if (h->event_type == ALLIANCE_HISTORY_MEMBER_REMOVED)
            snprintf(out, out_size, "%s 被清退。", a);
        else if (h->event_type == ALLIANCE_HISTORY_MEMBER_REMOVED_BY_WAR_DEFEAT)
            snprintf(out, out_size, "%s国因战败被清退。", a);
        else if (h->event_type == ALLIANCE_HISTORY_CREATED)
            snprintf(out, out_size, "%s 创建。", alliance);
        else if (h->event_type == ALLIANCE_HISTORY_DISSOLVED)
            snprintf(out, out_size, "%s 解散。", alliance);
        else if (h->event_type == ALLIANCE_HISTORY_UNION_FORMED)
            snprintf(out, out_size, has_b ? "%s 完成联合，建立 %s；创始领袖为 %s。" :
                     "%s 完成联合，建立 %s。", alliance, a, b);
        else if (h->event_type == ALLIANCE_HISTORY_MILITARY_UPGRADE_INITIATED)
            snprintf(out, out_size, "%s 发起军事同盟升级投票。", alliance);
        else if (h->event_type == ALLIANCE_HISTORY_MILITARY_UPGRADE_PASSED)
            snprintf(out, out_size, "%s 的军事同盟升级投票通过。", alliance);
        else if (h->event_type == ALLIANCE_HISTORY_MILITARY_UPGRADE_FAILED)
            snprintf(out, out_size, "%s 的军事同盟升级投票未通过。", alliance);
        else if (h->event_type == ALLIANCE_HISTORY_UPGRADED_TO_MILITARY)
            snprintf(out, out_size, "%s 升级为军事同盟。", alliance);
        else if (h->event_type == ALLIANCE_HISTORY_DOWNGRADED_LEADER_COLLAPSED)
            snprintf(out, out_size, "%s 因领袖崩溃降级为防御同盟。", alliance);
        else if (h->event_type == ALLIANCE_HISTORY_DOWNGRADED_LEADER_FELL)
            snprintf(out, out_size, "%s 因领袖灭亡降级为防御同盟。", alliance);
        else if (h->event_type == ALLIANCE_HISTORY_DOWNGRADED_LEADER_TRANSFERRED)
            snprintf(out, out_size, "%s 因领袖转移降级为防御同盟。", alliance);
        else if (h->event_type == ALLIANCE_HISTORY_COUNCIL_REDISTRIBUTED)
            snprintf(out, out_size, "联盟议会重新分配席位。");
        else if (h->event_type == ALLIANCE_HISTORY_UNION_VOTE_INITIATED)
            snprintf(out, out_size, "%s国发起联合投票。", a);
        else if (h->event_type == ALLIANCE_HISTORY_UNION_VOTE_PASSED)
            snprintf(out, out_size, "%s国的联合投票通过。", a);
        else if (h->event_type == ALLIANCE_HISTORY_UNION_VOTE_FAILED)
            snprintf(out, out_size, "%s国的联合投票未通过。", a);
        else if (h->event_type == ALLIANCE_HISTORY_UNION_ABSORBED)
            snprintf(out, out_size, "%s国吞并其他成员国，完成联合。", a);
        else if (has_b) snprintf(out, out_size, "%s：%s，%s。", history_label(h->event_type), a, b);
        else snprintf(out, out_size, "%s：%s。", history_label(h->event_type), a);
    } else {
        if (h->event_type == ALLIANCE_HISTORY_CANDIDATE_APPEARED)
            snprintf(out, out_size, "%s became a candidate.", a);
        else if (h->event_type == ALLIANCE_HISTORY_VOTE_PASSED)
            snprintf(out, out_size, "%s %s passed.", a, vote);
        else if (h->event_type == ALLIANCE_HISTORY_VOTE_FAILED &&
                 h->rejection_reason == ALLIANCE_REJECT_VOTE_FAILED)
            snprintf(out, out_size, "%s %s was not passed; waiting for the next vote.", a, vote);
        else if (h->event_type == ALLIANCE_HISTORY_VOTE_RESOLVED)
            snprintf(out, out_size, "%s %s was counted.", a, vote);
        else if (h->event_type == ALLIANCE_HISTORY_MEMBER_JOINED)
            snprintf(out, out_size, "%s joined the alliance.", a);
        else if (h->event_type == ALLIANCE_HISTORY_MEMBER_LEFT)
            snprintf(out, out_size, "%s left the alliance.", a);
        else if (h->event_type == ALLIANCE_HISTORY_MEMBER_REMOVED)
            snprintf(out, out_size, "%s was removed.", a);
        else if (h->event_type == ALLIANCE_HISTORY_MEMBER_REMOVED_BY_WAR_DEFEAT)
            snprintf(out, out_size, "%s was removed after military defeat.", a);
        else if (h->event_type == ALLIANCE_HISTORY_CREATED)
            snprintf(out, out_size, "%s was created.", alliance);
        else if (h->event_type == ALLIANCE_HISTORY_DISSOLVED)
            snprintf(out, out_size, "%s dissolved.", alliance);
        else if (h->event_type == ALLIANCE_HISTORY_UNION_FORMED)
            snprintf(out, out_size, has_b ? "%s united into %s; founder leader was %s." :
                     "%s united into %s.", alliance, a, b);
        else if (h->event_type == ALLIANCE_HISTORY_MILITARY_UPGRADE_INITIATED)
            snprintf(out, out_size, "%s started a Military Alliance upgrade vote.", alliance);
        else if (h->event_type == ALLIANCE_HISTORY_MILITARY_UPGRADE_PASSED)
            snprintf(out, out_size, "%s passed the Military Alliance upgrade vote.", alliance);
        else if (h->event_type == ALLIANCE_HISTORY_MILITARY_UPGRADE_FAILED)
            snprintf(out, out_size, "%s did not pass the Military Alliance upgrade vote.", alliance);
        else if (h->event_type == ALLIANCE_HISTORY_UPGRADED_TO_MILITARY)
            snprintf(out, out_size, "%s upgraded to a Military Alliance.", alliance);
        else if (h->event_type == ALLIANCE_HISTORY_DOWNGRADED_LEADER_COLLAPSED)
            snprintf(out, out_size, "%s downgraded to a Defensive Alliance because the leader collapsed.", alliance);
        else if (h->event_type == ALLIANCE_HISTORY_DOWNGRADED_LEADER_FELL)
            snprintf(out, out_size, "%s downgraded to a Defensive Alliance because the leader fell.", alliance);
        else if (h->event_type == ALLIANCE_HISTORY_DOWNGRADED_LEADER_TRANSFERRED)
            snprintf(out, out_size, "%s downgraded to a Defensive Alliance because leadership transferred.", alliance);
        else if (h->event_type == ALLIANCE_HISTORY_COUNCIL_REDISTRIBUTED)
            snprintf(out, out_size, "Alliance council seats were redistributed.");
        else if (h->event_type == ALLIANCE_HISTORY_UNION_VOTE_INITIATED)
            snprintf(out, out_size, "%s initiated a union vote.", a);
        else if (h->event_type == ALLIANCE_HISTORY_UNION_VOTE_PASSED)
            snprintf(out, out_size, "%s's union vote passed.", a);
        else if (h->event_type == ALLIANCE_HISTORY_UNION_VOTE_FAILED)
            snprintf(out, out_size, "%s's union vote failed.", a);
        else if (h->event_type == ALLIANCE_HISTORY_UNION_ABSORBED)
            snprintf(out, out_size, "%s absorbed the other members and completed the union.", a);
        else if (has_b) snprintf(out, out_size, "%s: %s, %s.", history_label(h->event_type), a, b);
        else snprintf(out, out_size, "%s: %s.", history_label(h->event_type), a);
    }
}

static void draw_history_card(HDC hdc, UiCursor *cursor, const RenderSnapshot *snapshot,
                              const AllianceSnapshotRecord *record, const AllianceHistoryRecord *h) {
    RECT card = ui_take_rect(cursor, h->rejection_reason == ALLIANCE_REJECT_NONE ? 58 : 76);
    RECT stripe = card;
    RECT line = {card.left + 10, card.top + 5, card.right - 10, card.top + 27};
    RECT status = {card.right - 100, card.top + 7, card.right - 8, card.top + 25};
    char text[320];
    line.right = status.left - 8;
    fill_rect(hdc, card, ui_theme_color(UI_COLOR_PANEL_SOFT));
    stripe.right = stripe.left + 4;
    fill_rect(hdc, stripe, history_color(h));
    if (h->civ_id >= 0 && h->civ_id < snapshot->civ_count)
        draw_chip(hdc, &line, snapshot->civs[h->civ_id].color, civ_name(snapshot, h->civ_id));
    if (h->target_civ_id >= 0 && h->target_civ_id < snapshot->civ_count)
        draw_chip(hdc, &line, snapshot->civs[h->target_civ_id].color, civ_name(snapshot, h->target_civ_id));
    snprintf(text, sizeof(text), "[%d] %s", h->event_year, history_label(h->event_type));
    draw_text_rect(hdc, line, text, ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_SINGLELINE | DT_RIGHT | DT_VCENTER | DT_END_ELLIPSIS);
    fill_rect(hdc, status, history_color(h));
    draw_center_text(hdc, status, history_label(h->event_type), readable_text_color(history_color(h)));
    history_sentence(text, sizeof(text), snapshot, h);
    draw_text_rect(hdc, (RECT){card.left + 10, card.top + 29, card.right - 10, card.top + 51},
                   text, ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    if (h->rejection_reason != ALLIANCE_REJECT_NONE) {
        snprintf(text, sizeof(text), "%s: %s   %s: %s", tr("Reason", "原因"),
                 alliance_detail_reason_label(h->rejection_reason), tr("Alliance", "联盟"),
                 alliance_name(snapshot, record->id));
        draw_text_rect(hdc, (RECT){card.left + 10, card.top + 52, card.right - 10, card.bottom - 4},
                       text, ui_theme_color(UI_COLOR_TEXT_MUTED),
                       DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }
    cursor->y += 7;
}

void alliance_history_draw_content(HDC hdc, UiCursor *cursor, const RenderSnapshot *snapshot,
                                   const AlliancePanelRow *row) {
    const AllianceSnapshotRecord *record = alliance_panel_snapshot_record(snapshot, row->alliance_id);
    int i, shown = 0;
    ui_section(hdc, cursor, tr("Alliance History", "联盟历史"));
    if (record) for (i = 0; i < record->history_count && i < ALLIANCE_HISTORY_RECORD_CAP; i++) {
        int idx = ring_index(record->history_next, ALLIANCE_HISTORY_RECORD_CAP, i);
        if (record->history[idx].active) {
            draw_history_card(hdc, cursor, snapshot, record, &record->history[idx]);
            shown++;
        }
    }
    if (!shown) ui_row_text(hdc, cursor, tr("History", "历史"),
                            tr("No structured alliance history yet.", "暂无结构化联盟历史。"));
}

int alliance_history_content_height(const RenderSnapshot *snapshot, const AlliancePanelRow *row) {
    const AllianceSnapshotRecord *record = row ? alliance_panel_snapshot_record(snapshot, row->alliance_id) : NULL;
    int i, h = 50;
    (void)snapshot;
    if (!record) return h + 60;
    for (i = 0; i < record->history_count && i < ALLIANCE_HISTORY_RECORD_CAP; i++) {
        int idx = ring_index(record->history_next, ALLIANCE_HISTORY_RECORD_CAP, i);
        const AllianceHistoryRecord *entry = &record->history[idx];
        if (entry->active) h += entry->rejection_reason == ALLIANCE_REJECT_NONE ? 66 : 84;
    }
    if (record->history_count == 0) h += 38;
    return h;
}

void alliance_history_probe_sentence(char *out, int out_size, const RenderSnapshot *snapshot,
                                     const AllianceHistoryRecord *history) {
    history_sentence(out, out_size, snapshot, history);
}
