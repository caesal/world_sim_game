#include "render_panel_internal.h"

#include "sim/regions.h"
#include "ui/ui_theme.h"
#include "ui/ui_worldgen_layout.h"

static void draw_section_rect(HDC hdc, RECT rect, const char *title) {
    RECT rule = rect;
    draw_text_rect(hdc, rect, title, ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    rule.top = rect.bottom - 2;
    rule.bottom = rule.top + 1;
    fill_rect(hdc, rule, ui_theme_color(UI_COLOR_PANEL_LINE));
}

static void draw_input_frame(HDC hdc, const WorldgenLayout *layout, RECT rect) {
    HBRUSH border;
    if (!worldgen_rect_visible(layout->viewport, rect)) return;
    fill_rect(hdc, rect, RGB(32, 39, 43));
    border = CreateSolidBrush(RGB(86, 104, 113));
    FrameRect(hdc, &rect, border);
    DeleteObject(border);
}

static void draw_text_rect_clipped(HDC hdc, RECT rect, const char *text, COLORREF color, unsigned int format) {
    int saved = SaveDC(hdc);
    IntersectClipRect(hdc, rect.left, rect.top, rect.right, rect.bottom);
    draw_text_rect(hdc, rect, text, color, format);
    RestoreDC(hdc, saved);
}

static void draw_random_button(HDC hdc, RECT rect) {
    RECT icon = {rect.left + 7, rect.top + 5, rect.left + 20, rect.top + 18};
    RECT text = {rect.left + 22, rect.top, rect.right - 6, rect.bottom};
    int hover = point_in_rect_local(rect, hover_x, hover_y);
    int pressed = hover && (GetKeyState(VK_LBUTTON) & 0x8000);
    COLORREF bg = pressed ? RGB(34, 42, 47) : hover ? RGB(55, 66, 72) : RGB(39, 48, 53);
    COLORREF border = hover ? RGB(115, 139, 148) : RGB(72, 88, 96);
    HBRUSH brush;
    fill_rect(hdc, rect, bg);
    brush = CreateSolidBrush(border);
    FrameRect(hdc, &rect, brush);
    DeleteObject(brush);
    draw_icon(hdc, ICON_ADAPTATION, icon, RGB(190, 206, 214));
    draw_text_rect(hdc, text, tr("Random", "随机"), RGB(232, 238, 244),
                   DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_END_ELLIPSIS);
}

static void draw_section_rect_with_button(HDC hdc, RECT rect, RECT button, const char *title) {
    RECT title_rect = rect;
    title_rect.right = button.left - 8;
    draw_section_rect(hdc, title_rect, title);
    rect.left = title_rect.right;
    draw_section_rect(hdc, rect, "");
    draw_random_button(hdc, button);
}

static void maybe_tooltip(RECT rect, const char *text, const char **active_tooltip) {
    if (point_in_rect_local(rect, hover_x, hover_y)) *active_tooltip = text;
}

static const char *worldgen_mode_label(void) {
    switch (display_mode) {
        case DISPLAY_GEOGRAPHY: return tr("Geography", "地理");
        case DISPLAY_CLIMATE: return tr("Climate", "气候");
        case DISPLAY_REGIONS: return tr("Regions", "区域");
        case DISPLAY_ROUTE_POTENTIAL: return tr("Routes", "航道潜力网");
        default: return tr("Political", "政治");
    }
}

static void draw_viewer_controls_summary(HDC hdc, const WorldgenLayout *layout) {
    char text[160];
    draw_section_rect(hdc, layout->viewer_section, tr("Viewer Controls (display only)", "查看器控制（仅显示）"));
    snprintf(text, sizeof(text), "%s: %s    %s: %d%%",
             tr("Layer", "图层"), worldgen_mode_label(), tr("Zoom", "缩放"), map_zoom_percent);
    draw_text_rect(hdc, layout->viewer_row[0], text, ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    snprintf(text, sizeof(text), "%s: %s    %s: %s",
             tr("Legend", "图例"), map_legend_collapsed ? tr("collapsed", "已收起") : tr("visible", "显示中"),
             tr("Sidebar", "侧栏"), side_panel_collapsed ? tr("collapsed", "已收起") : tr("expanded", "展开中"));
    draw_text_rect(hdc, layout->viewer_row[1], text, ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, layout->viewer_row[2],
                   tr("Display controls update immediately and never change world generation settings.",
                      "显示控制会立即生效，但不会改变世界生成设置。"),
                   ui_theme_color(UI_COLOR_TEXT_DIM), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}

static void draw_map_size_selector(HDC hdc, const WorldgenLayout *layout) {
    const char *size_en[3] = {"Small", "Medium", "Large"};
    const char *size_zh[3] = {"小", "中", "大"};
    for (int i = 0; i < MAP_SIZE_COUNT; i++) {
        RECT button = layout->map_size_buttons[i];
        if (!worldgen_rect_visible(layout->viewport, button)) continue;
        fill_rect(hdc, button, i == pending_map_size ? RGB(87, 93, 78) : ui_theme_color(UI_COLOR_PANEL_SOFT));
        draw_center_text(hdc, button, ui_language == UI_LANG_ZH ? size_zh[i] : size_en[i], ui_theme_color(UI_COLOR_TEXT));
    }
}

static const char *slider_help(int index) {
    switch (index) {
        case WORLD_SLIDER_OCEAN:
            return tr("Ocean amount for the next physical world generation.", "下次物理世界生成使用的海洋比例。");
        case WORLD_SLIDER_CONTINENT:
            return tr("Higher values create more fragmented continents next generation.", "数值越高，下次生成的大陆越破碎。");
        case WORLD_SLIDER_RELIEF:
            return tr("Elevation roughness for mountains, plateaus, basins, and coasts.", "控制山脉、高原、盆地和海岸的地势起伏。");
        case WORLD_SLIDER_MOISTURE:
            return tr("Global moisture used by climate, ecology, rivers, and resources.", "影响气候、生态、河流和资源的整体湿润度。");
        case WORLD_SLIDER_DROUGHT:
            return tr("Aridity pressure; higher values make dry climate more common.", "干旱压力；数值越高，干燥气候越常见。");
        case WORLD_SLIDER_VEGETATION:
            return tr("Plant density preference used by ecology and resource features.", "生态和资源特征使用的植被密度倾向。");
        case WORLD_SLIDER_BIAS_FOREST:
        case WORLD_SLIDER_BIAS_DESERT:
        case WORLD_SLIDER_BIAS_MOUNTAIN:
        case WORLD_SLIDER_BIAS_WETLAND:
            return tr("Soft terrain bias, not an exact percentage. Applies next generation.", "软性地形偏好，不是精确百分比；下次生成生效。");
        case UI_SLIDER_REGION_SIZE:
            return tr("Natural region size before civilizations. Changes regenerate regions.", "文明出现前的自然区域大小；改变会重生成自然区域。");
        default:
            return "";
    }
}

static void draw_worldgen_slider(HDC hdc, const WorldgenLayout *layout, int index,
                                 const char *name, int value, const char **active_tooltip) {
    const WorldgenSliderLayout *slider = &layout->sliders[index];
    RECT fill = slider->track;
    int knob_x = slider->track.left + (slider->track.right - slider->track.left) * value / 100;
    RECT knob = {knob_x - 7, slider->track.top - 8, knob_x + 7, slider->track.top + 18};
    char value_text[24];
    const char *help = slider_help(index);

    if (!worldgen_rect_visible(layout->viewport, slider->hit)) return;
    snprintf(value_text, sizeof(value_text), "%d / 0-100", value);
    draw_text_rect(hdc, slider->label, name, RGB(205, 214, 222),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, slider->value, value_text, RGB(232, 238, 244),
                   DT_SINGLELINE | DT_VCENTER | DT_RIGHT);
    fill_rect(hdc, slider->track, RGB(82, 94, 104));
    fill.right = knob_x;
    fill_rect(hdc, fill, RGB(82, 136, 171));
    fill_rect(hdc, knob, RGB(232, 238, 244));
    draw_text_rect_clipped(hdc, slider->help, help, ui_theme_color(UI_COLOR_TEXT_DIM),
                           DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    maybe_tooltip(slider->hit, help, active_tooltip);
}

static void draw_region_size_estimate(HDC hdc, const WorldgenLayout *layout) {
    int map_w_setting, map_h_setting, cap_reached = 0;
    int target_area = regions_target_size_from_slider(region_size_slider);
    int estimated_count;
    char text[192];

    map_size_dimensions(pending_map_size, &map_w_setting, &map_h_setting);
    estimated_count = regions_estimated_count_for_settings(map_w_setting, map_h_setting,
                                                           ocean_slider, region_size_slider,
                                                           &cap_reached);
    if (worldgen_rect_visible(layout->viewport, layout->region_estimate)) {
        snprintf(text, sizeof(text), "%s: %d    %s: %d / %d    %s: %d",
                 tr("Target area", "目标面积"), target_area,
                 tr("Estimated regions", "估算区域数"), estimated_count,
                 MAX_NATURAL_REGIONS, tr("Current", "当前"), world_generated ? region_count : 0);
        draw_text_rect_clipped(hdc, layout->region_estimate, text,
                               ui_theme_color(UI_COLOR_TEXT_DIM),
                               DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }
    if (cap_reached && worldgen_rect_visible(layout->viewport, layout->region_warning)) {
        draw_text_rect_clipped(hdc, layout->region_warning,
                               tr("Region cap reached; some regions may be larger than requested.",
                                  "区域数量已达到上限，部分区域可能比设定更大。"),
                               RGB(225, 178, 92), DT_WORDBREAK | DT_END_ELLIPSIS);
    }
}

static const char *metric_help(int index) {
    switch (index) {
        case WORLDGEN_METRIC_MILITARY: return tr("Army organization, weapons, war readiness.", "军队组织、武器和战争准备。");
        case WORLDGEN_METRIC_LOGISTICS: return tr("Supply, movement, migration, distant expansion.", "补给、移动、迁徙和远距离扩张。");
        case WORLDGEN_METRIC_GOVERNANCE: return tr("Administration, order, taxes, controlled expansion.", "行政、秩序、税收和受控扩张。");
        case WORLDGEN_METRIC_COHESION: return tr("Social stability, unity, assimilation, anti-collapse.", "社会稳定、统一、同化和抗崩溃。");
        case WORLDGEN_METRIC_PRODUCTION: return tr("Construction, tools, infrastructure, resource development.", "建设、工具、基础设施和资源开发。");
        case WORLDGEN_METRIC_COMMERCE: return tr("Money, markets, ports, trade exchange.", "金钱、市场、港口和贸易交换。");
        case WORLDGEN_METRIC_INNOVATION: return tr("Learning, tools, systems, long-term growth.", "学习、工具、制度和长期成长。");
        default: return "";
    }
}

static COLORREF color32_to_colorref(Color32 color) {
    return RGB((int)(color & 0xff), (int)((color >> 8) & 0xff), (int)((color >> 16) & 0xff));
}

void draw_civ_color_palette(HDC hdc, const WorldgenLayout *layout) {
    draw_text_rect(hdc, layout->civ_color_label, tr("Civilization Color", "文明颜色"),
                   ui_theme_color(UI_COLOR_TEXT_MUTED), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    for (int i = 0; i < CIV_COLOR_PALETTE_COUNT; i++) {
        RECT swatch = layout->civ_color_swatch[i];
        RECT inner = swatch;
        HBRUSH outline;
        if (!worldgen_rect_visible(layout->viewport, swatch)) continue;
        fill_rect(hdc, swatch, RGB(24, 29, 34));
        inner.left += 3; inner.top += 3; inner.right -= 3; inner.bottom -= 3;
        fill_rect(hdc, inner, color32_to_colorref(UI_CIV_COLOR_PALETTE[i]));
        outline = CreateSolidBrush(i == selected_civ_color_index ? RGB(238, 228, 181) : RGB(82, 94, 104));
        FrameRect(hdc, &swatch, outline);
        DeleteObject(outline);
    }
}

static void draw_civ_color_picker_preview(HDC hdc, const WorldgenLayout *layout) {
    RECT preview = layout->civ_color_preview;
    RECT swatch = {preview.left + 8, preview.top + 6, preview.left + 34, preview.bottom - 6};
    RECT label = {swatch.right + 10, preview.top, preview.right - 10, preview.bottom};
    HBRUSH outline;
    draw_text_rect(hdc, layout->civ_color_label, tr("Civilization Color", "文明颜色"),
                   ui_theme_color(UI_COLOR_TEXT_MUTED), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    if (!worldgen_rect_visible(layout->viewport, preview)) return;
    fill_rect(hdc, preview, RGB(35, 42, 46));
    fill_rect(hdc, swatch, color32_to_colorref(selected_civ_color));
    outline = CreateSolidBrush(RGB(122, 136, 144));
    FrameRect(hdc, &preview, outline);
    FrameRect(hdc, &swatch, outline);
    DeleteObject(outline);
    draw_text_rect(hdc, label, tr("Click to choose with HSV wheel", "点击打开 HSV 色轮"),
                   ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}

static void draw_metric_inputs(HDC hdc, const WorldgenLayout *layout, const char **active_tooltip) {
    static const char *labels_en[WORLDGEN_CORE_METRIC_COUNT] = {
        "Military Rating (0-10)", "Logistics (0-10)", "Governance (0-10)", "Cohesion (0-10)",
        "Production (0-10)", "Commerce (0-10)", "Innovation (0-10)"
    };
    static const char *labels_zh[WORLDGEN_CORE_METRIC_COUNT] = {
        "军备力 (0-10)", "后勤 (0-10)", "治理 (0-10)", "凝聚 (0-10)",
        "生产 (0-10)", "贸易 (0-10)", "技术 (0-10)"
    };
    for (int i = 0; i < WORLDGEN_CORE_METRIC_COUNT; i++) {
        draw_text_rect(hdc, layout->metric_label[i], ui_language == UI_LANG_ZH ? labels_zh[i] : labels_en[i],
                       ui_theme_color(UI_COLOR_TEXT_MUTED), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        draw_input_frame(hdc, layout, layout->metric_input_frame[i]);
        draw_text_rect_clipped(hdc, layout->metric_help[i], metric_help(i), ui_theme_color(UI_COLOR_TEXT_DIM),
                               DT_WORDBREAK | DT_END_ELLIPSIS);
        maybe_tooltip(layout->metric_label[i], metric_help(i), active_tooltip);
        maybe_tooltip(layout->metric_help[i], metric_help(i), active_tooltip);
    }
}

static void draw_civilization_placement(HDC hdc, const WorldgenLayout *layout, const char **active_tooltip) {
    draw_section_rect_with_button(hdc, layout->civ_section, layout->civ_random_button,
                                  tr("Step 3 Civilizations", "步骤 3 文明"));
    draw_text_rect(hdc, layout->civ_hint,
                   tr("Select a natural region, then add; capital uses its best site.",
                      "选择自然区域后添加；首都使用区域最佳首府点。"),
                   ui_theme_color(UI_COLOR_TEXT_MUTED), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_text_rect_clipped(hdc, layout->civ_range_help,
                           tr("Metric values are ratings from 0 to 10. 0 = very weak, 5 = average, 10 = very strong.",
                              "属性值范围为 0 到 10。0 = 很弱，5 = 平均，10 = 很强。"),
                           ui_theme_color(UI_COLOR_TEXT_MUTED), DT_WORDBREAK | DT_END_ELLIPSIS);
    draw_text_rect(hdc, layout->name_label, tr("Name", "名称"), ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, layout->symbol_label, tr("Symbol", "符号"), ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_input_frame(hdc, layout, layout->name_input_frame);
    draw_input_frame(hdc, layout, layout->symbol_input_frame);
    draw_civ_color_picker_preview(hdc, layout);
    draw_metric_inputs(hdc, layout, active_tooltip);
}

static void draw_setup_stages(HDC hdc, const WorldgenLayout *layout) {
    char text[192];
    draw_section_rect(hdc, layout->stage_section, tr("Setup Stages", "设置阶段"));
    snprintf(text, sizeof(text), "%s: %s - %s", tr("1 Physical World", "1 物理世界"),
             world_generated ? tr("generated", "已生成") : tr("not generated", "未生成"),
             tr("F5 applies the generation controls below.", "F5 会应用下方生成设置。"));
    draw_text_rect(hdc, layout->stage_row[0], text, ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    snprintf(text, sizeof(text), "%s: %s - %d %s", tr("2 Natural Regions", "2 自然区域"),
             world_generated ? tr("ready", "已就绪") : tr("generate a physical world first", "请先生成物理世界"),
             region_count, tr("regions", "个区域"));
    draw_text_rect(hdc, layout->stage_row[1], text, ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    snprintf(text, sizeof(text), "%s: %d / %d - %s", tr("3 Civilizations", "3 文明放置"),
             civ_count, initial_civ_count,
             world_generated ? tr("select a country or owned region", "选择国家或已拥有区域") :
                               tr("regions are needed before placement", "放置文明前需要自然区域"));
    draw_text_rect(hdc, layout->stage_row[2], text, ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    snprintf(text, sizeof(text), "%s: %s - %s", tr("4 Simulation", "4 模拟"),
             !world_generated ? tr("waiting for world", "等待世界") :
             civ_count <= 0 ? tr("place civilizations first", "请先放置文明") :
             auto_run ? tr("running", "运行中") : tr("paused", "已暂停"),
             tr("Space controls month progression.", "空格控制月份推进。"));
    draw_text_rect(hdc, layout->stage_row[3], text,
                   ui_theme_color(UI_COLOR_TEXT_MUTED), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}

void draw_worldgen_panel(HDC hdc, RECT client, int x, HFONT title_font, HFONT body_font) {
    WorldgenLayout layout;
    HRGN clip;
    int scroll;
    const char *tooltip_text = NULL;
    char initial_text[96];
    (void)x;

    scroll = worldgen_layout_clamp_scroll(client, side_panel_w, worldgen_scroll_offset);
    worldgen_layout_build(client, side_panel_w, scroll, &layout);
    clip = CreateRectRgn(layout.viewport.left, layout.viewport.top, layout.viewport.right, layout.viewport.bottom);
    SelectClipRgn(hdc, clip);
    SelectObject(hdc, title_font);
    draw_text_rect(hdc, layout.title, tr("World Setup", "世界设置"), ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    SelectObject(hdc, body_font);
    draw_setup_stages(hdc, &layout);
    draw_viewer_controls_summary(hdc, &layout);
    draw_section_rect(hdc, layout.generation_section, tr("World Generation Controls (next F5)", "世界生成控制（下次 F5）"));
    draw_section_rect(hdc, layout.map_size_section, tr("Step 1 Physical World", "步骤 1 物理世界"));
    draw_map_size_selector(hdc, &layout);
    draw_text_rect(hdc, layout.map_size_help,
                   tr("Affects the next generated world; it does not resize the current map.",
                      "影响下次生成的世界；不会缩放当前地图。"),
                   ui_theme_color(UI_COLOR_TEXT_DIM), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    snprintf(initial_text, sizeof(initial_text), "%s (0-%d)", tr("Initial civilizations", "初始文明数量"), MAX_CIVS);
    draw_text_rect(hdc, layout.initial_label, initial_text,
                   ui_theme_color(UI_COLOR_TEXT_MUTED), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_input_frame(hdc, &layout, layout.initial_input_frame);
    draw_text_rect(hdc, layout.initial_help,
                   tr("Applies when F5 generates and auto-places civilizations.", "F5 生成并自动放置文明时生效。"),
                   ui_theme_color(UI_COLOR_TEXT_DIM), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_section_rect_with_button(hdc, layout.physical_section, layout.physical_random_button,
                                  tr("Physical Parameters", "物理参数"));
    draw_worldgen_slider(hdc, &layout, WORLD_SLIDER_OCEAN, tr("Ocean Amount", "海洋比例"), ocean_slider, &tooltip_text);
    draw_worldgen_slider(hdc, &layout, WORLD_SLIDER_CONTINENT, tr("Continent Fragmentation", "大陆破碎度"), continent_slider, &tooltip_text);
    draw_worldgen_slider(hdc, &layout, WORLD_SLIDER_RELIEF, tr("Elevation Relief", "地势起伏"), relief_slider, &tooltip_text);
    draw_worldgen_slider(hdc, &layout, WORLD_SLIDER_MOISTURE, tr("Moisture", "湿润度"), moisture_slider, &tooltip_text);
    draw_worldgen_slider(hdc, &layout, WORLD_SLIDER_DROUGHT, tr("Aridity", "干旱度"), drought_slider, &tooltip_text);
    draw_worldgen_slider(hdc, &layout, WORLD_SLIDER_VEGETATION, tr("Plant Density", "植被密度"), vegetation_slider, &tooltip_text);
    draw_section_rect_with_button(hdc, layout.terrain_section, layout.terrain_random_button,
                                  tr("Advanced Terrain Bias", "高级地形偏好"));
    draw_worldgen_slider(hdc, &layout, WORLD_SLIDER_BIAS_FOREST, tr("Forest Bias", "森林偏好"), bias_forest_slider, &tooltip_text);
    draw_worldgen_slider(hdc, &layout, WORLD_SLIDER_BIAS_DESERT, tr("Desert Bias", "沙漠偏好"), bias_desert_slider, &tooltip_text);
    draw_worldgen_slider(hdc, &layout, WORLD_SLIDER_BIAS_MOUNTAIN, tr("Mountain Bias", "山脉偏好"), bias_mountain_slider, &tooltip_text);
    draw_worldgen_slider(hdc, &layout, WORLD_SLIDER_BIAS_WETLAND, tr("Wetland Bias", "湿地偏好"), bias_wetland_slider, &tooltip_text);
    draw_section_rect(hdc, layout.regions_section, tr("Step 2 Natural Regions", "步骤 2 自然区域"));
    draw_worldgen_slider(hdc, &layout, UI_SLIDER_REGION_SIZE, tr("Region Size", "区域大小"), region_size_slider, &tooltip_text);
    draw_region_size_estimate(hdc, &layout);
    draw_text_rect(hdc, layout.generate_row, tr("Step 4 Simulation: F5 rebuilds; Space starts or pauses time.",
                   "步骤 4 模拟：F5 重建世界；空格开始或暂停时间。"),
                   ui_theme_color(UI_COLOR_TEXT_MUTED), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_civilization_placement(hdc, &layout, &tooltip_text);
    SelectClipRgn(hdc, NULL);
    DeleteObject(clip);
    draw_tooltip(hdc, client, tooltip_text);
}
