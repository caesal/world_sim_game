#include "render_panel_internal.h"

#include "sim/plague.h"
#include "sim/population.h"
#include "ui/ui_widgets.h"

static int displayed_country(void) {
    int owner = selected_tile_owner();
    if (owner >= 0 && owner < civ_count) return owner;
    if (selected_civ >= 0 && selected_civ < civ_count) return selected_civ;
    return -1;
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
    int hidden = 0;
    char text[192];

    ui_section(hdc, cursor, tr("Provinces / Cities", "行省 / 城市"));
    for (i = 0; i < city_count; i++) {
        if (!cities[i].alive || cities[i].owner != civ_id) continue;
        if (cursor->y > cursor->bottom - 132) {
            hidden++;
            continue;
        }
        snprintf(text, sizeof(text), "%s%s%s  %s %d",
                 cities[i].capital ? "* " : "",
                 cities[i].name,
                 cities[i].port ? tr(" Port", " 港口") : "",
                 tr("Pop", "人口"), cities[i].population);
        draw_icon_text_line(hdc, cursor->x, cursor->y,
                            cities[i].capital ? ICON_CITY_CAPITAL : ICON_TERRITORY,
                            text, ui_theme_color(UI_COLOR_TEXT_MUTED));
        cursor->y += 22;
    }
    if (hidden > 0) {
        snprintf(text, sizeof(text), "%d %s", hidden, tr("more hidden", "项未显示"));
        ui_row_text(hdc, cursor, tr("Overflow", "更多"), text);
    }
}

static void draw_civ_list(HDC hdc, UiCursor *cursor) {
    int i;
    char text[160];

    ui_section(hdc, cursor, tr("Civilizations", "文明"));
    for (i = 0; i < civ_count && cursor->y < cursor->bottom - 40; i++) {
        RECT swatch = {cursor->x, cursor->y + 4, cursor->x + 14, cursor->y + 18};
        fill_rect(hdc, swatch, civs[i].color);
        snprintf(text, sizeof(text), "%c %.42s  %s %d  %s %d",
                 civs[i].symbol, civs[i].name,
                 tr("Pop", "人口"), civs[i].population,
                 tr("Land", "土地"), civs[i].territory);
        draw_text_line(hdc, cursor->x + 22, cursor->y, text,
                       i == selected_civ ? RGB(234, 208, 130) : ui_theme_color(UI_COLOR_TEXT_MUTED));
        cursor->y += 21;
    }
}

void draw_country_panel(HDC hdc, RECT client, int x, HFONT title_font, HFONT body_font) {
    UiCursor cursor = ui_cursor(x, TOP_BAR_H + 62, side_panel_w - FORM_X_PAD * 2, client.bottom - 64);
    int civ_id = displayed_country();
    CountrySummary country;

    if (civ_id < 0 || civ_id >= civ_count) {
        SelectObject(hdc, title_font);
        draw_text_line(hdc, x, cursor.y, tr("Country Dashboard", "国家面板"), ui_theme_color(UI_COLOR_TEXT));
        cursor.y += 34;
        SelectObject(hdc, body_font);
        ui_row_text(hdc, &cursor, tr("Selection", "选择"), tr("Select a country tile or a civilization.", "选择国家地块或文明。"));
        draw_civ_list(hdc, &cursor);
        return;
    }
    country = summarize_country(civ_id);
    draw_country_header(hdc, &cursor, civ_id, title_font, body_font);
    draw_metric_row(hdc, &cursor, country.population, country.territory, country.cities,
                    ICON_POPULATION, ICON_TERRITORY, ICON_CITY_VILLAGE,
                    metric_label("Pop", "人口"), metric_label("Land", "土地"), metric_label("Cities", "城市"));
    draw_metric_row(hdc, &cursor, country.ports, civs[civ_id].disorder, population_pressure_for_civ(civ_id),
                    ICON_HARBOR, ICON_DISORDER, ICON_POPULATION,
                    metric_label("Ports", "港口"), metric_label("Disorder", "混乱"), metric_label("Pressure", "压力"));
    ui_section(hdc, &cursor, tr("Civilization Metrics", "文明指标"));
    draw_metric_row(hdc, &cursor, civs[civ_id].governance, civs[civ_id].cohesion, civs[civ_id].production,
                    ICON_GOVERNANCE, ICON_COHESION, ICON_PRODUCTION,
                    metric_label("Gov", "治理"), metric_label("Coh", "凝聚"), metric_label("Prod", "生产"));
    draw_metric_row(hdc, &cursor, civs[civ_id].military, civs[civ_id].commerce, civs[civ_id].logistics,
                    ICON_MILITARY, ICON_COMMERCE, ICON_LOGISTICS,
                    metric_label("Mil", "军备"), metric_label("Trade", "贸易"), metric_label("Log", "后勤"));
    draw_metric_pair(hdc, &cursor, civs[civ_id].innovation, civs[civ_id].adaptation,
                     ICON_INNOVATION, ICON_ADAPTATION,
                     metric_label("Tech", "技术"), metric_label("Adapt", "适应"));
    ui_row_text(hdc, &cursor, tr("Adaptation", "适应力"), tr("Dynamic state from environment and pressure.", "由环境与压力派生的动态状态。"));
    ui_section(hdc, &cursor, tr("Resources", "资源"));
    draw_metric_row(hdc, &cursor, country.food, country.water, country.pop_capacity,
                    ICON_FOOD, ICON_WATER, ICON_POPULATION,
                    metric_label("Food", "粮食"), metric_label("Water", "水源"), metric_label("Cap", "承载"));
    draw_metric_row(hdc, &cursor, country.livestock, country.wood, country.stone,
                    ICON_LIVESTOCK, ICON_WOOD, ICON_STONE,
                    metric_label("Herd", "畜牧"), metric_label("Wood", "木材"), metric_label("Stone", "石料"));
    draw_metric_row(hdc, &cursor, country.minerals, country.money, country.habitability,
                    ICON_ORE, ICON_MONEY, ICON_HABITABILITY,
                    metric_label("Ore", "矿石"), metric_label("Money", "金钱"), metric_label("Live", "宜居"));
    ui_section(hdc, &cursor, tr("Disorder", "混乱"));
    draw_metric_row(hdc, &cursor, civs[civ_id].disorder_resource, civs[civ_id].disorder_plague, civs[civ_id].disorder_migration,
                    ICON_DISORDER, ICON_DISORDER, ICON_MIGRATION,
                    metric_label("Resource", "资源"), metric_label("Plague", "瘟疫"), metric_label("Migration", "迁徙"));
    draw_metric_pair(hdc, &cursor, civs[civ_id].disorder_stability, civs[civ_id].disorder,
                     ICON_COUNTRY_DEFENSE, ICON_DISORDER,
                     metric_label("Stability", "稳定"), metric_label("Total", "总计"));
    if (plague_civ_active_count(civ_id) > 0) {
        ui_section(hdc, &cursor, tr("Plague", "瘟疫"));
        draw_metric_row(hdc, &cursor, plague_civ_active_count(civ_id), plague_civ_peak_severity(civ_id), plague_civ_deaths_total(civ_id),
                        ICON_CITY_VILLAGE, ICON_DISORDER, ICON_POPULATION,
                        metric_label("Cities", "城市"), metric_label("Severity", "烈度"), metric_label("Deaths", "死亡"));
    }
    draw_city_list(hdc, &cursor, civ_id);
    draw_civ_list(hdc, &cursor);
}
