#include "render_panel_internal.h"
#include "render/panel_country.h"

#include "sim/plague.h"
#include "sim/population.h"
#include "sim/regions.h"
#include "sim/technology.h"
#include "ui/ui_widgets.h"

#include <string.h>

typedef struct {
    RECT header;
    RECT sort_label;
    RECT fallen_toggle;
    RECT back_to_list;
    RECT selected_summary;
    RECT cards[MAX_CIVS];
    int card_civ_ids[MAX_CIVS];
    int card_count;
    RECT detail_viewport;
    int detail_scroll;
    int detail_max_scroll;
    int selected_detail;
} CountryPanelLayout;

static int displayed_country(void) {
    if (selected_civ >= 0 && selected_civ < civ_count &&
        (civs[selected_civ].alive || country_show_fallen)) return selected_civ;
    return -1;
}

static int country_province_count(int civ_id) {
    int i;
    int count = 0;
    for (i = 0; i < region_count; i++) {
        if (natural_regions[i].alive && natural_regions[i].owner_civ == civ_id) count++;
    }
    return count > 0 ? count : summarize_country(civ_id).cities;
}

static COLORREF disorder_color(int disorder) {
    if (disorder <= 30) return RGB(104, 153, 92);
    if (disorder <= 65) return RGB(191, 154, 78);
    return RGB(188, 78, 68);
}

static int include_country_in_list(int civ_id) {
    return civ_id >= 0 && civ_id < civ_count && (civs[civ_id].alive || country_show_fallen);
}

static void sorted_country_ids(int *ids, int *out_count) {
    int i;
    int j;
    int count = 0;

    for (i = 0; i < civ_count; i++) {
        if (!include_country_in_list(i)) continue;
        ids[count++] = i;
    }
    for (i = 1; i < count; i++) {
        int id = ids[i];
        j = i - 1;
        while (j >= 0 && civs[ids[j]].population < civs[id].population) {
            ids[j + 1] = ids[j];
            j--;
        }
        ids[j + 1] = id;
    }
    *out_count = count;
}

static int country_detail_content_height(int civ_id) {
    int owned_cities = 0;
    int i;
    int height = 49 + 3 * 36 + 78 + 21 + 31 + 3 * 36 + 21 + 31 + 3 * 36 + 31 + 2 * 36;

    for (i = 0; i < city_count; i++) {
        if (cities[i].alive && cities[i].owner == civ_id) owned_cities++;
    }
    if (plague_civ_active_count(civ_id) > 0) height += 31 + 36;
    height += 31 + owned_cities * 22;
    return height + 28;
}

static void country_panel_layout_build(RECT client, CountryPanelLayout *layout) {
    int x = client.right - side_panel_w + FORM_X_PAD;
    int width = side_panel_w - FORM_X_PAD * 2;
    int y = TOP_BAR_H + 66;
    int civ_id = displayed_country();
    int ids[MAX_CIVS];
    int count;
    int i;

    memset(layout, 0, sizeof(*layout));
    layout->selected_detail = civ_id >= 0 && civ_id < civ_count;
    layout->header = (RECT){x, y, x + width, y + 24};
    y += 25;
    layout->sort_label = (RECT){x, y, x + width, y + 20};
    y += 25;
    layout->fallen_toggle = (RECT){x, y, x + width, y + 26};
    y += 36;
    if (layout->selected_detail) {
        layout->back_to_list = (RECT){x, y, x + width, y + 26};
        y += 34;
        layout->selected_summary = (RECT){x, y, x + width, y + 50};
        y += 62;
        layout->detail_viewport = (RECT){x, y, x + width, client.bottom - 64};
        layout->detail_max_scroll = max(0, country_detail_content_height(civ_id) -
                                           (layout->detail_viewport.bottom - layout->detail_viewport.top));
        layout->detail_scroll = clamp(country_detail_scroll_offset, 0, layout->detail_max_scroll);
        return;
    }
    sorted_country_ids(ids, &count);
    for (i = 0; i < count && layout->card_count < MAX_CIVS; i++) {
        RECT card = {x, y, x + width, y + 72};
        if (card.bottom > client.bottom - 68) break;
        layout->cards[layout->card_count] = card;
        layout->card_civ_ids[layout->card_count] = ids[i];
        layout->card_count++;
        y += 80;
    }
    layout->detail_viewport = (RECT){x, y, x + width, client.bottom - 64};
}

static void draw_card_metric(HDC hdc, RECT rect, const char *label, int value, COLORREF color) {
    char text[32];
    RECT label_rect = {rect.left, rect.top, rect.right, rect.top + 15};
    RECT value_rect = {rect.left, rect.top + 16, rect.right, rect.top + 38};

    format_metric_value(value, text, sizeof(text));
    draw_text_rect(hdc, label_rect, label, ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_SINGLELINE | DT_CENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, value_rect, text, color, DT_SINGLELINE | DT_CENTER | DT_END_ELLIPSIS);
}

static void draw_country_card(HDC hdc, RECT rect, int civ_id) {
    RECT swatch = {rect.left + 8, rect.top + 8, rect.left + 24, rect.top + 24};
    RECT name_rect = {rect.left + 32, rect.top + 5, rect.right - 8, rect.top + 27};
    int metric_w = (rect.right - rect.left - 16) / 5;
    RECT metric = {rect.left + 8, rect.top + 30, rect.left + 8 + metric_w - 4, rect.top + 68};
    CountrySummary country = summarize_country(civ_id);
    char title[160];
    COLORREF text_color = civs[civ_id].alive ? ui_theme_color(UI_COLOR_TEXT) : ui_theme_color(UI_COLOR_TEXT_DIM);

    fill_rect(hdc, rect, civ_id == selected_civ ? RGB(54, 61, 58) : ui_theme_color(UI_COLOR_PANEL_SOFT));
    fill_rect(hdc, swatch, civs[civ_id].color);
    snprintf(title, sizeof(title), "%c  %.80s%s", civs[civ_id].symbol, civs[civ_id].name,
             civs[civ_id].alive ? "" : tr(" (fallen)", "（已灭亡）"));
    draw_text_rect(hdc, name_rect, title, text_color, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_card_metric(hdc, metric, metric_label("Pop", "人口"), country.population, text_color);
    metric.left += metric_w; metric.right += metric_w;
    draw_card_metric(hdc, metric, metric_label("Provinces", "省份"), country_province_count(civ_id), RGB(190, 204, 216));
    metric.left += metric_w; metric.right += metric_w;
    draw_card_metric(hdc, metric, metric_label("Army", "军队"), war_estimated_soldiers(civ_id), RGB(204, 172, 112));
    metric.left += metric_w; metric.right += metric_w;
    draw_card_metric(hdc, metric, metric_label("Money", "金钱"), country.money, RGB(212, 186, 103));
    metric.left += metric_w; metric.right += metric_w;
    draw_card_metric(hdc, metric, metric_label("Disorder", "混乱"), civs[civ_id].disorder, disorder_color(civs[civ_id].disorder));
}

static void draw_country_selector(HDC hdc, RECT rect, int civ_id) {
    RECT swatch = {rect.left + 8, rect.top + 8, rect.left + 24, rect.top + 24};
    RECT name_rect = {rect.left + 32, rect.top + 5, rect.right - 8, rect.top + 26};
    RECT summary_rect = {rect.left + 32, rect.top + 27, rect.right - 8, rect.bottom - 4};
    CountrySummary country = summarize_country(civ_id);
    char title[160];
    char summary[192];

    fill_rect(hdc, rect, RGB(54, 61, 58));
    fill_rect(hdc, swatch, civs[civ_id].color);
    snprintf(title, sizeof(title), "%c  %.80s%s", civs[civ_id].symbol, civs[civ_id].name,
             civs[civ_id].alive ? "" : tr(" (fallen)", "（已灭亡）"));
    snprintf(summary, sizeof(summary), "%s %d   %s %d   %s %d   %s %d   %s %d",
             tr("Pop", "人口"), country.population, tr("Provinces", "省份"), country_province_count(civ_id),
             tr("Army", "军队"), war_estimated_soldiers(civ_id), tr("Money", "金钱"), country.money,
             tr("Disorder", "混乱"), civs[civ_id].disorder);
    draw_text_rect(hdc, name_rect, title, ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, summary_rect, summary, ui_theme_color(UI_COLOR_TEXT_MUTED), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}

static void draw_country_list(HDC hdc, const CountryPanelLayout *layout) {
    int i;
    char text[96];

    draw_text_rect(hdc, layout->header, tr("Country Dashboard", "国家面板"),
                   ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    snprintf(text, sizeof(text), "%s  %s", tr("Sort", "排序"), tr("Population desc", "人口降序"));
    draw_text_rect(hdc, layout->sort_label, text, ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    fill_rect(hdc, layout->fallen_toggle, country_show_fallen ? RGB(87, 93, 78) : RGB(43, 49, 52));
    draw_center_text(hdc, layout->fallen_toggle, tr("Show Fallen Countries", "显示已灭亡国家"), ui_theme_color(UI_COLOR_TEXT));
    if (layout->selected_detail) {
        fill_rect(hdc, layout->back_to_list, RGB(43, 49, 52));
        draw_center_text(hdc, layout->back_to_list, tr("All Countries / Back to list", "全部国家 / 返回列表"),
                         ui_theme_color(UI_COLOR_TEXT));
        draw_country_selector(hdc, layout->selected_summary, displayed_country());
        return;
    }
    for (i = 0; i < layout->card_count; i++) {
        draw_country_card(hdc, layout->cards[i], layout->card_civ_ids[i]);
    }
    if (layout->card_count == 0) {
        draw_text_rect(hdc, layout->detail_viewport,
                       tr("No alive countries. Toggle fallen countries for historical entries.",
                          "没有存活国家。可切换显示已灭亡国家查看历史条目。"),
                       ui_theme_color(UI_COLOR_TEXT_MUTED), DT_WORDBREAK | DT_END_ELLIPSIS);
    }
}

static void draw_scrollbar(HDC hdc, RECT viewport, int scroll, int max_scroll) {
    RECT track = {viewport.right - 5, viewport.top, viewport.right - 2, viewport.bottom};
    RECT thumb = track;
    int height = viewport.bottom - viewport.top;
    int thumb_h;

    if (max_scroll <= 0 || height <= 0) return;
    thumb_h = max(28, height * height / max(1, height + max_scroll));
    thumb.top = viewport.top + scroll * (height - thumb_h) / max_scroll;
    thumb.bottom = thumb.top + thumb_h;
    fill_rect(hdc, track, RGB(40, 47, 52));
    fill_rect(hdc, thumb, RGB(115, 130, 138));
}

static void draw_metric_row(HDC hdc, UiCursor *cursor, int a, int b, int c,
                            IconId ia, IconId ib, IconId ic,
                            const char *la, const char *lb, const char *lc) {
    int w = (cursor->width - 16) / 3;
    RECT r = {cursor->x, cursor->y, cursor->x + w, cursor->y + 28};
    ui_metric_chip(hdc, r, ia, la, a, RGB(118, 143, 95));
    r.left += w + 8; r.right += w + 8;
    ui_metric_chip(hdc, r, ib, lb, b, RGB(83, 123, 166));
    r.left += w + 8; r.right += w + 8;
    ui_metric_chip(hdc, r, ic, lc, c, RGB(188, 154, 88));
    cursor->y += 36;
}

static void draw_metric_pair(HDC hdc, UiCursor *cursor, int a, int b,
                             IconId ia, IconId ib, const char *la, const char *lb) {
    int w = (cursor->width - 8) / 2;
    RECT r = {cursor->x, cursor->y, cursor->x + w, cursor->y + 28};
    ui_metric_chip(hdc, r, ia, la, a, RGB(118, 143, 95));
    r.left += w + 8; r.right += w + 8;
    ui_metric_chip(hdc, r, ib, lb, b, RGB(83, 123, 166));
    cursor->y += 36;
}

static void draw_country_header(HDC hdc, UiCursor *cursor, int civ_id, HFONT title_font, HFONT body_font) {
    RECT swatch = {cursor->x, cursor->y + 4, cursor->x + 18, cursor->y + 22};
    char text[128];

    SelectObject(hdc, title_font);
    fill_rect(hdc, swatch, civs[civ_id].color);
    snprintf(text, sizeof(text), "%c  %.63s", civs[civ_id].symbol, civs[civ_id].name);
    draw_text_line(hdc, cursor->x + 28, cursor->y, text, ui_theme_color(UI_COLOR_TEXT));
    cursor->y += 28;
    SelectObject(hdc, body_font);
    ui_row_text(hdc, cursor, tr("Capital", "首都"), capital_name_for_civ(civ_id));
}

static void draw_city_list(HDC hdc, UiCursor *cursor, int civ_id) {
    int i;
    char text[192];

    ui_section(hdc, cursor, tr("Provinces / Cities", "行省 / 城市"));
    for (i = 0; i < city_count; i++) {
        if (!cities[i].alive || cities[i].owner != civ_id) continue;
        snprintf(text, sizeof(text), "%s%s%s  %s %d",
                 cities[i].capital ? "* " : "", cities[i].name,
                 cities[i].port ? tr(" Port", " 港口") : "",
                 tr("Pop", "人口"), cities[i].population);
        draw_icon_text_line(hdc, cursor->x, cursor->y,
                            cities[i].capital ? ICON_CITY_CAPITAL : ICON_TERRITORY,
                            text, ui_theme_color(UI_COLOR_TEXT_MUTED));
        cursor->y += 22;
    }
}

static void draw_country_detail(HDC hdc, UiCursor *cursor, int civ_id, HFONT title_font, HFONT body_font) {
    CountrySummary country = summarize_country(civ_id);

    draw_country_header(hdc, cursor, civ_id, title_font, body_font);
    draw_metric_row(hdc, cursor, country.population, country_province_count(civ_id), country.cities,
                    ICON_POPULATION, ICON_TERRITORY, ICON_CITY_VILLAGE,
                    metric_label("Pop", "人口"), metric_label("Provinces", "省份"), metric_label("Cities", "城市"));
    draw_metric_row(hdc, cursor, war_estimated_soldiers(civ_id), country.money, civs[civ_id].disorder,
                    ICON_MILITARY, ICON_MONEY, ICON_DISORDER,
                    metric_label("Army", "军队"), metric_label("Money", "金钱"), metric_label("Disorder", "混乱"));
    draw_metric_row(hdc, cursor, country.ports, country.territory, population_pressure_for_civ(civ_id),
                    ICON_HARBOR, ICON_TERRITORY, ICON_POPULATION,
                    metric_label("Ports", "港口"), metric_label("Land", "土地"), metric_label("Pressure", "压力"));
    ui_section(hdc, cursor, tr("Technology", "科技阶段"));
    ui_row_text(hdc, cursor, tr("Stage", "阶段"), technology_stage_name(civs[civ_id].tech_stage, ui_language));
    {
        char text[96];
        snprintf(text, sizeof(text), "%d/10   %s %d", civs[civ_id].tech_stage,
                 tr("Years to next", "距下阶段年数"), technology_years_to_next(civ_id));
        ui_row_text(hdc, cursor, tr("Progress", "进度"), text);
    }
    ui_row_text(hdc, cursor, tr("Effect", "效果"), technology_stage_effect(civs[civ_id].tech_stage, ui_language));
    ui_row_text(hdc, cursor, tr("Pressure", "压力"),
                tr("Population and resource saturation; high values affect disorder and expansion.",
                   "人口与资源饱和压力；高压力会影响混乱和扩张。"));
    ui_section(hdc, cursor, tr("Civilization Metrics", "文明指标"));
    draw_metric_row(hdc, cursor, civs[civ_id].governance, civs[civ_id].cohesion, civs[civ_id].production,
                    ICON_GOVERNANCE, ICON_COHESION, ICON_PRODUCTION,
                    metric_label("Gov", "治理"), metric_label("Coh", "凝聚"), metric_label("Prod", "生产"));
    draw_metric_row(hdc, cursor, civs[civ_id].military, civs[civ_id].commerce, civs[civ_id].logistics,
                    ICON_MILITARY, ICON_COMMERCE, ICON_LOGISTICS,
                    metric_label("Mil", "军备"), metric_label("Trade", "贸易"), metric_label("Log", "后勤"));
    draw_metric_pair(hdc, cursor, civs[civ_id].innovation, civs[civ_id].adaptation,
                     ICON_INNOVATION, ICON_ADAPTATION, metric_label("Tech", "技术"), metric_label("Adapt", "适应"));
    ui_row_text(hdc, cursor, tr("Adaptation", "适应力"),
                tr("Dynamic state from environment and pressure.", "由环境与压力派生的动态状态。"));
    ui_section(hdc, cursor, tr("Resources", "资源"));
    draw_metric_row(hdc, cursor, country.food, country.water, country.pop_capacity,
                    ICON_FOOD, ICON_WATER, ICON_POPULATION,
                    metric_label("Food", "粮食"), metric_label("Water", "水源"), metric_label("Cap", "承载"));
    draw_metric_row(hdc, cursor, country.livestock, country.wood, country.stone,
                    ICON_LIVESTOCK, ICON_WOOD, ICON_STONE,
                    metric_label("Herd", "畜牧"), metric_label("Wood", "木材"), metric_label("Stone", "石料"));
    draw_metric_row(hdc, cursor, country.minerals, country.money, country.habitability,
                    ICON_ORE, ICON_MONEY, ICON_HABITABILITY,
                    metric_label("Ore", "矿石"), metric_label("Money", "金钱"), metric_label("Live", "宜居"));
    ui_section(hdc, cursor, tr("Disorder", "混乱"));
    draw_metric_row(hdc, cursor, civs[civ_id].disorder_resource, civs[civ_id].disorder_plague, civs[civ_id].disorder_migration,
                    ICON_DISORDER, ICON_DISORDER, ICON_MIGRATION,
                    metric_label("Resource", "资源"), metric_label("Plague", "瘟疫"), metric_label("Migration", "迁徙"));
    draw_metric_pair(hdc, cursor, civs[civ_id].disorder_stability, civs[civ_id].disorder,
                     ICON_COUNTRY_DEFENSE, ICON_DISORDER, metric_label("Stability", "稳定"), metric_label("Total", "总计"));
    if (plague_civ_active_count(civ_id) > 0) {
        ui_section(hdc, cursor, tr("Plague", "瘟疫"));
        draw_metric_row(hdc, cursor, plague_civ_active_count(civ_id), plague_civ_peak_severity(civ_id), plague_civ_deaths_total(civ_id),
                        ICON_CITY_VILLAGE, ICON_DISORDER, ICON_POPULATION,
                        metric_label("Cities", "城市"), metric_label("Severity", "烈度"), metric_label("Deaths", "死亡"));
    }
    draw_city_list(hdc, cursor, civ_id);
}

void draw_country_panel(HDC hdc, RECT client, int x, HFONT title_font, HFONT body_font) {
    CountryPanelLayout layout;
    int civ_id = displayed_country();
    (void)x;

    SelectObject(hdc, body_font);
    country_panel_layout_build(client, &layout);
    draw_country_list(hdc, &layout);
    if (civ_id < 0 || civ_id >= civ_count) return;
    {
        HRGN clip = CreateRectRgn(layout.detail_viewport.left, layout.detail_viewport.top,
                                  layout.detail_viewport.right, layout.detail_viewport.bottom);
        UiCursor cursor = ui_cursor(layout.detail_viewport.left, layout.detail_viewport.top - layout.detail_scroll,
                                    layout.detail_viewport.right - layout.detail_viewport.left - 8,
                                    layout.detail_viewport.top - layout.detail_scroll +
                                    country_detail_content_height(civ_id) + 80);
        SelectClipRgn(hdc, clip);
        draw_country_detail(hdc, &cursor, civ_id, title_font, body_font);
        SelectClipRgn(hdc, NULL);
        DeleteObject(clip);
        draw_scrollbar(hdc, layout.detail_viewport, layout.detail_scroll, layout.detail_max_scroll);
    }
}

int country_panel_hit_test(RECT client, int mouse_x, int mouse_y) {
    CountryPanelLayout layout;
    int i;

    country_panel_layout_build(client, &layout);
    if (point_in_rect_local(layout.fallen_toggle, mouse_x, mouse_y)) return COUNTRY_PANEL_HIT_TOGGLE_FALLEN;
    if (layout.selected_detail && point_in_rect_local(layout.back_to_list, mouse_x, mouse_y)) {
        return COUNTRY_PANEL_HIT_BACK_TO_LIST;
    }
    if (layout.selected_detail && point_in_rect_local(layout.selected_summary, mouse_x, mouse_y)) return displayed_country();
    for (i = 0; i < layout.card_count; i++) {
        if (point_in_rect_local(layout.cards[i], mouse_x, mouse_y)) return layout.card_civ_ids[i];
    }
    return COUNTRY_PANEL_HIT_NONE;
}

int country_panel_scroll(RECT client, int delta) {
    CountryPanelLayout layout;
    int old_offset = country_detail_scroll_offset;

    country_panel_layout_build(client, &layout);
    if (!layout.selected_detail || layout.detail_max_scroll <= 0) return 0;
    country_detail_scroll_offset = clamp(country_detail_scroll_offset + delta, 0, layout.detail_max_scroll);
    return country_detail_scroll_offset != old_offset;
}
