#include "render/panel_country_resources.h"

#include "render_panel_internal.h"
#include "sim/disorder.h"
#include "sim/expansion.h"
#include "sim/population.h"
#include "sim/regions.h"
#include "sim/technology.h"
#include "ui/ui_widgets.h"

static void metric3(HDC hdc, UiCursor *cursor, int a, int b, int c,
                    IconId ia, IconId ib, IconId ic,
                    const char *la, const char *lb, const char *lc) {
    int w = (cursor->width - 16) / 3;
    RECT r = {cursor->x, cursor->y, cursor->x + w, cursor->y + 28};
    ui_metric_chip(hdc, r, ia, la, a, RGB(118, 143, 95));
    r.left += w + 8; r.right += w + 8;
    ui_metric_chip(hdc, r, ib, lb, b, RGB(83, 123, 166));
    r.left += w + 8; r.right += w + 8;
    ui_metric_chip(hdc, r, ic, lc, c, RGB(188, 154, 88));
    cursor->y += 36;
}

static const char *stat_name(int index) {
    static const char *en[] = {"Food", "Water", "Livestock", "Wood", "Stone", "Ore", "Money", "Habitability", "Capacity"};
    static const char *zh[] = {"粮食", "水源", "畜牧", "木材", "石料", "矿石", "金钱", "宜居", "承载"};
    return tr(en[index], zh[index]);
}

static int stat_value(CountrySummary s, int index) {
    switch (index) {
        case 0: return s.food;
        case 1: return s.water;
        case 2: return s.livestock;
        case 3: return s.wood;
        case 4: return s.stone;
        case 5: return s.minerals;
        case 6: return s.money;
        case 7: return s.habitability;
        default: return s.pop_capacity;
    }
}

static void country_shortage_surplus(CountrySummary s, const char **shortage, const char **surplus) {
    int min_i = 0;
    int max_i = 0;
    int i;
    for (i = 1; i < 9; i++) {
        if (stat_value(s, i) < stat_value(s, min_i)) min_i = i;
        if (stat_value(s, i) > stat_value(s, max_i)) max_i = i;
    }
    *shortage = stat_name(min_i);
    *surplus = stat_name(max_i);
}

static TerrainStats selected_stats(int *tiles, int *population, const char **name) {
    static char region_name[80];
    int city_id = city_for_tile(selected_x, selected_y);
    int natural_id = world[selected_y][selected_x].region_id;
    const NaturalRegion *region = regions_get(natural_id);
    TerrainStats stats = tile_stats(selected_x, selected_y);

    *tiles = 1;
    *population = 0;
    *name = tr("Tile", "地块");
    if (city_id >= 0) {
        RegionSummary summary = summarize_city_region(city_id);
        stats.food = summary.food;
        stats.livestock = summary.livestock;
        stats.wood = summary.wood;
        stats.stone = summary.stone;
        stats.minerals = summary.minerals;
        stats.water = summary.water;
        stats.pop_capacity = summary.pop_capacity;
        stats.money = summary.money;
        stats.habitability = summary.habitability;
        stats.attack = summary.attack;
        stats.defense = summary.defense;
        *tiles = summary.tiles;
        *population = summary.population;
        *name = cities[city_id].name;
    } else if (region) {
        stats = region->average_stats;
        *tiles = region->tile_count;
        snprintf(region_name, sizeof(region_name), "%s %d", tr("Natural Region", "自然区域"), region->id + 1);
        *name = region_name;
    }
    return stats;
}

static void draw_country_resource_summary(HDC hdc, UiCursor *cursor, int civ_id) {
    CountrySummary country = summarize_country(civ_id);
    const char *shortage;
    const char *surplus;
    char text[160];
    int tech = technology_resource_percent(civ_id);
    int disorder = disorder_productivity_percent(civs[civ_id].disorder);

    country_shortage_surplus(country, &shortage, &surplus);
    ui_section(hdc, cursor, tr("Country Resources", "国家资源"));
    metric3(hdc, cursor, country.resource_score, expansion_resource_score_for_civ(civ_id),
            population_pressure_for_civ(civ_id), ICON_HABITABILITY, ICON_DISORDER, ICON_POPULATION,
            metric_label("Score", "评分"), metric_label("Pressure", "压力"), metric_label("Pop Pressure", "人口压力"));
    snprintf(text, sizeof(text), "%s: %s   %s: %s",
             tr("Biggest shortage", "最大短板"), shortage, tr("Biggest surplus", "最大优势"), surplus);
    ui_row_text(hdc, cursor, tr("Balance", "资源平衡"), text);
    snprintf(text, sizeof(text), "%s %d%%   %s %d%%   %s %d%%",
             tr("Tech", "科技"), tech, tr("Disorder", "混乱"), disorder,
             tr("Final", "合计"), tech * disorder / 100);
    ui_row_text(hdc, cursor, tr("Output Modifier", "产出修正"), text);
    metric3(hdc, cursor, country.food, country.water, country.pop_capacity,
            ICON_FOOD, ICON_WATER, ICON_POPULATION,
            metric_label("Food", "粮食"), metric_label("Water", "水源"), metric_label("Capacity", "承载"));
    metric3(hdc, cursor, country.livestock, country.wood, country.stone,
            ICON_LIVESTOCK, ICON_WOOD, ICON_STONE,
            metric_label("Herd", "畜牧"), metric_label("Wood", "木材"), metric_label("Stone", "石料"));
    metric3(hdc, cursor, country.minerals, country.money, country.habitability,
            ICON_ORE, ICON_MONEY, ICON_HABITABILITY,
            metric_label("Ore", "矿石"), metric_label("Money", "金钱"), metric_label("Live", "宜居"));
    ui_row_text(hdc, cursor, tr("Effects", "影响"),
                tr("Resources affect expansion pressure, stability, population capacity, and war economy.",
                   "资源影响扩张压力、稳定度、人口承载与战争经济。"));
}

static void draw_selection_inspector(HDC hdc, UiCursor *cursor) {
    TerrainStats stats;
    const NaturalRegion *region;
    int city_id;
    int province_city;
    int region_id;
    int tiles;
    int population;
    const char *name;
    char text[160];

    ui_section(hdc, cursor, tr("Selection Inspector", "选择检查器"));
    if (!world_generated) {
        ui_row_text(hdc, cursor, tr("World", "世界"), tr("Generate a physical world first.", "请先生成物理世界。"));
        return;
    }
    if (selected_x < 0 || selected_y < 0) {
        ui_row_text(hdc, cursor, tr("Selection", "选择"),
                    tr("Select a city, province, or tile on the map to inspect resources.",
                       "在地图上选择城市、行省或地块来检查资源。"));
        return;
    }
    stats = selected_stats(&tiles, &population, &name);
    city_id = city_at(selected_x, selected_y);
    province_city = city_for_tile(selected_x, selected_y);
    region_id = world[selected_y][selected_x].region_id;
    region = regions_get(region_id);
    snprintf(text, sizeof(text), "%d, %d", selected_x, selected_y);
    ui_row_text(hdc, cursor, tr("Tile", "地块"), text);
    ui_row_text(hdc, cursor, tr("Area", "区域"), name);
    ui_row_text(hdc, cursor, tr("Geography", "地理"), geography_name(world[selected_y][selected_x].geography));
    ui_row_text(hdc, cursor, tr("Climate", "气候"), climate_name(world[selected_y][selected_x].climate));
    ui_row_text(hdc, cursor, tr("Ecology", "生态"), ecology_name(world[selected_y][selected_x].ecology));
    ui_row_text(hdc, cursor, tr("Resource", "资源点"), resource_name(world[selected_y][selected_x].resource));
    ui_row_text(hdc, cursor, tr("Owner", "所属"),
                world[selected_y][selected_x].owner >= 0 ? civilization_display_name(world[selected_y][selected_x].owner) : tr("Unowned", "无主"));
    snprintf(text, sizeof(text), "%d   %s %d", region_id, tr("Province", "行省"), province_city);
    ui_row_text(hdc, cursor, tr("Natural Region", "自然区域"), text);
    snprintf(text, sizeof(text), "%s %d  %s %d  %s %d",
             tr("Elev", "海拔"), world[selected_y][selected_x].elevation,
             tr("Moist", "湿度"), world[selected_y][selected_x].moisture,
             tr("Temp", "温度"), world[selected_y][selected_x].temperature);
    ui_row_text(hdc, cursor, tr("Terrain Fields", "地形数值"), text);
    snprintf(text, sizeof(text), "%s%s%s%s",
             world[selected_y][selected_x].river ? tr("River ", "河流 ") : "",
             region && region->has_port_site ? tr("Port site ", "港口点 ") : "",
             region && region->has_port_site ? "" : tr("", ""),
             world[selected_y][selected_x].geography == GEO_MOUNTAIN ? tr("Mountain", "山地") : "");
    ui_row_text(hdc, cursor, tr("Flags", "标记"), text[0] ? text : tr("None", "无"));
    if (region) {
        snprintf(text, sizeof(text), "%d,%d   %s %d,%d", region->capital_x, region->capital_y,
                 tr("Port", "港口"), region->port_x, region->port_y);
        ui_row_text(hdc, cursor, tr("Best Sites", "候选点"), text);
        snprintf(text, sizeof(text), "%s %d   %s %d   %s %d",
                 tr("Dev", "开发"), region->development_score,
                 tr("Defense", "防御"), region->natural_defense,
                 tr("Cradle", "摇篮"), region->cradle_score);
        ui_row_text(hdc, cursor, tr("Strategic Value", "战略价值"), text);
    }
    metric3(hdc, cursor, stats.food, stats.water, stats.pop_capacity,
            ICON_FOOD, ICON_WATER, ICON_POPULATION,
            metric_label("Food", "粮食"), metric_label("Water", "水源"), metric_label("Capacity", "承载"));
    metric3(hdc, cursor, stats.livestock, stats.wood, stats.stone,
            ICON_LIVESTOCK, ICON_WOOD, ICON_STONE,
            metric_label("Herd", "畜牧"), metric_label("Wood", "木材"), metric_label("Stone", "石料"));
    metric3(hdc, cursor, stats.minerals, stats.money, stats.habitability,
            ICON_ORE, ICON_MONEY, ICON_HABITABILITY,
            metric_label("Ore", "矿石"), metric_label("Money", "金钱"), metric_label("Live", "宜居"));
    snprintf(text, sizeof(text), "%s %d   %s %d", tr("Tiles", "地块"), tiles, tr("Population", "人口"), population);
    ui_row_text(hdc, cursor, tr("Area Summary", "区域摘要"), text);
    if (city_id >= 0) {
        snprintf(text, sizeof(text), "%s%s  %s %d",
                 cities[city_id].capital ? tr("Capital ", "首都 ") : "",
                 cities[city_id].port ? tr("Port", "港口") : tr("City", "城市"),
                 tr("Pop", "人口"), cities[city_id].population);
        ui_row_text(hdc, cursor, cities[city_id].name, text);
    }
}

int country_resources_tab_height(int civ_id) {
    (void)civ_id;
    return selected_x >= 0 && selected_y >= 0 ? 760 : 520;
}

void draw_country_resources_tab(HDC hdc, UiCursor *cursor, int civ_id) {
    draw_country_resource_summary(hdc, cursor, civ_id);
    draw_selection_inspector(hdc, cursor);
}
