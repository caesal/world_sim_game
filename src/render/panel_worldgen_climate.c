#include "render/panel_worldgen_climate.h"

#include "render/panel_worldgen_controls.h"
#include "render/render_common.h"
#include "render/worldgen_ui_assets.h"
#include "ui/ui_clay_primitives.h"
#include "ui/ui_clay_theme.h"
#include "ui/ui_theme.h"

static int target_matches(UiWorldgenControlTarget target,
                          UiWorldgenControlKind kind, int index,
                          int index_matters) {
    return target.kind == kind && (!index_matters || target.index == index);
}

static PanelWorldgenControlFlags control_flags(
    const UiWorldgenControlState *state, UiWorldgenControlKind kind,
    int index, int index_matters) {
    PanelWorldgenControlFlags flags = {0};

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

static POINT rect_center(RECT rect) {
    POINT center = {(rect.left + rect.right) / 2,
                    (rect.top + rect.bottom) / 2};
    return center;
}

static void draw_envelope(HDC hdc,
                          const UiWorldgenClimateLayout *climate) {
    HGDIOBJ old_pen = SelectObject(hdc, GetStockObject(DC_PEN));
    HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    COLORREF old_color = SetDCPenColor(hdc, RGB(105, 216, 224));
    POINT corners[UI_WORLDGEN_CLIMATE_CORNER_COUNT];
    int i;

    for (i = 0; i < UI_WORLDGEN_CLIMATE_CORNER_COUNT; i++) {
        corners[i] = rect_center(climate->corner_handle[i]);
    }
    Polygon(hdc, corners, UI_WORLDGEN_CLIMATE_CORNER_COUNT);
    SetDCPenColor(hdc, old_color);
    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
}

static void draw_axis_labels(HDC hdc,
                             const UiWorldgenClimateLayout *climate) {
    RECT humid = {climate->plot.left + 5, climate->plot.top + 3,
                  climate->plot.right - 5, climate->plot.top + 22};
    RECT dry = {climate->plot.left + 5, climate->plot.bottom - 22,
                climate->plot.right - 5, climate->plot.bottom - 3};

    draw_text_rect(hdc, climate->x_low_label,
                   tr("Cold -50", "寒冷 -50"),
                   ui_clay_muted_text_color(),
                   DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    draw_text_rect(hdc, climate->x_high_label,
                   tr("Hot +50", "炎热 +50"),
                   ui_clay_muted_text_color(),
                   DT_SINGLELINE | DT_VCENTER | DT_RIGHT);
    draw_text_rect(hdc, humid, tr("Humid +50", "湿润 +50"),
                   ui_clay_muted_text_color(),
                   DT_SINGLELINE | DT_VCENTER | DT_CENTER);
    draw_text_rect(hdc, dry, tr("Dry -50", "干燥 -50"),
                   ui_clay_muted_text_color(),
                   DT_SINGLELINE | DT_VCENTER | DT_CENTER);
}

static void draw_biome_labels(HDC hdc,
                              const UiWorldgenClimateLayout *climate) {
    static const char *labels_en[UI_WORLDGEN_PANEL_BIOME_LABEL_COUNT] = {
        "Icefield", "Tundra", "Temperate Grassland", "Desert",
        "Forest", "Monsoon", "Tropical Rainforest"
    };
    static const char *labels_zh[UI_WORLDGEN_PANEL_BIOME_LABEL_COUNT] = {
        "冰原", "苔原", "温带草原", "沙漠", "森林", "季风区", "热带雨林"
    };
    int old_extra = SetTextCharacterExtra(hdc, -1);
    int i;

    for (i = 0; i < UI_WORLDGEN_PANEL_BIOME_LABEL_COUNT; i++) {
        RECT label = climate->biome_label[i];
        label.top -= 8;
        label.bottom += 8;
        draw_text_rect(hdc, label,
                       tr(labels_en[i], labels_zh[i]),
                       RGB(213, 222, 214),
                       DT_CENTER | DT_WORDBREAK);
    }
    SetTextCharacterExtra(hdc, old_extra);
}

static void draw_corner_handles(HDC hdc,
                                const UiWorldgenClimateLayout *climate,
                                const UiWorldgenControlState *state) {
    int i;
    for (i = 0; i < UI_WORLDGEN_CLIMATE_CORNER_COUNT; i++) {
        PanelWorldgenControlFlags flags = control_flags(
            state, UI_WORLDGEN_CONTROL_CLIMATE_CORNER, i, 1);
        panel_worldgen_controls_draw_handle(
            hdc, climate->corner_handle[i],
            (i == UI_WORLDGEN_CLIMATE_TOP_RIGHT ||
             i == UI_WORLDGEN_CLIMATE_BOTTOM_LEFT) ?
                PANEL_WORLDGEN_HANDLE_SECONDARY :
                PANEL_WORLDGEN_HANDLE_PRIMARY,
            0, flags);
    }
}

static int climate_handle_overlaps(
    const UiWorldgenClimateLayout *climate, int index) {
    POINT center = rect_center(climate->corner_handle[index]);
    int i;
    for (i = 0; i < UI_WORLDGEN_CLIMATE_CORNER_COUNT; i++) {
        POINT other;
        if (i == index) continue;
        other = rect_center(climate->corner_handle[i]);
        if (center.x == other.x && center.y == other.y) return 1;
    }
    return 0;
}

static void draw_overlap_identity(HDC hdc,
                                  const UiWorldgenClimateLayout *climate,
                                  const UiWorldgenControlState *state,
                                  int index) {
    static const int offsets[UI_WORLDGEN_CLIMATE_CORNER_COUNT][2] = {
        {-20, -20}, {20, -20}, {20, 20}, {-20, 20}
    };
    static const char *labels[UI_WORLDGEN_CLIMATE_CORNER_COUNT] = {
        "TL", "TR", "BR", "BL"
    };
    POINT center = rect_center(climate->corner_handle[index]);
    POINT end = {center.x + offsets[index][0],
                 center.y + offsets[index][1]};
    RECT label = {end.x - 11, end.y - 9, end.x + 11, end.y + 9};
    PanelWorldgenControlFlags flags = control_flags(
        state, UI_WORLDGEN_CONTROL_CLIMATE_CORNER, index, 1);
    COLORREF color = flags.pressed || flags.focused ?
        RGB(255, 236, 180) :
        (index == UI_WORLDGEN_CLIMATE_TOP_RIGHT ||
         index == UI_WORLDGEN_CLIMATE_BOTTOM_LEFT ?
             RGB(221, 181, 101) : RGB(126, 194, 218));
    HGDIOBJ old_pen = SelectObject(hdc, GetStockObject(DC_PEN));
    COLORREF old_color = SetDCPenColor(hdc, color);

    MoveToEx(hdc, center.x, center.y, NULL);
    LineTo(hdc, end.x, end.y);
    SetDCPenColor(hdc, old_color);
    SelectObject(hdc, old_pen);
    draw_text_rect(hdc, label, labels[index], color,
                   DT_SINGLELINE | DT_CENTER | DT_VCENTER);
}

static void draw_overlap_identities(
    HDC hdc, const UiWorldgenClimateLayout *climate,
    const UiWorldgenControlState *state) {
    int i;
    for (i = 0; i < UI_WORLDGEN_CLIMATE_CORNER_COUNT; i++) {
        if (climate_handle_overlaps(climate, i)) {
            draw_overlap_identity(hdc, climate, state, i);
        }
    }
}

static void draw_vegetation(HDC hdc,
                            const UiWorldgenClimateLayout *climate,
                            const UiWorldgenEffectiveConfig *config,
                            const UiWorldgenControlState *state) {
    PanelWorldgenSliderGeometry geometry;
    PanelWorldgenControlFlags flags = control_flags(
        state, UI_WORLDGEN_CONTROL_VEGETATION, 0, 0);

    ui_clay_draw_card(hdc, climate->vegetation_section, UI_CLAY_STATE_NORMAL);
    geometry.clip = climate->vegetation_section;
    geometry.label = climate->vegetation_label;
    geometry.value = climate->vegetation_value;
    geometry.track = climate->vegetation_track;
    panel_worldgen_controls_draw_continuous_slider(
        hdc, &geometry, tr("Plant Density", "植被密度"),
        config ? config->vegetation_slider : 50, flags);
}

void panel_worldgen_climate_draw(
    HDC hdc, const UiWorldgenPanelLayout *layout,
    const UiWorldgenEffectiveConfig *config,
    const UiWorldgenControlState *state, HFONT body_font) {
    const UiWorldgenClimateLayout *climate;
    int saved;

    if (!hdc || !layout) return;
    climate = &layout->climate;
    saved = SaveDC(hdc);
    if (!saved) return;
    IntersectClipRect(hdc, layout->content_viewport.left,
                      layout->content_viewport.top,
                      layout->content_viewport.right,
                      layout->content_viewport.bottom);
    SelectObject(hdc, body_font);
    ui_clay_draw_card(hdc, climate->section, UI_CLAY_STATE_NORMAL);
    draw_text_rect(hdc, climate->title,
                   tr("Temperature & Humidity", "温度与湿度"),
                   ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    worldgen_ui_assets_draw_fit(
        hdc, WORLDGEN_UI_ASSET_CLIMATE_BIOME_ENVELOPE, climate->asset);
    panel_worldgen_controls_draw_grid(
        hdc, climate->plot, 4, 4, RGB(109, 126, 125));
    panel_worldgen_controls_draw_axes(
        hdc, climate->plot, 50, 50, RGB(207, 217, 207));
    panel_worldgen_controls_draw_ticks(
        hdc, climate->plot, 50, 50, 4, 4, 3, RGB(207, 217, 207));
    draw_envelope(hdc, climate);
    draw_biome_labels(hdc, climate);
    draw_axis_labels(hdc, climate);
    draw_corner_handles(hdc, climate, state);
    draw_overlap_identities(hdc, climate, state);
    draw_vegetation(hdc, climate, config, state);
    RestoreDC(hdc, saved);
}
