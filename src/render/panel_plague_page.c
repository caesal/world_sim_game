#include "render_panel_internal.h"

#include "render/panel_plague_history.h"
#include "render/panel_plague_impact.h"
#include "render/panel_plague_live.h"
#include "render/panel_plague_page.h"
#include "render/panel_plague_probability.h"
#include "render/snapshot_ui.h"
#include "ui/ui_clay_widgets.h"
#include "ui/ui_plague_fog.h"
#include "ui/ui_plague_panel.h"
#include "ui/ui_plague_panel_layout.h"
#include "ui/ui_plague_probability.h"
#include "ui/ui_pressed_state.h"
#include "ui/ui_widgets.h"

static int last_live_content_mode = -2;

static void pre_sync_content_state(const RenderSnapshot *snapshot) {
    PlaguePanelTab tab = ui_plague_panel_main_tab();
    if (tab == PLAGUE_PANEL_TAB_IMPACT &&
        (!snapshot || !snapshot->plague_impact.active)) {
        ui_plague_panel_set_content_height(tab, 118);
    }
    if (tab == PLAGUE_PANEL_TAB_LIVE) {
        int mode = snapshot ? (int)plague_panel_live_mode(snapshot) : -1;
        if (mode != last_live_content_mode) {
            ui_plague_panel_set_content_height(tab, 0);
            last_live_content_mode = mode;
        }
    }
}

static const char *main_tab_label(PlaguePanelTab tab) {
    if (tab == PLAGUE_PANEL_TAB_IMPACT) return tr("Impact", "影响");
    if (tab == PLAGUE_PANEL_TAB_HISTORY) return tr("History", "历史对比");
    return tr("Live", "实时");
}

static void draw_main_tabs(HDC hdc, const UiPlaguePanelLayout *layout) {
    PlaguePanelTab selected = ui_plague_panel_main_tab();
    int hover = ui_plague_panel_hover_target();
    int i;
    for (i = 0; i < PLAGUE_PANEL_TAB_COUNT; i++) {
        int hovered = hover == UI_PLAGUE_PANEL_HIT_MAIN_TAB_BASE + i;
        UiClayState state = ui_clay_state_from_flags(
            hovered,
            ui_pressed_control_is_active(UI_PRESSED_PLAGUE_MAIN_TAB, i),
            selected == (PlaguePanelTab)i, 0);
        ui_clay_draw_button(hdc, layout->main_tabs[i],
                            main_tab_label((PlaguePanelTab)i), state);
    }
}

static int draw_tab_content(HDC hdc, const UiPlaguePanelLayout *layout,
                            const RenderSnapshot *snapshot) {
    PlaguePanelTab tab = ui_plague_panel_main_tab();
    if (!snapshot) {
        UiCursor cursor = ui_cursor(layout->content_viewport.left,
            layout->content_origin_y,
            layout->content_viewport.right - layout->content_viewport.left,
            layout->content_origin_y + 20000);
        ui_row_text(hdc, &cursor, tr("Status", "状态"),
                    tr("Waiting for a render snapshot.", "正在等待渲染快照。"));
        return cursor.y - layout->content_origin_y + 8;
    }
    if (tab == PLAGUE_PANEL_TAB_IMPACT) {
        UiPlagueImpactLayout impact;
        ui_plague_panel_impact_layout_build(layout, &impact);
        plague_panel_impact_draw(hdc, &impact, snapshot);
        return snapshot->plague_impact.active ? impact.content_height : 118;
    }
    if (tab == PLAGUE_PANEL_TAB_HISTORY) {
        UiPlagueHistoryLayout history;
        ui_plague_panel_history_layout_build(layout, &history);
        plague_panel_history_draw(hdc, &history, snapshot);
        return history.content_height;
    }
    {
        UiCursor cursor = ui_cursor(layout->content_viewport.left,
            layout->content_origin_y,
            layout->content_viewport.right - layout->content_viewport.left,
            layout->content_origin_y + 20000);
        plague_panel_live_draw(hdc, &cursor, snapshot);
        return cursor.y - layout->content_origin_y + 8;
    }
}

int plague_panel_scroll(RECT client, int delta) {
    return ui_plague_panel_scroll(client, side_panel_w, delta);
}

void plague_panel_reset_scroll(void) {
    ui_plague_panel_reset_presentation_state();
}

void draw_plague_panel(HDC hdc, RECT client, int x, HFONT title_font,
                       HFONT body_font) {
    const RenderSnapshot *snapshot = snapshot_ui_current();
    UiPlaguePanelLayout layout;
    UiCursor fixed;
    RECT effect_rect;
    int fog_percent = ui_plague_fog_percent(plague_fog_alpha);
    int content_height;
    int saved_dc;

    pre_sync_content_state(snapshot);
    ui_plague_panel_layout_build(client, side_panel_w, &layout);
    fixed = ui_cursor(x, layout.fog.title_top,
                      side_panel_w - FORM_X_PAD * 2,
                      layout.content_viewport.bottom);
    SelectObject(hdc, title_font);
    draw_text_line(hdc, x, fixed.y, tr("Plague", "瘟疫"),
                   ui_theme_color(UI_COLOR_TEXT));
    fixed.y += 30;
    SelectObject(hdc, body_font);
    ui_section(hdc, &fixed, tr("Map Overlay", "地图叠加"));
    ui_row_int(hdc, &fixed,
               tr("Plague fog strength 0-100", "瘟疫雾强度 0-100"),
               fog_percent);
    effect_rect = layout.fog.effect_help;
    fixed.y = effect_rect.bottom;
    draw_text_rect(hdc, effect_rect,
                   tr("Display only; fog also reflects plague severity and infected scope. Does not change spread or deaths.",
                      "仅影响显示；雾效仍随瘟疫烈度和感染范围变化，不改变传播或死亡。"),
                   ui_theme_color(UI_COLOR_TEXT_MUTED),
                   DT_LEFT | DT_TOP | DT_WORDBREAK);
    draw_setup_slider(hdc, client, UI_SLIDER_PLAGUE_FOG_ALPHA,
                      tr("Plague fog strength", "瘟疫雾强度"), fog_percent);

    plague_panel_probability_draw(hdc, &layout.probability, snapshot);
    draw_main_tabs(hdc, &layout);
    saved_dc = SaveDC(hdc);
    IntersectClipRect(hdc, layout.content_viewport.left,
                      layout.content_viewport.top,
                      layout.content_viewport.right,
                      layout.content_viewport.bottom);
    content_height = draw_tab_content(hdc, &layout, snapshot);
    RestoreDC(hdc, saved_dc);
    ui_plague_panel_set_content_height(ui_plague_panel_main_tab(),
                                       content_height);
}
