#include "render/panel_country_diplomacy_war_history.h"

#include "render/panel_country_diplomacy_result.h"
#include "ui/ui_clay_primitives.h"
#include "ui/ui_theme.h"
#include "ui/ui_types.h"

#include <stdio.h>
#include <string.h>

#define WAR_HISTORY_CARD_GAP 6
#define WAR_HISTORY_SECTION_H 31
#define WAR_HISTORY_SWATCH 10
#define WAR_HISTORY_PRODUCTION_LINE_H 17
#define WAR_HISTORY_WIDE_MIN 500
#define WAR_HISTORY_ROW_PAD 4

int country_diplomacy_war_history_card_height(int panel_width,
                                               int line_height) {
    int lines = 1 + 3 + 2;
    (void)panel_width;
    line_height = clamp(line_height, 12, 32);
    return lines * line_height + WAR_HISTORY_ROW_PAD * 3;
}

void war_history_card_layout_for_metrics(
    RECT card, int panel_width, int line_height,
    WarHistoryCardLayout *layout) {
    int inner_left;
    int inner_right;
    int inner_width;
    int center;
    int result_width;
    int date_width;
    int wide;
    int title_h;
    int matchup_h;
    int footer_h;
    int name_lines;
    if (!layout) return;
    line_height = clamp(line_height, 12, 32);
    wide = panel_width >= WAR_HISTORY_WIDE_MIN;
    title_h = line_height + WAR_HISTORY_ROW_PAD;
    matchup_h = line_height * 3 + WAR_HISTORY_ROW_PAD;
    footer_h = line_height * 2 + WAR_HISTORY_ROW_PAD;
    name_lines = wide ? 2 : 1;
    card.bottom = card.top + title_h + matchup_h + footer_h;
    inner_left = card.left + 7;
    inner_right = card.right - 7;
    inner_width = max(1, inner_right - inner_left);
    center = inner_left + inner_width / 2;
    result_width = clamp(inner_width * 20 / 100, 90, 112);
    date_width = clamp(inner_width * 45 / 100, 148, 220);
    layout->card = card;
    layout->title = (RECT){card.left, card.top, card.right,
                           card.top + title_h};
    layout->title_text = (RECT){inner_left, card.top, inner_right,
                                layout->title.bottom};
    layout->matchup = (RECT){card.left, layout->title.bottom, card.right,
                             layout->title.bottom + matchup_h};
    layout->footer = (RECT){card.left, layout->matchup.bottom, card.right,
                            layout->matchup.bottom + footer_h};
    layout->result = (RECT){center - result_width / 2, layout->matchup.top,
                            center + (result_width + 1) / 2,
                            layout->matchup.bottom};
    layout->local_swatch = (RECT){
                                  inner_left,
                                  layout->matchup.top +
                                      (line_height - WAR_HISTORY_SWATCH) / 2,
                                  inner_left + WAR_HISTORY_SWATCH,
                                  layout->matchup.top +
                                      (line_height - WAR_HISTORY_SWATCH) / 2 +
                                      WAR_HISTORY_SWATCH};
    layout->local_name = (RECT){layout->local_swatch.right + 4,
                                layout->matchup.top,
                                layout->result.left - 4,
                                layout->matchup.top +
                                    name_lines * line_height + 2};
    layout->opponent_swatch = (RECT){inner_right - WAR_HISTORY_SWATCH,
                                     layout->matchup.top +
                                         (line_height - WAR_HISTORY_SWATCH) / 2,
                                     inner_right,
                                     layout->matchup.top +
                                         (line_height - WAR_HISTORY_SWATCH) / 2 +
                                         WAR_HISTORY_SWATCH};
    layout->opponent_name = (RECT){layout->result.right + 4,
                                   layout->matchup.top,
                                   layout->opponent_swatch.left - 4,
                                   layout->local_name.bottom};
    layout->local_casualties = (RECT){inner_left,
                                      layout->local_name.bottom,
                                      layout->result.left - 4,
                                      layout->matchup.bottom};
    layout->opponent_casualties = (RECT){layout->result.right + 4,
                                         layout->opponent_name.bottom,
                                         inner_right,
                                         layout->matchup.bottom};
    if (wide) {
        layout->settlement = (RECT){inner_left, layout->footer.top,
                                    inner_right - date_width - 5,
                                    layout->footer.bottom};
        layout->date = (RECT){inner_right - date_width, layout->footer.top,
                              inner_right, layout->footer.bottom};
    } else {
        int split = layout->footer.top + footer_h / 2;
        layout->settlement = (RECT){inner_left, layout->footer.top,
                                    inner_right, split};
        layout->date = (RECT){inner_left, split, inner_right,
                              layout->footer.bottom};
    }
    layout->line_height = line_height;
    layout->wide = wide;
    layout->footer_rows = wide ? 1 : 2;
}

void war_history_card_layout(RECT card, WarHistoryCardLayout *layout) {
    int panel_width = card.right - card.left + 16;
    war_history_card_layout_for_metrics(
        card, panel_width, WAR_HISTORY_PRODUCTION_LINE_H, layout);
}

static int visible_history_count(const SnapshotCiv *civ) {
    if (!civ || civ->uid == WAR_HISTORY_INVALID_UID ||
        civ->war_history.owner_uid != civ->uid) return 0;
    return clamp(civ->war_history.count, 0, WAR_HISTORY_CAPACITY);
}

static const char *principal_name(const SnapshotWarHistoryPrincipal *principal) {
    if (!principal) return "";
    return ui_language == UI_LANG_ZH ? principal->name_zh : principal->name_en;
}

static void format_grouped_nonnegative(char *out, size_t size, int value) {
    char digits[32];
    size_t length;
    size_t source;
    size_t target = 0;
    snprintf(digits, sizeof(digits), "%d", max(0, value));
    length = strlen(digits);
    for (source = 0; source < length && target + 1 < size; source++) {
        if (source > 0 && (length - source) % 3 == 0 && target + 2 < size) {
            out[target++] = ',';
        }
        out[target++] = digits[source];
    }
    if (size > 0) out[target < size ? target : size - 1] = '\0';
}

static void format_casualties(char *out, size_t size, int casualties) {
    char value[32];
    format_grouped_nonnegative(value, sizeof(value), casualties);
    snprintf(out, size, tr("Casualties %s", "阵亡 %s"), value);
}

static void format_settlement(char *out, size_t size,
                              const SnapshotWarHistoryRecord *record) {
    char cession[64] = "";
    char indemnity[96] = "";
    char value[32];
    int has_cession = record->transferred_regions > 0;
    int has_indemnity = record->indemnity_paid > 0;
    if (!has_cession && !has_indemnity) {
        snprintf(out, size, "%s",
                 tr("No cession · No reparations", "无割让 · 无赔款"));
        return;
    }
    if (has_cession) {
        format_grouped_nonnegative(value, sizeof(value),
                                   record->transferred_regions);
        snprintf(cession, sizeof(cession),
                 tr("Cession %s", "割让 %s"), value);
    }
    if (has_indemnity) {
        format_grouped_nonnegative(value, sizeof(value),
                                   record->indemnity_paid);
        snprintf(indemnity, sizeof(indemnity),
                 tr("Reparations %s", "赔款 %s"), value);
    }
    if (has_cession && has_indemnity) {
        snprintf(out, size, "%s · %s", cession, indemnity);
    } else {
        snprintf(out, size, "%s", has_cession ? cession : indemnity);
    }
}

static void format_settlement_direction(
    char *out, size_t size, const SnapshotWarHistoryRecord *record) {
    char settlement[160];
    int has_settlement = record->transferred_regions > 0 ||
                         record->indemnity_paid > 0;
    format_settlement(settlement, sizeof(settlement), record);
    if (!has_settlement) {
        snprintf(out, size, "%s", settlement);
    } else if (record->beneficiary_uid == record->local.uid) {
        snprintf(out, size, "← %s", settlement);
    } else if (record->beneficiary_uid == record->opponent.uid) {
        snprintf(out, size, "%s →", settlement);
    } else {
        snprintf(out, size, "%s", settlement);
    }
}

static void format_end_date(char *out, size_t size,
                            const SnapshotWarHistoryRecord *record) {
    int duration = max(1, record->duration_months);
    int years = duration / 12;
    int months = duration % 12;
    if (ui_language == UI_LANG_ZH) {
        snprintf(out, size, "%d年%d月结束 · 持续%d年%d个月",
                 record->end_year, record->end_month, years, months);
    } else {
        snprintf(out, size, "Ended Y%d M%d · %dy %dm",
                 record->end_year, record->end_month, years, months);
    }
}

static void draw_result_outline(HDC hdc, RECT card, COLORREF color) {
    fill_rect(hdc, (RECT){card.left + 4, card.top,
                          card.right - 4, card.top + 1}, color);
    fill_rect(hdc, (RECT){card.left, card.top + 4,
                          card.left + 1, card.bottom - 4}, color);
    fill_rect(hdc, (RECT){card.right - 1, card.top + 4,
                          card.right, card.bottom - 4}, color);
    fill_rect(hdc, (RECT){card.left + 4, card.bottom - 1,
                          card.right - 4, card.bottom}, color);
}

static void draw_wrapped_centered_text(HDC hdc, RECT rect,
                                       const char *text, COLORREF color,
                                       UINT alignment) {
    WCHAR wide[512];
    RECT measured = {0, 0, rect.right - rect.left, 0};
    int length = MultiByteToWideChar(
        CP_UTF8, 0, text, -1, wide,
        (int)(sizeof(wide) / sizeof(wide[0])));
    UINT flags = alignment | DT_WORDBREAK | DT_NOPREFIX;
    if (length <= 0) return;
    DrawTextW(hdc, wide, -1, &measured, flags | DT_CALCRECT);
    if (measured.bottom < rect.bottom - rect.top) {
        rect.top += ((rect.bottom - rect.top) - measured.bottom) / 2;
    }
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    DrawTextW(hdc, wide, -1, &rect, flags);
}

static void draw_history_card(
    HDC hdc, RECT card, const SnapshotWarHistoryRecord *record,
    int panel_width, int line_height) {
    WarHistoryCardLayout layout;
    COLORREF result_color = panel_country_diplomacy_result_color(
        record->local.uid, record->winner_uid, record->loser_uid,
        record->result);
    char local_casualties[64];
    char opponent_casualties[64];
    char settlement_text[192];
    char date_text[96];
    war_history_card_layout_for_metrics(
        card, panel_width, line_height, &layout);
    ui_clay_draw_card(hdc, card, UI_CLAY_STATE_NORMAL);
    draw_result_outline(hdc, card, result_color);
    draw_text_rect(hdc, layout.title_text, tr("War Ended", "战争结束"),
                   result_color,
                   DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    fill_rect(hdc, layout.local_swatch,
              record ? (COLORREF)record->local.color : RGB(96, 100, 104));
    fill_rect(hdc, layout.opponent_swatch,
              record ? (COLORREF)record->opponent.color : RGB(96, 100, 104));
    if (layout.wide) {
        draw_wrapped_centered_text(
            hdc, layout.local_name, principal_name(&record->local),
            ui_theme_color(UI_COLOR_TEXT), DT_LEFT);
        draw_wrapped_centered_text(
            hdc, layout.opponent_name, principal_name(&record->opponent),
            ui_theme_color(UI_COLOR_TEXT), DT_RIGHT);
    } else {
        draw_text_rect(
            hdc, layout.local_name, principal_name(&record->local),
            ui_theme_color(UI_COLOR_TEXT),
            DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        draw_text_rect(
            hdc, layout.opponent_name, principal_name(&record->opponent),
            ui_theme_color(UI_COLOR_TEXT),
            DT_SINGLELINE | DT_RIGHT | DT_VCENTER |
                DT_END_ELLIPSIS | DT_NOPREFIX);
    }
    format_casualties(local_casualties, sizeof(local_casualties),
                      record->local_casualties);
    format_casualties(opponent_casualties, sizeof(opponent_casualties),
                      record->opponent_casualties);
    draw_wrapped_centered_text(
        hdc, layout.local_casualties, local_casualties,
        ui_theme_color(UI_COLOR_TEXT_MUTED), DT_LEFT);
    draw_wrapped_centered_text(
        hdc, layout.opponent_casualties, opponent_casualties,
        ui_theme_color(UI_COLOR_TEXT_MUTED), DT_RIGHT);
    draw_wrapped_centered_text(
        hdc, layout.result,
        panel_country_diplomacy_result_text(
            record->local.uid, record->winner_uid,
            record->loser_uid, record->result),
        result_color, DT_CENTER);
    fill_rect(hdc, (RECT){card.left + 7, layout.footer.top,
                          card.right - 7, layout.footer.top + 1},
              ui_theme_color(UI_COLOR_PANEL_LINE));
    format_settlement_direction(settlement_text, sizeof(settlement_text),
                                record);
    format_end_date(date_text, sizeof(date_text), record);
    draw_wrapped_centered_text(
        hdc, layout.settlement, settlement_text,
        record->transferred_regions > 0 || record->indemnity_paid > 0 ?
            result_color : ui_theme_color(UI_COLOR_TEXT_MUTED),
        DT_LEFT);
    draw_wrapped_centered_text(
        hdc, layout.date, date_text, ui_theme_color(UI_COLOR_TEXT_MUTED),
        DT_RIGHT);
}

static int utf8_text_fits(HDC hdc, RECT rect, const char *text, UINT flags) {
    WCHAR wide[512];
    RECT measured = {0, 0, rect.right - rect.left, 0};
    int length = MultiByteToWideChar(CP_UTF8, 0, text, -1, wide,
                                     (int)(sizeof(wide) / sizeof(wide[0])));
    if (length <= 0 || measured.right <= 0 || rect.bottom <= rect.top) return 0;
    DrawTextW(hdc, wide, -1, &measured,
              DT_CALCRECT | DT_NOPREFIX | flags);
    return measured.right <= rect.right - rect.left &&
           measured.bottom <= rect.bottom - rect.top;
}

static int ellipsis_lane_fits(HDC hdc, RECT rect) {
    return utf8_text_fits(hdc, rect, "W", DT_SINGLELINE);
}

int country_diplomacy_war_history_record_text_fits(
    HDC hdc, int panel_width, int card_width,
    const SnapshotWarHistoryRecord *record) {
    RECT card;
    WarHistoryCardLayout layout;
    TEXTMETRICW metrics;
    char local_casualties[64];
    char opponent_casualties[64];
    char settlement_text[192];
    char date_text[96];
    int fits;
    if (!hdc || !record || card_width <= 40) return 0;
    if (!GetTextMetricsW(hdc, &metrics)) return 0;
    card = (RECT){
        0, 0, card_width,
        country_diplomacy_war_history_card_height(
            panel_width, metrics.tmHeight)
    };
    war_history_card_layout_for_metrics(
        card, panel_width, metrics.tmHeight, &layout);
    format_casualties(local_casualties, sizeof(local_casualties),
                      record->local_casualties);
    format_casualties(opponent_casualties, sizeof(opponent_casualties),
                      record->opponent_casualties);
    format_settlement_direction(settlement_text, sizeof(settlement_text),
                                record);
    format_end_date(date_text, sizeof(date_text), record);
    fits = (layout.wide ?
                utf8_text_fits(hdc, layout.local_name,
                               principal_name(&record->local),
                               DT_WORDBREAK) :
                ellipsis_lane_fits(hdc, layout.local_name)) &&
           (layout.wide ?
                utf8_text_fits(hdc, layout.opponent_name,
                               principal_name(&record->opponent),
                               DT_WORDBREAK | DT_RIGHT) :
                ellipsis_lane_fits(hdc, layout.opponent_name)) &&
           utf8_text_fits(hdc, layout.local_casualties,
                          local_casualties, DT_WORDBREAK) &&
           utf8_text_fits(hdc, layout.opponent_casualties,
                          opponent_casualties, DT_WORDBREAK | DT_RIGHT) &&
           utf8_text_fits(
               hdc, layout.result,
               panel_country_diplomacy_result_text(
                   record->local.uid, record->winner_uid,
                   record->loser_uid, record->result),
               DT_CENTER | DT_WORDBREAK) &&
           utf8_text_fits(hdc, layout.settlement, settlement_text,
                          DT_WORDBREAK) &&
           utf8_text_fits(hdc, layout.date, date_text,
                          DT_WORDBREAK | DT_RIGHT);
    return fits && GetCurrentObject(hdc, OBJ_FONT) != NULL;
}

int country_diplomacy_war_history_height(const SnapshotCiv *civ) {
    int count = visible_history_count(civ);
    int card_height = country_diplomacy_war_history_card_height(
        side_panel_w, WAR_HISTORY_PRODUCTION_LINE_H);
    return count > 0 ? WAR_HISTORY_SECTION_H + count * card_height +
           (count - 1) * WAR_HISTORY_CARD_GAP : 0;
}

void draw_country_diplomacy_war_history(HDC hdc, UiCursor *cursor,
                                        const SnapshotCiv *civ) {
    int count = visible_history_count(civ);
    TEXTMETRICW metrics;
    int line_height = WAR_HISTORY_PRODUCTION_LINE_H;
    int card_height;
    int i;
    if (!cursor || count <= 0) return;
    if (GetTextMetricsW(hdc, &metrics)) line_height = metrics.tmHeight;
    card_height = country_diplomacy_war_history_card_height(
        side_panel_w, line_height);
    ui_section(hdc, cursor, tr("Recently Ended", "最近结束"));
    for (i = 0; i < count; i++) {
        RECT card = ui_take_rect(cursor, card_height);
        draw_history_card(hdc, card, &civ->war_history.records[i],
                          side_panel_w, line_height);
        if (i + 1 < count) cursor->y += WAR_HISTORY_CARD_GAP;
    }
}
