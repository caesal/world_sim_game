#include "render_panel_internal.h"

#include "core/worldgen_progress.h"
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

#if 0
static void draw_worldgen_progress_status(HDC hdc, const WorldgenLayout *layout) {
    WorldGenProgress progress;
    RECT card, title, stage, bar, fill;
    HBRUSH border;
    char percent[32];
    worldgen_progress_get(&progress);
    if (!progress.active) return;
    card = (RECT){layout->viewport.left, layout->viewport.top + 6,
                  layout->viewport.right, layout->viewport.top + 83};
    if (!worldgen_rect_visible(layout->viewport, card)) return;
    fill_rect_alpha(hdc, card, RGB(22, 28, 33), 238);
    border = CreateSolidBrush(RGB(78, 96, 106));
    FrameRect(hdc, &card, border);
    DeleteObject(border);
    title = (RECT){card.left + 10, card.top + 7, card.right - 10, card.top + 27};
    stage = (RECT){card.left + 10, title.bottom + 3, card.right - 10, title.bottom + 23};
    bar = (RECT){card.left + 10, card.bottom - 20, card.right - 10, card.bottom - 8};
    fill = bar;
    fill.right = fill.left + (bar.right - bar.left) * progress.percent / 100;
    snprintf(percent, sizeof(percent), "%s: %d%%", tr("Progress", "进度"), progress.percent);
    draw_text_rect(hdc, title, tr("Generating world", "正在生成世界"),
                   ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, stage, progress.stage_total > 0 ?
                   (ui_language == UI_LANG_ZH ? progress.message_zh : progress.message_en) :
                   (ui_language == UI_LANG_ZH ? worldgen_stage_name_zh(progress.stage) : worldgen_stage_name_en(progress.stage)),
                   ui_theme_color(UI_COLOR_TEXT_MUTED), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    fill_rect(hdc, bar, RGB(42, 50, 58));
    if (fill.right > fill.left) fill_rect(hdc, fill, RGB(103, 164, 196));
    draw_center_text(hdc, bar, percent, RGB(242, 246, 248));
}
#endif

static void draw_worldgen_progress_status_v2(HDC hdc, const WorldgenLayout *layout) {
    WorldGenProgress progress;
    RECT card, title, overall, overall_bar, stage, detail, stage_bar, fill;
    HBRUSH border;
    char text[160];
    const char *stage_name;
    worldgen_progress_get(&progress);
    if (!progress.active) return;
    card = (RECT){layout->viewport.left, layout->viewport.top + 6,
                  layout->viewport.right, layout->viewport.top + 132};
    if (!worldgen_rect_visible(layout->viewport, card)) return;
    fill_rect_alpha(hdc, card, RGB(22, 28, 33), 238);
    border = CreateSolidBrush(RGB(78, 96, 106));
    FrameRect(hdc, &card, border);
    DeleteObject(border);
    title = (RECT){card.left + 10, card.top + 7, card.right - 10, card.top + 27};
    overall = (RECT){card.left + 10, title.bottom + 2, card.right - 10, title.bottom + 22};
    overall_bar = (RECT){card.left + 10, overall.bottom + 3, card.right - 10, overall.bottom + 14};
    stage = (RECT){card.left + 10, overall_bar.bottom + 8, card.right - 10, overall_bar.bottom + 28};
    detail = (RECT){card.left + 10, stage.bottom + 1, card.right - 10, stage.bottom + 19};
    stage_bar = (RECT){card.left + 10, card.bottom - 18, card.right - 10, card.bottom - 7};
    stage_name = ui_language == UI_LANG_ZH ? worldgen_stage_name_zh(progress.stage) :
                                             worldgen_stage_name_en(progress.stage);
    draw_text_rect(hdc, title, tr("Generating world", "正在生成世界"),
                   ui_theme_color(UI_COLOR_TEXT), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    snprintf(text, sizeof(text), "%s: %d%%", tr("Overall progress", "总体进度"), progress.percent);
    draw_text_rect(hdc, overall, text, ui_theme_color(UI_COLOR_TEXT_MUTED), DT_SINGLELINE | DT_VCENTER);
    fill_rect(hdc, overall_bar, RGB(42, 50, 58));
    fill = overall_bar;
    fill.right = fill.left + (overall_bar.right - overall_bar.left) * progress.percent / 100;
    if (fill.right > fill.left) fill_rect(hdc, fill, RGB(103, 164, 196));
    snprintf(text, sizeof(text), "%s: %s %d%%", tr("Current stage", "当前阶段"),
             stage_name, progress.stage_percent);
    draw_text_rect(hdc, stage, text, ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    if (progress.stage_total > 0) snprintf(text, sizeof(text), "%d / %d", progress.stage_current, progress.stage_total);
    else snprintf(text, sizeof(text), "%s", stage_name);
    draw_text_rect(hdc, detail, text, ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    fill_rect(hdc, stage_bar, RGB(42, 50, 58));
    fill = stage_bar;
    fill.right = fill.left + (stage_bar.right - stage_bar.left) * progress.stage_percent / 100;
    if (fill.right > fill.left) fill_rect(hdc, fill, RGB(154, 184, 112));
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

static void draw_civ_color_palette(HDC hdc, const WorldgenLayout *layout) {
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
                                  tr("Civilization Setup", "文明设置"));
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
    draw_civ_color_palette(hdc, layout);
    draw_metric_inputs(hdc, layout, active_tooltip);
}

static void draw_setup_stages(HDC hdc, const WorldgenLayout *layout) {
    char text[192];
    draw_section_rect(hdc, layout->stage_section, tr("Setup Stages", "设置阶段"));
    snprintf(text, sizeof(text), "%s: %s", tr("1 Physical World", "1 物理世界"),
             world_generated ? tr("generated", "已生成") : tr("generate first", "请先生成"));
    draw_text_rect(hdc, layout->stage_row[0], text, ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    snprintf(text, sizeof(text), "%s: %d %s", tr("2 Natural Regions", "2 自然区域"), region_count,
             tr("regions", "个区域"));
    draw_text_rect(hdc, layout->stage_row[1], text, ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    snprintf(text, sizeof(text), "%s: %d / %d", tr("3 Civilizations", "3 文明放置"), civ_count, initial_civ_count);
    draw_text_rect(hdc, layout->stage_row[2], text, ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, layout->stage_row[3],
                   tr("4 Start Simulation: Space controls month progression.",
                      "4 开始模拟：空格控制月份推进。"),
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
    draw_section_rect(hdc, layout.map_size_section, tr("Map Size", "地图大小"));
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
                                  tr("Physical World", "物理世界"));
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
    draw_section_rect(hdc, layout.regions_section, tr("Natural Regions", "自然区域"));
    draw_worldgen_slider(hdc, &layout, UI_SLIDER_REGION_SIZE, tr("Region Size", "区域大小"), region_size_slider, &tooltip_text);
    draw_text_rect(hdc, layout.generate_row, tr("Generate: F5 rebuilds world with these settings.",
                   "生成：F5 使用这些设置重建世界。"),
                   ui_theme_color(UI_COLOR_TEXT_MUTED), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_civilization_placement(hdc, &layout, &tooltip_text);
    draw_worldgen_progress_status_v2(hdc, &layout);
    SelectClipRgn(hdc, NULL);
    DeleteObject(clip);
    draw_tooltip(hdc, client, tooltip_text);
}
