#include "render_panel_internal.h"

#include "sim/population.h"
#include "ui/ui_widgets.h"

static int displayed_country(void) {
    int owner = selected_tile_owner();
    if (owner >= 0 && owner < civ_count) return owner;
    if (selected_civ >= 0 && selected_civ < civ_count) return selected_civ;
    return -1;
}

void draw_population_panel(HDC hdc, RECT client, int x, HFONT title_font, HFONT body_font) {
    UiCursor cursor = ui_cursor(x, TOP_BAR_H + 62, side_panel_w - FORM_X_PAD * 2, client.bottom - 64);
    int civ_id = displayed_country();
    PopulationSummary summary;
    char text[160];

    SelectObject(hdc, title_font);
    draw_text_line(hdc, x, cursor.y, tr("Population", "人口"), ui_theme_color(UI_COLOR_TEXT));
    cursor.y += 30;
    SelectObject(hdc, body_font);
    if (civ_id < 0 || civ_id >= civ_count) {
        ui_row_text(hdc, &cursor, tr("Selection", "选择"), tr("Select a country to inspect population.", "选择国家查看人口。"));
        return;
    }
    summary = population_country_summary(civ_id);
    snprintf(text, sizeof(text), "%c %.63s", civs[civ_id].symbol, civs[civ_id].name);
    ui_row_text(hdc, &cursor, tr("Country", "国家"), text);
    draw_population_pyramid(hdc, client, x, cursor.y + 8, cursor.width, civ_id, body_font);
    cursor.y += 190;
    ui_section(hdc, &cursor, tr("Cohort Summary", "年龄摘要"));
    ui_row_int(hdc, &cursor, tr("Total", "总人口"), summary.total);
    ui_row_int(hdc, &cursor, tr("Male", "男"), summary.male);
    ui_row_int(hdc, &cursor, tr("Female", "女"), summary.female);
    ui_row_int(hdc, &cursor, tr("Children", "儿童"), summary.children);
    ui_row_int(hdc, &cursor, tr("Working", "劳力"), summary.working);
    ui_row_int(hdc, &cursor, tr("Fertile", "育龄"), summary.fertile);
    ui_row_int(hdc, &cursor, tr("Recruitable", "征召"), summary.recruitable);
    ui_row_int(hdc, &cursor, tr("Elder", "老人"), summary.elder);
    ui_section(hdc, &cursor, tr("Pressure", "压力"));
    ui_row_int(hdc, &cursor, tr("Carrying capacity", "承载力"), summary.carrying_capacity);
    ui_row_int(hdc, &cursor, tr("Population pressure", "人口压力"), summary.pressure);
    ui_progress_bar(hdc, ui_take_rect(&cursor, 12), summary.pressure, 140,
                    summary.pressure > 100 ? ui_theme_color(UI_COLOR_DANGER) : ui_theme_color(UI_COLOR_GOOD));
}
