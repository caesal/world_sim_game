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

typedef enum {
    TOOLTIP_GROUP_BASIC,
    TOOLTIP_GROUP_MUTUAL,
    TOOLTIP_GROUP_SECURITY,
    TOOLTIP_GROUP_CONFLICT,
    TOOLTIP_GROUP_RECOVERY,
    TOOLTIP_GROUP_COUNT
} TooltipFactorGroup;

static ScoreTooltipHit tooltip_build_hits[SCORE_TOOLTIP_MAX];
static int tooltip_build_hit_count;
static int tooltip_build_scope = SCORE_TOOLTIP_SCOPE_NONE;
static ScoreTooltipHit tooltip_active_hits[SCORE_TOOLTIP_MAX];
static int tooltip_active_hit_count;
static int tooltip_active_scope = SCORE_TOOLTIP_SCOPE_NONE;

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
        case DIP_REL_FACTOR_CONTEMPT: return tr("Contempt", "轻视");
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
    } else if (id == DIP_REL_FACTOR_POWER || id == DIP_REL_FACTOR_CONTEMPT) {
        snprintf(out, size, "%d%%", value);
    } else if (id == DIP_REL_FACTOR_LONG_PEACE) {
        snprintf(out, size, ui_language == UI_LANG_ZH ? "%d年" : "%dy", value);
    } else if (id == DIP_REL_FACTOR_HERITAGE) {
        snprintf(out, size, "%s", tr("annual", "年度"));
    } else if (id == DIP_REL_FACTOR_WAR_MEMORY) {
        snprintf(out, size, ui_language == UI_LANG_ZH ? "%d年" : "%dy", value);
    } else if (value > 0) {
        snprintf(out, size, "%d", value);
    } else {
        snprintf(out, size, "-");
    }
}

void diplomacy_score_tooltip_format_delta(int delta_x100, char *out, size_t size) {
    int abs_delta = abs(delta_x100);
    if (abs_delta % 100 == 0) {
        snprintf(out, size, "%c%d.0/y", delta_x100 < 0 ? '-' : '+', abs_delta / 100);
    } else if (abs_delta % 10 == 0) {
        snprintf(out, size, "%c%d.%d/y", delta_x100 < 0 ? '-' : '+',
                 abs_delta / 100, (abs_delta % 100) / 10);
    } else {
        snprintf(out, size, "%c%d.%02d/y", delta_x100 < 0 ? '-' : '+',
                 abs_delta / 100, abs_delta % 100);
    }
}

static int relation_factor_sum(SnapshotDiplomacyRelation relation) {
    int i, sum = 0;
    for (i = 0; i < DIP_REL_FACTOR_SLOTS; i++) {
        if (relation.relation_factor_ids[i] == DIP_REL_FACTOR_NONE) continue;
        sum += relation.relation_factor_delta_x100[i];
    }
    return sum;
}

int diplomacy_score_tooltip_net_for_relation(int civ_id, int other_id,
                                             int *factor_sum_x100,
                                             int *other_x100,
                                             int *display_x100) {
    SnapshotDiplomacyRelation relation = tooltip_relation(civ_id, other_id);
    int sum = relation_factor_sum(relation);
    int other = relation.yearly_delta_x100 - sum;
    if (factor_sum_x100) *factor_sum_x100 = sum;
    if (other_x100) *other_x100 = other;
    if (display_x100) *display_x100 = relation.yearly_delta_x100;
    return sum + other == relation.yearly_delta_x100;
}

void diplomacy_score_tooltip_begin(void) {
    diplomacy_score_tooltip_begin_scope(SCORE_TOOLTIP_SCOPE_COUNTRY_DIPLOMACY);
}

void diplomacy_score_tooltip_begin_scope(int scope) {
    tooltip_build_hit_count = 0;
    tooltip_build_scope = scope;
}

void diplomacy_score_tooltip_register_bar(RECT rect, int civ_id, int other_id) {
    if (tooltip_build_hit_count >= SCORE_TOOLTIP_MAX) return;
    tooltip_build_hits[tooltip_build_hit_count].rect = rect;
    tooltip_build_hits[tooltip_build_hit_count].civ_id = civ_id;
    tooltip_build_hits[tooltip_build_hit_count].other_id = other_id;
    tooltip_build_hit_count++;
}

void diplomacy_score_tooltip_commit(void) {
    diplomacy_score_tooltip_commit_scope(tooltip_build_scope);
}

void diplomacy_score_tooltip_commit_scope(int scope) {
    if (scope != tooltip_build_scope) return;
    memcpy(tooltip_active_hits, tooltip_build_hits,
           (size_t)tooltip_build_hit_count * sizeof(tooltip_active_hits[0]));
    tooltip_active_hit_count = tooltip_build_hit_count;
    tooltip_active_scope = scope;
}

int diplomacy_score_tooltip_registered_count(void) {
    return tooltip_active_hit_count;
}

int diplomacy_score_tooltip_build_count(void) {
    return tooltip_build_hit_count;
}

int diplomacy_score_tooltip_active_scope(void) {
    return tooltip_active_scope;
}

int diplomacy_score_tooltip_hit_test(int mouse_x, int mouse_y, int *civ_id, int *other_id) {
    int i;
    for (i = 0; i < tooltip_active_hit_count; i++) {
        if (!point_in_rect(tooltip_active_hits[i].rect, mouse_x, mouse_y)) continue;
        if (civ_id) *civ_id = tooltip_active_hits[i].civ_id;
        if (other_id) *other_id = tooltip_active_hits[i].other_id;
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

int diplomacy_score_tooltip_hover_key_for_scope(int scope, int mouse_x, int mouse_y) {
    if (tooltip_active_scope != scope) return 0;
    return diplomacy_score_tooltip_hover_key(mouse_x, mouse_y);
}

static int hovered_hit(void) {
    int i;
    for (i = 0; i < tooltip_active_hit_count; i++) {
        if (point_in_rect(tooltip_active_hits[i].rect, hover_x, hover_y)) return i;
    }
    return -1;
}

static TooltipFactorGroup factor_group(int id) {
    switch ((DiplomacyRelationFactor)id) {
        case DIP_REL_FACTOR_CONTACT:
        case DIP_REL_FACTOR_HERITAGE:
        case DIP_REL_FACTOR_LONG_PEACE:
        case DIP_REL_FACTOR_ALLIANCE:
            return TOOLTIP_GROUP_BASIC;
        case DIP_REL_FACTOR_TRADE:
        case DIP_REL_FACTOR_SHARED_WAR:
        case DIP_REL_FACTOR_SHARED_THREAT:
            return TOOLTIP_GROUP_MUTUAL;
        case DIP_REL_FACTOR_POWER:
        case DIP_REL_FACTOR_CONTEMPT:
            return TOOLTIP_GROUP_SECURITY;
        case DIP_REL_FACTOR_RESOURCE:
        case DIP_REL_FACTOR_BORDER:
            return TOOLTIP_GROUP_CONFLICT;
        case DIP_REL_FACTOR_TRUCE_RECOVERY:
        case DIP_REL_FACTOR_QUIET_DRIFT:
        case DIP_REL_FACTOR_WAR_MEMORY:
            return TOOLTIP_GROUP_RECOVERY;
        default:
            return TOOLTIP_GROUP_BASIC;
    }
}

static const char *group_label(TooltipFactorGroup group) {
    switch (group) {
        case TOOLTIP_GROUP_BASIC: return tr("Basic relationship", "基础关系");
        case TOOLTIP_GROUP_MUTUAL: return tr("Mutual benefit", "互利关系");
        case TOOLTIP_GROUP_SECURITY: return tr("Security pressure", "安全压力");
        case TOOLTIP_GROUP_CONFLICT: return tr("Interest conflict", "利益冲突");
        case TOOLTIP_GROUP_RECOVERY: return tr("Post-war recovery", "战后修复");
        default: return tr("Other", "其他");
    }
}

static int same_heritage_baseline(int civ_id, int other_id, SnapshotDiplomacyRelation relation) {
    const RenderSnapshot *snapshot = snapshot_ui_current();
    if (!snapshot || relation.contact_kind == DIP_CONTACT_NONE ||
        civ_id < 0 || other_id < 0 || civ_id >= snapshot->civ_count ||
        other_id >= snapshot->civ_count) return 0;
    return snapshot->civs[civ_id].heritage == snapshot->civs[other_id].heritage;
}

static int count_group_rows(SnapshotDiplomacyRelation relation, TooltipFactorGroup group,
                            int civ_id, int other_id) {
    int i, rows = group == TOOLTIP_GROUP_BASIC && same_heritage_baseline(civ_id, other_id, relation);
    for (i = 0; i < DIP_REL_FACTOR_SLOTS; i++) {
        int id = relation.relation_factor_ids[i];
        int delta = relation.relation_factor_delta_x100[i];
        if (id == DIP_REL_FACTOR_NONE || delta == 0 || factor_group(id) != group) continue;
        rows++;
    }
    return rows;
}

static int group_subtotal(SnapshotDiplomacyRelation relation, TooltipFactorGroup group) {
    int i, total = 0;
    for (i = 0; i < DIP_REL_FACTOR_SLOTS; i++) {
        int id = relation.relation_factor_ids[i];
        int delta = relation.relation_factor_delta_x100[i];
        if (id == DIP_REL_FACTOR_NONE || delta == 0 || factor_group(id) != group) continue;
        total += delta;
    }
    return total;
}

static int group_visible(SnapshotDiplomacyRelation relation, TooltipFactorGroup group,
                         int civ_id, int other_id) {
    return count_group_rows(relation, group, civ_id, other_id) > 0 ||
           group_subtotal(relation, group) != 0;
}

static const char *change_subtitle(int display_x100) {
    if (display_x100 > 0) return tr("Relationship is improving", "关系正在改善");
    if (display_x100 < 0) return tr("Relationship is worsening", "关系正在恶化");
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

static void draw_delta_text(HDC hdc, RECT rect, int delta_x100, COLORREF color) {
    char delta[24];
    diplomacy_score_tooltip_format_delta(delta_x100, delta, sizeof(delta));
    draw_text_rect(hdc, rect, delta, color,
                   DT_SINGLELINE | DT_RIGHT | DT_VCENTER | DT_END_ELLIPSIS);
}

static void draw_factor_row(HDC hdc, RECT row, const char *label,
                            const char *value, int delta_x100, COLORREF delta_color) {
    RECT marker = {row.left, row.top + 5, row.left + 3, row.bottom - 5};
    RECT label_rect = {row.left + 9, row.top, row.right - 118, row.bottom};
    RECT value_rect = {row.right - 116, row.top, row.right - 64, row.bottom};
    RECT delta_rect = {row.right - 60, row.top, row.right, row.bottom};
    fill_rect(hdc, marker, delta_color);
    draw_text_rect(hdc, label_rect, label, ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, value_rect, value, ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_RIGHT | DT_VCENTER | DT_END_ELLIPSIS);
    draw_delta_text(hdc, delta_rect, delta_x100, delta_color);
}

static void draw_baseline_row(HDC hdc, RECT row, COLORREF color) {
    RECT marker = {row.left, row.top + 5, row.left + 3, row.bottom - 5};
    RECT label_rect = {row.left + 9, row.top, row.right - 118, row.bottom};
    RECT value_rect = {row.right - 116, row.top, row.right - 64, row.bottom};
    RECT delta_rect = {row.right - 60, row.top, row.right, row.bottom};
    fill_rect(hdc, marker, color);
    draw_text_rect(hdc, label_rect, factor_label(DIP_REL_FACTOR_HERITAGE),
                   ui_theme_color(UI_COLOR_TEXT_MUTED), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, value_rect, tr("baseline", "基线"), ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_RIGHT | DT_VCENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, delta_rect, tr("+15 base", "+15基线"), color,
                   DT_SINGLELINE | DT_RIGHT | DT_VCENTER | DT_END_ELLIPSIS);
}

static void draw_section_header(HDC hdc, RECT row, const char *label, int subtotal_x100,
                                COLORREF color) {
    RECT label_rect = {row.left, row.top, row.right - 82, row.bottom};
    RECT total_rect = {row.right - 80, row.top, row.right, row.bottom};
    draw_text_rect(hdc, label_rect, label, ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_delta_text(hdc, total_rect, subtotal_x100, color);
}

static void draw_net_bar(HDC hdc, RECT rect, int display_x100,
                         COLORREF positive, COLORREF negative) {
    int mid = rect.left + (rect.right - rect.left) / 2;
    int max_extent = max(1, (rect.right - rect.left) / 2);
    int extent = min(abs(display_x100), 400) * max_extent / 400;
    RECT track = {rect.left, rect.top + 5, rect.right, rect.top + 8};
    fill_rect(hdc, track, RGB(44, 52, 54));
    fill_rect(hdc, (RECT){mid, rect.top + 3, mid + 1, rect.bottom - 2}, RGB(112, 122, 122));
    if (display_x100 > 0) {
        fill_rect(hdc, (RECT){mid, rect.top + 4, mid + extent, rect.top + 9}, positive);
    } else if (display_x100 < 0) {
        fill_rect(hdc, (RECT){mid - extent, rect.top + 4, mid, rect.top + 9}, negative);
    }
}

static int draw_group(HDC hdc, RECT *cursor, SnapshotDiplomacyRelation relation,
                      TooltipFactorGroup group, int civ_id, int other_id,
                      COLORREF positive, COLORREF negative) {
    int i, rows = 0;
    RECT header = {cursor->left, cursor->top, cursor->right, cursor->top + SCORE_TOOLTIP_SECTION_H};
    int subtotal = group_subtotal(relation, group);
    COLORREF color = subtotal < 0 ? negative : subtotal > 0 ? positive : ui_theme_color(UI_COLOR_TEXT_MUTED);
    draw_section_header(hdc, header, group_label(group), subtotal, color);
    cursor->top += SCORE_TOOLTIP_SECTION_H;
    if (group == TOOLTIP_GROUP_BASIC && same_heritage_baseline(civ_id, other_id, relation)) {
        RECT row = {cursor->left, cursor->top, cursor->right, cursor->top + SCORE_TOOLTIP_ROW_H};
        draw_baseline_row(hdc, row, positive);
        cursor->top += SCORE_TOOLTIP_ROW_H;
        rows++;
    }
    for (i = 0; i < DIP_REL_FACTOR_SLOTS; i++) {
        int id = relation.relation_factor_ids[i];
        int delta = relation.relation_factor_delta_x100[i];
        char value[24];
        RECT row;
        if (id == DIP_REL_FACTOR_NONE || delta == 0 || factor_group(id) != group) continue;
        format_factor_value(id, relation.relation_factor_values[i], value, sizeof(value));
        row = (RECT){cursor->left, cursor->top, cursor->right, cursor->top + SCORE_TOOLTIP_ROW_H};
        draw_factor_row(hdc, row, factor_label(id), value, delta, delta < 0 ? negative : positive);
        cursor->top += SCORE_TOOLTIP_ROW_H;
        rows++;
    }
    return rows;
}

void diplomacy_score_tooltip_draw(HDC hdc, RECT bounds) {
    int hit = hovered_hit();
    SnapshotDiplomacyRelation relation;
    int factor_sum, other_x100, display_x100, group_rows = 0, visible_groups = 0, g;
    int height, width = SCORE_TOOLTIP_W;
    RECT box, inner, header, subtitle, bar;
    COLORREF positive = RGB(92, 172, 126);
    COLORREF negative = RGB(206, 101, 92);
    COLORREF net_color;
    UiClaySemanticStyle semantic;
    HFONT header_font = NULL;
    HGDIOBJ old_font = NULL;
    const ScoreTooltipHit *hit_info;
    if (hit < 0 || !hdc) return;
    hit_info = &tooltip_active_hits[hit];
    relation = tooltip_relation(hit_info->civ_id, hit_info->other_id);
    diplomacy_score_tooltip_net_for_relation(hit_info->civ_id, hit_info->other_id,
                                             &factor_sum, &other_x100, &display_x100);
    for (g = 0; g < TOOLTIP_GROUP_COUNT; g++) {
        TooltipFactorGroup group = (TooltipFactorGroup)g;
        if (!group_visible(relation, group, hit_info->civ_id, hit_info->other_id)) continue;
        visible_groups++;
        group_rows += count_group_rows(relation, group, hit_info->civ_id, hit_info->other_id);
    }
    if (other_x100) group_rows++;
    height = SCORE_TOOLTIP_PAD * 2 + SCORE_TOOLTIP_HEADER_H + SCORE_TOOLTIP_SUBTITLE_H +
             SCORE_TOOLTIP_BAR_H + SCORE_TOOLTIP_GAP * (visible_groups + (other_x100 ? 1 : 0) + 1) +
             SCORE_TOOLTIP_SECTION_H * visible_groups + SCORE_TOOLTIP_ROW_H * group_rows;
    box = (RECT){hover_x + 14, hover_y + 18, hover_x + 14 + width, hover_y + 18 + height};
    if (box.right > bounds.right - 4) { box.left = bounds.right - 4 - width; box.right = bounds.right - 4; }
    if (box.bottom > bounds.bottom - 4) { box.top = bounds.bottom - 4 - height; box.bottom = bounds.bottom - 4; }
    if (box.left < bounds.left + 4) { box.right += bounds.left + 4 - box.left; box.left = bounds.left + 4; }
    semantic = ui_clay_semantic_style(tone_for_state(relation.state));
    net_color = display_x100 < 0 ? negative : display_x100 > 0 ? positive : ui_theme_color(UI_COLOR_TEXT_MUTED);
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
                    display_x100, net_color);
    if (header_font && old_font) SelectObject(hdc, old_font);
    if (header_font) DeleteObject(header_font);
    inner.top += SCORE_TOOLTIP_HEADER_H;
    subtitle = (RECT){inner.left, inner.top, inner.right, inner.top + SCORE_TOOLTIP_SUBTITLE_H};
    draw_text_rect(hdc, subtitle, change_subtitle(display_x100), ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    inner.top += SCORE_TOOLTIP_SUBTITLE_H;
    bar = (RECT){inner.left, inner.top, inner.right, inner.top + SCORE_TOOLTIP_BAR_H};
    draw_net_bar(hdc, bar, display_x100, positive, negative);
    inner.top += SCORE_TOOLTIP_BAR_H + SCORE_TOOLTIP_GAP;
    for (g = 0; g < TOOLTIP_GROUP_COUNT; g++) {
        TooltipFactorGroup group = (TooltipFactorGroup)g;
        if (!group_visible(relation, group, hit_info->civ_id, hit_info->other_id)) continue;
        draw_group(hdc, &inner, relation, group,
                   hit_info->civ_id, hit_info->other_id, positive, negative);
        inner.top += SCORE_TOOLTIP_GAP;
    }
    if (other_x100) {
        RECT row = {inner.left, inner.top, inner.right, inner.top + SCORE_TOOLTIP_ROW_H};
        draw_factor_row(hdc, row, tr("Unattributed adjustment", "未归因调整"), "-",
                        other_x100, other_x100 < 0 ? negative : positive);
    }
    (void)factor_sum;
}
