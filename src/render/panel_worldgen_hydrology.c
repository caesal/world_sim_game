#include "render/panel_worldgen_hydrology.h"

#include "render/panel_worldgen_controls.h"
#include "render/render_common.h"
#include "render/worldgen_ui_assets.h"
#include "sim/regions.h"
#include "ui/ui_clay_primitives.h"
#include "ui/ui_clay_widgets.h"
#include "ui/ui_theme.h"

#include <stdio.h>

static int target_matches(UiWorldgenControlTarget target,
                          UiWorldgenControlKind kind, int index,
                          int index_matters) {
    return target.kind == kind && (!index_matters || target.index == index);
}

static PanelWorldgenControlFlags control_flags(
    const UiWorldgenControlState *state, UiWorldgenControlKind kind,
    int index, int index_matters, int selected) {
    PanelWorldgenControlFlags flags = {0};

    flags.selected = selected;
    if (!state) return flags;
    flags.hovered = target_matches(state->interaction.hovered, kind, index,
                                   index_matters);
    flags.pressed = target_matches(state->interaction.pressed, kind, index,
                                   index_matters) ||
                    target_matches(state->interaction.dragging, kind, index,
                                   index_matters);
    flags.focused = target_matches(state->interaction.focused, kind, index,
                                   index_matters);
    return flags;
}

static void draw_slot_outline(HDC hdc, RECT rect,
                              PanelWorldgenControlFlags flags) {
    HGDIOBJ old_pen;
    HGDIOBJ old_brush;
    COLORREF old_color;
    COLORREF color;

    if (!flags.selected && !flags.hovered && !flags.pressed && !flags.focused) {
        return;
    }
    color = flags.pressed ? RGB(245, 224, 170) :
            flags.selected ? ui_theme_color(UI_COLOR_ACCENT) :
                             RGB(177, 203, 208);
    InflateRect(&rect, -2, -2);
    old_pen = SelectObject(hdc, GetStockObject(DC_PEN));
    old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    old_color = SetDCPenColor(hdc, color);
    RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, 8, 8);
    SetDCPenColor(hdc, old_color);
    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    panel_worldgen_controls_draw_focus_ring(hdc, rect, 4, flags.focused);
}

static RECT left_half(RECT rect) {
    rect.right -= 92;
    return rect;
}

static RECT right_half(RECT rect) {
    rect.left = rect.right - 88;
    return rect;
}

static int river_preset_index(int value) {
    if (value < 0 || value > 100 || value % 25 != 0) return -1;
    return value / 25;
}

static void draw_river(HDC hdc,
                       const UiWorldgenHydrologyLayout *hydrology,
                       const UiWorldgenEffectiveConfig *config,
                       const UiWorldgenControlState *state) {
    static const char *labels_en[UI_WORLDGEN_PANEL_RIVER_SLOT_COUNT] = {
        "Very\nSparse", "Sparse", "Balanced", "Dense", "Very\nDense"
    };
    static const char *labels_zh[UI_WORLDGEN_PANEL_RIVER_SLOT_COUNT] = {
        "极疏", "稀疏", "均衡", "密集", "极密"
    };
    int value = config ? config->bias_wetland_slider : 50;
    int selected = river_preset_index(value);
    PanelWorldgenControlFlags slider_flags = control_flags(
        state, UI_WORLDGEN_CONTROL_RIVER_DENSITY, 0, 0, 0);
    RECT title = left_half(hydrology->river_title);
    RECT value_rect = right_half(hydrology->river_title);
    char value_text[32];
    int old_extra;
    int i;

    ui_clay_draw_card(hdc, hydrology->river_section, UI_CLAY_STATE_NORMAL);
    draw_text_rect(hdc, title, tr("River Network Density", "河网密度"),
                   ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    snprintf(value_text, sizeof(value_text), "%d / 0-100", value);
    draw_text_rect(hdc, value_rect, value_text,
                   ui_clay_muted_text_color(),
                   DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_END_ELLIPSIS);
    worldgen_ui_assets_draw_fit(
        hdc, WORLDGEN_UI_ASSET_RIVER_DENSITY_ATLAS,
        hydrology->river_asset);
    old_extra = SetTextCharacterExtra(hdc, -1);
    for (i = 0; i < UI_WORLDGEN_PANEL_RIVER_SLOT_COUNT; i++) {
        PanelWorldgenControlFlags slot_flags = control_flags(
            state, UI_WORLDGEN_CONTROL_RIVER_DENSITY, i, 1,
            selected == i);
        draw_slot_outline(hdc, hydrology->river_slot[i], slot_flags);
        draw_text_rect(hdc, hydrology->river_slot_label[i],
                       tr(labels_en[i], labels_zh[i]),
                       slot_flags.selected ? ui_theme_color(UI_COLOR_ACCENT) :
                                             ui_clay_muted_text_color(),
                       DT_CENTER | DT_WORDBREAK | DT_END_ELLIPSIS);
    }
    SetTextCharacterExtra(hdc, old_extra);
    ui_clay_draw_slider(hdc, hydrology->river_track, value,
                        panel_worldgen_controls_clay_state(slider_flags));
    panel_worldgen_controls_draw_focus_ring(
        hdc, hydrology->river_track, 4, slider_flags.focused);
}

static void draw_region_estimates(
    HDC hdc, const UiWorldgenHydrologyLayout *hydrology,
    const UiWorldgenEffectiveConfig *config, int map_width, int map_height) {
    char text[96];
    int target_area;
    int estimated;
    int maximum;
    int cap_reached = 0;
    int ocean = config ? config->ocean_slider : 50;
    int region_size = config ? config->region_size_slider : 50;

    if (map_width < 1) map_width = 1;
    if (map_height < 1) map_height = 1;
    target_area = regions_target_size_from_slider(region_size);
    estimated = regions_estimated_count_for_settings(
        map_width, map_height, ocean, region_size, &cap_reached);
    maximum = regions_max_count_for_dimensions(map_width, map_height);
    snprintf(text, sizeof(text), "%s: %d",
             tr("Target Area", "目标面积"), target_area);
    draw_text_rect(hdc, hydrology->region_target_area, text,
                   ui_clay_muted_text_color(),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    snprintf(text, sizeof(text), "%s: %d / %d%s",
             tr("Estimated Regions", "估算区域数"), estimated, maximum,
             cap_reached ? tr(" (cap)", "（上限）") : "");
    draw_text_rect(hdc, hydrology->region_estimated_count, text,
                   cap_reached ? RGB(225, 178, 92) :
                                 ui_clay_muted_text_color(),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}

static void draw_region(HDC hdc,
                        const UiWorldgenHydrologyLayout *hydrology,
                        const UiWorldgenEffectiveConfig *config,
                        const UiWorldgenControlState *state,
                        int map_width, int map_height) {
    static const char *labels_en[UI_WORLDGEN_REGION_PRESET_COUNT] = {
        "Very\nSmall", "Small", "Medium", "Large", "Very\nLarge"
    };
    static const char *labels_zh[UI_WORLDGEN_REGION_PRESET_COUNT] = {
        "极小", "小", "中", "大", "极大"
    };
    UiWorldgenRegionCategory category = state ? state->region_category :
                                               UI_WORLDGEN_REGION_MEDIUM;
    int raw_value = config ? config->region_size_slider : 70;
    RECT title = left_half(hydrology->region_title);
    RECT value_rect = right_half(hydrology->region_title);
    char value_text[64];
    int old_extra;
    int i;

    ui_clay_draw_card(hdc, hydrology->region_section, UI_CLAY_STATE_NORMAL);
    draw_text_rect(hdc, title, tr("Natural Region Scale", "自然区域尺度"),
                   ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    snprintf(value_text, sizeof(value_text), "%s: %d",
             tr("Raw", "原始值"), raw_value);
    draw_text_rect(hdc, value_rect, value_text,
                   ui_clay_muted_text_color(),
                   DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_END_ELLIPSIS);
    worldgen_ui_assets_draw_fit(
        hdc, WORLDGEN_UI_ASSET_NATURAL_REGION_SCALE_ATLAS,
        hydrology->region_asset);
    old_extra = SetTextCharacterExtra(hdc, -1);
    for (i = 0; i < UI_WORLDGEN_REGION_PRESET_COUNT; i++) {
        PanelWorldgenControlFlags flags = control_flags(
            state, UI_WORLDGEN_CONTROL_REGION_CATEGORY, i, 1,
            category == (UiWorldgenRegionCategory)i);
        draw_slot_outline(hdc, hydrology->region_slot[i], flags);
        draw_text_rect(hdc, hydrology->region_slot_label[i],
                       tr(labels_en[i], labels_zh[i]),
                       flags.selected ? ui_theme_color(UI_COLOR_ACCENT) :
                                        ui_clay_muted_text_color(),
                       DT_CENTER | DT_WORDBREAK | DT_END_ELLIPSIS);
    }
    SetTextCharacterExtra(hdc, old_extra);
    {
        PanelWorldgenControlFlags custom_flags = control_flags(
            state, UI_WORLDGEN_CONTROL_REGION_CUSTOM_INPUT, 0, 0,
            category == UI_WORLDGEN_REGION_CUSTOM);
        custom_flags.disabled = category != UI_WORLDGEN_REGION_CUSTOM;
        draw_text_rect(hdc, hydrology->region_custom_label,
                       tr("Custom (0-100)", "自定义 (0-100)"),
                       ui_clay_muted_text_color(),
                       DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        ui_clay_draw_input_frame(
            hdc, hydrology->region_custom_input,
            panel_worldgen_controls_clay_state(custom_flags));
        panel_worldgen_controls_draw_focus_ring(
            hdc, hydrology->region_custom_input, 6, custom_flags.focused);
    }
    draw_region_estimates(hdc, hydrology, config, map_width, map_height);
}

static void draw_initial_civs(
    HDC hdc, const UiWorldgenHydrologyLayout *hydrology) {
    ui_clay_draw_card(hdc, hydrology->initial_civs_section,
                      UI_CLAY_STATE_NORMAL);
    draw_text_rect(hdc, hydrology->initial_civs_label,
                   tr("Initial civilizations (0-200)",
                      "初始文明数量 (0-200)"),
                   ui_clay_muted_text_color(),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    ui_clay_draw_input_frame(hdc, hydrology->initial_civs_input_frame,
                             UI_CLAY_STATE_NORMAL);
}

void panel_worldgen_hydrology_draw(
    HDC hdc, const UiWorldgenPanelLayout *layout,
    const UiWorldgenEffectiveConfig *config,
    const UiWorldgenControlState *state,
    int map_width, int map_height, HFONT body_font) {
    int saved;

    if (!hdc || !layout) return;
    saved = SaveDC(hdc);
    if (!saved) return;
    IntersectClipRect(hdc, layout->content_viewport.left,
                      layout->content_viewport.top,
                      layout->content_viewport.right,
                      layout->content_viewport.bottom);
    SelectObject(hdc, body_font);
    draw_river(hdc, &layout->hydrology, config, state);
    draw_region(hdc, &layout->hydrology, config, state,
                map_width, map_height);
    draw_initial_civs(hdc, &layout->hydrology);
    RestoreDC(hdc, saved);
}
