#include "render_internal.h"

static void draw_map_tab(HDC hdc, RECT client, int x, HFONT title_font, HFONT body_font) {
    int y = TOP_BAR_H + 58;
    const char *size_en[3] = {"Small", "Medium", "Large"};
    const char *size_zh[3] = {"小", "中", "大"};
    int i;

    SelectObject(hdc, title_font);
    draw_text_line(hdc, x, y, tr("Map View", "地图视角"), RGB(245, 245, 245));
    SelectObject(hdc, body_font);
    draw_mode_buttons(hdc, client);

    y = TOP_BAR_H + 216;
    SelectObject(hdc, title_font);
    draw_text_line(hdc, x, y, tr("World Generation", "世界生成"), RGB(245, 245, 245));
    SelectObject(hdc, body_font);
    draw_text_line(hdc, x, y + 30, tr("Map size", "地图大小"), RGB(200, 210, 218));
    for (i = 0; i < 3; i++) {
        RECT button = get_map_size_button_rect(client, i);
        fill_rect(hdc, button, i == pending_map_size ? RGB(76, 95, 112) : RGB(35, 45, 54));
        draw_center_text(hdc, button, ui_language == UI_LANG_ZH ? size_zh[i] : size_en[i], RGB(236, 242, 246));
    }
    draw_text_line(hdc, x, y + 92, tr("Initial civilizations", "初始文明数量"), RGB(200, 210, 218));
    draw_setup_slider(hdc, client, WORLD_SLIDER_OCEAN, tr("Ocean", "海陆比例"), ocean_slider);
    draw_setup_slider(hdc, client, WORLD_SLIDER_CONTINENT, tr("Fragment", "大陆破碎"), continent_slider);
    draw_setup_slider(hdc, client, WORLD_SLIDER_RELIEF, tr("Relief", "地势起伏"), relief_slider);
    draw_setup_slider(hdc, client, WORLD_SLIDER_MOISTURE, tr("Moisture", "湿润度"), moisture_slider);
    draw_setup_slider(hdc, client, WORLD_SLIDER_DROUGHT, tr("Drought", "干旱度"), drought_slider);
    draw_setup_slider(hdc, client, WORLD_SLIDER_VEGETATION, tr("Vegetation", "植被密度"), vegetation_slider);
    draw_text_line(hdc, x, TOP_BAR_H + 558, tr("Advanced Terrain Bias", "高级地形偏好"), RGB(245, 245, 245));
    draw_setup_slider(hdc, client, WORLD_SLIDER_BIAS_FOREST, tr("Forest", "森林"), bias_forest_slider);
    draw_setup_slider(hdc, client, WORLD_SLIDER_BIAS_DESERT, tr("Desert", "沙漠"), bias_desert_slider);
    draw_setup_slider(hdc, client, WORLD_SLIDER_BIAS_MOUNTAIN, tr("Mountain", "山脉"), bias_mountain_slider);
    draw_setup_slider(hdc, client, WORLD_SLIDER_BIAS_WETLAND, tr("Wetland", "湿地"), bias_wetland_slider);
    draw_text_line(hdc, x, client.bottom - 64, tr("F5 rebuilds with these settings.", "F5 使用这些设置重建世界。"), RGB(160, 171, 180));
}

void draw_side_panel(HDC hdc, RECT client) {
    int x = client.right - side_panel_w + 18;
    RECT panel = {client.right - side_panel_w, TOP_BAR_H, client.right, client.bottom};
    RECT divider = {client.right - side_panel_w - 3, TOP_BAR_H, client.right - side_panel_w + 3, client.bottom};
    HFONT title_font = CreateFontA(21, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET,
                                   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                   DEFAULT_PITCH | FF_SWISS, "Microsoft YaHei UI");
    HFONT body_font = CreateFontA(17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
                                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                  DEFAULT_PITCH | FF_SWISS, "Microsoft YaHei UI");
    HFONT old_font;

    fill_rect(hdc, panel, RGB(31, 37, 43));
    fill_rect(hdc, divider, RGB(71, 82, 92));
    draw_panel_tabs(hdc, client);

    old_font = SelectObject(hdc, title_font);
    if (panel_tab == PANEL_INFO) draw_info_tab(hdc, client, x, TOP_BAR_H + 58, title_font, body_font);
    else if (panel_tab == PANEL_CIV) draw_civ_tab(hdc, client, x, title_font, body_font);
    else if (panel_tab == PANEL_DIPLOMACY) draw_diplomacy_tab(hdc, client, x, title_font, body_font);
    else draw_map_tab(hdc, client, x, title_font, body_font);

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
    int i;

    fill_rect(hdc, bar, RGB(30, 34, 38));
    fill_rect(hdc, play, auto_run ? RGB(68, 88, 104) : RGB(42, 58, 72));
    draw_center_text(hdc, play, auto_run ? "||" : ">", RGB(245, 248, 250));

    for (i = 0; i < 3; i++) {
        RECT button = get_speed_button_rect(client, i);
        fill_rect(hdc, button, i == speed_index ? RGB(84, 110, 132) : RGB(42, 58, 72));
        draw_center_text(hdc, button, i == 0 ? ">" : (i == 1 ? ">>" : ">>>"), RGB(235, 240, 244));
    }

    snprintf(text, sizeof(text), "%s    %s: %s (%s/%s)    %s    F1 %s    F2 %s    F5 %s",
              tr("Space toggles run/pause", "空格开始/暂停"),
              tr("Speed", "速度"), speed_name, speed_seconds_text(speed_index), tr("month", "月"),
              tr("Wheel zoom", "滚轮缩放"), tr("add", "添加"), tr("apply", "应用"), tr("new world", "新世界"));
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
