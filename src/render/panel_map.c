#include "render_panel_internal.h"

#include "core/profiler.h"
#include "game/game_loop.h"
#include "ui/ui_theme.h"

static void draw_side_panel_handle(HDC hdc, RECT client) {
    RECT handle = get_side_panel_handle_rect(client);
    COLORREF bg = side_panel_collapsed ? RGB(38, 48, 54) : RGB(33, 40, 46);
    COLORREF fg = RGB(220, 228, 232);
    HBRUSH border;
    int hot = point_in_rect(handle, hover_x, hover_y);
    int pressed = hot && (GetKeyState(VK_LBUTTON) & 0x8000);

    if (hot) bg = RGB(54, 66, 74);
    if (pressed) bg = RGB(64, 75, 82);
    fill_rect(hdc, handle, bg);
    border = CreateSolidBrush(RGB(75, 90, 100));
    FrameRect(hdc, &handle, border);
    DeleteObject(border);
    draw_center_text(hdc, handle, side_panel_collapsed ? "<" : ">", fg);
}

void draw_side_panel(HDC hdc, RECT client) {
    int x = client.right - side_panel_w + 18;
    RECT panel = {client.right - side_panel_w, TOP_BAR_H, client.right, client.bottom};
    RECT divider = {client.right - side_panel_w - 3, TOP_BAR_H, client.right - side_panel_w + 3, client.bottom};
    HFONT title_font = CreateFontW(21, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                   DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    HFONT body_font = CreateFontW(17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                  DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    HFONT old_font;

    fill_rect(hdc, panel, ui_theme_color(UI_COLOR_PANEL));
    fill_rect(hdc, divider, ui_theme_color(UI_COLOR_PANEL_LINE));
    draw_side_panel_handle(hdc, client);
    if (side_panel_collapsed) {
        DeleteObject(title_font);
        DeleteObject(body_font);
        return;
    }
    draw_panel_tabs(hdc, client);

    old_font = SelectObject(hdc, title_font);
    if (panel_tab == PANEL_COUNTRY) draw_country_panel(hdc, client, x, title_font, body_font);
    else if (panel_tab == PANEL_POPULATION) draw_population_panel(hdc, client, x, title_font, body_font);
    else if (panel_tab == PANEL_PLAGUE) draw_plague_panel(hdc, client, x, title_font, body_font);
    else if (panel_tab == PANEL_WORLD) draw_worldgen_panel(hdc, client, x, title_font, body_font);
    else draw_debug_panel(hdc, client, x, title_font, body_font);

    SelectObject(hdc, old_font);
    DeleteObject(title_font);
    DeleteObject(body_font);
}

void draw_bottom_bar(HDC hdc, RECT client) {
    RECT bar = {0, client.bottom - BOTTOM_BAR_H, client.right - side_panel_w, client.bottom};
    RECT play = get_play_button_rect(client);
    char text[256];
    const char *speed_name_zh[SPEED_COUNT] = {"观察 10秒/月", "慢速 5秒/月", "正常 1秒/月", "快速 0.25秒/月", "极速 0.1秒/月"};
    const char *speed_name = ui_language == UI_LANG_ZH ? speed_name_zh[speed_index] : SPEED_NAMES[speed_index];
    int actual_ms = game_loop_actual_ms_per_month();
    int pending = game_loop_pending_months();
    RuntimeProfilerSnapshot profiler;
    const char *render_status;
    const char *sim_status;
    char actual_text[32];
    RECT status_rect = {336, client.bottom - 40, client.right - side_panel_w - 12, client.bottom - 8};
    int i;

    profiler_snapshot(&profiler);
    render_status = profiler.render_avg_ms <= 18 ? tr("smooth", "流畅") :
                    profiler.render_avg_ms <= 33 ? tr("busy", "繁忙") : tr("slow", "偏慢");
    sim_status = !auto_run ? tr("paused", "已暂停") :
                 game_loop_simulation_overloaded() ? tr("overloaded", "过载") : tr("stable", "稳定");
    if (actual_ms > 0) snprintf(actual_text, sizeof(actual_text), "%.1fs/%s", actual_ms / 1000.0, tr("month", "月"));
    else snprintf(actual_text, sizeof(actual_text), "--");

    fill_rect(hdc, bar, ui_theme_color(UI_COLOR_CHROME));
    fill_rect(hdc, play, auto_run ? RGB(87, 93, 78) : RGB(48, 56, 58));
    draw_center_text(hdc, play, auto_run ? "||" : ">", RGB(245, 248, 250));

    for (i = 0; i < SPEED_COUNT; i++) {
        RECT button = get_speed_button_rect(client, i);
        fill_rect(hdc, button, i == speed_index ? RGB(87, 93, 78) : RGB(48, 56, 58));
        draw_center_text(hdc, button, speed_button_icon(i), RGB(235, 240, 244));
    }

    snprintf(text, sizeof(text), "%s: %s %dms | %s: %s %s/%s | %s: %s | %s: %d | %s: %s",
             tr("Render", "渲染"), render_status, profiler.render_avg_ms,
             tr("Sim target", "模拟目标"), speed_name, speed_seconds_text(speed_index), tr("month", "月"),
             tr("Actual", "实际"), actual_text, tr("Queue", "队列"), pending,
             tr("Status", "状态"), sim_status);
    draw_text_rect(hdc, status_rect, text, RGB(225, 230, 235),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}

static void draw_legend_item(HDC hdc, int x, int y, COLORREF color, const char *name) {
    RECT swatch = {x, y + 3, x + 16, y + 15};
    fill_rect(hdc, swatch, color);
    draw_text_line(hdc, x + 22, y, name, RGB(232, 238, 242));
}

static void draw_legend_group(HDC hdc, int x, int y, const char *name) {
    draw_text_line(hdc, x, y, name, RGB(156, 174, 184));
}

void draw_map_legend(HDC hdc, RECT client) {
    const Geography geographies[] = {
        GEO_PLAIN, GEO_HILL, GEO_MOUNTAIN, GEO_PLATEAU,
        GEO_BASIN, GEO_CANYON, GEO_VOLCANO, GEO_DELTA,
        GEO_WETLAND, GEO_OASIS, GEO_ISLAND
    };
    const Climate climates[] = {
        CLIMATE_TROPICAL_RAINFOREST, CLIMATE_TROPICAL_MONSOON, CLIMATE_TROPICAL_SAVANNA,
        CLIMATE_DESERT, CLIMATE_SEMI_ARID, CLIMATE_MEDITERRANEAN, CLIMATE_OCEANIC,
        CLIMATE_TEMPERATE_MONSOON, CLIMATE_CONTINENTAL, CLIMATE_SUBARCTIC, CLIMATE_TUNDRA,
        CLIMATE_ICE_CAP, CLIMATE_ALPINE, CLIMATE_HIGHLAND_PLATEAU
    };
    int geo_count = (int)(sizeof(geographies) / sizeof(geographies[0]));
    int climate_count = (int)(sizeof(climates) / sizeof(climates[0]));
    int line_h = 20;
    int x;
    int y;
    int i;
    RECT box = get_map_legend_box_rect(client);
    RECT toggle = get_map_legend_toggle_rect(client);
    HBRUSH border_brush;
    int saved_dc;

    if (IsRectEmpty(&box)) return;

    fill_rect_alpha(hdc, box, RGB(31, 37, 43), 218);
    border_brush = CreateSolidBrush(RGB(76, 92, 104));
    FrameRect(hdc, &box, border_brush);
    DeleteObject(border_brush);
    saved_dc = SaveDC(hdc);
    IntersectClipRect(hdc, box.left, box.top, box.right, box.bottom);
    fill_rect_alpha(hdc, toggle, RGB(47, 58, 68), 236);
    draw_center_text(hdc, toggle, map_legend_collapsed ? "^" : "v", RGB(236, 242, 246));
    draw_text_line(hdc, box.left + 10, box.top + 8, tr("Map Legend", "地图图例"), RGB(245, 245, 245));
    if (map_legend_collapsed) {
        RestoreDC(hdc, saved_dc);
        return;
    }

    x = box.left + 10;
    y = box.top + 30;

    if (display_mode == DISPLAY_CLIMATE) {
        draw_legend_group(hdc, x, y, tr("Climate", "气候"));
        y += line_h;
        for (i = 0; i < climate_count; i++) {
            draw_legend_item(hdc, x, y + i * line_h, climate_color(climates[i]), climate_name(climates[i]));
        }
        RestoreDC(hdc, saved_dc);
        return;
    }

    draw_legend_group(hdc, x, y, tr("Water", "水域"));
    y += line_h;
    draw_legend_item(hdc, x, y, RGB(92, 177, 214), tr("Shallow Sea", "浅海"));
    draw_legend_item(hdc, x, y + line_h, RGB(38, 92, 154), tr("Deep Sea", "深海"));
    y += line_h * 3;
    draw_legend_group(hdc, x, y - line_h, tr("Terrain", "地形"));
    for (i = 0; i < geo_count; i++) {
        draw_legend_item(hdc, x, y + i * line_h, geography_color(geographies[i]), geography_name(geographies[i]));
    }
    if (display_mode == DISPLAY_ALL) {
        x = box.left + 190;
        y = box.top + 30;
        draw_legend_group(hdc, x, y, tr("Climate", "气候"));
        y += line_h;
        for (i = 0; i < climate_count; i++) {
            draw_legend_item(hdc, x, y + i * line_h, climate_color(climates[i]), climate_name(climates[i]));
        }
    }
    RestoreDC(hdc, saved_dc);
}
