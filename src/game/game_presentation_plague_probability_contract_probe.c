#include "game/game_presentation_plague_probability_contract_probe.h"

#include "render/panel_view_model_cache_keys.h"
#include "sim/plague_probability.h"
#include "ui/ui_clay_widgets.h"
#include "ui/ui_plague_panel_layout.h"
#include "ui/ui_plague_probability.h"
#include "ui/ui_types.h"

#include <string.h>

static int center_x(RECT rect) { return (rect.left + rect.right) / 2; }
static int center_y(RECT rect) { return (rect.top + rect.bottom) / 2; }

static int rect_equal(RECT a, RECT b) {
    return a.left == b.left && a.top == b.top &&
           a.right == b.right && a.bottom == b.bottom;
}

static int rect_contains(RECT outer, RECT inner) {
    return inner.left >= outer.left && inner.top >= outer.top &&
           inner.right <= outer.right && inner.bottom <= outer.bottom;
}

static int probability_geometry_case(int panel_width) {
    RECT client = {0, 0, panel_width + 240, 1040};
    UiPlaguePanelLayout panel;
    UiPlagueImpactLayout impact;
    int helper_height;
    int i;
    int ok = 1;
    ui_plague_panel_reset_presentation_state();
    ui_plague_panel_layout_build(client, panel_width, &panel);
    ui_plague_panel_impact_layout_build(&panel, &impact);
    helper_height = panel.fog.effect_help.bottom -
                    panel.fog.effect_help.top;
    ok &= helper_height >= 36;
    ok &= panel.fog.effect_help.bottom == panel.fog.effect_bottom;
    ok &= panel.fog.effect_help.bottom < panel.fog.slider.label.top;
    ok &= panel.fog.effect_help.left == panel.fog.slider.track.left;
    ok &= panel.fog.effect_help.right == panel.fog.slider.track.right;
    ok &= panel.fog.slider.track.bottom <= panel.fog.slider.help.top;
    ok &= panel.fog.slider.help.bottom <= panel.fog.slider.hit.bottom;
    ok &= panel.fog.slider.hit.bottom < panel.probability.bounds.top;
    ok &= panel.probability.bounds.top == panel.fog.content_top;
    ok &= panel.probability.bottom == panel.main_tabs[0].top;
    ok &= panel.main_tabs[0].bottom < panel.content_viewport.top;
    ok &= impact.previous.right - impact.previous.left == 30;
    ok &= impact.previous.bottom - impact.previous.top == 30;
    ok &= impact.next.right - impact.next.left == 30;
    ok &= impact.next.bottom - impact.next.top == 30;
    ok &= impact.page_label.left == impact.previous.right + 4;
    ok &= impact.page_label.right == impact.next.left - 4;
    for (i = 0; i < PLAGUE_PROBABILITY_COUNT; i++) {
        const UiPlagueProbabilityControlLayout *control =
            &panel.probability.controls[i];
        ok &= control->label.bottom <= control->track.top;
        ok &= rect_contains(control->hit, control->track);
        ok &= ui_plague_probability_hit_test(&panel.probability,
            center_x(control->track), center_y(control->track)) ==
                UI_PLAGUE_PANEL_HIT_PROBABILITY_BASE + i;
    }
    ok &= panel.probability.controls[0].label.top ==
          panel.probability.controls[1].label.top;
    ok &= panel.probability.controls[2].label.top ==
          panel.probability.controls[3].label.top;
    ok &= panel.probability.controls[2].label.top >
          panel.probability.controls[0].hit.bottom;
    ok &= ui_plague_probability_hit_test(&panel.probability,
        center_x(panel.probability.apply_button),
        center_y(panel.probability.apply_button)) ==
            UI_PLAGUE_PANEL_HIT_PROBABILITY_APPLY;
    ok &= ui_plague_probability_hit_test(&panel.probability,
        center_x(panel.probability.reset_button),
        center_y(panel.probability.reset_button)) ==
            UI_PLAGUE_PANEL_HIT_PROBABILITY_RESET;
    return ok;
}

static int same_fixed_layout(const UiPlaguePanelLayout *a,
                             const UiPlaguePanelLayout *b) {
    int i;
    if (!rect_equal(a->fog.effect_help, b->fog.effect_help) ||
        !rect_equal(a->fog.slider.hit, b->fog.slider.hit) ||
        !rect_equal(a->probability.bounds, b->probability.bounds) ||
        !rect_equal(a->probability.apply_button,
                    b->probability.apply_button) ||
        !rect_equal(a->probability.reset_button,
                    b->probability.reset_button)) return 0;
    for (i = 0; i < PLAGUE_PANEL_TAB_COUNT; i++) {
        if (!rect_equal(a->main_tabs[i], b->main_tabs[i])) return 0;
    }
    return rect_equal(a->content_viewport, b->content_viewport);
}

static int fixed_section_case(void) {
    RECT client = {0, 0, 700, 1040};
    UiPlaguePanelLayout baseline;
    UiPlaguePanelLayout current;
    int tab;
    ui_plague_panel_reset_presentation_state();
    for (tab = 0; tab < PLAGUE_PANEL_TAB_COUNT; tab++) {
        ui_plague_panel_set_content_height((PlaguePanelTab)tab, 2000);
    }
    ui_plague_panel_set_main_tab(PLAGUE_PANEL_TAB_LIVE);
    ui_plague_panel_layout_build(client, 460, &baseline);
    ui_plague_panel_scroll(client, 460, 72);
    ui_plague_panel_layout_build(client, 460, &baseline);
    for (tab = PLAGUE_PANEL_TAB_IMPACT; tab < PLAGUE_PANEL_TAB_COUNT; tab++) {
        ui_plague_panel_set_main_tab((PlaguePanelTab)tab);
        ui_plague_panel_layout_build(client, 460, &current);
        ui_plague_panel_scroll(client, 460, 72 * (tab + 1));
        ui_plague_panel_layout_build(client, 460, &current);
        if (!same_fixed_layout(&baseline, &current)) return 0;
    }
    return baseline.scroll_offset == 72 && current.scroll_offset == 216;
}

static int draft_lifecycle_case(void) {
    PlagueStateView view;
    PlagueProbabilityDistribution defaults;
    PlagueProbabilityDistribution custom = {20, 20, 35, 25};
    PlagueProbabilityDistribution draft;
    int ok = 1;
    memset(&view, 0, sizeof(view));
    plague_probability_defaults(&defaults);
    view.effective_probabilities = defaults;
    view.pending_probabilities = defaults;
    ui_plague_probability_reset_presentation_state();
    ui_plague_probability_sync(&view);
    ui_plague_probability_get_draft(&draft);
    ok &= plague_probability_equal(&draft, &defaults);
    ok &= !ui_plague_probability_apply_enabled();
    ok &= !ui_plague_probability_reset_enabled();
    ui_plague_probability_reset_presentation_state();
    view.effective_probabilities = custom;
    view.pending_probabilities = custom;
    ui_plague_probability_sync(&view);
    ui_plague_probability_get_draft(&draft);
    ok &= plague_probability_equal(&draft, &custom);
    ok &= !ui_plague_probability_apply_enabled();
    ok &= ui_plague_probability_reset_enabled();
    ui_plague_probability_reset_presentation_state();
    view.episode.active = 1;
    view.effective_probabilities = defaults;
    view.pending_probabilities = custom;
    view.pending_probabilities_valid = 1;
    ui_plague_probability_sync(&view);
    ui_plague_probability_get_draft(&draft);
    ok &= plague_probability_equal(&draft, &custom);
    ui_plague_probability_adjust_draft(PLAGUE_PROBABILITY_LARGE, 60);
    ui_plague_probability_panel_closed();
    ui_plague_probability_sync(&view);
    ui_plague_probability_get_draft(&draft);
    return ok && plague_probability_equal(&draft, &custom);
}

static int release_outside_case(void) {
    RECT client = {0, 0, 700, 1040};
    UiPlaguePanelLayout panel;
    PlagueStateView view;
    PlagueProbabilityDistribution defaults;
    PlagueProbabilityDistribution before;
    PlagueProbabilityDistribution after;
    int apply_hit = UI_PLAGUE_PANEL_HIT_PROBABILITY_APPLY;
    int ok = 1;
    memset(&view, 0, sizeof(view));
    plague_probability_defaults(&defaults);
    view.effective_probabilities = defaults;
    view.pending_probabilities = defaults;
    ui_plague_panel_reset_presentation_state();
    ui_plague_probability_sync(&view);
    ui_plague_probability_adjust_draft(PLAGUE_PROBABILITY_SMALL, 40);
    ui_plague_panel_layout_build(client, 460, &panel);
    ui_plague_probability_get_draft(&before);
    ok &= ui_plague_probability_mouse_down(NULL, &panel.probability,
        center_x(panel.probability.apply_button),
        center_y(panel.probability.apply_button));
    ok &= ui_plague_probability_pressed(apply_hit);
    ok &= ui_plague_probability_mouse_up(NULL, &panel.probability,
        panel.probability.bounds.left - 20,
        panel.probability.bounds.bottom + 20);
    ui_plague_probability_get_draft(&after);
    ok &= plague_probability_equal(&before, &after);
    ok &= !ui_plague_probability_pressed(apply_hit);
    ui_plague_probability_reset_presentation_state();
    ui_plague_probability_sync(&view);
    ui_plague_panel_layout_build(client, 460, &panel);
    ok &= ui_plague_probability_mouse_down(NULL, &panel.probability,
        center_x(panel.probability.controls[PLAGUE_PROBABILITY_SMALL].track),
        center_y(panel.probability.controls[PLAGUE_PROBABILITY_SMALL].track));
    ok &= ui_plague_probability_mouse_move(NULL, &panel.probability,
        panel.probability.bounds.right + 200,
        panel.probability.bounds.bottom + 200);
    ok &= ui_plague_probability_mouse_up(NULL, &panel.probability,
        panel.probability.bounds.right + 200,
        panel.probability.bounds.bottom + 200);
    ui_plague_probability_get_draft(&after);
    return ok && after.no_plague == 0 && after.small == 100 &&
           after.medium == 0 && after.large == 0 &&
           plague_probability_validate(&after) &&
           !ui_plague_probability_pressed(
               UI_PLAGUE_PANEL_HIT_PROBABILITY_BASE +
               PLAGUE_PROBABILITY_SMALL);
}

static int cache_scope_case(void) {
    RECT client = {0, 0, 700, 1040};
    PlagueStateView view;
    PlagueProbabilityDistribution defaults;
    unsigned int plague_before;
    unsigned int plague_after;
    unsigned int country_before;
    unsigned int country_after;
    unsigned int stable;
    memset(&view, 0, sizeof(view));
    plague_probability_defaults(&defaults);
    view.effective_probabilities = defaults;
    view.pending_probabilities = defaults;
    ui_plague_panel_reset_presentation_state();
    ui_plague_probability_sync(&view);
    plague_before = panel_view_model_cache_ui_key(client, PANEL_CACHE_PLAGUE);
    country_before = panel_view_model_cache_ui_key(
        client, PANEL_CACHE_COUNTRY_DETAIL);
    ui_plague_probability_adjust_draft(PLAGUE_PROBABILITY_SMALL, 40);
    plague_after = panel_view_model_cache_ui_key(client, PANEL_CACHE_PLAGUE);
    country_after = panel_view_model_cache_ui_key(
        client, PANEL_CACHE_COUNTRY_DETAIL);
    stable = panel_view_model_cache_ui_key(client, PANEL_CACHE_PLAGUE);
    ui_plague_probability_sync(&view);
    return plague_after != plague_before && country_after == country_before &&
           panel_view_model_cache_ui_key(client, PANEL_CACHE_PLAGUE) == stable;
}

static int metric_icon_case(void) {
    RECT cards[3] = {{8, 10, 180, 52}, {8, 10, 220, 62},
                     {8, 10, 260, 80}};
    int i;
    for (i = 0; i < 3; i++) {
        RECT icon = ui_clay_metric_icon_rect(cards[i]);
        if (icon.right - icon.left != 20 ||
            icon.bottom - icon.top != 20 ||
            icon.top + icon.bottom != cards[i].top + cards[i].bottom) {
            return 0;
        }
    }
    return 1;
}

int game_presentation_plague_probability_contract_probe(FILE *summary) {
    int old_width = side_panel_w;
    int old_collapsed = side_panel_collapsed;
    int old_panel = panel_tab;
    int geometry_340;
    int geometry_460;
    int fixed;
    int lifecycle;
    int outside;
    int cache;
    int icons;
    int ok;
    side_panel_w = 460;
    side_panel_collapsed = 0;
    panel_tab = PANEL_PLAGUE;
    geometry_340 = probability_geometry_case(340);
    geometry_460 = probability_geometry_case(460);
    fixed = fixed_section_case();
    lifecycle = draft_lifecycle_case();
    outside = release_outside_case();
    cache = cache_scope_case();
    icons = metric_icon_case();
    ok = geometry_340 && geometry_460 && fixed && lifecycle && outside &&
         cache && icons;
    fprintf(summary,
            "case=plague_probability_contract ok=%d geometry_340=%d geometry_460=%d fog_helper_multiline=%d fog_probability_nonoverlap=%d grid_2x2=%d hit_rects_match=%d fixed_live_impact_history=%d pager_30x30=%d effective_pending_draft=%d drag_release_outside=%d cache_plague_only=%d metric_icons_20x20=%d\n",
            ok, geometry_340, geometry_460, geometry_340 && geometry_460,
            geometry_340 && geometry_460, geometry_340 && geometry_460,
            geometry_340 && geometry_460, fixed,
            geometry_340 && geometry_460, lifecycle, outside, cache, icons);
    side_panel_w = old_width;
    side_panel_collapsed = old_collapsed;
    panel_tab = old_panel;
    ui_plague_panel_reset_presentation_state();
    return ok;
}
