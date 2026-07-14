#include "game/game_presentation_plague_probability_artifact_probe.h"

#include "game/game_presentation_plague_fixture.h"
#include "io/map_save_plague.h"
#include "sim/plague.h"
#include "sim/plague_probability.h"
#include "sim/plague_state.h"
#include "ui/ui_plague_probability.h"
#include "ui/ui_types.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int language;
    int width;
    const char *suffix;
} ArtifactVariant;

typedef struct {
    PlagueProbabilityBucket bucket;
    int value;
} DraftPreparation;

static const ArtifactVariant variants[] = {
    {UI_LANG_EN, 340, "en_340"},
    {UI_LANG_EN, 460, "en_460"},
    {UI_LANG_ZH, 340, "zh_340"},
    {UI_LANG_ZH, 460, "zh_460"}
};

static void set_effective(RenderSnapshot *snapshot,
                          PlagueProbabilityDistribution probabilities) {
    snapshot->plague_state.effective_probabilities = probabilities;
    snapshot->plague_state.pending_probabilities = probabilities;
    snapshot->plague_state.pending_probabilities_valid = 0;
}

static void prepare_dirty_draft(const RenderSnapshot *snapshot,
                                void *context) {
    const DraftPreparation *preparation = context;
    ui_plague_probability_sync(&snapshot->plague_state);
    if (preparation) {
        ui_plague_probability_adjust_draft(preparation->bucket,
                                            preparation->value);
    }
}

static void prepare_country_metrics(const RenderSnapshot *snapshot,
                                    void *context) {
    (void)snapshot;
    (void)context;
    selected_civ = 0;
    country_show_fallen = 0;
    country_detail_subtab = COUNTRY_DETAIL_OVERVIEW;
    country_detail_scroll_offset = 0;
    country_detail_scroll_offsets[COUNTRY_DETAIL_OVERVIEW] = 0;
}

static int prepared_matrix(RenderSnapshot *snapshot, const char *stem,
                           PlaguePanelTab tab, PlagueHistoryMetric metric,
                           int hover, UiPressedControlKind pressed_kind,
                           int pressed_index, PlagueProbeUiPrepareFn prepare,
                           void *context) {
    char file_name[160];
    int ok = 1;
    int i;
    for (i = 0; i < (int)(sizeof(variants) / sizeof(variants[0])); i++) {
        snprintf(file_name, sizeof(file_name), "%s_%s.bmp", stem,
                 variants[i].suffix);
        ok &= plague_probe_render_prepared_artifact(
            snapshot, file_name, variants[i].language, variants[i].width,
            tab, metric, hover, pressed_kind, pressed_index,
            prepare, context);
    }
    return ok;
}

static int plain_matrix(RenderSnapshot *snapshot, const char *stem,
                        PlaguePanelTab tab, PlagueHistoryMetric metric,
                        int impact_page) {
    char file_name[160];
    int ok = 1;
    int i;
    for (i = 0; i < (int)(sizeof(variants) / sizeof(variants[0])); i++) {
        snprintf(file_name, sizeof(file_name), "%s_%s.bmp", stem,
                 variants[i].suffix);
        ok &= plague_probe_render_artifact(snapshot, file_name,
            variants[i].language, variants[i].width, tab, metric,
            impact_page, 0);
    }
    return ok;
}

static int outer_panel_matrix(RenderSnapshot *snapshot, const char *stem,
                              PanelTab outer_panel,
                              PlagueProbeUiPrepareFn prepare) {
    char file_name[160];
    int ok = 1;
    int i;
    for (i = 0; i < (int)(sizeof(variants) / sizeof(variants[0])); i++) {
        snprintf(file_name, sizeof(file_name), "%s_%s.bmp", stem,
                 variants[i].suffix);
        ok &= plague_probe_render_outer_panel_artifact(
            snapshot, file_name, variants[i].language, variants[i].width,
            outer_panel, prepare, NULL);
    }
    return ok;
}

static void fill_metric_callers(RenderSnapshot *snapshot) {
    int i;
    snapshot->civ_alive_count = snapshot->civ_count;
    snapshot->civ_independent_alive_count = snapshot->civ_count;
    for (i = 0; i < snapshot->civ_count; i++) {
        PopulationSummary *population =
            &snapshot->civs[i].population_summary;
        population->total = 120000 + i * 45000;
        population->male = population->total * 49 / 100;
        population->female = population->total - population->male;
        population->children = population->total * 22 / 100;
        population->working = population->total * 63 / 100;
        population->elder = population->total - population->children -
                            population->working;
        population->fertile = population->total * 28 / 100;
        population->recruitable = population->total * 18 / 100;
        population->carrying_capacity = population->total * 3 / 2;
        population->pressure = 67 + i * 8;
        snapshot->civs[i].summary.population = population->total;
    }
}

static int load_active_pending_roundtrip(
    RenderSnapshot *snapshot,
    const PlagueProbabilityDistribution *pending) {
    PlagueModelState *saved = malloc(sizeof(*saved));
    int (*legacy)[3] = malloc(sizeof(*legacy) * MAX_CIVS);
    PlagueEpisodeState episode = snapshot->plague_state.episode;
    PlagueStateView loaded;
    FILE *file = NULL;
    int write_ok = 0;
    int read_ok = 0;
    int restored = 0;
    int ok = 0;
    int i;
    if (!saved || !legacy || !pending) {
        free(saved);
        free(legacy);
        return 0;
    }
    for (i = 0; i < MAX_CIVS; i++) {
        legacy[i][0] = civs[i].plague_random_immunity_months;
        legacy[i][1] = civs[i].plague_was_active_last_month;
        legacy[i][2] = civs[i].plague_recovery_months;
    }
    plague_state_copy(saved);
    plague_state_reset();
    if (plague_state_begin_episode(&episode) &&
        plague_state_apply_probabilities(pending)) {
        file = tmpfile();
    }
    if (file) {
        write_ok = map_save_plague_write(file);
        plague_state_reset();
        rewind(file);
        read_ok = map_save_plague_read(file);
        fclose(file);
    }
    if (write_ok && read_ok) {
        plague_state_build_view(snapshot->year * 12 + snapshot->month - 1,
                                &loaded);
        ok = MAP_SAVE_PLAGUE_BLOCK_VERSION == 2 && loaded.episode.active &&
             loaded.pending_probabilities_valid &&
             plague_probability_equal(&loaded.pending_probabilities,
                                      pending);
        if (ok) snapshot->plague_state = loaded;
    }
    restored = plague_state_restore(saved);
    if (restored) plague_after_restore();
    for (i = 0; i < MAX_CIVS; i++) {
        civs[i].plague_random_immunity_months = legacy[i][0];
        civs[i].plague_was_active_last_month = legacy[i][1];
        civs[i].plague_recovery_months = legacy[i][2];
    }
    free(saved);
    free(legacy);
    return ok && restored;
}

static int artifact_matrix(RenderSnapshot *snapshot, int *out_identity,
                           int *out_spores, int *out_roundtrip) {
    PlagueProbabilityDistribution defaults;
    PlagueProbabilityDistribution custom = {20, 20, 35, 25};
    PlagueProbabilityDistribution pending = {5, 45, 30, 20};
    DraftPreparation dirty = {PLAGUE_PROBABILITY_SMALL, 40};
    DraftPreparation endpoint = {PLAGUE_PROBABILITY_SMALL, 100};
    int apply = UI_PLAGUE_PANEL_HIT_PROBABILITY_APPLY;
    int reset = UI_PLAGUE_PANEL_HIT_PROBABILITY_RESET;
    int slider = UI_PLAGUE_PANEL_HIT_PROBABILITY_BASE +
                 PLAGUE_PROBABILITY_SMALL;
    int ok = 1;
    plague_probability_defaults(&defaults);

    plague_probe_fill_inactive(snapshot, 7);
    set_effective(snapshot, defaults);
    ok &= prepared_matrix(snapshot, "plague_probability_default",
        PLAGUE_PANEL_TAB_LIVE, PLAGUE_HISTORY_METRIC_TYPE,
        UI_PLAGUE_PANEL_HIT_NONE, UI_PRESSED_NONE, -1, NULL, NULL);
    ok &= prepared_matrix(snapshot, "plague_probability_apply_hover",
        PLAGUE_PANEL_TAB_LIVE, PLAGUE_HISTORY_METRIC_TYPE,
        apply, UI_PRESSED_NONE, -1, prepare_dirty_draft, &dirty);
    ok &= prepared_matrix(snapshot, "plague_probability_apply_pressed",
        PLAGUE_PANEL_TAB_LIVE, PLAGUE_HISTORY_METRIC_TYPE,
        apply, UI_PRESSED_PLAGUE_PROBABILITY, apply,
        prepare_dirty_draft, &dirty);
    ok &= prepared_matrix(snapshot, "plague_probability_apply_disabled",
        PLAGUE_PANEL_TAB_LIVE, PLAGUE_HISTORY_METRIC_TYPE,
        apply, UI_PRESSED_PLAGUE_PROBABILITY, apply, NULL, NULL);
    ok &= prepared_matrix(snapshot, "plague_probability_reset_disabled",
        PLAGUE_PANEL_TAB_LIVE, PLAGUE_HISTORY_METRIC_TYPE,
        reset, UI_PRESSED_PLAGUE_PROBABILITY, reset, NULL, NULL);
    ok &= prepared_matrix(snapshot, "plague_probability_slider_drag_endpoint",
        PLAGUE_PANEL_TAB_LIVE, PLAGUE_HISTORY_METRIC_TYPE,
        slider, UI_PRESSED_PLAGUE_PROBABILITY, slider,
        prepare_dirty_draft, &endpoint);
    ok &= prepared_matrix(snapshot, "plague_probability_slider_release_outside",
        PLAGUE_PANEL_TAB_LIVE, PLAGUE_HISTORY_METRIC_TYPE,
        UI_PLAGUE_PANEL_HIT_NONE, UI_PRESSED_NONE, -1,
        prepare_dirty_draft, &endpoint);

    plague_probe_fill_inactive(snapshot, 7);
    set_effective(snapshot, custom);
    ok &= plain_matrix(snapshot, "plague_probability_inactive_effective",
        PLAGUE_PANEL_TAB_LIVE, PLAGUE_HISTORY_METRIC_TYPE, 0);
    ok &= prepared_matrix(snapshot, "plague_probability_reset_hover",
        PLAGUE_PANEL_TAB_LIVE, PLAGUE_HISTORY_METRIC_TYPE,
        reset, UI_PRESSED_NONE, -1, NULL, NULL);
    ok &= prepared_matrix(snapshot, "plague_probability_reset_pressed",
        PLAGUE_PANEL_TAB_LIVE, PLAGUE_HISTORY_METRIC_TYPE,
        reset, UI_PRESSED_PLAGUE_PROBABILITY, reset, NULL, NULL);

    plague_probe_fill_active(snapshot, PLAGUE_SIZE_LARGE, 7);
    plague_probe_fill_impact(snapshot, 13, 5);
    *out_roundtrip = load_active_pending_roundtrip(snapshot, &pending);
    *out_identity =
        GetRValue(snapshot->plague_impact.countries[0].color) < 64 &&
        GetRValue(snapshot->plague_impact.countries[1].color) > 200 &&
        snapshot->plague_impact.countries[0].status ==
            SNAPSHOT_PLAGUE_IMPACT_RECOVERED &&
        snapshot->plague_impact.countries[1].status ==
            SNAPSHOT_PLAGUE_IMPACT_ACTIVE &&
        snapshot->plague_impact.countries[2].status ==
            SNAPSHOT_PLAGUE_IMPACT_NO_LONGER_EXISTS &&
        !snapshot->plague_impact.countries[2].alive;
    ok &= plain_matrix(snapshot, "plague_probability_active_pending_live",
        PLAGUE_PANEL_TAB_LIVE, PLAGUE_HISTORY_METRIC_TYPE, 0);
    ok &= plain_matrix(snapshot, "plague_probability_fixed_impact",
        PLAGUE_PANEL_TAB_IMPACT, PLAGUE_HISTORY_METRIC_TYPE, 0);
    ok &= plain_matrix(snapshot, "plague_probability_fixed_history",
        PLAGUE_PANEL_TAB_HISTORY, PLAGUE_HISTORY_METRIC_SPORES, 0);
    ok &= plain_matrix(snapshot, "plague_impact_country_status_blocks",
        PLAGUE_PANEL_TAB_IMPACT, PLAGUE_HISTORY_METRIC_TYPE, 0);
    ok &= plain_matrix(snapshot, "plague_impact_pager_multi_page",
        PLAGUE_PANEL_TAB_IMPACT, PLAGUE_HISTORY_METRIC_TYPE, 1);
    plague_probe_fill_impact(snapshot, 4, 5);
    ok &= plain_matrix(snapshot, "plague_impact_pager_one_page",
        PLAGUE_PANEL_TAB_IMPACT, PLAGUE_HISTORY_METRIC_TYPE, 0);

    plague_probe_fill_inactive(snapshot, 7);
    set_effective(snapshot, custom);
    *out_spores = snapshot->plague_state.recent_history_count == 7 &&
        snapshot->plague_state.recent_history[0].spores_initial > 0 &&
        snapshot->plague_state.recent_history[0].spores_used ==
            snapshot->plague_state.recent_history[0].spores_initial &&
        snapshot->plague_state.recent_history[3].spores_used > 0 &&
        snapshot->plague_state.recent_history[3].spores_used <
            snapshot->plague_state.recent_history[3].spores_initial &&
        snapshot->plague_state.recent_history[6].spores_initial > 0 &&
        snapshot->plague_state.recent_history[6].spores_used == 0;
    ok &= plain_matrix(snapshot, "plague_spores_0_partial_100",
        PLAGUE_PANEL_TAB_HISTORY, PLAGUE_HISTORY_METRIC_SPORES, 0);
    ok &= plain_matrix(snapshot, "plague_spores_completed_live",
        PLAGUE_PANEL_TAB_LIVE, PLAGUE_HISTORY_METRIC_TYPE, 0);
    fill_metric_callers(snapshot);
    ok &= outer_panel_matrix(snapshot, "metric_icons_country_caller",
                             PANEL_COUNTRY, prepare_country_metrics);
    ok &= outer_panel_matrix(snapshot, "metric_icons_population_caller",
                             PANEL_POPULATION, NULL);
    return ok;
}

int game_presentation_plague_probability_artifact_probe(FILE *summary) {
    RenderSnapshot *snapshot = calloc(1, sizeof(RenderSnapshot));
    int old_selected = selected_civ;
    int old_subtab = country_detail_subtab;
    int old_scroll = country_detail_scroll_offset;
    int old_overview_scroll =
        country_detail_scroll_offsets[COUNTRY_DETAIL_OVERVIEW];
    int old_show_fallen = country_show_fallen;
    int old_display = display_mode;
    int identity = 0;
    int spores = 0;
    int roundtrip = 0;
    int artifacts;
    int ok;
    if (!snapshot) {
        fprintf(summary,
                "case=plague_probability_artifacts ok=0 allocation=0\n");
        return 0;
    }
    display_mode = DISPLAY_POLITICAL;
    artifacts = artifact_matrix(snapshot, &identity, &spores, &roundtrip);
    ok = artifacts && identity && spores && roundtrip;
    fprintf(summary,
            "case=plague_probability_artifacts ok=%d artifacts_ok=%d artifacts=80 matrices=20 languages=2 widths=340_460 active_pending_plg19_v2_roundtrip=%d active_recovered_no_longer_exists=%d country_colors_dark_light=%d spores_true_0_partial_100=%d country_metric_callers=4 population_metric_callers=4\n",
            ok, artifacts, roundtrip, identity, identity, spores);
    selected_civ = old_selected;
    country_detail_subtab = old_subtab;
    country_detail_scroll_offset = old_scroll;
    country_detail_scroll_offsets[COUNTRY_DETAIL_OVERVIEW] =
        old_overview_scroll;
    country_show_fallen = old_show_fallen;
    display_mode = old_display;
    free(snapshot);
    return ok;
}
