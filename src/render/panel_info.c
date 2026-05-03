#include "render_internal.h"

void draw_mode_buttons(HDC hdc, RECT client) {
    const char *names_en[4] = {"All", "Climate", "Geography", "Political"};
    const char *names_zh[4] = {"全部", "气候", "地理", "政治"};
    int i;
    for (i = 0; i < 4; i++) {
        RECT button = get_mode_button_rect(client, i);
        int mode = MAP_DISPLAY_MODES[i];
        fill_rect(hdc, button, mode == display_mode ? RGB(49, 63, 76) : RGB(36, 46, 56));
        draw_center_text(hdc, button, tr(names_en[i], names_zh[i]), RGB(238, 243, 247));
    }
}

void draw_top_bar(HDC hdc, RECT client) {
    RECT bar = {0, 0, client.right, TOP_BAR_H};
    RECT year_box = {client.right / 2 - 112, 9, client.right / 2 + 112, 50};
    RECT language_button = get_language_button_rect(client);
    char text[80];
    HFONT title_font = CreateFontA(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET,
                                   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                   DEFAULT_PITCH | FF_SWISS, "Microsoft YaHei UI");
    HFONT old_font;

    fill_rect(hdc, bar, RGB(52, 153, 216));
    fill_rect(hdc, year_box, RGB(42, 42, 42));
    snprintf(text, sizeof(text), "%s %d  %s %d", tr("Year", "年"), year, tr("Month", "月"), month);
    old_font = SelectObject(hdc, title_font);
    draw_center_text(hdc, year_box, text, RGB(199, 230, 107));
    SelectObject(hdc, old_font);
    DeleteObject(title_font);
    draw_text_line(hdc, 18, 20, WORLD_SIM_VERSION_LABEL, RGB(245, 250, 255));
    fill_rect(hdc, language_button, RGB(39, 81, 111));
    draw_center_text(hdc, language_button, ui_language == UI_LANG_ZH ? "中文" : "EN", RGB(245, 250, 255));
}

int selected_tile_owner(void) {
    if (selected_x < 0 || selected_y < 0) return -1;
    return world[selected_y][selected_x].owner;
}

const char *capital_name_for_civ(int civ_id) {
    int city_id;

    if (civ_id < 0 || civ_id >= civ_count) return tr("None", "无");
    city_id = civs[civ_id].capital_city;
    if (city_id < 0 || city_id >= city_count || !cities[city_id].alive) return tr("None", "无");
    return cities[city_id].name;
}

void draw_panel_tabs(HDC hdc, RECT client) {
    const char *names_en[PANEL_TAB_COUNT] = {"Info", "Civ", "Diplomacy", "Map"};
    const char *names_zh[PANEL_TAB_COUNT] = {"信息", "文明", "外交", "地图"};
    int i;

    for (i = 0; i < PANEL_TAB_COUNT; i++) {
        RECT tab = get_panel_tab_rect(client, i);
        fill_rect(hdc, tab, i == panel_tab ? RGB(56, 68, 80) : RGB(39, 47, 56));
        draw_center_text(hdc, tab, tr(names_en[i], names_zh[i]), RGB(238, 243, 247));
    }
}

static RECT setup_slider_rect(RECT client, int index) {
    RECT rect;
    int x = client.right - side_panel_w + FORM_X_PAD;
    int section_gap = index >= WORLD_SLIDER_BIAS_FOREST ? 34 : 0;
    rect.left = x + 158;
    rect.right = client.right - FORM_X_PAD - 8;
    rect.top = TOP_BAR_H + 292 + index * 34 + section_gap;
    rect.bottom = rect.top + 10;
    return rect;
}

void draw_setup_slider(HDC hdc, RECT client, int index, const char *name, int value) {
    RECT track = setup_slider_rect(client, index);
    RECT fill = track;
    int label_x = client.right - side_panel_w + FORM_X_PAD;
    int knob_x = track.left + (track.right - track.left) * value / 100;
    RECT knob = {knob_x - 7, track.top - 8, knob_x + 7, track.top + 18};
    char text[80];
    char value_text[16];

    snprintf(text, sizeof(text), "%s", name);
    snprintf(value_text, sizeof(value_text), "%d", value);
    draw_text_line(hdc, label_x, track.top - 8, text, RGB(205, 214, 222));
    draw_text_line(hdc, track.left - 28, track.top - 8, value_text, RGB(232, 238, 244));
    fill_rect(hdc, track, RGB(82, 94, 104));
    fill.right = knob_x;
    fill_rect(hdc, fill, RGB(82, 136, 171));
    fill_rect(hdc, knob, RGB(232, 238, 244));
}

void draw_info_tab(HDC hdc, RECT client, int x, int y, HFONT title_font, HFONT body_font) {
    char text[180];
    int owner = selected_tile_owner();
    int civ_id = selected_civ;
    int inner_w = side_panel_w - FORM_X_PAD * 2;
    int quad_w = (inner_w - 24) / 4;
    int metric_h = 30;
    const char *tooltip_text = NULL;

    if (owner >= 0 && owner < civ_count) civ_id = owner;
    if (civ_id >= 0 && civ_id < civ_count) {
        RECT swatch = {x, y + 3, x + 18, y + 21};
        CountrySummary country = summarize_country(civ_id);
        fill_rect(hdc, swatch, civs[civ_id].color);
        snprintf(text, sizeof(text), "%c  %.63s", civs[civ_id].symbol, civs[civ_id].name);
        SelectObject(hdc, title_font);
        draw_text_line(hdc, x + 28, y, text, RGB(245, 245, 245));
        y += 22;
        SelectObject(hdc, body_font);
        snprintf(text, sizeof(text), "%s: %s  %s %d", tr("Capital", "首都"), capital_name_for_civ(civ_id),
                 tr("Ports", "港口"), country.ports);
        draw_text_line(hdc, x + 28, y, text, RGB(180, 190, 198));
        y += 22;
        {
            RECT m = metric_grid_rect(x, y, quad_w, metric_h, 0);
            draw_metric_box(hdc, m, ICON_POPULATION, metric_label("POP", "人口"), country.population, RGB(74, 112, 160), tr("Country population", "国家总人口"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 1);
            draw_metric_box(hdc, m, ICON_TERRITORY, metric_label("LAND", "土地"), country.territory, RGB(88, 137, 83), tr("Total owned land tiles", "国家拥有土地格数"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 2);
            draw_metric_box(hdc, m, ICON_CITY_CAPITAL, metric_label("CITY", "城市"), country.cities, RGB(154, 128, 74), tr("Total cities", "国家城市数量"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 3);
            draw_metric_box(hdc, m, ICON_DISORDER, metric_label("DIS", "混乱"), civs[civ_id].disorder, RGB(170, 73, 73), tr("Total disorder", "总混乱度"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 4);
            draw_metric_box(hdc, m, ICON_COUNTRY_DEFENSE, metric_label("GOV", "治理"), civs[civ_id].governance, RGB(82, 114, 153), localized_text(CIVILIZATION_METRIC_RULES[CIV_METRIC_GOVERNANCE].ability, ui_language), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 5);
            draw_metric_box(hdc, m, ICON_CULTURE, metric_label("COH", "凝聚"), civs[civ_id].cohesion, RGB(147, 105, 167), localized_text(CIVILIZATION_METRIC_RULES[CIV_METRIC_COHESION].ability, ui_language), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 6);
            draw_metric_box(hdc, m, ICON_PRODUCTION, metric_label("PROD", "生产"), civs[civ_id].production, RGB(67, 128, 76), localized_text(CIVILIZATION_METRIC_RULES[CIV_METRIC_PRODUCTION].ability, ui_language), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 7);
            draw_metric_box(hdc, m, ICON_BATTLE, metric_label("MIL", "军备"), civs[civ_id].military, RGB(158, 74, 62), localized_text(CIVILIZATION_METRIC_RULES[CIV_METRIC_MILITARY].ability, ui_language), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 8);
            draw_metric_box(hdc, m, ICON_ECONOMY, metric_label("COM", "贸易"), civs[civ_id].commerce, RGB(169, 134, 54), localized_text(CIVILIZATION_METRIC_RULES[CIV_METRIC_COMMERCE].ability, ui_language), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 9);
            draw_metric_box(hdc, m, ICON_MIGRATION, metric_label("LOG", "后勤"), civs[civ_id].logistics, RGB(82, 133, 87), localized_text(CIVILIZATION_METRIC_RULES[CIV_METRIC_LOGISTICS].ability, ui_language), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 10);
            draw_metric_box(hdc, m, ICON_INNOVATION, metric_label("INN", "技术"), civs[civ_id].innovation, RGB(102, 128, 180), localized_text(CIVILIZATION_METRIC_RULES[CIV_METRIC_INNOVATION].ability, ui_language), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 11);
            draw_metric_box(hdc, m, ICON_HABITABILITY, metric_label("ADP", "适应"), civs[civ_id].adaptation, RGB(116, 145, 94), tr("Dynamic adaptation from environment, resources, culture, and disorder", "由环境、资源、文化和混乱度动态决定的适应力"), &tooltip_text);
            y += 3 * (metric_h + 6) + 4;
        }
        draw_text_line(hdc, x, y, tr("Country Resources", "国家资源"), RGB(205, 214, 222));
        y += 22;
        {
            RECT m = metric_grid_rect(x, y, quad_w, metric_h, 0);
            draw_metric_box(hdc, m, ICON_FOOD, metric_label("FOOD", "粮食"), country.food, RGB(124, 154, 70), tr("Average country food", "国家平均粮食"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 1);
            draw_metric_box(hdc, m, ICON_LIVESTOCK, metric_label("HERD", "畜牧"), country.livestock, RGB(126, 104, 70), tr("Average country livestock", "国家平均畜牧"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 2);
            draw_metric_box(hdc, m, ICON_WOOD, metric_label("WOOD", "木材"), country.wood, RGB(67, 128, 76), tr("Average country wood", "国家平均木材"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 3);
            draw_metric_box(hdc, m, ICON_STONE, metric_label("STON", "石料"), country.stone, RGB(130, 120, 104), tr("Average country stone", "国家平均石料"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 4);
            draw_metric_box(hdc, m, ICON_ORE, metric_label("ORE", "矿石"), country.minerals, RGB(130, 120, 104), tr("Average country ore", "国家平均矿石"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 5);
            draw_metric_box(hdc, m, ICON_WATER, metric_label("WATR", "水源"), country.water, RGB(65, 126, 174), tr("Average country water", "国家平均水"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 6);
            draw_metric_box(hdc, m, ICON_POPULATION, metric_label("CAP", "承载"), country.pop_capacity, RGB(74, 112, 160), tr("Average population capacity", "平均人口承载"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 7);
            draw_metric_box(hdc, m, ICON_ECONOMY, metric_label("CASH", "金钱"), country.money, RGB(169, 134, 54), tr("Average money potential", "平均金钱潜力"), &tooltip_text);
            y += 2 * (metric_h + 6) + 6;
        }
        draw_text_line(hdc, x, y, tr("Disorder Factors", "混乱因素"), RGB(205, 214, 222));
        y += 22;
        {
            RECT m = metric_grid_rect(x, y, quad_w, metric_h, 0);
            draw_metric_box(hdc, m, ICON_HABITABILITY, metric_label("RES", "资源"), civs[civ_id].disorder_resource, RGB(170, 73, 73), tr("Resource pressure disorder", "资源压力混乱"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 1);
            draw_metric_box(hdc, m, ICON_DISORDER, metric_label("PLG", "瘟疫"), civs[civ_id].disorder_plague, RGB(170, 73, 73), tr("Plague disorder", "瘟疫混乱"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 2);
            draw_metric_box(hdc, m, ICON_MIGRATION, metric_label("MIG", "迁徙"), civs[civ_id].disorder_migration, RGB(170, 73, 73), tr("Migration disorder", "迁徙混乱"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 3);
            draw_metric_box(hdc, m, ICON_COUNTRY_DEFENSE, metric_label("STAB", "稳定"), civs[civ_id].disorder_stability, RGB(82, 114, 153), tr("Stability pressure reduction", "稳定因素"), &tooltip_text);
            y += metric_h + 18;
        }
    } else {
        SelectObject(hdc, title_font);
        draw_text_line(hdc, x, y, tr("No Country Selected", "未选择国家"), RGB(245, 245, 245));
        SelectObject(hdc, body_font);
        y += 28;
        draw_text_line(hdc, x, y, tr("Select a country or one of its tiles.", "选择国家或它的地块。"), RGB(180, 190, 198));
        y += 42;
    }

    if (selected_x >= 0 && selected_y >= 0) {
        TerrainStats stats = tile_stats(selected_x, selected_y);
        int city_id = city_at(selected_x, selected_y);
        int region_id = city_for_tile(selected_x, selected_y);
        const char *province_name = region_id >= 0 ? cities[region_id].name : tr("Unorganized Land", "未设行省");
        int metric_food = stats.food;
        int metric_livestock = stats.livestock;
        int metric_wood = stats.wood;
        int metric_stone = stats.stone;
        int metric_ore = stats.minerals;
        int metric_water = stats.water;
        int metric_pop = stats.pop_capacity;
        int metric_money = stats.money;
        int metric_live = stats.habitability;
        int metric_attack = stats.attack;
        int metric_defense = stats.defense;
        int province_tiles = 0;
        int province_population = 0;

        if (region_id >= 0) {
            RegionSummary summary = summarize_city_region(region_id);
            metric_food = summary.food;
            metric_livestock = summary.livestock;
            metric_wood = summary.wood;
            metric_stone = summary.stone;
            metric_ore = summary.minerals;
            metric_water = summary.water;
            metric_pop = summary.pop_capacity;
            metric_money = summary.money;
            metric_live = summary.habitability;
            metric_attack = summary.attack;
            metric_defense = summary.defense;
            province_tiles = summary.tiles;
            province_population = summary.population;
        }

        SelectObject(hdc, title_font);
        draw_text_line(hdc, x, y, province_name, RGB(245, 245, 245));
        y += 26;
        SelectObject(hdc, body_font);
        snprintf(text, sizeof(text), "%s: %d, %d", tr("Tile", "地块"), selected_x, selected_y);
        draw_text_line(hdc, x, y, text, RGB(220, 225, 230)); y += 20;
        snprintf(text, sizeof(text), "%s: %s%s", tr("Geography", "地理"),
                 geography_name(world[selected_y][selected_x].geography),
                 world[selected_y][selected_x].river ? tr(" + River", " + 河流") : "");
        draw_icon_text_line(hdc, x, y, ICON_GEOGRAPHY, text, RGB(220, 225, 230)); y += 22;
        snprintf(text, sizeof(text), "%s: %s", tr("Climate", "气候"), climate_name(world[selected_y][selected_x].climate));
        draw_icon_text_line(hdc, x, y, ICON_CLIMATE, text, RGB(220, 225, 230)); y += 22;
        snprintf(text, sizeof(text), "%s: %s", tr("Ecology", "生态"), ecology_name(world[selected_y][selected_x].ecology));
        draw_text_line(hdc, x + 26, y, text, RGB(220, 225, 230)); y += 20;
        snprintf(text, sizeof(text), "%s: %s", tr("Resource", "资源点"), resource_name(world[selected_y][selected_x].resource));
        draw_text_line(hdc, x + 26, y, text, RGB(220, 225, 230)); y += 20;
        snprintf(text, sizeof(text), "%s %d  %s %d  %s %d",
                 tr("Elev", "海拔"),
                 world[selected_y][selected_x].elevation,
                 tr("Moist", "湿度"),
                 world[selected_y][selected_x].moisture,
                 tr("Temp", "温度"),
                 world[selected_y][selected_x].temperature);
        draw_text_line(hdc, x, y, text, RGB(180, 190, 198)); y += 20;
        if (region_id >= 0) {
            if (cities[region_id].owner >= 0 && cities[region_id].owner < civ_count) {
                snprintf(text, sizeof(text), "%s: %s", tr("Country", "国家"), civs[cities[region_id].owner].name);
                draw_text_line(hdc, x, y, text, RGB(180, 190, 198)); y += 20;
            }
            snprintf(text, sizeof(text), "%s %d  %s %d", tr("Tiles", "地块"), province_tiles, tr("Pop", "人口"), province_population);
            draw_text_line(hdc, x, y, text, RGB(180, 190, 198)); y += 22;
        } else {
            draw_text_line(hdc, x, y, tr("No province has been established here.", "这里还没有建立行省。"), RGB(180, 190, 198)); y += 22;
        }
        {
            RECT m = metric_grid_rect(x, y, quad_w, metric_h, 0);
            draw_metric_box(hdc, m, ICON_HABITABILITY, metric_label("LIVE", "宜居"), metric_live, RGB(116, 145, 94), tr("Habitability factors: food, livestock, wood, stone, ore, water, temperature", "宜居度：粮食、畜牧、木材、石料、矿石、水、温度综合"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 1);
            draw_metric_box(hdc, m, ICON_FOOD, metric_label("FOOD", "粮食"), metric_food, RGB(124, 154, 70), region_id >= 0 ? tr("Province average food", "行省平均粮食") : tr("Tile food", "地块粮食"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 2);
            draw_metric_box(hdc, m, ICON_LIVESTOCK, metric_label("HERD", "畜牧"), metric_livestock, RGB(126, 104, 70), region_id >= 0 ? tr("Province average livestock", "行省平均畜牧") : tr("Tile livestock", "地块畜牧"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 3);
            draw_metric_box(hdc, m, ICON_WOOD, metric_label("WOOD", "木材"), metric_wood, RGB(67, 128, 76), region_id >= 0 ? tr("Province average wood", "行省平均木材") : tr("Tile wood", "地块木材"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 4);
            draw_metric_box(hdc, m, ICON_STONE, metric_label("STON", "石料"), metric_stone, RGB(130, 120, 104), region_id >= 0 ? tr("Province average stone", "行省平均石料") : tr("Tile stone", "地块石料"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 5);
            draw_metric_box(hdc, m, ICON_ORE, metric_label("ORE", "矿石"), metric_ore, RGB(130, 120, 104), region_id >= 0 ? tr("Province average ore", "行省平均矿石") : tr("Tile ore", "地块矿石"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 6);
            draw_metric_box(hdc, m, ICON_WATER, metric_label("WATR", "水源"), metric_water, RGB(65, 126, 174), region_id >= 0 ? tr("Province average water", "行省平均水") : tr("Tile water", "地块水"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 7);
            draw_metric_box(hdc, m, ICON_ECONOMY, metric_label("CASH", "金钱"), metric_money, RGB(169, 134, 54), region_id >= 0 ? tr("Province average money potential", "行省平均金钱潜力") : tr("Tile money potential", "地块金钱潜力"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 8);
            draw_metric_box(hdc, m, ICON_POPULATION, metric_label("POP", "人口"), metric_pop, RGB(74, 112, 160), region_id >= 0 ? tr("Province average population potential", "行省平均人口潜力") : tr("Tile population potential", "地块人口潜力"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 9);
            draw_metric_box(hdc, m, ICON_ATTACK, metric_label("ATK", "攻击"), metric_attack, RGB(158, 74, 62), region_id >= 0 ? tr("Province average attack modifier", "行省平均攻击修正") : tr("Tile attack modifier", "地块攻击修正"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 10);
            draw_metric_box(hdc, m, ICON_TILE_DEFENSE, metric_label("DEF", "防御"), metric_defense, RGB(80, 101, 148), region_id >= 0 ? tr("Province average defense modifier", "行省平均防御修正") : tr("Tile defense modifier", "地块防御修正"), &tooltip_text);
            y += 3 * (metric_h + 6) + 2;
        }
        if (ui_language == UI_LANG_ZH) {
            snprintf(text, sizeof(text), "行省平均: 宜居 %d 粮食 %d 畜牧 %d 木材 %d 石料 %d 矿石 %d 水源 %d 金钱 %d",
                     metric_live, metric_food, metric_livestock, metric_wood,
                     metric_stone, metric_ore, metric_water, metric_money);
        } else {
            snprintf(text, sizeof(text), "Province avg: LIVE %d FOOD %d HERD %d WOOD %d STON %d ORE %d WATER %d CASH %d",
                     metric_live, metric_food, metric_livestock, metric_wood,
                     metric_stone, metric_ore, metric_water, metric_money);
        }
        draw_text_line(hdc, x, y, text, RGB(145, 158, 168));
        y += 18;
        if (city_id >= 0) {
            snprintf(text, sizeof(text), "%s: %s%s  %s %d", tr("City", "城市"), cities[city_id].name,
                     cities[city_id].port ? tr(" Port", " 港口") : "",
                     tr("Pop", "人口"), cities[city_id].population);
            draw_text_line(hdc, x, y, text, RGB(220, 225, 230)); y += 20;
        }
    } else {
        SelectObject(hdc, title_font);
        draw_text_line(hdc, x, y, tr("No Province Selected", "未选择行省"), RGB(245, 245, 245));
        SelectObject(hdc, body_font);
        y += 28;
        draw_text_line(hdc, x, y, tr("Click a tile on the map.", "点击地图地块查看信息。"), RGB(180, 190, 198));
    }
    draw_tooltip(hdc, client, tooltip_text);
}

void draw_civ_tab(HDC hdc, RECT client, int x, HFONT title_font, HFONT body_font) {
    int y = TOP_BAR_H + 58;
    int i;
    char text[512];

    SelectObject(hdc, title_font);
    draw_text_line(hdc, x, y, tr("Civilizations", "文明"), RGB(245, 245, 245));
    y += 30;
    SelectObject(hdc, body_font);
    for (i = 0; i < civ_count && y < client.bottom - 500; i++) {
        RECT swatch = {x, y + 3, x + 15, y + 18};
        fill_rect(hdc, swatch, civs[i].color);
        snprintf(text, sizeof(text), "%c  %.63s", civs[i].symbol, civs[i].name);
        draw_text_line(hdc, x + 24, y, text, i == selected_civ ? RGB(255, 238, 150) : RGB(238, 238, 238));
        y += 19;
        if (ui_language == UI_LANG_ZH) {
            snprintf(text, sizeof(text), "人口 %d 土地 %d 治理%d 凝聚%d 生产%d 军备%d",
                     civs[i].population, civs[i].territory, civs[i].governance,
                     civs[i].cohesion, civs[i].production, civs[i].military);
        } else {
            snprintf(text, sizeof(text), "Pop %d Land %d GOV%d COH%d PROD%d MIL%d",
                     civs[i].population, civs[i].territory, civs[i].governance,
                     civs[i].cohesion, civs[i].production, civs[i].military);
        }
        draw_text_line(hdc, x + 24, y, text, RGB(175, 186, 196));
        y += 25;
    }

    SelectObject(hdc, title_font);
    draw_text_line(hdc, x, client.bottom - 262, tr("Civilization Form", "文明表单"), RGB(245, 245, 245));
    SelectObject(hdc, body_font);
    draw_text_line(hdc, x, client.bottom - 235, tr("Name", "名字"), RGB(200, 210, 218));
    draw_text_line(hdc, x + 176, client.bottom - 235, tr("Symbol", "符号"), RGB(200, 210, 218));
    draw_text_line(hdc, x, client.bottom - 175, tr("Military", "军备"), RGB(200, 210, 218));
    draw_text_line(hdc, x + 82, client.bottom - 175, tr("Logistics", "后勤"), RGB(200, 210, 218));
    draw_text_line(hdc, x + 164, client.bottom - 175, tr("Governance", "治理"), RGB(200, 210, 218));
    draw_text_line(hdc, x + 246, client.bottom - 175, tr("Cohesion", "凝聚"), RGB(200, 210, 218));
    draw_text_line(hdc, x, client.bottom - 64, tr("F1 add. F2 apply. Select empty land before adding.", "F1 添加，F2 应用。添加前先选空地。"), RGB(160, 171, 180));
}
