#include "render_panel_internal.h"

#include "sim/plague.h"
#include "ui/ui_widgets.h"

void draw_plague_panel(HDC hdc, RECT client, int x, HFONT title_font, HFONT body_font) {
    UiCursor cursor = ui_cursor(x, TOP_BAR_H + 62, side_panel_w - FORM_X_PAD * 2, client.bottom - 64);
    int active_cities = 0;
    int i;
    char text[192];

    SelectObject(hdc, title_font);
    draw_text_line(hdc, x, cursor.y, tr("Plague", "瘟疫"), ui_theme_color(UI_COLOR_TEXT));
    cursor.y += 30;
    SelectObject(hdc, body_font);
    ui_section(hdc, &cursor, tr("Map Overlay", "地图叠加"));
    ui_row_int(hdc, &cursor, tr("Fog opacity", "瘟疫雾"), plague_fog_alpha);
    draw_setup_slider(hdc, client, UI_SLIDER_PLAGUE_FOG_ALPHA, tr("Plague Fog", "瘟疫雾"), plague_fog_alpha);
    cursor.y = TOP_BAR_H + 226;
    ui_section(hdc, &cursor, tr("Active Cities", "感染城市"));
    for (i = 0; i < city_count; i++) {
        if (!cities[i].alive || !plague_city_active(i)) continue;
        if (cursor.y > cursor.bottom - 30) break;
        snprintf(text, sizeof(text), "%s  %s %d  %s %d  %s %d",
                 cities[i].name,
                 tr("Severity", "烈度"), plague_city_severity(i),
                 tr("Months", "剩余"), plague_city_months_left(i),
                 tr("Deaths", "死亡"), plague_city_deaths_total(i));
        draw_icon_text_line(hdc, cursor.x, cursor.y, ICON_DISORDER, text, RGB(190, 220, 196));
        cursor.y += 23;
        active_cities++;
    }
    if (active_cities == 0) {
        ui_row_text(hdc, &cursor, tr("Status", "状态"), tr("No active plague cities.", "没有正在感染的城市。"));
    }
    ui_section(hdc, &cursor, tr("Country Summary", "国家摘要"));
    for (i = 0; i < civ_count; i++) {
        if (!civs[i].alive || plague_civ_active_count(i) <= 0) continue;
        if (cursor.y > cursor.bottom - 30) break;
        snprintf(text, sizeof(text), "%c %.36s  %s %d  %s %d  %s %d",
                 civs[i].symbol, civs[i].name,
                 tr("Cities", "城市"), plague_civ_active_count(i),
                 tr("Peak", "最高"), plague_civ_peak_severity(i),
                 tr("Deaths", "死亡"), plague_civ_deaths_total(i));
        draw_text_line(hdc, cursor.x, cursor.y, text, ui_theme_color(UI_COLOR_TEXT_MUTED));
        cursor.y += 22;
    }
    ui_section(hdc, &cursor, tr("Spread Factors", "传播因素"));
    ui_row_text(hdc, &cursor, tr("Factors", "因素"), tr("Water, ports, migration, war casualties, density.", "水源、港口、迁徙、战争伤亡、密度。"));
    ui_row_text(hdc, &cursor, tr("Simulation", "模拟"), tr("Monthly disease logic is unchanged.", "瘟疫月度逻辑未改变。"));
}
