#include "render_panel_internal.h"

#include "sim/plague.h"
#include "sim/population.h"
#include "ui/ui_widgets.h"

static void metric3(HDC hdc, UiCursor *cursor, int a, int b, int c,
                    IconId ia, IconId ib, IconId ic,
                    const char *la, const char *lb, const char *lc) {
    int w = (cursor->width - 16) / 3;
    RECT r = {cursor->x, cursor->y, cursor->x + w, cursor->y + 28};
    ui_metric_chip(hdc, r, ia, la, a, RGB(83, 123, 166));
    r.left += w + 8; r.right += w + 8;
    ui_metric_chip(hdc, r, ib, lb, b, RGB(118, 143, 95));
    r.left += w + 8; r.right += w + 8;
    ui_metric_chip(hdc, r, ic, lc, c, RGB(188, 154, 88));
    cursor->y += 36;
}

static void sort_population_ids(int *ids, int count) {
    int i;
    int j;
    for (i = 1; i < count; i++) {
        int id = ids[i];
        int value = population_country_summary(id).total;
        j = i - 1;
        while (j >= 0 && population_country_summary(ids[j]).total < value) {
            ids[j + 1] = ids[j];
            j--;
        }
        ids[j + 1] = id;
    }
}

void draw_population_panel(HDC hdc, RECT client, int x, HFONT title_font, HFONT body_font) {
    UiCursor cursor = ui_cursor(x, TOP_BAR_H + 62, side_panel_w - FORM_X_PAD * 2, client.bottom - 64);
    PopulationSummary world = {0};
    int ids[MAX_CIVS];
    int id_count = 0;
    int active_civs = 0;
    int high_pressure = 0;
    int plague_deaths = 0;
    int active_fronts = 0;
    int i;
    char text[192];

    SelectObject(hdc, title_font);
    draw_text_line(hdc, x, cursor.y, tr("World Population", "世界人口"), ui_theme_color(UI_COLOR_TEXT));
    cursor.y += 30;
    SelectObject(hdc, body_font);
    for (i = 0; i < civ_count; i++) {
        PopulationSummary s;
        if (!civs[i].alive) continue;
        s = population_country_summary(i);
        ids[id_count++] = i;
        active_civs++;
        world.total += s.total;
        world.male += s.male;
        world.female += s.female;
        world.children += s.children;
        world.working += s.working;
        world.fertile += s.fertile;
        world.recruitable += s.recruitable;
        world.elder += s.elder;
        world.carrying_capacity += s.carrying_capacity;
        world.pressure += s.pressure;
        for (int band = 0; band < POP_COHORT_COUNT; band++) {
            world.cohorts[band].male += s.cohorts[band].male;
            world.cohorts[band].female += s.cohorts[band].female;
        }
        if (s.pressure > 100) high_pressure++;
        plague_deaths += plague_civ_deaths_total(i);
        active_fronts += war_front_count_for_civ(i);
    }
    if (active_civs <= 0) {
        ui_row_text(hdc, &cursor, tr("World", "世界"), tr("No alive countries yet.", "尚无存活国家。"));
        return;
    }
    world.pressure /= active_civs;
    metric3(hdc, &cursor, world.total, active_civs, world.pressure,
            ICON_POPULATION, ICON_TERRITORY, ICON_DISORDER,
            metric_label("World Pop", "世界人口"), metric_label("Countries", "国家"), metric_label("Pressure", "压力"));
    metric3(hdc, &cursor, world.children, world.working, world.elder,
            ICON_POPULATION, ICON_PRODUCTION, ICON_POPULATION,
            metric_label("Children", "儿童"), metric_label("Working", "劳力"), metric_label("Elder", "老人"));
    ui_section(hdc, &cursor, tr("Global Structure", "全球结构"));
    cursor.y = draw_population_pyramid_summary(hdc, client, x, cursor.y + 2, cursor.width, world, body_font);
    if (world.total > 0) {
        snprintf(text, sizeof(text), "%s %d%%   %s %d%%   %s %d%%",
                 tr("Children", "儿童"), world.children * 100 / world.total,
                 tr("Working", "劳力"), world.working * 100 / world.total,
                 tr("Elder", "老人"), world.elder * 100 / world.total);
        ui_row_text(hdc, &cursor, tr("Age Share", "年龄占比"), text);
        snprintf(text, sizeof(text), "%s %d%%   %s %d%%   %s %d%%",
                 tr("Male", "男性"), world.male * 100 / world.total,
                 tr("Fertile", "育龄"), world.fertile * 100 / world.total,
                 tr("Recruitable", "可征召"), world.recruitable * 100 / world.total);
        ui_row_text(hdc, &cursor, tr("Population Mix", "人口结构"), text);
    }
    ui_row_int(hdc, &cursor, tr("Male", "男性"), world.male);
    ui_row_int(hdc, &cursor, tr("Female", "女性"), world.female);
    ui_row_int(hdc, &cursor, tr("Fertile population", "育龄人口"), world.fertile);
    ui_row_int(hdc, &cursor, tr("Recruitable population", "可征召人口"), world.recruitable);
    ui_row_int(hdc, &cursor, tr("Carrying capacity", "人口承载力"), world.carrying_capacity);
    ui_progress_bar(hdc, ui_take_rect(&cursor, 12), world.pressure, 140,
                    world.pressure > 100 ? ui_theme_color(UI_COLOR_DANGER) : ui_theme_color(UI_COLOR_GOOD));
    ui_section(hdc, &cursor, tr("Global Loss / Pressure", "全球损失 / 压力"));
    ui_row_int(hdc, &cursor, tr("Plague deaths", "瘟疫死亡"), plague_deaths);
    ui_row_int(hdc, &cursor, tr("Active war fronts", "活跃战线"), active_fronts);
    ui_row_int(hdc, &cursor, tr("High-pressure countries", "高压国家"), high_pressure);
    ui_section(hdc, &cursor, tr("Population Ranking", "人口排行"));
    sort_population_ids(ids, id_count);
    for (i = 0; i < id_count && i < 8; i++) {
        PopulationSummary s = population_country_summary(ids[i]);
        snprintf(text, sizeof(text), "%d. %c %.62s", i + 1, civs[ids[i]].symbol,
                 civilization_display_name(ids[i]));
        ui_row_int(hdc, &cursor, text, s.total);
    }
}
