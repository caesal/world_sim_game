#include "render/panel_country_diplomacy_tooltip.h"

#include "render/render_common.h"
#include "render/snapshot_ui.h"
#include "sim/diplomacy_relation_score.h"
#include "ui/ui_clay_primitives.h"
#include "ui/ui_theme.h"
#include "ui/ui_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCORE_TOOLTIP_MAX (MAX_CIVS + 8)
#define SCORE_TOOLTIP_W 360
#define SCORE_TOOLTIP_PAD 12
#define SCORE_TOOLTIP_ROW_H 20
#define SCORE_TOOLTIP_HEADER_H 24
#define SCORE_TOOLTIP_SUBTITLE_H 18
#define SCORE_TOOLTIP_BAR_H 12
#define SCORE_TOOLTIP_SECTION_H 22
#define SCORE_TOOLTIP_GAP 7

typedef struct {
    RECT rect;
    int civ_id;
    int other_id;
} ScoreTooltipHit;

static ScoreTooltipHit tooltip_hits[SCORE_TOOLTIP_MAX];
static int tooltip_hit_count;

static SnapshotDiplomacyRelation tooltip_relation(int civ_id, int other_id) {
    const RenderSnapshot *snapshot = snapshot_ui_current();
    SnapshotDiplomacyRelation relation = {0};
    if (!snapshot || civ_id < 0 || other_id < 0 ||
        civ_id >= MAX_CIVS || other_id >= MAX_CIVS) return relation;
    return snapshot->relations[civ_id][other_id];
}

static const char *factor_label(int id) {
    switch ((DiplomacyRelationFactor)id) {
        case DIP_REL_FACTOR_CONTACT: return tr("Contact", "接触");
        case DIP_REL_FACTOR_HERITAGE: return tr("Same heritage", "同源文明");
        case DIP_REL_FACTOR_TRADE: return tr("Trade fit", "贸易互补");
        case DIP_REL_FACTOR_LONG_PEACE: return tr("Long peace", "长期和平");
        case DIP_REL_FACTOR_ALLIANCE: return tr("Alliance", "同盟");
        case DIP_REL_FACTOR_SHARED_WAR: return tr("Shared war", "共同作战");
        case DIP_REL_FACTOR_SHARED_THREAT: return tr("Shared threat", "共同威胁");
        case DIP_REL_FACTOR_POWER: return tr("Opponent power", "对方强势");
        case DIP_REL_FACTOR_RESOURCE: return tr("Resource conflict", "资源冲突");
        case DIP_REL_FACTOR_BORDER: return tr("Border friction", "边境摩擦");
        case DIP_REL_FACTOR_TRUCE_RECOVERY: return tr("Truce recovery", "停战恢复");
        case DIP_REL_FACTOR_QUIET_DRIFT: return tr("Quiet drift", "自然回归");
        case DIP_REL_FACTOR_WAR_MEMORY: return tr("War memory", "战争记忆");
        default: return tr("Other", "其他");
    }
}

static void format_factor_value(int id, int value, char *out, size_t size) {
    if (id == DIP_REL_FACTOR_CONTACT) {
        const char *label = value == DIP_CONTACT_LAND_BORDER ? tr("land", "陆地") :
                            value == DIP_CONTACT_SHALLOW_SEA_NETWORK ? tr("sea", "海路") :
                            value == DIP_CONTACT_DEEP_SEA_NETWORK ? tr("deep sea", "深海") :
                            value == DIP_CONTACT_VASSAL_PROXY ? tr("vassal", "附庸") : "-";
        snprintf(out, size, "%s", label);
    } else if (id == DIP_REL_FACTOR_POWER) {
        snprintf(out, size, "%d%%", value);
    } else if (id == DIP_REL_FACTOR_LONG_PEACE) {
        snprintf(out, size, ui_language == UI_LANG_ZH ? "%d年" : "%dy", value);
    } else if (value > 0) {
        snprintf(out, size, "%d", value);
    } else {
        snprintf(out, size, "-");
    }
}

static void format_delta_x10(int delta_x10, char *out, size_t size) {
    int abs_delta = abs(delta_x10);
    snprintf(out, size, "%c%d.%d/y", delta_x10 < 0 ? '-' : '+',
             abs_delta / 10, abs_delta % 10);
}

static int relation_factor_sum(SnapshotDiplomacyRelation relation) {
    int i, sum = 0;
    for (i = 0; i < DIP_REL_FACTOR_SLOTS; i++) {
        if (relation.relation_factor_ids[i] == DIP_REL_FACTOR_NONE) continue;
        sum += relation.relation_factor_delta_x10[i];
    }
    return sum;
}

int diplomacy_score_tooltip_net_for_relation(int civ_id, int other_id,
                                             int *factor_sum_x10,
                                             int *other_x10,
                                             int *display_x10) {
    SnapshotDiplomacyRelation relation = tooltip_relation(civ_id, other_id);
    int sum = relation_factor_sum(relation);
    int other = relation.yearly_delta_x10 - sum;
    if (factor_sum_x10) *factor_sum_x10 = sum;
    if (other_x10) *other_x10 = other;
    if (display_x10) *display_x10 = relation.yearly_delta_x10;
    return sum + other == relation.yearly_delta_x10;
}

void diplomacy_score_tooltip_begin(void) {
    tooltip_hit_count = 0;
}

void diplomacy_score_tooltip_register_bar(RECT rect, int civ_id, int other_id) {
    if (tooltip_hit_count >= SCORE_TOOLTIP_MAX) return;
    tooltip_hits[tooltip_hit_count].rect = rect;
    tooltip_hits[tooltip_hit_count].civ_id = civ_id;
    tooltip_hits[tooltip_hit_count].other_id = other_id;
    tooltip_hit_count++;
}

int diplomacy_score_tooltip_registered_count(void) {
    return tooltip_hit_count;
}

int diplomacy_score_tooltip_hit_test(int mouse_x, int mouse_y, int *civ_id, int *other_id) {
    int i;
    for (i = 0; i < tooltip_hit_count; i++) {
        if (!point_in_rect(tooltip_hits[i].rect, mouse_x, mouse_y)) continue;
        if (civ_id) *civ_id = tooltip_hits[i].civ_id;
        if (other_id) *other_id = tooltip_hits[i].other_id;
        return 1;
    }
    return 0;
}

int diplomacy_score_tooltip_hover_key(int mouse_x, int mouse_y) {
    int civ_id = -1;
    int other_id = -1;
    if (!diplomacy_score_tooltip_hit_test(mouse_x, mouse_y, &civ_id, &other_id)) return 0;
    if (civ_id < 0 || other_id < 0 || civ_id >= MAX_CIVS || other_id >= MAX_CIVS) return 0;
    return 1 + civ_id * MAX_CIVS + other_id;
}

static int hovered_hit(void) {
    int i;
    for (i = 0; i < tooltip_hit_count; i++) {
        if (point_in_rect(tooltip_hits[i].rect, hover_x, hover_y)) return i;
    }
    return -1;
}

static int count_signed_rows(SnapshotDiplomacyRelation relation, int positive, int other_x10) {
    int i, rows = other_x10 && ((other_x10 > 0) == positive) ? 1 : 0;
    for (i = 0; i < DIP_REL_FACTOR_SLOTS; i++) {
        int delta = relation.relation_factor_delta_x10[i];
        if (relation.relation_factor_ids[i] == DIP_REL_FACTOR_NONE || delta == 0) continue;
        if ((delta > 0) == positive) rows++;
    }
    return rows;
}

static int signed_subtotal(SnapshotDiplomacyRelation relation, int positive, int other_x10) {
    int i, total = 0;
    for (i = 0; i < DIP_REL_FACTOR_SLOTS; i++) {
        int delta = relation.relation_factor_delta_x10[i];
        if (relation.relation_factor_ids[i] == DIP_REL_FACTOR_NONE || delta == 0) continue;
        if ((delta > 0) == positive) total += delta;
    }
    if (other_x10 && ((other_x10 > 0) == positive)) total += other_x10;
    return total;
}

static const char *change_subtitle(int display_x10) {
    if (display_x10 > 0) return tr("Relationship is improving", "关系正在改善");
    if (display_x10 < 0) return tr("Relationship is worsening", "关系正在恶化");
    return tr("Relationship is stable", "关系保持稳定");
}

static UiClaySemanticTone tone_for_state(int state) {
    switch (state) {
        case DIPLOMACY_ALLIANCE: return UI_CLAY_TONE_ALLIANCE;
        case DIPLOMACY_PEACE: return UI_CLAY_TONE_PEACE;
        case DIPLOMACY_TENSE: return UI_CLAY_TONE_TENSE;
        case DIPLOMACY_TRUCE: return UI_CLAY_TONE_TRUCE;
        case DIPLOMACY_WAR: return UI_CLAY_TONE_WAR;
        case DIPLOMACY_VASSAL: return UI_CLAY_TONE_VASSAL;
        default: return UI_CLAY_TONE_NEUTRAL;
    }
}

static void draw_delta_text(HDC hdc, RECT rect, int delta_x10, COLORREF color) {
    char delta[24];
    format_delta_x10(delta_x10, delta, sizeof(delta));
    draw_text_rect(hdc, rect, delta, color,
                   DT_SINGLELINE | DT_RIGHT | DT_VCENTER | DT_END_ELLIPSIS);
}

static void draw_factor_row(HDC hdc, RECT row, const char *label,
                            const char *value, int delta_x10, COLORREF delta_color) {
    RECT marker = {row.left, row.top + 5, row.left + 3, row.bottom - 5};
    RECT label_rect = {row.left + 9, row.top, row.right - 118, row.bottom};
    RECT value_rect = {row.right - 116, row.top, row.right - 64, row.bottom};
    RECT delta_rect = {row.right - 60, row.top, row.right, row.bottom};
    fill_rect(hdc, marker, delta_color);
    draw_text_rect(hdc, label_rect, label, ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, value_rect, value, ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_RIGHT | DT_VCENTER | DT_END_ELLIPSIS);
    draw_delta_text(hdc, delta_rect, delta_x10, delta_color);
}

static void draw_section_header(HDC hdc, RECT row, const char *label, int subtotal_x10,
                                COLORREF color) {
    RECT label_rect = {row.left, row.top, row.right - 82, row.bottom};
    RECT total_rect = {row.right - 80, row.top, row.right, row.bottom};
    draw_text_rect(hdc, label_rect, label, ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_delta_text(hdc, total_rect, subtotal_x10, color);
}

static void draw_net_bar(HDC hdc, RECT rect, int display_x10,
                         COLORREF positive, COLORREF negative) {
    int mid = rect.left + (rect.right - rect.left) / 2;
    int max_extent = max(1, (rect.right - rect.left) / 2);
    int extent = min(abs(display_x10), 40) * max_extent / 40;
    RECT track = {rect.left, rect.top + 5, rect.right, rect.top + 8};
    fill_rect(hdc, track, RGB(44, 52, 54));
    fill_rect(hdc, (RECT){mid, rect.top + 3, mid + 1, rect.bottom - 2}, RGB(112, 122, 122));
    if (display_x10 > 0) {
        fill_rect(hdc, (RECT){mid, rect.top + 4, mid + extent, rect.top + 9}, positive);
    } else if (display_x10 < 0) {
        fill_rect(hdc, (RECT){mid - extent, rect.top + 4, mid, rect.top + 9}, negative);
    }
}

static int draw_group(HDC hdc, RECT *cursor, SnapshotDiplomacyRelation relation,
                      int positive, int other_x10, COLORREF color) {
    int i, rows = 0;
    RECT header = {cursor->left, cursor->top, cursor->right, cursor->top + SCORE_TOOLTIP_SECTION_H};
    const char *label = positive ? tr("Positive influence", "正面影响") :
                                  tr("Negative pressure", "负面压力");
    draw_section_header(hdc, header, label, signed_subtotal(relation, positive, other_x10), color);
    cursor->top += SCORE_TOOLTIP_SECTION_H;
    for (i = 0; i < DIP_REL_FACTOR_SLOTS; i++) {
        int id = relation.relation_factor_ids[i];
        int delta = relation.relation_factor_delta_x10[i];
        char value[24];
        RECT row;
        if (id == DIP_REL_FACTOR_NONE || delta == 0 || ((delta > 0) != positive)) continue;
        format_factor_value(id, relation.relation_factor_values[i], value, sizeof(value));
        row = (RECT){cursor->left, cursor->top, cursor->right, cursor->top + SCORE_TOOLTIP_ROW_H};
        draw_factor_row(hdc, row, factor_label(id), value, delta, color);
        cursor->top += SCORE_TOOLTIP_ROW_H;
        rows++;
    }
    if (other_x10 && ((other_x10 > 0) == positive)) {
        RECT row = {cursor->left, cursor->top, cursor->right, cursor->top + SCORE_TOOLTIP_ROW_H};
        draw_factor_row(hdc, row, tr("Unattributed adjustment", "未归因调整"), "-",
                        other_x10, color);
        cursor->top += SCORE_TOOLTIP_ROW_H;
        rows++;
    }
    if (rows == 0) {
        RECT row = {cursor->left, cursor->top, cursor->right, cursor->top + SCORE_TOOLTIP_ROW_H};
        draw_text_rect(hdc, row, tr("None", "无"), ui_theme_color(UI_COLOR_TEXT_DIM),
                       DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        cursor->top += SCORE_TOOLTIP_ROW_H;
    }
    return rows;
}

void diplomacy_score_tooltip_draw(HDC hdc, RECT bounds) {
    int hit = hovered_hit();
    SnapshotDiplomacyRelation relation;
    int factor_sum, other_x10, display_x10, positive_rows, negative_rows;
    int height, width = SCORE_TOOLTIP_W;
    RECT box, inner, header, subtitle, bar;
    COLORREF positive = RGB(92, 172, 126);
    COLORREF negative = RGB(206, 101, 92);
    COLORREF net_color;
    UiClaySemanticStyle semantic;
    HFONT header_font = NULL;
    HGDIOBJ old_font = NULL;
    if (hit < 0 || !hdc) return;
    relation = tooltip_relation(tooltip_hits[hit].civ_id, tooltip_hits[hit].other_id);
    diplomacy_score_tooltip_net_for_relation(tooltip_hits[hit].civ_id, tooltip_hits[hit].other_id,
                                             &factor_sum, &other_x10, &display_x10);
    positive_rows = count_signed_rows(relation, 1, other_x10);
    negative_rows = count_signed_rows(relation, 0, other_x10);
    height = SCORE_TOOLTIP_PAD * 2 + SCORE_TOOLTIP_HEADER_H + SCORE_TOOLTIP_SUBTITLE_H +
             SCORE_TOOLTIP_BAR_H + SCORE_TOOLTIP_GAP * 2 + SCORE_TOOLTIP_SECTION_H * 2 +
             SCORE_TOOLTIP_ROW_H * (max(1, positive_rows) + max(1, negative_rows));
    box = (RECT){hover_x + 14, hover_y + 18, hover_x + 14 + width, hover_y + 18 + height};
    if (box.right > bounds.right - 4) { box.left = bounds.right - 4 - width; box.right = bounds.right - 4; }
    if (box.bottom > bounds.bottom - 4) { box.top = bounds.bottom - 4 - height; box.bottom = bounds.bottom - 4; }
    if (box.left < bounds.left + 4) { box.right += bounds.left + 4 - box.left; box.left = bounds.left + 4; }
    semantic = ui_clay_semantic_style(tone_for_state(relation.state));
    net_color = display_x10 < 0 ? negative : display_x10 > 0 ? positive : ui_theme_color(UI_COLOR_TEXT_MUTED);
    ui_clay_draw_card(hdc, box, UI_CLAY_STATE_NORMAL);
    fill_rect(hdc, (RECT){box.left + 3, box.top + 12, box.left + 6, box.bottom - 12}, semantic.accent);
    inner = (RECT){box.left + SCORE_TOOLTIP_PAD, box.top + SCORE_TOOLTIP_PAD,
                   box.right - SCORE_TOOLTIP_PAD, box.bottom - SCORE_TOOLTIP_PAD};
    header = (RECT){inner.left, inner.top, inner.right, inner.top + SCORE_TOOLTIP_HEADER_H};
    header_font = CreateFontW(-16, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                              DEFAULT_PITCH | FF_SWISS,
                              ui_language == UI_LANG_ZH ? L"Microsoft YaHei UI" : L"Segoe UI");
    if (header_font) old_font = SelectObject(hdc, header_font);
    draw_text_rect(hdc, (RECT){header.left, header.top, header.right - 92, header.bottom},
                   tr("Yearly relation change", "每年关系变化"),
                   ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_delta_text(hdc, (RECT){header.right - 90, header.top, header.right, header.bottom},
                    display_x10, net_color);
    if (header_font && old_font) SelectObject(hdc, old_font);
    if (header_font) DeleteObject(header_font);
    inner.top += SCORE_TOOLTIP_HEADER_H;
    subtitle = (RECT){inner.left, inner.top, inner.right, inner.top + SCORE_TOOLTIP_SUBTITLE_H};
    draw_text_rect(hdc, subtitle, change_subtitle(display_x10), ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    inner.top += SCORE_TOOLTIP_SUBTITLE_H;
    bar = (RECT){inner.left, inner.top, inner.right, inner.top + SCORE_TOOLTIP_BAR_H};
    draw_net_bar(hdc, bar, display_x10, positive, negative);
    inner.top += SCORE_TOOLTIP_BAR_H + SCORE_TOOLTIP_GAP;
    draw_group(hdc, &inner, relation, 1, other_x10, positive);
    inner.top += SCORE_TOOLTIP_GAP;
    draw_group(hdc, &inner, relation, 0, other_x10, negative);
    (void)factor_sum;
}
