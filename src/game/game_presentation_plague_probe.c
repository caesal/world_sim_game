#include "game/game_presentation_plague_fixture.h"
#include "game/game_presentation_plague_probe.h"

#include "render/panel_plague_live.h"
#include "sim/plague_rules.h"
#include "ui/ui_plague_fog.h"
#include "ui/ui_types.h"

#include <limits.h>
#include <stdlib.h>

static int fog_control_contract_case(FILE *summary) {
    RECT client = {0, 0, 900, 1040};
    PlaguePanelLayout layout;
    uint32_t scaled_80;
    uint32_t scaled_100;
    uint32_t scaled_120;
    uint32_t saturated_120;
    int monotonic = 1;
    int previous = -1;
    int percent;
    int mapping;
    int geometry;
    int scaling;
    ui_plague_fog_layout_build(client, 460, &layout);
    for (percent = 0; percent <= 100; percent++) {
        int strength = ui_plague_fog_effective_strength(percent);
        if (strength < previous) monotonic = 0;
        previous = strength;
    }
    mapping = PLAGUE_FOG_DEFAULT_PERCENT == 50 &&
              plague_fog_alpha == PLAGUE_FOG_DEFAULT_PERCENT &&
              ui_plague_fog_effective_strength(0) == 0 &&
              ui_plague_fog_effective_strength(50) == 80 &&
              ui_plague_fog_effective_strength(100) == 120 &&
              ui_plague_fog_effective_strength(-1) == 0 &&
              ui_plague_fog_effective_strength(101) == 120 &&
              ui_plague_fog_effective_strength(INT_MAX) == 120 && monotonic;
    geometry = layout.effect_help.bottom == layout.effect_bottom &&
               layout.effect_help.bottom - layout.effect_help.top >= 36 &&
               layout.effect_help.left == layout.slider.track.left &&
               layout.effect_help.right == layout.slider.track.right &&
               layout.effect_help.bottom < layout.slider.label.top &&
               layout.slider.label.top == layout.effect_bottom + 9 &&
               layout.slider.value.top == layout.slider.label.top &&
               layout.slider.track.top == layout.slider.label.top + 24 &&
               layout.slider.track.bottom <= layout.slider.help.top &&
               layout.slider.help.bottom <= layout.slider.hit.bottom &&
               layout.slider.hit.top == layout.slider.label.top - 4 &&
               layout.content_top == layout.slider.hit.bottom + 18;
    scaled_80 = ui_plague_fog_scale_premultiplied(0x644b3219u, 80);
    scaled_100 = ui_plague_fog_scale_premultiplied(0x644b3219u, 100);
    scaled_120 = ui_plague_fog_scale_premultiplied(0x644b3219u, 120);
    saturated_120 = ui_plague_fog_scale_premultiplied(0xf0c8a064u, 120);
    scaling = ui_plague_fog_scale_premultiplied(0x644b3219u, 0) == 0 &&
              scaled_80 == 0x503c2814u && scaled_100 == 0x644b3219u &&
              scaled_120 == 0x785a3c1eu && scaled_120 > scaled_100 &&
              saturated_120 == 0xfff0c078u;
    fprintf(summary,
            "case=plague_fog_control_contract ok=%d default=%d mapping=0/80/120 above100=%d monotonic=%d slider_top=%d effect_bottom=%d content_top=%d premultiplied_scale=%d\n",
            mapping && geometry && scaling, PLAGUE_FOG_DEFAULT_PERCENT,
            ui_plague_fog_effective_strength(INT_MAX), monotonic,
            (int)layout.slider.label.top, layout.effect_bottom,
            layout.content_top, scaling);
    return mapping && geometry && scaling;
}

static int live_source_order_case(RenderSnapshot *snapshot) {
    int empty;
    int completed;
    int active;
    plague_probe_fill_inactive(snapshot, 0);
    empty = plague_panel_live_mode(snapshot) == PLAGUE_PANEL_LIVE_EMPTY;
    plague_probe_fill_inactive(snapshot, 7);
    completed = plague_panel_live_mode(snapshot) ==
                PLAGUE_PANEL_LIVE_COMPLETED;
    plague_probe_fill_active(snapshot, PLAGUE_SIZE_MEDIUM, 7);
    active = plague_panel_live_mode(snapshot) == PLAGUE_PANEL_LIVE_ACTIVE;
    snapshot->plague_state.episode.active = 0;
    completed &= plague_panel_live_mode(snapshot) ==
                 PLAGUE_PANEL_LIVE_COMPLETED;
    return empty && completed && active;
}

static int live_artifacts(RenderSnapshot *snapshot) {
    static const PlagueSize sizes[3] = {
        PLAGUE_SIZE_SMALL, PLAGUE_SIZE_MEDIUM, PLAGUE_SIZE_LARGE
    };
    static const char *files_en[3] = {
        "plague_live_small_en_wide.bmp",
        "plague_live_medium_en_wide.bmp",
        "plague_live_large_en_wide.bmp"
    };
    static const char *files_zh[3] = {
        "plague_live_small_zh_narrow.bmp",
        "plague_live_medium_zh_narrow.bmp",
        "plague_live_large_zh_narrow.bmp"
    };
    int ok = 1;
    int i;
    for (i = 0; i < 3; i++) {
        plague_probe_fill_active(snapshot, sizes[i], 7);
        plague_probe_fill_impact(snapshot, 13, 5);
        ok &= plague_probe_render_artifact(
            snapshot, files_en[i], UI_LANG_EN, 460, PLAGUE_PANEL_TAB_LIVE,
            PLAGUE_HISTORY_METRIC_TYPE, 0, 0);
        ok &= plague_probe_render_artifact(
            snapshot, files_zh[i], UI_LANG_ZH, 340, PLAGUE_PANEL_TAB_LIVE,
            PLAGUE_HISTORY_METRIC_TYPE, 0, 0);
    }
    plague_probe_fill_active(snapshot, PLAGUE_SIZE_LARGE, 7);
    plague_probe_fill_impact(snapshot, 13, 5);
    ok &= plague_probe_render_artifact(
        snapshot, "plague_live_large_bottom_en_wide.bmp", UI_LANG_EN, 460,
        PLAGUE_PANEL_TAB_LIVE, PLAGUE_HISTORY_METRIC_TYPE, 0, 10000);
    ok &= plague_probe_render_artifact(
        snapshot, "plague_live_large_bottom_zh_narrow.bmp", UI_LANG_ZH, 340,
        PLAGUE_PANEL_TAB_LIVE, PLAGUE_HISTORY_METRIC_TYPE, 0, 10000);
    return ok;
}

static int impact_artifacts(RenderSnapshot *snapshot) {
    int ok = 1;
    plague_probe_fill_active(snapshot, PLAGUE_SIZE_LARGE, 7);
    plague_probe_fill_impact(snapshot, 13, 5);
    ok &= plague_probe_render_artifact(
        snapshot, "plague_impact_active_en_wide_page1.bmp", UI_LANG_EN, 460,
        PLAGUE_PANEL_TAB_IMPACT, PLAGUE_HISTORY_METRIC_TYPE, 0, 0);
    ok &= plague_probe_render_artifact(
        snapshot, "plague_impact_active_en_narrow_page2.bmp", UI_LANG_EN, 340,
        PLAGUE_PANEL_TAB_IMPACT, PLAGUE_HISTORY_METRIC_TYPE, 1, 0);
    ok &= plague_probe_render_artifact(
        snapshot, "plague_impact_active_zh_wide_page1.bmp", UI_LANG_ZH, 460,
        PLAGUE_PANEL_TAB_IMPACT, PLAGUE_HISTORY_METRIC_TYPE, 0, 0);
    ok &= plague_probe_render_artifact(
        snapshot, "plague_impact_active_zh_narrow_page3.bmp", UI_LANG_ZH, 340,
        PLAGUE_PANEL_TAB_IMPACT, PLAGUE_HISTORY_METRIC_TYPE, 2, 0);
    ok &= plague_probe_render_artifact(
        snapshot, "plague_impact_city_top5_en_wide.bmp", UI_LANG_EN, 460,
        PLAGUE_PANEL_TAB_IMPACT, PLAGUE_HISTORY_METRIC_TYPE, 0, 10000);
    ok &= plague_probe_render_artifact(
        snapshot, "plague_impact_city_top5_zh_narrow.bmp", UI_LANG_ZH, 340,
        PLAGUE_PANEL_TAB_IMPACT, PLAGUE_HISTORY_METRIC_TYPE, 0, 10000);
    return ok;
}

static int history_metric_artifacts(RenderSnapshot *snapshot) {
    static const char *metric_names[PLAGUE_HISTORY_METRIC_COUNT] = {
        "type", "severity", "duration", "deaths", "cities", "countries",
        "spores"
    };
    int ok = 1;
    int i;
    plague_probe_fill_active(snapshot, PLAGUE_SIZE_MEDIUM, 7);
    plague_probe_fill_impact(snapshot, 13, 5);
    for (i = 0; i < PLAGUE_HISTORY_METRIC_COUNT; i++) {
        char file_en[96];
        char file_zh[96];
        snprintf(file_en, sizeof(file_en), "plague_history_%s_en_wide.bmp",
                 metric_names[i]);
        snprintf(file_zh, sizeof(file_zh), "plague_history_%s_zh_narrow.bmp",
                 metric_names[i]);
        ok &= plague_probe_render_artifact(
            snapshot, file_en, UI_LANG_EN, 460, PLAGUE_PANEL_TAB_HISTORY,
            (PlagueHistoryMetric)i, 0, 0);
        ok &= plague_probe_render_artifact(
            snapshot, file_zh, UI_LANG_ZH, 340, PLAGUE_PANEL_TAB_HISTORY,
            (PlagueHistoryMetric)i, 0, 0);
    }
    return ok;
}

static int inactive_artifacts(RenderSnapshot *snapshot) {
    int ok = 1;
    plague_probe_fill_inactive(snapshot, 0);
    plague_probe_fill_impact(snapshot, 0, 0);
    ok &= plague_probe_render_artifact(
        snapshot, "plague_inactive_live_en_narrow.bmp", UI_LANG_EN, 340,
        PLAGUE_PANEL_TAB_LIVE, PLAGUE_HISTORY_METRIC_TYPE, 0, 0);
    ok &= plague_probe_render_artifact(
        snapshot, "plague_inactive_impact_zh_wide.bmp", UI_LANG_ZH, 460,
        PLAGUE_PANEL_TAB_IMPACT, PLAGUE_HISTORY_METRIC_TYPE, 0, 0);
    ok &= plague_probe_render_artifact(
        snapshot, "plague_inactive_history_empty_en_wide.bmp", UI_LANG_EN, 460,
        PLAGUE_PANEL_TAB_HISTORY, PLAGUE_HISTORY_METRIC_TYPE, 0, 0);
    plague_probe_fill_inactive(snapshot, 7);
    ok &= plague_probe_render_artifact(
        snapshot, "plague_inactive_live_completed_en_wide.bmp", UI_LANG_EN, 460,
        PLAGUE_PANEL_TAB_LIVE, PLAGUE_HISTORY_METRIC_TYPE, 0, 0);
    ok &= plague_probe_render_artifact(
        snapshot, "plague_inactive_live_completed_zh_narrow.bmp", UI_LANG_ZH, 340,
        PLAGUE_PANEL_TAB_LIVE, PLAGUE_HISTORY_METRIC_TYPE, 0, 0);
    ok &= plague_probe_render_artifact(
        snapshot, "plague_inactive_live_completed_bottom_en_wide.bmp", UI_LANG_EN, 460,
        PLAGUE_PANEL_TAB_LIVE, PLAGUE_HISTORY_METRIC_TYPE, 0, 10000);
    ok &= plague_probe_render_artifact(
        snapshot, "plague_inactive_live_completed_bottom_zh_narrow.bmp", UI_LANG_ZH, 340,
        PLAGUE_PANEL_TAB_LIVE, PLAGUE_HISTORY_METRIC_TYPE, 0, 10000);
    ok &= plague_probe_render_artifact(
        snapshot, "plague_inactive_history_en_narrow.bmp", UI_LANG_EN, 340,
        PLAGUE_PANEL_TAB_HISTORY, PLAGUE_HISTORY_METRIC_DEATHS, 0, 0);
    ok &= plague_probe_render_artifact(
        snapshot, "plague_inactive_history_zh_wide.bmp", UI_LANG_ZH, 460,
        PLAGUE_PANEL_TAB_HISTORY, PLAGUE_HISTORY_METRIC_DURATION, 0, 0);
    return ok;
}

static int interaction_artifacts(RenderSnapshot *snapshot) {
    int ok = 1;
    plague_probe_fill_active(snapshot, PLAGUE_SIZE_MEDIUM, 7);
    plague_probe_fill_impact(snapshot, 13, 5);
    ok &= plague_probe_render_interaction_artifact(
        snapshot, "plague_interaction_main_tab_hover_en_wide.bmp",
        UI_LANG_EN, 460, PLAGUE_PANEL_TAB_LIVE,
        PLAGUE_HISTORY_METRIC_TYPE,
        UI_PLAGUE_PANEL_HIT_MAIN_TAB_BASE + PLAGUE_PANEL_TAB_IMPACT,
        UI_PRESSED_NONE, -1);
    ok &= plague_probe_render_interaction_artifact(
        snapshot, "plague_interaction_metric_pressed_zh_narrow.bmp",
        UI_LANG_ZH, 340, PLAGUE_PANEL_TAB_HISTORY,
        PLAGUE_HISTORY_METRIC_DURATION,
        UI_PLAGUE_PANEL_HIT_HISTORY_METRIC_BASE +
            PLAGUE_HISTORY_METRIC_DURATION,
        UI_PRESSED_PLAGUE_HISTORY_METRIC,
        PLAGUE_HISTORY_METRIC_DURATION);
    return ok;
}

int game_presentation_plague_probe(FILE *summary) {
    RenderSnapshot *snapshot = calloc(1, sizeof(RenderSnapshot));
    int fog;
    int impact;
    int history;
    int interaction;
    int live_source;
    int live_bmps = 0;
    int impact_bmps = 0;
    int history_bmps = 0;
    int inactive_bmps = 0;
    int interaction_bmps = 0;
    int artifacts;
    int ok;
    if (!snapshot) {
        fprintf(summary, "case=plague_panel_redesign ok=0 allocation=0\n");
        return 0;
    }
    fog = fog_control_contract_case(summary);
    live_source = live_source_order_case(snapshot);
    impact = game_presentation_plague_impact_probe(summary);
    history = game_presentation_plague_history_probe(summary);
    interaction = game_presentation_plague_interaction_probe(summary);
    live_bmps = live_artifacts(snapshot);
    impact_bmps = impact_artifacts(snapshot);
    history_bmps = history_metric_artifacts(snapshot);
    inactive_bmps = inactive_artifacts(snapshot);
    interaction_bmps = interaction_artifacts(snapshot);
    artifacts = live_bmps && impact_bmps && history_bmps && inactive_bmps &&
                interaction_bmps;
    ok = fog && live_source && impact && history && interaction && artifacts;
    fprintf(summary,
            "case=plague_panel_redesign ok=%d live_source_active_completed_empty=%d live_tabs=%d impact_tabs=%d history_metrics=%d inactive_states=%d hover_pressed_artifacts=%d languages=2 narrow_width=340 wide_width=460 impact_pages=3 metric_rows=4_plus_3 artifacts=39 fog_preserved=%d\n",
            ok, live_source, live_bmps, impact_bmps, history_bmps,
            inactive_bmps, interaction_bmps, fog);
    free(snapshot);
    return ok;
}
