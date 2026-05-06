#include "render_panel_internal.h"

#include "sim/regions.h"
#include "ui/ui_widgets.h"

static void draw_resource_chips(HDC hdc, UiCursor *cursor, TerrainStats stats) {
    int chip_w = (cursor->width - 16) / 3;
    int h = 28;
    int y = cursor->y;
    int x = cursor->x;

    ui_stat_chip(hdc, (RECT){x, y, x + chip_w, y + h}, metric_label("Food", "粮食"), stats.food, RGB(124, 154, 70));
    ui_stat_chip(hdc, (RECT){x + chip_w + 8, y, x + chip_w * 2 + 8, y + h}, metric_label("Water", "水源"), stats.water, RGB(65, 126, 174));
    ui_stat_chip(hdc, (RECT){x + (chip_w + 8) * 2, y, x + chip_w * 3 + 16, y + h}, metric_label("Live", "宜居"), stats.habitability, RGB(116, 145, 94));
    y += h + 8;
    ui_stat_chip(hdc, (RECT){x, y, x + chip_w, y + h}, metric_label("Wood", "木材"), stats.wood, RGB(67, 128, 76));
    ui_stat_chip(hdc, (RECT){x + chip_w + 8, y, x + chip_w * 2 + 8, y + h}, metric_label("Stone", "石料"), stats.stone, RGB(130, 120, 104));
    ui_stat_chip(hdc, (RECT){x + (chip_w + 8) * 2, y, x + chip_w * 3 + 16, y + h}, metric_label("Ore", "矿石"), stats.minerals, RGB(130, 120, 104));
    y += h + 8;
    ui_stat_chip(hdc, (RECT){x, y, x + chip_w, y + h}, metric_label("Herd", "畜牧"), stats.livestock, RGB(126, 104, 70));
    ui_stat_chip(hdc, (RECT){x + chip_w + 8, y, x + chip_w * 2 + 8, y + h}, metric_label("Money", "金钱"), stats.money, RGB(169, 134, 54));
    ui_stat_chip(hdc, (RECT){x + (chip_w + 8) * 2, y, x + chip_w * 3 + 16, y + h}, metric_label("Cap", "承载"), stats.pop_capacity, RGB(74, 112, 160));
    cursor->y = y + h + 6;
}

static TerrainStats selected_area_stats(int *out_tiles, int *out_population, const char **out_name) {
    static char natural_name[80];
    TerrainStats stats = tile_stats(selected_x, selected_y);
    int city_id = city_for_tile(selected_x, selected_y);
    int natural_id = world[selected_y][selected_x].region_id;
    const NaturalRegion *region = regions_get(natural_id);

    *out_tiles = 1;
    *out_population = 0;
    *out_name = tr("Tile", "地块");
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
        *out_tiles = summary.tiles;
        *out_population = summary.population;
        *out_name = cities[city_id].name;
    } else if (region) {
        stats = region->average_stats;
        *out_tiles = region->tile_count;
        snprintf(natural_name, sizeof(natural_name), "%s %d", tr("Natural Region", "自然区域"), region->id + 1);
        *out_name = natural_name;
    }
    return stats;
}

void draw_selection_panel(HDC hdc, RECT client, int x, HFONT title_font, HFONT body_font) {
    UiCursor cursor = ui_cursor(x, TOP_BAR_H + 62, side_panel_w - FORM_X_PAD * 2, client.bottom - 70);
    char text[256];
    int city_id;
    int admin_id;
    int natural_id;
    int tiles;
    int population;
    const char *area_name;
    const NaturalRegion *natural_region;
    TerrainStats stats;

    SelectObject(hdc, title_font);
    draw_text_line(hdc, x, cursor.y - 2, tr("Selection Inspector", "选择检查器"), ui_theme_color(UI_COLOR_TEXT));
    cursor.y += 34;
    SelectObject(hdc, body_font);
    if (!world_generated) {
        ui_row_text(hdc, &cursor, tr("World", "世界"), tr("Not generated", "尚未生成"));
        ui_row_text(hdc, &cursor, tr("Next", "下一步"), tr("Open World and generate a map.", "打开世界页生成地图。"));
        return;
    }
    if (selected_x < 0 || selected_y < 0) {
        ui_row_text(hdc, &cursor, tr("Selection", "选择"), tr("Click any tile on the map.", "点击地图地块。"));
        return;
    }
    city_id = city_at(selected_x, selected_y);
    admin_id = city_for_tile(selected_x, selected_y);
    natural_id = world[selected_y][selected_x].region_id;
    natural_region = regions_get(natural_id);
    stats = selected_area_stats(&tiles, &population, &area_name);
    ui_section(hdc, &cursor, area_name);
    snprintf(text, sizeof(text), "%d, %d", selected_x, selected_y);
    ui_row_text(hdc, &cursor, tr("Tile", "地块"), text);
    ui_row_text(hdc, &cursor, tr("Geography", "地理"), geography_name(world[selected_y][selected_x].geography));
    ui_row_text(hdc, &cursor, tr("Climate", "气候"), climate_name(world[selected_y][selected_x].climate));
    ui_row_text(hdc, &cursor, tr("Ecology", "生态"), ecology_name(world[selected_y][selected_x].ecology));
    ui_row_text(hdc, &cursor, tr("Resource", "资源点"), resource_name(world[selected_y][selected_x].resource));
    ui_row_text(hdc, &cursor, tr("River", "河流"), world[selected_y][selected_x].river ? tr("Yes", "有") : tr("No", "无"));
    ui_row_int(hdc, &cursor, tr("Elevation", "海拔"), world[selected_y][selected_x].elevation);
    ui_row_int(hdc, &cursor, tr("Moisture", "湿度"), world[selected_y][selected_x].moisture);
    ui_row_int(hdc, &cursor, tr("Temperature", "温度"), world[selected_y][selected_x].temperature);
    ui_section(hdc, &cursor, tr("Region / Province", "区域 / 行省"));
    ui_row_int(hdc, &cursor, tr("Natural region", "自然区域"), natural_id);
    if (admin_id >= 0) ui_row_text(hdc, &cursor, tr("Administrative city", "行政城市"), cities[admin_id].name);
    else if (natural_region) ui_row_text(hdc, &cursor, tr("Capital site", "首府候选"), text);
    if (natural_region) {
        snprintf(text, sizeof(text), "%d,%d", natural_region->capital_x, natural_region->capital_y);
        ui_row_text(hdc, &cursor, tr("Best capital", "最佳首府"), text);
        ui_row_int(hdc, &cursor, tr("Development", "开发"), natural_region->development_score);
        ui_row_int(hdc, &cursor, tr("Defense", "防御"), natural_region->natural_defense);
        ui_row_int(hdc, &cursor, tr("Cradle", "摇篮"), natural_region->cradle_score);
    }
    ui_row_int(hdc, &cursor, tr("Tiles", "地块数"), tiles);
    ui_row_int(hdc, &cursor, tr("Population", "人口"), population);
    draw_resource_chips(hdc, &cursor, stats);
    if (city_id >= 0) {
        ui_section(hdc, &cursor, tr("City", "城市"));
        ui_row_text(hdc, &cursor, tr("Name", "名称"), cities[city_id].name);
        ui_row_text(hdc, &cursor, tr("Owner", "所属"), cities[city_id].owner >= 0 ? civs[cities[city_id].owner].name : tr("None", "无"));
        ui_row_int(hdc, &cursor, tr("Population", "人口"), cities[city_id].population);
        ui_row_text(hdc, &cursor, tr("Port", "港口"), cities[city_id].port ? tr("Yes", "有") : tr("No", "无"));
        ui_row_text(hdc, &cursor, tr("Capital", "首都"), cities[city_id].capital ? tr("Yes", "是") : tr("No", "否"));
    }
}
