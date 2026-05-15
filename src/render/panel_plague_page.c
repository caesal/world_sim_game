#include "render_panel_internal.h"

#include "render/snapshot_ui.h"
#include "render/ui_format.h"
#include "ui/ui_widgets.h"

void draw_plague_panel(HDC hdc, RECT client, int x, HFONT title_font, HFONT body_font) {
    UiCursor cursor = ui_cursor(x, TOP_BAR_H + 62, side_panel_w - FORM_X_PAD * 2, client.bottom - 64);
    int active_cities = 0;
    int active_countries = 0;
    int peak_severity = 0;
    int total_deaths = 0;
    int i;
    char text[192];
    const RenderSnapshot *snapshot = snapshot_ui_current();

    SelectObject(hdc, title_font);
    draw_text_line(hdc, x, cursor.y, tr("Plague", "瘟疫"), ui_theme_color(UI_COLOR_TEXT));
    cursor.y += 30;
    SelectObject(hdc, body_font);
    ui_section(hdc, &cursor, tr("Map Overlay", "地图叠加"));
    ui_row_int(hdc, &cursor, tr("Fog opacity 0-100", "瘟疫雾 0-100"), plague_fog_alpha);
    ui_row_text(hdc, &cursor, tr("Effect", "作用"),
                tr("Viewer only; does not change plague severity, spread, or deaths.",
                   "仅影响显示；不改变瘟疫烈度、传播或死亡。"));
    draw_setup_slider(hdc, client, UI_SLIDER_PLAGUE_FOG_ALPHA, tr("Plague Fog", "瘟疫雾"), plague_fog_alpha);
    cursor.y = TOP_BAR_H + 252;
    for (i = 0; snapshot && i < snapshot->civ_count; i++) {
        const SnapshotCiv *civ = &snapshot->civs[i];
        if (!civ->alive || civ->plague_active_count <= 0) continue;
        active_countries++;
        active_cities += civ->plague_active_count;
        peak_severity = max(peak_severity, civ->plague_peak_severity);
        total_deaths += civ->plague_deaths_total;
    }
    ui_section(hdc, &cursor, tr("Outbreak Summary", "爆发摘要"));
    snprintf(text, sizeof(text), "%s %d   %s %d   %s %d   %s %d",
             tr("Countries", "国家"), active_countries,
             tr("Cities", "城市"), active_cities,
             tr("Peak", "最高"), peak_severity,
             tr("Deaths", "死亡"), total_deaths);
    ui_row_text(hdc, &cursor, tr("Active outbreak", "当前爆发"), text);
    ui_section(hdc, &cursor, tr("Infected Cities", "感染城市"));
    active_cities = 0;
    for (i = 0; snapshot && i < snapshot->city_count; i++) {
        const SnapshotCity *city = &snapshot->cities[i];
        if (!city->alive || !city->plague_active) continue;
        if (cursor.y > cursor.bottom - 30) break;
        {
            char span[48];
            ui_format_months(span, sizeof(span), city->plague_months_left, UI_MONTH_ZERO_DONE);
            snprintf(text, sizeof(text), "%s  %s %d  %s %s  %s %d",
                 city->name,
                 tr("Severity", "烈度"), city->plague_severity,
                 tr("Months", "剩余"), span,
                 tr("Deaths", "死亡"), city->plague_deaths_total);
        }
        draw_icon_text_line(hdc, cursor.x, cursor.y, ICON_DISORDER, text, RGB(190, 220, 196));
        cursor.y += 23;
        active_cities++;
    }
    if (active_cities == 0) {
        ui_row_text(hdc, &cursor, tr("Status", "状态"), tr("No active plague cities.", "没有正在感染的城市。"));
    }
    ui_section(hdc, &cursor, tr("Country Summary", "国家摘要"));
    for (i = 0; snapshot && i < snapshot->civ_count; i++) {
        const SnapshotCiv *civ = &snapshot->civs[i];
        if (!civ->alive || civ->plague_active_count <= 0) continue;
        if (cursor.y > cursor.bottom - 30) break;
        snprintf(text, sizeof(text), "%c %.36s  %s %d  %s %d  %s %d",
                 civ->symbol, snapshot_ui_civ_name(i),
                 tr("Cities", "城市"), civ->plague_active_count,
                 tr("Peak", "最高"), civ->plague_peak_severity,
                 tr("Deaths", "死亡"), civ->plague_deaths_total);
        draw_text_line(hdc, cursor.x, cursor.y, text, ui_theme_color(UI_COLOR_TEXT_MUTED));
        cursor.y += 22;
    }
    ui_section(hdc, &cursor, tr("Spread Factors", "传播因素"));
    ui_row_text(hdc, &cursor, tr("Factors", "因素"), tr("Water, ports, migration, war casualties, density.", "水源、港口、迁徙、战争伤亡、密度。"));
    ui_row_text(hdc, &cursor, tr("Simulation", "模拟"), tr("Monthly disease logic is unchanged.", "瘟疫月度逻辑未改变。"));
}
