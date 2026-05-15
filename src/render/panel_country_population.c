#include "render/panel_country_population.h"

#include "render/snapshot_ui.h"
#include "render_panel_internal.h"

static COLORREF pressure_color(int pressure) {
    if (pressure < 50) return RGB(83, 143, 98);
    if (pressure <= 80) return RGB(106, 158, 186);
    if (pressure <= 105) return RGB(196, 154, 72);
    return RGB(188, 78, 68);
}

static const char *pressure_label(int pressure) {
    if (pressure < 50) return tr("Low", "低");
    if (pressure <= 80) return tr("Normal", "正常");
    if (pressure <= 105) return tr("High", "偏高");
    return tr("Overloaded", "过载");
}

int country_population_tab_height(int civ_id) {
    int owned = 0;
    int i;
    for (i = 0; i < snapshot_ui_city_count(); i++) {
        const SnapshotCity *city = snapshot_ui_city(i);
        if (city && city->alive && city->owner == civ_id) owned++;
    }
    return 365 + owned * 22;
}

void draw_country_population_tab(HDC hdc, RECT client, UiCursor *cursor,
                                 int civ_id, HFONT body_font) {
    const SnapshotCiv *civ = snapshot_ui_civ(civ_id);
    PopulationSummary summary = civ ? civ->population_summary : (PopulationSummary){0};
    int i;
    char text[192];

    ui_section(hdc, cursor, tr("Population", "人口"));
    cursor->y = draw_population_pyramid_summary(hdc, client, cursor->x, cursor->y + 2,
                                                cursor->width, summary, body_font);
    snprintf(text, sizeof(text), "%d / %d   %s",
             summary.total, summary.carrying_capacity, pressure_label(summary.pressure));
    ui_row_text(hdc, cursor, tr("Population / Capacity", "人口 / 承载"), text);
    ui_progress_bar(hdc, ui_take_rect(cursor, 12), summary.pressure, 140, pressure_color(summary.pressure));
    ui_row_int(hdc, cursor, tr("Male", "男性"), summary.male);
    ui_row_int(hdc, cursor, tr("Female", "女性"), summary.female);
    ui_row_int(hdc, cursor, tr("Fertile population", "育龄人口"), summary.fertile);
    ui_row_int(hdc, cursor, tr("Recruitable population", "可征召人口"), summary.recruitable);
    ui_section(hdc, cursor, tr("City Population", "城市人口"));
    for (i = 0; i < snapshot_ui_city_count(); i++) {
        const SnapshotCity *city = snapshot_ui_city(i);
        if (!city || !city->alive || city->owner != civ_id) continue;
        snprintf(text, sizeof(text), "%s%s%s",
                 city->capital ? "* " : "", city->name,
                 city->port ? tr(" Port", " 港口") : "");
        ui_row_int(hdc, cursor, text, city->population);
    }
}
