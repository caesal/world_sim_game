#include "game/game_presentation_plague_fixture.h"
#include "game/game_presentation_plague_probe.h"

#include "sim/plague_rules.h"
#include "sim/plague_state.h"
#include "ui/ui_clay_widgets.h"
#include "ui/ui_plague_panel_layout.h"
#include "ui/ui_types.h"

#include <stdlib.h>

static int view_count_case(int pushed, int expected_count,
                           int expected_newest, int expected_oldest) {
    PlagueStateView view;
    PlagueModelState *model;
    int i;
    plague_state_reset();
    for (i = 1; i <= pushed; i++) {
        PlagueEpisodeHistory history;
        plague_probe_fill_history(&history, 100 + i, pushed - i);
        plague_state_push_history(&history);
    }
    model = plague_state_mutable();
    model->episode.active = 1;
    model->episode.episode_id = 999;
    plague_state_build_view(2000, &view);
    if (view.recent_history_count != expected_count) return 0;
    if (expected_count <= 0) return 1;
    if (view.recent_history[0].episode_id != expected_newest ||
        view.recent_history[expected_count - 1].episode_id !=
            expected_oldest) return 0;
    for (i = 0; i < view.recent_history_count; i++) {
        if (view.recent_history[i].episode_id == 999) return 0;
    }
    return 1;
}

static int history_ring_case(void) {
    return view_count_case(0, 0, 0, 0) &&
           view_count_case(1, 1, 101, 101) &&
           view_count_case(6, 6, 106, 101) &&
           view_count_case(7, 7, 107, 101) &&
           view_count_case(20, 7, 120, 114);
}

static int chart_math_case(PlagueEpisodeHistory history[7]) {
    int duration_max = plague_panel_chart_duration_max(history, 7);
    int boundaries =
        plague_rules_immunity_percent_for_duration(79) == 30 &&
        plague_rules_immunity_percent_for_duration(80) == 50 &&
        plague_rules_immunity_percent_for_duration(139) == 50 &&
        plague_rules_immunity_percent_for_duration(140) == 80 &&
        plague_rules_immunity_percent_for_duration(199) == 80 &&
        plague_rules_immunity_percent_for_duration(200) == 100;
    return plague_panel_chart_rounded_max(0) == 1 &&
           plague_panel_chart_rounded_max(1) == 1 &&
           plague_panel_chart_rounded_max(3) == 5 &&
           plague_panel_chart_rounded_max(11) == 20 &&
           plague_panel_chart_rounded_max(99) == 100 &&
           plague_panel_chart_duration_max(NULL, 0) == 300 &&
           duration_max == 360 && boundaries &&
           plague_panel_chart_value_height(0, 1000000, 180) == 0 &&
           plague_panel_chart_value_height(1, 1000000, 180) == 1 &&
           plague_panel_chart_value_height(1000000, 1000000, 180) == 180;
}

static int chart_geometry_case(void) {
    int type = plague_panel_chart_type_category_y(
                   PLAGUE_SIZE_SMALL, 0, 180) == 150 &&
               plague_panel_chart_type_category_y(
                   PLAGUE_SIZE_MEDIUM, 0, 180) == 90 &&
               plague_panel_chart_type_category_y(
                   PLAGUE_SIZE_LARGE, 0, 180) == 30;
    int severity = plague_panel_chart_value_height(1, 10, 180) == 18 &&
                   plague_panel_chart_value_height(10, 10, 180) == 180;
    int duration = plague_panel_chart_value_height(80, 300, 180) == 48 &&
                   plague_panel_chart_value_height(140, 300, 180) == 84 &&
                   plague_panel_chart_value_height(200, 300, 180) == 120;
    int linear = plague_panel_chart_value_height(627062, 1000000, 180) == 112 &&
                 plague_panel_chart_value_height(79, 100, 180) == 142 &&
                 plague_panel_chart_value_height(14, 20, 180) == 126;
    int spores = plague_panel_chart_spore_fill_height(180, 137, 158) == 156 &&
                 plague_panel_chart_spore_fill_height(180, 0, 158) == 0 &&
                 plague_panel_chart_spore_fill_height(180, 158, 158) == 180 &&
                 plague_panel_chart_spore_fill_height(180, 0, 0) == 0 &&
                 plague_panel_chart_spore_fill_height(180, 200, 158) == 180;
    return type && severity && duration && linear && spores;
}

static int immunity_projection_case(RenderSnapshot *snapshot) {
    static const int history_percent[7] = {30, 50, 50, 80, 100, 100, 100};
    int live;
    int history = 1;
    int i;
    plague_probe_fill_active(snapshot, PLAGUE_SIZE_MEDIUM, 7);
    live = snapshot->plague_state.projected_immunity_percent == 30 &&
           snapshot->plague_state.next_immunity_percent == 50 &&
           snapshot->plague_state.months_to_next_immunity == 42;
    for (i = 0; i < 7; i++) {
        const PlagueEpisodeHistory *episode =
            &snapshot->plague_state.recent_history[i];
        history &= plague_rules_immunity_percent_for_duration(
                       episode->duration_months) == history_percent[i];
    }
    return live && history;
}

static int metric_icon_geometry_case(void) {
    RECT card = {12, 20, 180, 72};
    RECT icon = ui_clay_metric_icon_rect(card);
    return icon.right - icon.left == 20 && icon.bottom - icon.top == 20 &&
           icon.left == card.left + 7 &&
           icon.top + icon.bottom == card.top + card.bottom;
}

static int all_metric_smoke_case(PlagueEpisodeHistory history[7]) {
    int ok = PLAGUE_HISTORY_METRIC_COUNT == 7 && PLAGUE_CHART_COUNT == 7;
    int i;
    ui_plague_panel_reset_presentation_state();
    for (i = 0; i < PLAGUE_HISTORY_METRIC_COUNT; i++) {
        ui_plague_panel_set_history_metric((PlagueHistoryMetric)i);
        ok &= ui_plague_panel_history_metric() == (PlagueHistoryMetric)i;
        ok &= plague_probe_chart_smoke(history, 7,
                                       (PlaguePanelChartMetric)i);
    }
    return ok && history[6].spores_initial > 0 &&
           history[6].spores_used == 0;
}

static int spore_usage_fixture_case(const PlagueEpisodeHistory history[7]) {
    return history[6].spores_initial > 0 && history[6].spores_used == 0 &&
           history[3].spores_used > 0 &&
           history[3].spores_used < history[3].spores_initial &&
           history[0].spores_initial > 0 &&
           history[0].spores_used == history[0].spores_initial;
}

static int metric_layout_case(void) {
    RECT client = {0, 0, 720, 1040};
    UiPlaguePanelLayout panel;
    UiPlagueHistoryLayout history;
    int i;
    ui_plague_panel_reset_presentation_state();
    ui_plague_panel_set_main_tab(PLAGUE_PANEL_TAB_HISTORY);
    ui_plague_panel_set_content_height(PLAGUE_PANEL_TAB_HISTORY, 520);
    ui_plague_panel_layout_build(client, 460, &panel);
    ui_plague_panel_history_layout_build(&panel, &history);
    for (i = 1; i < 4; i++) {
        if (history.metric_tabs[i].top != history.metric_tabs[0].top ||
            history.metric_tabs[i].left <= history.metric_tabs[i - 1].left) {
            return 0;
        }
    }
    for (i = 5; i < 7; i++) {
        if (history.metric_tabs[i].top != history.metric_tabs[4].top ||
            history.metric_tabs[i].left <= history.metric_tabs[i - 1].left) {
            return 0;
        }
    }
    return history.metric_tabs[4].top > history.metric_tabs[0].bottom &&
           history.chart.top > history.metric_tabs[4].bottom &&
           history.detail_strip.top > history.chart.bottom;
}

static int oldest_to_newest_render_case(RenderSnapshot *snapshot) {
    int artifact;
    int i;
    artifact = plague_probe_render_artifact(
        snapshot, "plague_history_mapping_probe_en.bmp", UI_LANG_EN, 460,
        PLAGUE_PANEL_TAB_HISTORY, PLAGUE_HISTORY_METRIC_TYPE, 0, 0);
    if (!artifact || ui_plague_panel_selected_history_episode_id() != 107) {
        return 0;
    }
    for (i = 0; i < 7; i++) {
        if (ui_plague_panel_history_slot_episode_id(i) != 101 + i) return 0;
    }
    return 1;
}

int game_presentation_plague_history_probe(FILE *summary) {
    PlagueModelState *saved_model = malloc(sizeof(PlagueModelState));
    RenderSnapshot *snapshot = calloc(1, sizeof(RenderSnapshot));
    PlagueEpisodeHistory history[7];
    int ring = 0;
    int metrics = 0;
    int math = 0;
    int geometry = 0;
    int metric_icon = 0;
    int spore_usage = 0;
    int layout = 0;
    int mapping = 0;
    int immunity_projection = 0;
    int restored = 0;
    int i;
    int ok;
    if (!saved_model || !snapshot) {
        free(saved_model);
        free(snapshot);
        fprintf(summary, "case=plague_history_chart ok=0 allocation=0\n");
        return 0;
    }
    plague_state_copy(saved_model);
    for (i = 0; i < 7; i++) {
        plague_probe_fill_history(&history[i], 107 - i, i);
    }
    ring = history_ring_case();
    math = chart_math_case(history);
    geometry = chart_geometry_case();
    metric_icon = metric_icon_geometry_case();
    spore_usage = spore_usage_fixture_case(history);
    metrics = all_metric_smoke_case(history);
    layout = metric_layout_case();
    immunity_projection = immunity_projection_case(snapshot);
    plague_probe_fill_inactive(snapshot, 7);
    mapping = oldest_to_newest_render_case(snapshot);
    restored = plague_state_restore(saved_model);
    ok = ring && math && geometry && metric_icon && spore_usage && metrics &&
         layout && immunity_projection && mapping && restored;
    fprintf(summary,
            "case=plague_history_chart ok=%d histories_0_1_6_7=%d persistent_newest7=%d active_excluded=%d oldest_to_newest=%d metrics7=%d metric_layout_4_plus_3=%d type_categories=%d severity_1_10=%d duration_axis_min300=%d duration_bands_80_140_200=%d immunity_live_history_projection=%d exact_linear_values=%d spore_capacity_fill=%d spore_0_partial_100=%d zero_spores_safe=%d minimum_nonzero_height=%d metric_icon_20x20=%d restored=%d\n",
            ok, ring, ring, ring, mapping, metrics, layout, geometry,
            geometry, math, math, immunity_projection, geometry, geometry,
            spore_usage, geometry, math, metric_icon, restored);
    free(saved_model);
    free(snapshot);
    return ok;
}
