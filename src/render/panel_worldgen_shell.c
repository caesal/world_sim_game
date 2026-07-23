#include "render/panel_worldgen_shell.h"

#include "render/panel_worldgen_controls.h"
#include "render/render_common.h"
#include "ui/ui_clay_primitives.h"
#include "ui/ui_clay_widgets.h"
#include "ui/ui_theme.h"

static int target_matches(UiWorldgenControlTarget target,
                          UiWorldgenControlKind kind, int index,
                          int index_matters) {
    return target.kind == kind && (!index_matters || target.index == index);
}

static PanelWorldgenControlFlags control_flags(
    const UiWorldgenControlState *state, UiWorldgenControlKind kind,
    int index, int index_matters, int selected) {
    PanelWorldgenControlFlags flags = {0};

    if (!state) {
        flags.selected = selected;
        return flags;
    }
    flags.hovered = target_matches(state->interaction.hovered, kind, index,
                                   index_matters);
    flags.pressed = target_matches(state->interaction.pressed, kind, index,
                                   index_matters) ||
                    target_matches(state->interaction.dragging, kind, index,
                                   index_matters);
    flags.focused = target_matches(state->interaction.focused, kind, index,
                                   index_matters);
    flags.selected = selected;
    return flags;
}

static void draw_preset(HDC hdc, const UiWorldgenPanelLayout *layout,
                        const UiWorldgenControlState *state) {
    UiWorldgenPreset preset = state ? state->preset : UI_WORLDGEN_PRESET_BALANCED;
    PanelWorldgenControlFlags flags = control_flags(
        state, UI_WORLDGEN_CONTROL_PRESET, 0, 0, 1);

    draw_text_rect(hdc, layout->preset_label, tr("Preset", "预设"),
                   ui_clay_muted_text_color(),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    ui_clay_draw_button(
        hdc, layout->preset_selector,
        preset == UI_WORLDGEN_PRESET_BALANCED ?
            tr("Balanced Continents", "均衡大陆") : tr("Custom", "自定义"),
        panel_worldgen_controls_clay_state(flags));
    panel_worldgen_controls_draw_focus_ring(
        hdc, layout->preset_selector, 7, flags.focused);

    flags = control_flags(state, UI_WORLDGEN_CONTROL_DICE, 0, 0, 0);
    panel_worldgen_controls_draw_glyph_button(
        hdc, layout->dice_button, PANEL_WORLDGEN_GLYPH_DICE, flags);
    flags = control_flags(state, UI_WORLDGEN_CONTROL_RESET, 0, 0, 0);
    panel_worldgen_controls_draw_glyph_button(
        hdc, layout->reset_button, PANEL_WORLDGEN_GLYPH_RESET, flags);
}

static void draw_tab_label(HDC hdc, RECT rect, int index, COLORREF color) {
    static const char *labels_en[UI_WORLDGEN_TAB_COUNT][2] = {
        {"Physical", "Terrain"}, {"Climate &", "Vegetation"},
        {"Hydrology &", "Regions"}, {"Legacy", "Modules"}
    };
    static const char *labels_zh[UI_WORLDGEN_TAB_COUNT] = {
        "物理地形", "气候植被", "水文区域", "传统模块"
    };
    TEXTMETRICA metrics;
    RECT line = rect;
    int line_height;

    if (ui_language == UI_LANG_ZH) {
        draw_text_rect(hdc, rect, labels_zh[index], color,
                       DT_CENTER | DT_SINGLELINE | DT_VCENTER |
                           DT_END_ELLIPSIS | DT_NOPREFIX);
        return;
    }
    line_height = GetTextMetricsA(hdc, &metrics) ? metrics.tmHeight :
                                                  (rect.bottom - rect.top) / 2;
    if (line_height * 2 > rect.bottom - rect.top)
        line_height = (rect.bottom - rect.top) / 2;
    line.top = rect.top + (rect.bottom - rect.top - line_height * 2) / 2;
    line.bottom = line.top + line_height;
    draw_text_rect(hdc, line, labels_en[index][0], color,
                   DT_CENTER | DT_SINGLELINE | DT_VCENTER |
                       DT_END_ELLIPSIS | DT_NOPREFIX);
    line.top = line.bottom;
    line.bottom += line_height;
    draw_text_rect(hdc, line, labels_en[index][1], color,
                   DT_CENTER | DT_SINGLELINE | DT_VCENTER |
                       DT_END_ELLIPSIS | DT_NOPREFIX);
}

static void draw_tabs(HDC hdc, const UiWorldgenPanelLayout *layout,
                      const UiWorldgenControlState *state) {
    UiWorldgenControlTab selected = state ? state->tab : UI_WORLDGEN_TAB_PHYSICAL;
    int i;

    for (i = 0; i < UI_WORLDGEN_TAB_COUNT; i++) {
        PanelWorldgenControlFlags flags = control_flags(
            state, UI_WORLDGEN_CONTROL_TAB, i, 1,
            selected == (UiWorldgenControlTab)i);
        UiClayState clay_state = panel_worldgen_controls_clay_state(flags);

        ui_clay_draw_tab(hdc, layout->tab_button[i], clay_state);
        draw_tab_label(hdc, layout->tab_label[i], i,
                       ui_clay_text_color(clay_state));
        panel_worldgen_controls_draw_focus_ring(
            hdc, layout->tab_button[i], 7, flags.focused);
    }
}

static void draw_footer(HDC hdc, const UiWorldgenPanelLayout *layout,
                        const UiWorldgenControlState *state) {
    PanelWorldgenControlFlags flags = control_flags(
        state, UI_WORLDGEN_CONTROL_FOOTER_GENERATE, 0, 0, 0);
    int dirty = state ? state->dirty : 0;
    COLORREF status_color = dirty ?
        (state && state->generation_failed ?
             ui_theme_color(UI_COLOR_DANGER) : ui_theme_color(UI_COLOR_ACCENT)) :
        ui_theme_color(UI_COLOR_GOOD);

    ui_clay_draw_button(hdc, layout->generate_button,
                        tr("F5 Generate World", "F5 生成世界"),
                        panel_worldgen_controls_clay_state(flags));
    panel_worldgen_controls_draw_focus_ring(
        hdc, layout->generate_button, 7, flags.focused);
    draw_text_rect(
        hdc, layout->status,
        dirty ? tr("Parameters modified, waiting to generate",
                   "参数已修改，等待生成") :
                tr("Parameters ready", "参数已就绪"),
        status_color, DT_WORDBREAK | DT_VCENTER | DT_END_ELLIPSIS);
}

void panel_worldgen_shell_draw(HDC hdc,
                               const UiWorldgenPanelLayout *layout,
                               const UiWorldgenControlState *state,
                               HFONT title_font, HFONT body_font) {
    int saved;

    if (!hdc || !layout) return;
    saved = SaveDC(hdc);
    if (!saved) return;
    SelectObject(hdc, layout->inner.right - layout->inner.left < 380 ?
                      body_font : title_font);
    draw_text_rect(hdc, layout->title,
                   tr("World Generation Controls (Next F5)",
                      "世界生成控制（下次 F5）"),
                   ui_theme_color(UI_COLOR_TEXT),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    SelectObject(hdc, body_font);
    draw_text_rect(hdc, layout->subtitle,
                   tr("Only affects the next generation",
                      "仅影响下一次生成"),
                   ui_theme_color(UI_COLOR_TEXT_DIM),
                   DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    draw_preset(hdc, layout, state);
    draw_tabs(hdc, layout, state);
    draw_footer(hdc, layout, state);
    RestoreDC(hdc, saved);
}

void panel_worldgen_shell_draw_tooltip(
    HDC hdc, RECT client, const UiWorldgenControlState *state) {
    if (!hdc || !state) return;
    if (target_matches(state->interaction.hovered,
                       UI_WORLDGEN_CONTROL_DICE, 0, 0)) {
        draw_tooltip(
            hdc, client,
            tr("Randomize physical and advanced terrain parameters",
               "随机设置物理与高级地形参数"));
    } else if (target_matches(state->interaction.hovered,
                              UI_WORLDGEN_CONTROL_RESET, 0, 0)) {
        draw_tooltip(hdc, client,
                     tr("Restore Balanced Continents", "恢复均衡大陆"));
    }
}
