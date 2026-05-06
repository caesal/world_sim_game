#include "render_panel_internal.h"

#include "sim/plague.h"
#include "sim/regions.h"
#include "ui/ui_theme.h"
#include "ui/ui_worldgen_layout.h"

void draw_mode_buttons(HDC hdc, RECT client) {
    const char *names_en[MAP_DISPLAY_MODE_COUNT] = {"All", "Climate", "Geography", "Regions", "Political"};
    const char *names_zh[MAP_DISPLAY_MODE_COUNT] = {"全部", "气候", "地理", "区域", "政治"};
    int i;
    for (i = 0; i < MAP_DISPLAY_MODE_COUNT; i++) {
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
    HFONT title_font = CreateFontW(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                   DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    HFONT old_font;

    fill_rect(hdc, bar, ui_theme_color(UI_COLOR_CHROME));
    fill_rect(hdc, year_box, RGB(38, 43, 42));
    snprintf(text, sizeof(text), "%s %d  %s %d", tr("Year", "年"), year, tr("Month", "月"), month);
    old_font = SelectObject(hdc, title_font);
    draw_center_text(hdc, year_box, text, RGB(222, 205, 132));
    SelectObject(hdc, old_font);
    DeleteObject(title_font);
    draw_text_line(hdc, 18, 20, WORLD_SIM_VERSION_LABEL, RGB(245, 250, 255));
    fill_rect(hdc, language_button, RGB(58, 68, 64));
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
    const char *names_en[PANEL_TAB_COUNT] = {"Select", "Country", "Diplomacy", "Pop", "Plague", "World", "Debug"};
    const char *names_zh[PANEL_TAB_COUNT] = {"选择", "国家", "外交", "人口", "瘟疫", "世界", "调试"};
    int i;

    for (i = 0; i < PANEL_TAB_COUNT; i++) {
        RECT tab = get_panel_tab_rect(client, i);
        fill_rect(hdc, tab, i == panel_tab ? RGB(77, 80, 68) : RGB(43, 49, 52));
        draw_center_text(hdc, tab, tr(names_en[i], names_zh[i]), RGB(238, 243, 247));
    }
}

static WorldgenSliderLayout plague_slider_layout(RECT client) {
    WorldgenSliderLayout slider;
    int x = client.right - side_panel_w + FORM_X_PAD;
    int width = side_panel_w - FORM_X_PAD * 2 - 8;
    int y = TOP_BAR_H + 154;

    slider.label.left = x;
    slider.label.top = y;
    slider.label.right = x + width - 58;
    slider.label.bottom = y + 18;
    slider.value.left = x + width - 50;
    slider.value.top = y;
    slider.value.right = x + width;
    slider.value.bottom = y + 18;
    slider.track.left = x;
    slider.track.top = y + 24;
    slider.track.right = x + width;
    slider.track.bottom = y + 34;
    slider.hit.left = x - 8;
    slider.hit.top = y - 4;
    slider.hit.right = x + width + 8;
    slider.hit.bottom = y + 42;
    return slider;
}

void draw_setup_slider(HDC hdc, RECT client, int index, const char *name, int value) {
    WorldgenLayout layout;
    WorldgenSliderLayout slider;
    RECT fill;
    int knob_x;
    RECT knob;
    char value_text[16];

    if (index == UI_SLIDER_PLAGUE_FOG_ALPHA) {
        slider = plague_slider_layout(client);
    } else {
        worldgen_layout_build(client, side_panel_w, worldgen_scroll_offset, &layout);
        slider = layout.sliders[index];
    }
    fill = slider.track;
    knob_x = slider.track.left + (slider.track.right - slider.track.left) * value / 100;
    knob.left = knob_x - 7;
    knob.top = slider.track.top - 8;
    knob.right = knob_x + 7;
    knob.bottom = slider.track.top + 18;
    snprintf(value_text, sizeof(value_text), "%d", value);
    draw_text_rect(hdc, slider.label, name, RGB(205, 214, 222), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, slider.value, value_text, RGB(232, 238, 244), DT_SINGLELINE | DT_VCENTER | DT_RIGHT);
    fill_rect(hdc, slider.track, RGB(82, 94, 104));
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
        y = draw_population_pyramid(hdc, client, x, y, inner_w, civ_id, body_font);
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
        if (plague_civ_active_count(civ_id) > 0) {
            draw_text_line(hdc, x, y, tr("Plague Status", "瘟疫状态"), RGB(205, 214, 222));
            y += 22;
            {
                RECT m = metric_grid_rect(x, y, quad_w, metric_h, 0);
                draw_metric_box(hdc, m, ICON_DISORDER, metric_label("ACT", "感染"), plague_civ_active_count(civ_id), RGB(35, 115, 72), tr("Active plague cities", "正在感染的城市"), &tooltip_text);
                m = metric_grid_rect(x, y, quad_w, metric_h, 1);
                draw_metric_box(hdc, m, ICON_DISORDER, metric_label("SEV", "烈度"), plague_civ_peak_severity(civ_id), RGB(22, 96, 62), tr("Peak plague severity", "最高瘟疫烈度"), &tooltip_text);
                m = metric_grid_rect(x, y, quad_w, metric_h, 2);
                draw_metric_box(hdc, m, ICON_POPULATION, metric_label("DEAD", "死亡"), plague_civ_deaths_total(civ_id), RGB(76, 92, 78), tr("Total plague deaths", "瘟疫累计死亡"), &tooltip_text);
                m = metric_grid_rect(x, y, quad_w, metric_h, 3);
                draw_metric_box(hdc, m, ICON_MIGRATION, metric_label("LEFT", "剩余"), plague_civ_months_left(civ_id), RGB(56, 128, 78), tr("Longest months remaining", "最长剩余月份"), &tooltip_text);
                y += metric_h + 18;
            }
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
        int natural_region_id = world[selected_y][selected_x].region_id;
        const NaturalRegion *natural_region = regions_get(natural_region_id);
        char natural_region_name[80];
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

        if (region_id < 0 && natural_region) {
            snprintf(natural_region_name, sizeof(natural_region_name), "%s %d",
                     tr("Natural Region", "自然区域"), natural_region->id + 1);
            province_name = natural_region_name;
            metric_food = natural_region->average_stats.food;
            metric_livestock = natural_region->average_stats.livestock;
            metric_wood = natural_region->average_stats.wood;
            metric_stone = natural_region->average_stats.stone;
            metric_ore = natural_region->average_stats.minerals;
            metric_water = natural_region->average_stats.water;
            metric_pop = natural_region->average_stats.pop_capacity;
            metric_money = natural_region->average_stats.money;
            metric_live = natural_region->habitability;
            metric_attack = natural_region->average_stats.attack;
            metric_defense = natural_region->average_stats.defense;
            province_tiles = natural_region->tile_count;
        }
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
        } else if (natural_region) {
            snprintf(text, sizeof(text), "%s %d  %s: %d,%d",
                     tr("Tiles", "地块"), province_tiles,
                     tr("Capital", "首府"), natural_region->capital_x, natural_region->capital_y);
            draw_text_line(hdc, x, y, text, RGB(180, 190, 198)); y += 20;
            snprintf(text, sizeof(text), "%s %d  %s %d  %s %d",
                     tr("Develop", "开发"), natural_region->development_score,
                     tr("Defense", "防御"), natural_region->natural_defense,
                     tr("Cradle", "摇篮"), natural_region->cradle_score);
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
    draw_text_line(hdc, x, client.bottom - 116, tr("Civilization Placement", "文明放置"), RGB(245, 245, 245));
    SelectObject(hdc, body_font);
    draw_text_line(hdc, x, client.bottom - 88,
                   tr("Use the World page controls to add or edit civilizations.",
                      "使用世界页控件添加或编辑文明。"),
                   RGB(160, 171, 180));
}
