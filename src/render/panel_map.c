#include "render_panel_internal.h"

#include "game/game_loop.h"
#include "ui/ui_theme.h"

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
    draw_panel_tabs(hdc, client);

    old_font = SelectObject(hdc, title_font);
    if (panel_tab == PANEL_SELECTION) draw_selection_panel(hdc, client, x, title_font, body_font);
    else if (panel_tab == PANEL_COUNTRY) draw_country_panel(hdc, client, x, title_font, body_font);
    else if (panel_tab == PANEL_DIPLOMACY) draw_diplomacy_tab(hdc, client, x, title_font, body_font);
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
    const char *speed_name_zh[3] = {"慢速", "中速", "快速"};
    const char *speed_name = ui_language == UI_LANG_ZH ? speed_name_zh[speed_index] : SPEED_NAMES[speed_index];
    int actual_ms = game_loop_actual_ms_per_month();
    int pending = game_loop_pending_months();
    char speed_detail[64];
    int i;

    fill_rect(hdc, bar, ui_theme_color(UI_COLOR_CHROME));
    fill_rect(hdc, play, auto_run ? RGB(87, 93, 78) : RGB(48, 56, 58));
    draw_center_text(hdc, play, auto_run ? "||" : ">", RGB(245, 248, 250));

    for (i = 0; i < 3; i++) {
        RECT button = get_speed_button_rect(client, i);
        fill_rect(hdc, button, i == speed_index ? RGB(87, 93, 78) : RGB(48, 56, 58));
        draw_center_text(hdc, button, i == 0 ? ">" : (i == 1 ? ">>" : ">>>"), RGB(235, 240, 244));
    }

    if (auto_run && actual_ms > 0) {
        snprintf(speed_detail, sizeof(speed_detail), "%s/%s %s %.2fs/%s%s%d",
                 speed_seconds_text(speed_index), tr("month", "月"), tr("actual", "实际"),
                 actual_ms / 1000.0, tr("month", "月"),
                 pending > 0 ? tr(" queue ", " 队列 ") : "", pending);
    } else {
        snprintf(speed_detail, sizeof(speed_detail), "%s/%s %s",
                 speed_seconds_text(speed_index), tr("month", "月"), tr("target", "目标"));
    }
    snprintf(text, sizeof(text), "%s: %s (%s)    %s    F5 %s    F1/F2 %s",
              tr("Speed", "速度"), speed_name, speed_detail,
              tr("Space play/pause", "空格播放/暂停"),
              tr("World", "世界"), tr("Country commands", "国家命令"));
    draw_text_line(hdc, 216, client.bottom - 29, text, RGB(225, 230, 235));
}

static void draw_legend_item(HDC hdc, int x, int y, COLORREF color, const char *name) {
    RECT swatch = {x, y + 3, x + 16, y + 15};
    fill_rect(hdc, swatch, color);
    draw_text_line(hdc, x + 22, y, name, RGB(232, 238, 242));
}

void draw_map_legend(HDC hdc, RECT client) {
    const Geography geographies[] = {
        GEO_OCEAN, GEO_COAST, GEO_PLAIN, GEO_HILL, GEO_MOUNTAIN, GEO_PLATEAU,
        GEO_BASIN, GEO_CANYON, GEO_VOLCANO, GEO_LAKE, GEO_BAY, GEO_DELTA,
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

    if (IsRectEmpty(&box)) return;

    fill_rect_alpha(hdc, box, RGB(31, 37, 43), 218);
    border_brush = CreateSolidBrush(RGB(76, 92, 104));
    FrameRect(hdc, &box, border_brush);
    DeleteObject(border_brush);
    fill_rect_alpha(hdc, toggle, RGB(47, 58, 68), 236);
    draw_center_text(hdc, toggle, map_legend_collapsed ? "^" : "v", RGB(236, 242, 246));
    draw_text_line(hdc, box.left + 10, box.top + 8, tr("Map Legend", "地图图例"), RGB(245, 245, 245));
    if (map_legend_collapsed) return;

    x = box.left + 10;
    y = box.top + 30;

    if (display_mode == DISPLAY_CLIMATE) {
        for (i = 0; i < climate_count; i++) {
            draw_legend_item(hdc, x, y + i * line_h, climate_color(climates[i]), climate_name(climates[i]));
        }
        return;
    }

    for (i = 0; i < geo_count; i++) {
        draw_legend_item(hdc, x, y + i * line_h, geography_color(geographies[i]), geography_name(geographies[i]));
    }
    if (display_mode == DISPLAY_ALL) {
        x = box.left + 190;
        for (i = 0; i < climate_count; i++) {
            draw_legend_item(hdc, x, y + i * line_h, climate_color(climates[i]), climate_name(climates[i]));
        }
    }
}
