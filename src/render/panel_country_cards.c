#include "render/panel_country_cards.h"

#include "render/render_common.h"
#include "render/snapshot_ui.h"
#include "ui/ui_theme.h"
#include "ui/ui_types.h"

#include <stdio.h>
#include <string.h>

static COLORREF disorder_color(int disorder) {
    if (disorder <= 30) return RGB(104, 153, 92);
    if (disorder <= 65) return RGB(191, 154, 78);
    return RGB(188, 78, 68);
}

static const char *intent_label(const char *intent) {
    if (!intent) return tr("Unknown", "未知");
    if (strcmp(intent, "War") == 0) return tr("War", "战争");
    if (strcmp(intent, "Stability") == 0) return tr("Stability", "稳定");
    return tr("Expansion", "扩张");
}

static const char *status_label(const SnapshotCiv *civ) {
    if (!civ) return tr("Unknown", "未知");
    if (!civ->alive) return tr("Fallen", "已灭亡");
    if (civ->overlord >= 0) return tr("Vassal", "附庸");
    if (civ->vassal_count > 0) return tr("Overlord", "宗主");
    if (civ->war_active) return tr("At war", "战争中");
    return tr("Alive", "存活");
}

static void draw_card_metric(HDC hdc, RECT rect, const char *label, int value, COLORREF color) {
    char text[32];
    RECT label_rect = {rect.left, rect.top, rect.right, rect.top + 14};
    RECT value_rect = {rect.left, rect.top + 15, rect.right, rect.bottom};

    format_metric_value(value, text, sizeof(text));
    draw_text_rect(hdc, label_rect, label, ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_SINGLELINE | DT_CENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, value_rect, text, color,
                   DT_SINGLELINE | DT_CENTER | DT_END_ELLIPSIS);
}

void draw_country_summary_card(HDC hdc, RECT rect, int civ_id, int selected) {
    const SnapshotCiv *civ = snapshot_ui_civ(civ_id);
    CountrySummary country = civ ? civ->summary : (CountrySummary){0};
    RECT swatch = {rect.left + 8, rect.top + 8, rect.left + 24, rect.top + 24};
    RECT name_rect = {rect.left + 32, rect.top + 5, rect.right - 96, rect.top + 27};
    RECT status_rect = {rect.right - 88, rect.top + 6, rect.right - 8, rect.top + 25};
    int metric_w = (rect.right - rect.left - 16) / 6;
    RECT metric = {rect.left + 8, rect.top + 31, rect.left + 8 + metric_w - 4, rect.top + 68};
    char title[160];
    COLORREF text_color = civ && civ->alive ? ui_theme_color(UI_COLOR_TEXT) : ui_theme_color(UI_COLOR_TEXT_DIM);

    if (!civ) return;
    fill_rect(hdc, rect, selected ? RGB(54, 61, 58) : ui_theme_color(UI_COLOR_PANEL_SOFT));
    fill_rect(hdc, swatch, civ->color);
    snprintf(title, sizeof(title), "%c  %.80s", civ->symbol, snapshot_ui_civ_name(civ_id));
    draw_text_rect(hdc, name_rect, title, text_color, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    fill_rect(hdc, status_rect, civ->alive ? RGB(55, 70, 64) : RGB(64, 56, 56));
    draw_center_text(hdc, status_rect, status_label(civ), ui_theme_color(UI_COLOR_TEXT));
    draw_card_metric(hdc, metric, metric_label("Pop", "人口"), country.population, text_color);
    metric.left += metric_w; metric.right += metric_w;
    draw_card_metric(hdc, metric, metric_label("Prov", "省份"), snapshot_ui_province_count(civ_id), RGB(190, 204, 216));
    metric.left += metric_w; metric.right += metric_w;
    draw_card_metric(hdc, metric, metric_label("Army", "军队"), civ->current_soldiers, RGB(204, 172, 112));
    metric.left += metric_w; metric.right += metric_w;
    draw_card_metric(hdc, metric, metric_label("Money", "经济"), country.money, RGB(207, 184, 104));
    metric.left += metric_w; metric.right += metric_w;
    draw_card_metric(hdc, metric, metric_label("Tech", "科技"), clamp(civ->tech_stage, 0, 10), RGB(147, 176, 214));
    metric.left += metric_w; metric.right += metric_w;
    draw_card_metric(hdc, metric, metric_label("Chaos", "混乱"), civ->disorder, disorder_color(civ->disorder));
}

void draw_country_selected_summary(HDC hdc, RECT rect, int civ_id) {
    const SnapshotCiv *civ = snapshot_ui_civ(civ_id);
    CountrySummary country = civ ? civ->summary : (CountrySummary){0};
    RECT swatch = {rect.left + 8, rect.top + 8, rect.left + 24, rect.top + 24};
    RECT locate_rect = {rect.right - 64, rect.top + 6, rect.right - 8, rect.top + 26};
    RECT name_rect = {rect.left + 32, rect.top + 5, locate_rect.left - 6, rect.top + 26};
    RECT summary_rect = {rect.left + 32, rect.top + 27, rect.right - 8, rect.bottom - 4};
    char title[160];
    char summary[224];

    if (!civ) return;
    fill_rect(hdc, rect, RGB(54, 61, 58));
    fill_rect(hdc, swatch, civ->color);
    snprintf(title, sizeof(title), "%c  %.80s", civ->symbol, snapshot_ui_civ_name(civ_id));
    snprintf(summary, sizeof(summary), "%s %.20s   %s %d   %s %d   %s %d   %s %d   %s %s   %s %.16s",
             tr("Capital", "首都"), snapshot_ui_capital_name(civ_id),
             tr("Pop", "人口"), country.population,
             tr("Army", "军队"), civ->current_soldiers,
             tr("Money", "经济"), country.money,
             tr("Disorder", "混乱"), civ->disorder,
             tr("Status", "状态"), status_label(civ),
             tr("Intent", "意图"), intent_label(civ->main_intent));
    draw_text_rect(hdc, name_rect, title, ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    fill_rect(hdc, locate_rect, RGB(87, 93, 78));
    draw_center_text(hdc, locate_rect, tr("Locate", "定位"), RGB(255, 238, 190));
    draw_text_rect(hdc, summary_rect, summary, ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}
