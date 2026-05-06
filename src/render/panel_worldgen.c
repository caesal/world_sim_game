#include "render_panel_internal.h"

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

static void draw_map_size_selector(HDC hdc, const WorldgenLayout *layout) {
    const char *size_en[3] = {"Small", "Medium", "Large"};
    const char *size_zh[3] = {"小", "中", "大"};
    int i;

    for (i = 0; i < MAP_SIZE_COUNT; i++) {
        RECT button = layout->map_size_buttons[i];
        if (!worldgen_rect_visible(layout->viewport, button)) continue;
        fill_rect(hdc, button, i == pending_map_size ? RGB(87, 93, 78) : ui_theme_color(UI_COLOR_PANEL_SOFT));
        draw_center_text(hdc, button, ui_language == UI_LANG_ZH ? size_zh[i] : size_en[i], ui_theme_color(UI_COLOR_TEXT));
    }
}

static void draw_worldgen_slider(HDC hdc, const WorldgenLayout *layout, int index, const char *name, int value) {
    const WorldgenSliderLayout *slider = &layout->sliders[index];
    RECT fill = slider->track;
    int knob_x = slider->track.left + (slider->track.right - slider->track.left) * value / 100;
    RECT knob = {knob_x - 7, slider->track.top - 8, knob_x + 7, slider->track.top + 18};
    char value_text[16];

    if (!worldgen_rect_visible(layout->viewport, slider->hit)) return;
    snprintf(value_text, sizeof(value_text), "%d", value);
    draw_text_rect(hdc, slider->label, name, RGB(205, 214, 222),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, slider->value, value_text, RGB(232, 238, 244),
                   DT_SINGLELINE | DT_VCENTER | DT_RIGHT);
    fill_rect(hdc, slider->track, RGB(82, 94, 104));
    fill.right = knob_x;
    fill_rect(hdc, fill, RGB(82, 136, 171));
    fill_rect(hdc, knob, RGB(232, 238, 244));
}

static void draw_civilization_placement(HDC hdc, const WorldgenLayout *layout) {
    static const char *labels_en[WORLDGEN_CORE_METRIC_COUNT] = {
        "Military", "Logistics", "Governance", "Cohesion", "Production", "Commerce", "Innovation"
    };
    static const char *labels_zh[WORLDGEN_CORE_METRIC_COUNT] = {
        "军备", "后勤", "治理", "凝聚", "生产", "贸易", "技术"
    };
    int i;

    draw_section_rect(hdc, layout->civ_section, tr("Civilization Placement", "文明放置"));
    draw_text_rect(hdc, layout->civ_hint,
                   tr("7 core metrics; Adaptation is dynamic.", "七核心指标；适应力为动态状态。"),
                   ui_theme_color(UI_COLOR_TEXT_MUTED), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, layout->name_label, tr("Name", "名称"), ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_text_rect(hdc, layout->symbol_label, tr("Symbol", "符号"), ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    for (i = 0; i < WORLDGEN_CORE_METRIC_COUNT; i++) {
        draw_text_rect(hdc, layout->metric_label[i],
                       ui_language == UI_LANG_ZH ? labels_zh[i] : labels_en[i],
                       ui_theme_color(UI_COLOR_TEXT_MUTED), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }
}

void draw_worldgen_panel(HDC hdc, RECT client, int x, HFONT title_font, HFONT body_font) {
    WorldgenLayout layout;
    HRGN clip;
    int scroll;
    (void)x;

    scroll = worldgen_layout_clamp_scroll(client, side_panel_w, worldgen_scroll_offset);
    worldgen_layout_build(client, side_panel_w, scroll, &layout);
    clip = CreateRectRgn(layout.viewport.left, layout.viewport.top, layout.viewport.right, layout.viewport.bottom);
    SelectClipRgn(hdc, clip);
    SelectObject(hdc, title_font);
    draw_text_rect(hdc, layout.title, tr("World Generation", "世界生成"), ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    SelectObject(hdc, body_font);
    draw_section_rect(hdc, layout.map_size_section, tr("Map Size", "地图大小"));
    draw_map_size_selector(hdc, &layout);
    draw_text_rect(hdc, layout.initial_label, tr("Initial civilizations", "初始文明数量"),
                   ui_theme_color(UI_COLOR_TEXT_MUTED), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_section_rect(hdc, layout.physical_section, tr("Physical World", "自然世界"));
    draw_worldgen_slider(hdc, &layout, WORLD_SLIDER_OCEAN, tr("Ocean", "海陆比例"), ocean_slider);
    draw_worldgen_slider(hdc, &layout, WORLD_SLIDER_CONTINENT, tr("Fragment", "大陆破碎"), continent_slider);
    draw_worldgen_slider(hdc, &layout, WORLD_SLIDER_RELIEF, tr("Relief", "地势起伏"), relief_slider);
    draw_worldgen_slider(hdc, &layout, WORLD_SLIDER_MOISTURE, tr("Moisture", "湿润度"), moisture_slider);
    draw_worldgen_slider(hdc, &layout, WORLD_SLIDER_DROUGHT, tr("Drought", "干旱度"), drought_slider);
    draw_worldgen_slider(hdc, &layout, WORLD_SLIDER_VEGETATION, tr("Vegetation", "植被密度"), vegetation_slider);
    draw_section_rect(hdc, layout.terrain_section, tr("Terrain Bias", "地形偏好"));
    draw_worldgen_slider(hdc, &layout, WORLD_SLIDER_BIAS_FOREST, tr("Forest", "森林"), bias_forest_slider);
    draw_worldgen_slider(hdc, &layout, WORLD_SLIDER_BIAS_DESERT, tr("Desert", "沙漠"), bias_desert_slider);
    draw_worldgen_slider(hdc, &layout, WORLD_SLIDER_BIAS_MOUNTAIN, tr("Mountain", "山脉"), bias_mountain_slider);
    draw_worldgen_slider(hdc, &layout, WORLD_SLIDER_BIAS_WETLAND, tr("Wetland", "湿地"), bias_wetland_slider);
    draw_section_rect(hdc, layout.regions_section, tr("Natural Regions", "自然区域"));
    draw_worldgen_slider(hdc, &layout, UI_SLIDER_REGION_SIZE, tr("Region Size", "区域大小"), region_size_slider);
    draw_text_rect(hdc, layout.generate_row, tr("Generate: F5 rebuilds world with these settings.",
                   "生成：F5 使用这些设置重建世界。"),
                   ui_theme_color(UI_COLOR_TEXT_MUTED), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_civilization_placement(hdc, &layout);
    SelectClipRgn(hdc, NULL);
    DeleteObject(clip);
}
