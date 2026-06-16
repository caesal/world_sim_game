#include "render/panel_country_diplomacy_score.h"

#include "render/panel_country_diplomacy_tooltip.h"
#include "render/snapshot_ui.h"
#include "sim/diplomacy_stability.h"
#include "ui/ui_theme.h"

#include <stdio.h>
#include <stdlib.h>

#define SCORE_SUMMARY_H 52
#define SCORE_STATUS_ROW_H 15

static SnapshotDiplomacyRelation score_relation(int civ_id, int other_id) {
    const RenderSnapshot *snapshot = snapshot_ui_current();
    SnapshotDiplomacyRelation relation = {0};
    if (!snapshot || civ_id < 0 || other_id < 0 ||
        civ_id >= MAX_CIVS || other_id >= MAX_CIVS) return relation;
    return snapshot->relations[civ_id][other_id];
}

static int alliance_blocked(const SnapshotDiplomacyRelation *own, const SnapshotDiplomacyRelation *other) {
    if (!own || !other || own->state == DIPLOMACY_ALLIANCE) return 0;
    if (own->relation_score < 75 || other->relation_score < 75) return 0;
    return own->contact_kind == DIP_CONTACT_NONE || own->state == DIPLOMACY_WAR ||
           own->state == DIPLOMACY_TRUCE || own->state == DIPLOMACY_VASSAL;
}

static int mutual_snapshot_score(SnapshotDiplomacyRelation own, SnapshotDiplomacyRelation other) {
    return own.relation_score < other.relation_score ? own.relation_score : other.relation_score;
}

static int snapshot_severe_pressure(SnapshotDiplomacyRelation own) {
    return own.border_tension >= 75 || own.resource_conflict >= 80;
}

static int state_status_row_count(SnapshotDiplomacyRelation own, SnapshotDiplomacyRelation other) {
    int rows = 0;
    if (own.candidate_state != DIPLOMACY_NONE && own.candidate_state != own.state &&
        own.candidate_years > 0) rows++;
    if (own.state == DIPLOMACY_ALLIANCE && own.state_years < DIPLOMACY_SOFT_GRACE_YEARS) rows++;
    if (own.state == DIPLOMACY_ALLIANCE && snapshot_severe_pressure(own)) rows++;
    if (own.state == DIPLOMACY_PEACE && own.state_years < DIPLOMACY_SOFT_GRACE_YEARS &&
        (snapshot_severe_pressure(own) || mutual_snapshot_score(own, other) <= -45)) rows++;
    return rows;
}

static int status_row_count(SnapshotDiplomacyRelation own, SnapshotDiplomacyRelation other) {
    return state_status_row_count(own, other) + (alliance_blocked(&own, &other) ? 1 : 0);
}

int diplomacy_relation_score_block_height(int civ_id, int other_id) {
    SnapshotDiplomacyRelation own = score_relation(civ_id, other_id);
    SnapshotDiplomacyRelation other = score_relation(other_id, civ_id);
    return SCORE_SUMMARY_H + status_row_count(own, other) * SCORE_STATUS_ROW_H + 4;
}

static const char *alliance_block_text(const SnapshotDiplomacyRelation *own) {
    if (own->contact_kind == DIP_CONTACT_NONE) return tr("Alliance blocked: no contact", "同盟受阻：无接触");
    if (own->state == DIPLOMACY_WAR) return tr("Alliance blocked: at war", "同盟受阻：战争中");
    if (own->state == DIPLOMACY_TRUCE) return tr("Alliance blocked: truce", "同盟受阻：停战中");
    if (own->state == DIPLOMACY_VASSAL) return tr("Alliance blocked: vassal relation", "同盟受阻：附庸关系");
    return tr("Alliance blocked", "同盟受阻");
}

static const char *candidate_label(SnapshotDiplomacyRelation own) {
    if (own.candidate_state == DIPLOMACY_ALLIANCE) return tr("Alliance forming:", "同盟形成中：");
    if (own.candidate_state == DIPLOMACY_PEACE && own.state == DIPLOMACY_TENSE)
        return tr("Peace recovery:", "和平恢复中：");
    if (own.candidate_state == DIPLOMACY_TENSE) return tr("Tension building:", "紧张累积中：");
    return tr("State preparing:", "状态准备中：");
}

static const char *pressure_reason(SnapshotDiplomacyRelation own) {
    return own.border_tension >= 75 ? tr("border pressure high", "边境紧张") :
           tr("resource conflict high", "资源冲突");
}

static void format_delta_x10(int delta_x10, char *out, size_t size) {
    int abs_delta = abs(delta_x10);
    snprintf(out, size, "%c%d.%d/y", delta_x10 < 0 ? '-' : '+', abs_delta / 10, abs_delta % 10);
}

static int draw_state_status_rows(HDC hdc, UiCursor *cursor,
                                  SnapshotDiplomacyRelation own,
                                  SnapshotDiplomacyRelation other) {
    int rows = 0;
    if (own.candidate_state != DIPLOMACY_NONE && own.candidate_state != own.state &&
        own.candidate_years > 0) {
        char line[128];
        RECT detail = ui_take_rect(cursor, SCORE_STATUS_ROW_H);
        snprintf(line, sizeof(line), "%s %d / %d %s", candidate_label(own),
                 own.candidate_years, DIPLOMACY_SOFT_TRANSITION_YEARS, tr("years", "年"));
        draw_text_rect(hdc, detail, line, ui_theme_color(UI_COLOR_TEXT_MUTED),
                       DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        rows++;
    }
    if (own.state == DIPLOMACY_ALLIANCE && own.state_years < DIPLOMACY_SOFT_GRACE_YEARS) {
        char line[128];
        int left = DIPLOMACY_SOFT_GRACE_YEARS - own.state_years;
        RECT detail = ui_take_rect(cursor, SCORE_STATUS_ROW_H);
        snprintf(line, sizeof(line), "%s %d %s",
                 tr("Alliance protected:", "同盟保护期：剩余"), left, tr("years left", "年"));
        draw_text_rect(hdc, detail, line, ui_theme_color(UI_COLOR_TEXT_MUTED),
                       DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        rows++;
    }
    if (own.state == DIPLOMACY_ALLIANCE && snapshot_severe_pressure(own)) {
        char line[128];
        RECT detail = ui_take_rect(cursor, SCORE_STATUS_ROW_H);
        snprintf(line, sizeof(line), "%s %s", tr("Alliance strain:", "同盟压力："), pressure_reason(own));
        draw_text_rect(hdc, detail, line, ui_theme_color(UI_COLOR_TEXT_MUTED),
                       DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        rows++;
    }
    if (own.state == DIPLOMACY_PEACE && own.state_years < DIPLOMACY_SOFT_GRACE_YEARS &&
        (snapshot_severe_pressure(own) || mutual_snapshot_score(own, other) <= -45)) {
        RECT detail = ui_take_rect(cursor, SCORE_STATUS_ROW_H);
        const char *line = snapshot_severe_pressure(own) ?
            tr("Peace blocked: border pressure remains high", "和平受阻：边境紧张仍然过高") :
            tr("Peace blocked: attitude remains low", "和平受阻：态度仍然过低");
        draw_text_rect(hdc, detail, line, ui_theme_color(UI_COLOR_TEXT_MUTED),
                       DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        rows++;
    }
    return rows;
}

static void draw_score_metric(HDC hdc, RECT rect, const char *label, const char *value) {
    RECT label_rect = {rect.left, rect.top, rect.right, rect.top + 14};
    RECT value_rect = {rect.left, rect.top + 14, rect.right, rect.bottom};
    draw_text_rect(hdc, label_rect, label, ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, value_rect, value, ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_END_ELLIPSIS);
}

static void draw_relation_bar(HDC hdc, RECT rect, int score) {
    int mid = rect.left + (rect.right - rect.left) / 2;
    int fill;
    fill_rect(hdc, rect, RGB(43, 48, 50));
    fill_rect(hdc, (RECT){mid, rect.top - 2, mid + 1, rect.bottom + 2}, RGB(100, 106, 106));
    if (score >= 0) {
        fill = mid + (rect.right - mid) * clamp(score, 0, 100) / 100;
        fill_rect(hdc, (RECT){mid, rect.top, fill, rect.bottom}, RGB(86, 152, 218));
    } else {
        fill = mid - (mid - rect.left) * clamp(-score, 0, 100) / 100;
        fill_rect(hdc, (RECT){fill, rect.top, mid, rect.bottom}, RGB(184, 82, 76));
    }
}

void draw_diplomacy_relation_score_block(HDC hdc, UiCursor *cursor, int civ_id, int other_id) {
    SnapshotDiplomacyRelation own = score_relation(civ_id, other_id);
    SnapshotDiplomacyRelation other = score_relation(other_id, civ_id);
    RECT row = ui_take_rect(cursor, SCORE_SUMMARY_H);
    int gap = 8;
    int col_w = (row.right - row.left - gap * 2) / 3;
    RECT a = {row.left, row.top, row.left + col_w, row.top + 30};
    RECT b = {a.right + gap, row.top, a.right + gap + col_w, row.top + 30};
    RECT c = {b.right + gap, row.top, row.right, row.top + 30};
    RECT bar = {row.left, row.top + 36, row.right, row.top + 47};
    char own_score[24], other_score[24], delta[24];
    snprintf(own_score, sizeof(own_score), "%+d", own.relation_score);
    snprintf(other_score, sizeof(other_score), "%+d", other.relation_score);
    format_delta_x10(own.yearly_delta_x10, delta, sizeof(delta));
    draw_score_metric(hdc, a, tr("Our attitude", "我方态度"), own_score);
    draw_score_metric(hdc, b, tr("Their attitude", "对方态度"), other_score);
    draw_score_metric(hdc, c, tr("Yearly change", "年变化"), delta);
    draw_relation_bar(hdc, bar, own.relation_score);
    diplomacy_score_tooltip_register_bar(row, civ_id, other_id);
    draw_state_status_rows(hdc, cursor, own, other);
    if (alliance_blocked(&own, &other)) {
        RECT detail = ui_take_rect(cursor, SCORE_STATUS_ROW_H);
        draw_text_rect(hdc, detail, alliance_block_text(&own), ui_theme_color(UI_COLOR_TEXT_MUTED),
                       DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }
}
