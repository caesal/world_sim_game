#include "render/panel_worldgen_physical.h"

#include "render/panel_worldgen_controls.h"
#include "render/render_common.h"
#include "render/worldgen_ui_assets.h"
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

static RECT half_rect(RECT rect, int right_half) {
    int middle = (rect.left + rect.right) / 2;
    if (right_half) rect.left = middle + 3;
    else rect.right = middle - 3;
    return rect;
}

static void draw_map_size(HDC hdc, const UiWorldgenPhysicalLayout *physical,
                          const UiWorldgenEffectiveConfig *config,
                          const UiWorldgenControlState *state) {
    static const char *labels_en[UI_WORLDGEN_PANEL_MAP_SIZE_COUNT] = {
        "Small", "Medium", "Large", "Extreme"
    };
    static const char *labels_zh[UI_WORLDGEN_PANEL_MAP_SIZE_COUNT] = {
        "小", "中", "大", "极大"
    };
    int selected = config ? config->pending_map_size : 3;
    int i;

    ui_clay_draw_card(hdc, physical->map_size_section, UI_CLAY_STATE_NORMAL);
    draw_text_rect(hdc, physical->map_size_label, tr("Map Size", "地图尺寸"),
                   ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    for (i = 0; i < UI_WORLDGEN_PANEL_MAP_SIZE_COUNT; i++) {
        PanelWorldgenControlFlags flags = control_flags(
            state, UI_WORLDGEN_CONTROL_MAP_SIZE, i, 1, selected == i);
        ui_clay_draw_button(hdc, physical->map_size_button[i],
                            tr(labels_en[i], labels_zh[i]),
                            panel_worldgen_controls_clay_state(flags));
        panel_worldgen_controls_draw_focus_ring(
            hdc, physical->map_size_button[i], 7, flags.focused);
    }
}

static void draw_xy_axis_labels(HDC hdc,
                                const UiWorldgenPhysicalLayout *physical) {
    RECT low = physical->xy_x_low_label;
    RECT high = physical->xy_x_high_label;
    RECT more_ocean;
    RECT less_ocean;

    low.left = physical->xy_plot.left;
    low.right = (physical->xy_plot.left + physical->xy_plot.right) / 2 - 3;
    low.bottom = low.top + 38;
    high.left = low.right + 6;
    high.right = physical->xy_plot.right;
    high.bottom = high.top + 38;
    draw_text_rect(hdc, low, tr("Large /\nCoherent", "大型 / 连贯"),
                   ui_clay_muted_text_color(),
                   DT_CENTER | DT_WORDBREAK | DT_END_ELLIPSIS);
    draw_text_rect(hdc, high,
                   tr("Fragmented /\nArchipelagos", "破碎 / 群岛"),
                   ui_clay_muted_text_color(),
                   DT_CENTER | DT_WORDBREAK | DT_END_ELLIPSIS);

    more_ocean = (RECT){physical->xy_plot.left + 5,
                        physical->xy_plot.top + 3,
                        physical->xy_plot.right - 5,
                        physical->xy_plot.top + 22};
    less_ocean = (RECT){physical->xy_plot.left + 5,
                        physical->xy_plot.bottom - 22,
                        physical->xy_plot.right - 5,
                        physical->xy_plot.bottom - 3};
    draw_text_rect(hdc, more_ocean, tr("More Ocean", "海洋多"),
                   ui_clay_muted_text_color(),
                   DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    draw_text_rect(hdc, less_ocean, tr("Less Ocean", "海洋少"),
                   ui_clay_muted_text_color(),
                   DT_SINGLELINE | DT_CENTER | DT_VCENTER);
}

static void draw_xy(HDC hdc, const UiWorldgenPhysicalLayout *physical,
                    const UiWorldgenEffectiveConfig *config,
                    const UiWorldgenControlState *state) {
    PanelWorldgenControlFlags flags = control_flags(
        state, UI_WORLDGEN_CONTROL_PHYSICAL_XY, 0, 0, 0);
    RECT title = physical->xy_title;
    RECT value = half_rect(title, 1);
    char value_text[48];
    int continent = config ? config->continent_slider : 50;
    int ocean = config ? config->ocean_slider : 50;

    title = half_rect(title, 0);
    ui_clay_draw_card(hdc, physical->xy_section, UI_CLAY_STATE_NORMAL);
    draw_text_rect(hdc, title, tr("Ocean & Landmass", "海洋与陆块"),
                   ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    snprintf(value_text, sizeof(value_text), "X %d   Y %d", continent, ocean);
    draw_text_rect(hdc, value, value_text, ui_clay_muted_text_color(),
                   DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_END_ELLIPSIS);
    worldgen_ui_assets_draw_fit(
        hdc, WORLDGEN_UI_ASSET_LANDMASS_OCEAN_XY, physical->xy_asset);
    panel_worldgen_controls_draw_grid(
        hdc, physical->xy_plot, 4, 4, RGB(103, 126, 132));
    panel_worldgen_controls_draw_axes(
        hdc, physical->xy_plot, 50, 50, RGB(197, 213, 214));
    panel_worldgen_controls_draw_ticks(
        hdc, physical->xy_plot, 50, 50, 4, 4, 3, RGB(197, 213, 214));
    draw_xy_axis_labels(hdc, physical);
    panel_worldgen_controls_draw_handle(
        hdc, physical->xy_handle, PANEL_WORLDGEN_HANDLE_PRIMARY, 0, flags);
}

static void draw_baseline_ticks(HDC hdc, RECT baseline) {
    HGDIOBJ old_pen = SelectObject(hdc, GetStockObject(DC_PEN));
    COLORREF old_color = SetDCPenColor(hdc, RGB(206, 215, 208));
    int i;

    MoveToEx(hdc, baseline.left, (baseline.top + baseline.bottom) / 2, NULL);
    LineTo(hdc, baseline.right, (baseline.top + baseline.bottom) / 2);
    for (i = 0; i <= 4; i++) {
        int x = baseline.left + (baseline.right - baseline.left) * i / 4;
        MoveToEx(hdc, x, baseline.top - 3, NULL);
        LineTo(hdc, x, baseline.bottom + 4);
    }
    SetDCPenColor(hdc, old_color);
    SelectObject(hdc, old_pen);
}

static void draw_relief(HDC hdc, const UiWorldgenPhysicalLayout *physical,
                        const UiWorldgenEffectiveConfig *config,
                        const UiWorldgenControlState *state) {
    int values[UI_WORLDGEN_PANEL_RELIEF_HANDLE_COUNT] = {
        config ? config->relief_slider : 50,
        config ? config->bias_mountain_slider : 50
    };
    int coincident = values[0] == values[1];
    int i;
    char label[80];

    ui_clay_draw_card(hdc, physical->relief_section, UI_CLAY_STATE_NORMAL);
    draw_text_rect(hdc, physical->relief_title,
                   tr("Relief Profile", "地势剖面"),
                   ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    snprintf(label, sizeof(label), "%s",
             tr("Elevation Relief", "地势起伏"));
    draw_text_rect(hdc, physical->relief_label[0], label,
                   RGB(126, 194, 218),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    snprintf(label, sizeof(label), "%s",
             tr("Mountain Bias", "山脉偏好"));
    draw_text_rect(hdc, physical->relief_label[1], label,
                   RGB(221, 181, 101),
                   DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_END_ELLIPSIS);
    worldgen_ui_assets_draw_fit(
        hdc, WORLDGEN_UI_ASSET_RELIEF_PROFILE, physical->relief_asset);
    draw_baseline_ticks(hdc, physical->relief_baseline);
    for (i = 0; i < UI_WORLDGEN_PANEL_RELIEF_HANDLE_COUNT; i++) {
        PanelWorldgenControlFlags flags = control_flags(
            state, UI_WORLDGEN_CONTROL_RELIEF_HANDLE, i, 1, 0);
        RECT value_rect = physical->relief_handle[i];
        panel_worldgen_controls_draw_handle(
            hdc, physical->relief_handle[i],
            i == 0 ? PANEL_WORLDGEN_HANDLE_PRIMARY :
                     PANEL_WORLDGEN_HANDLE_SECONDARY,
            coincident, flags);
        value_rect.left -= 12;
        value_rect.right += 12;
        if (i == 0) OffsetRect(&value_rect, 0, -18);
        else OffsetRect(&value_rect, 0, 18);
        snprintf(label, sizeof(label), "%d", values[i]);
        draw_text_rect(hdc, value_rect, label,
                       i == 0 ? RGB(126, 194, 218) : RGB(221, 181, 101),
                       DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    }
}

void panel_worldgen_physical_draw(
    HDC hdc, const UiWorldgenPanelLayout *layout,
    const UiWorldgenEffectiveConfig *config,
    const UiWorldgenControlState *state, HFONT body_font) {
    int saved;

    if (!hdc || !layout) return;
    saved = SaveDC(hdc);
    if (!saved) return;
    IntersectClipRect(hdc, layout->content_viewport.left,
                      layout->content_viewport.top,
                      layout->content_viewport.right,
                      layout->content_viewport.bottom);
    SelectObject(hdc, body_font);
    draw_map_size(hdc, &layout->physical, config, state);
    draw_xy(hdc, &layout->physical, config, state);
    draw_relief(hdc, &layout->physical, config, state);
    RestoreDC(hdc, saved);
}
