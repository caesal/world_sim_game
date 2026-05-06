#include "render_panel_internal.h"

#include "game/game_loop.h"
#include "sim/expansion.h"
#include "ui/ui_widgets.h"

void draw_debug_panel(HDC hdc, RECT client, int x, HFONT title_font, HFONT body_font) {
    UiCursor cursor = ui_cursor(x, TOP_BAR_H + 62, side_panel_w - FORM_X_PAD * 2, client.bottom - 64);
    int i;
    char text[160];

    SelectObject(hdc, title_font);
    draw_text_line(hdc, x, cursor.y, tr("Debug / Advanced", "调试 / 高级"), ui_theme_color(UI_COLOR_TEXT));
    cursor.y += 32;
    SelectObject(hdc, body_font);
    ui_section(hdc, &cursor, tr("Map Layers", "地图图层"));
    for (i = 0; i < MAP_DISPLAY_MODE_COUNT; i++) {
        const char *names_en[MAP_DISPLAY_MODE_COUNT] = {"All", "Climate", "Geography", "Regions", "Political"};
        const char *names_zh[MAP_DISPLAY_MODE_COUNT] = {"全部", "气候", "地理", "区域", "政治"};
        RECT button = get_mode_button_rect(client, i);
        fill_rect(hdc, button, MAP_DISPLAY_MODES[i] == display_mode ? RGB(87, 93, 78) : ui_theme_color(UI_COLOR_PANEL_SOFT));
        draw_center_text(hdc, button, tr(names_en[i], names_zh[i]), ui_theme_color(UI_COLOR_TEXT));
    }
    cursor.y = TOP_BAR_H + 310;
    ui_section(hdc, &cursor, tr("Simulation Timing", "模拟计时"));
    ui_row_text(hdc, &cursor, tr("Target", "目标"), speed_seconds_text(speed_index));
    snprintf(text, sizeof(text), "%d ms/month", game_loop_actual_ms_per_month());
    ui_row_text(hdc, &cursor, tr("Actual", "实际"), game_loop_actual_ms_per_month() > 0 ? text : tr("No sample yet", "暂无样本"));
    ui_row_int(hdc, &cursor, tr("Pending months", "积压月份"), game_loop_pending_months());
    if (selected_civ >= 0 && selected_civ < civ_count) {
        CountrySummary country = summarize_country(selected_civ);
        ui_section(hdc, &cursor, tr("Expansion Gate", "扩张门槛"));
        ui_row_int(hdc, &cursor, tr("Need", "需求"), expansion_need_for_civ(selected_civ, country.resource_score));
        ui_row_int(hdc, &cursor, tr("Threshold", "门槛"), expansion_threshold_for_civ(selected_civ));
        ui_section(hdc, &cursor, tr("Legacy / Internal", "兼容 / 内部"));
        ui_row_int(hdc, &cursor, tr("Legacy culture", "兼容文化"), civs[selected_civ].culture);
    }
    ui_row_text(hdc, &cursor, tr("Raw tile modes", "原始地块模式"), tr("Use Map Layers above.", "使用上方地图图层。"));
    ui_section(hdc, &cursor, tr("Shortcuts", "快捷键"));
    ui_row_text(hdc, &cursor, "Space", tr("Run / pause", "开始 / 暂停"));
    ui_row_text(hdc, &cursor, "F1", tr("Add civilization from World form", "用世界页表单添加文明"));
    ui_row_text(hdc, &cursor, "F2", tr("Apply selected civilization form", "应用选中文明表单"));
    ui_row_text(hdc, &cursor, "F5", tr("Generate world from World page settings", "按世界页设置生成世界"));
}
