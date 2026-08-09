#include "game/game_presentation_decision_stability_probe.h"

#include "game/game_presentation_decision_artifact_fixture.h"
#include "game/game_presentation_decision_stability_visual_contract_probe.h"
#include "game/game_presentation_static_physical_artifacts.h"
#include "render/panel_country_decision.h"
#include "render/render_common.h"
#include "render/render_context.h"
#include "sim/decision_snapshot_cache.h"
#include "ui/ui_theme.h"
#include "ui/ui_types.h"
#include "ui/ui_widgets.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STABILITY_ARTIFACT_HEIGHT 1200
#define STABILITY_SCENARIO_COUNT 10
#define STABILITY_ARTIFACT_COUNT (STABILITY_SCENARIO_COUNT * 2 * 4)
#define BREAKDOWN(e, r, b, w, f, c, v, h, raw, final) \
    {.effective_disorder = (e), .resource_pressure = (r), \
     .base_contribution = (b), .war_status_contribution = (w), \
     .territory_fragmentation_contribution = (f), \
     .capital_connectivity_contribution = (c), \
     .vassal_governance_contribution = (v), \
     .high_disorder_contribution = (h), .raw_total = (raw), \
     .final_intent = (final)}

typedef struct {
    const char *name;
    int civ_id;
    int mode;
    int mode_months;
    int recovery_months;
    int owned_regions;
    int disconnected_components;
    int capital_connected_percent;
    DecisionStabilityBreakdown breakdown;
} StabilityScenario;

typedef struct {
    int selected_civ;
    int ui_language;
    int side_panel_w;
    int side_panel_collapsed;
    int panel_tab;
    int country_detail_subtab;
    int country_decision_subtab;
    int display_mode;
    int selected_alliance_id;
} SavedUiState;

typedef struct {
    int scenarios_valid;
    unsigned int mode_mask;
    int weights_covered;
    int raw_ranges_covered;
    int factor_states_covered;
    unsigned int factor_mask;
    int identities_covered;
    int long_text_covered;
    int artifacts_written;
    int stable_redraws;
    int stable_heights;
    int source_contract;
    uint64_t calculations_before;
    uint64_t calculations_after;
    DWORD gdi_before;
    DWORD gdi_after;
    DWORD user_before;
    DWORD user_after;
} StabilityProbeResults;

static const int probe_widths[] = {340, 460, 500, 720};
static const char *language_names[] = {"en", "zh"};

static const StabilityScenario scenarios[] = {
    {"zero_normal_early", 0, STABILITY_MODE_NORMAL, 0, 12, 0, 0, 100,
     BREAKDOWN(0, 0, 0, 0, 0, 0, 0, 0, 0, 0)},
    {"war_normal_early", 0, STABILITY_MODE_NORMAL, 196, 12, 0, 0, 100,
     BREAKDOWN(0, 0, 0, 12, 0, 0, 0, 0, 12, 12)},
    {"base_cautious_early", 0, STABILITY_MODE_CAUTIOUS, 49, 11, 1, 0, 100,
     BREAKDOWN(48, 21, 48, 0, 0, 0, 0, 0, 48, 48)},
    {"fragmentation_only_early", 0, STABILITY_MODE_NORMAL, 25, 0, 2, 1, 100,
     BREAKDOWN(0, 0, 0, 0, 24, 0, 0, 0, 24, 24)},
    {"connectivity_only_early", 0, STABILITY_MODE_NORMAL, 25, 0, 1, 0, 79,
     BREAKDOWN(0, 0, 0, 0, 0, 21, 0, 0, 21, 21)},
    {"vassal_only_late", 199, STABILITY_MODE_NORMAL, 25, 0, 1, 0, 100,
     BREAKDOWN(0, 0, 0, 0, 0, 0, 14, 0, 14, 14)},
    {"mixed_reorganizing_late", 199, STABILITY_MODE_REORGANIZING,
     197, 119, 2, 1, 100,
     BREAKDOWN(40, 52, 52, 12, 24, 0, 8, 0, 96, 96)},
    {"exact_crisis_late", 199, STABILITY_MODE_CRISIS, 31, 10, 1, 0, 100,
     BREAKDOWN(70, 70, 70, 0, 0, 0, 0, 30, 100, 100)},
    {"capped_emergency_late", 199, STABILITY_MODE_EMERGENCY, 64, 9, 1, 0, 100,
     BREAKDOWN(82, 65, 82, 12, 0, 0, 8, 30, 132, 100)},
    {"capped_collapse_late", 199, STABILITY_MODE_COLLAPSE, 240, 120, 2, 1, 70,
     BREAKDOWN(100, 84, 100, 12, 24, 30, 0, 30, 196, 100)}
};

static void report_line(FILE *local, FILE *parent, const char *format, ...) {
    va_list args;
    va_list parent_args;
    va_start(args, format);
    va_copy(parent_args, args);
    if (local) vfprintf(local, format, args);
    if (parent) vfprintf(parent, format, parent_args);
    va_end(parent_args);
    va_end(args);
}

static SavedUiState save_ui_state(void) {
    SavedUiState saved;
    saved.selected_civ = selected_civ;
    saved.ui_language = ui_language;
    saved.side_panel_w = side_panel_w;
    saved.side_panel_collapsed = side_panel_collapsed;
    saved.panel_tab = panel_tab;
    saved.country_detail_subtab = country_detail_subtab;
    saved.country_decision_subtab = country_decision_subtab;
    saved.display_mode = display_mode;
    saved.selected_alliance_id = selected_alliance_id;
    return saved;
}

static void restore_ui_state(SavedUiState saved) {
    selected_civ = saved.selected_civ;
    ui_language = saved.ui_language;
    side_panel_w = saved.side_panel_w;
    side_panel_collapsed = saved.side_panel_collapsed;
    panel_tab = saved.panel_tab;
    country_detail_subtab = saved.country_detail_subtab;
    country_decision_subtab = saved.country_decision_subtab;
    display_mode = saved.display_mode;
    selected_alliance_id = saved.selected_alliance_id;
}

static void set_probe_ui(int language, int width, int civ_id) {
    selected_civ = civ_id;
    ui_language = language;
    side_panel_w = width;
    side_panel_collapsed = 0;
    panel_tab = PANEL_COUNTRY;
    country_detail_subtab = COUNTRY_DETAIL_DECISION;
    country_decision_subtab = COUNTRY_DECISION_STABILITY;
    display_mode = DISPLAY_POLITICAL;
    selected_alliance_id = -1;
}

static HFONT select_probe_font(HDC dc, HFONT *previous) {
    HFONT font = CreateFontW(
        17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    *previous = font ? (HFONT)SelectObject(dc, font) : NULL;
    return font;
}

static int scenario_formula_ok(const StabilityScenario *scenario) {
    const DecisionStabilityBreakdown *b = &scenario->breakdown;
    int base = clamp(max(b->effective_disorder, b->resource_pressure), 0, 160);
    int fragmentation = scenario->disconnected_components * 24;
    int connectivity = scenario->owned_regions > 0 &&
                       scenario->capital_connected_percent < 80 ?
                       20 + (80 - scenario->capital_connected_percent) : 0;
    int raw = b->base_contribution + b->war_status_contribution +
              b->territory_fragmentation_contribution +
              b->capital_connectivity_contribution +
              b->vassal_governance_contribution +
              b->high_disorder_contribution;
    return b->effective_disorder >= 0 && b->resource_pressure >= 0 &&
           b->base_contribution >= 0 && b->war_status_contribution >= 0 &&
           b->territory_fragmentation_contribution >= 0 &&
           b->capital_connectivity_contribution >= 0 &&
           b->vassal_governance_contribution >= 0 &&
           b->high_disorder_contribution >= 0 &&
           b->base_contribution == base &&
           (b->war_status_contribution == 0 || b->war_status_contribution == 12) &&
           b->territory_fragmentation_contribution == fragmentation &&
           b->capital_connectivity_contribution == connectivity &&
           b->high_disorder_contribution ==
               (b->effective_disorder >= 70 ? 30 : 0) &&
           b->raw_total == raw && b->final_intent == clamp(raw, 0, 100);
}

static int positive_factor_count(const DecisionStabilityBreakdown *b) {
    return (b->base_contribution > 0) + (b->war_status_contribution > 0) +
           (b->territory_fragmentation_contribution > 0) +
           (b->capital_connectivity_contribution > 0) +
           (b->vassal_governance_contribution > 0) +
           (b->high_disorder_contribution > 0);
}

static void apply_scenario(RenderSnapshot *snapshot,
                           const StabilityScenario *scenario) {
    SnapshotCiv *civ = &snapshot->civs[scenario->civ_id];
    DecisionSnapshot *decision = &civ->decision;
    decision->stability_breakdown = scenario->breakdown;
    decision->stability_pressure = scenario->breakdown.base_contribution;
    decision->stability_weight = scenario->breakdown.final_intent;
    decision->stability_mode = scenario->mode;
    decision->stability_mode_months = scenario->mode_months;
    decision->stability_recovery_months = scenario->recovery_months;
    decision->owned_regions = scenario->owned_regions;
    decision->disconnected_components = scenario->disconnected_components;
    decision->capital_connected_percent = scenario->capital_connected_percent;
    decision->stability_allows_war = scenario->mode < STABILITY_MODE_REORGANIZING;
    decision->stability_allows_expansion = scenario->mode < STABILITY_MODE_COLLAPSE;
    decision->stability_peace_bonus = scenario->mode >= STABILITY_MODE_EMERGENCY ? 70 :
                                      scenario->mode == STABILITY_MODE_CRISIS ? 45 :
                                      scenario->mode == STABILITY_MODE_REORGANIZING ? 18 : 0;
    decision->war_stability_penalty = scenario->mode >= STABILITY_MODE_REORGANIZING ?
                                      decision->war_pre_stability_desire : 0;
    decision->war_raw_desire = max(0, decision->war_pre_stability_desire -
                                    decision->war_stability_penalty);
    decision->expansion.stability_expansion_penalty =
        scenario->mode >= STABILITY_MODE_REORGANIZING ? 24 : 0;
    decision->expansion.expansion_desire = max(
        0, decision->expansion.raw_expansion_desire -
           decision->expansion.stability_expansion_penalty);
}

static int renderer_source_contract(void) {
    return game_presentation_decision_stability_visual_contract_source_ok();
}

static int draw_once(StaticPhysicalProbeCanvas *canvas,
                     RenderSnapshot *snapshot, int language, int width,
                     int civ_id, uint64_t *hash, int *cursor_y) {
    const RenderSnapshot *previous_context = render_context_snapshot();
    UiCursor cursor;
    static_physical_probe_canvas_clear(canvas);
    fill_rect(canvas->dc, (RECT){0, 0, width, STABILITY_ARTIFACT_HEIGHT},
              ui_theme_color(UI_COLOR_PANEL));
    cursor = ui_cursor(8, 8, width - 16, STABILITY_ARTIFACT_HEIGHT - 8);
    set_probe_ui(language, width, civ_id);
    render_context_begin(snapshot);
    draw_country_decision_tab(canvas->dc, &cursor, civ_id);
    if (previous_context) render_context_begin(previous_context);
    else render_context_end();
    GdiFlush();
    *hash = static_physical_probe_canvas_hash(canvas);
    *cursor_y = cursor.y;
    return cursor.y > 42 && cursor.y <= STABILITY_ARTIFACT_HEIGHT - 8 && *hash != 0;
}

static int render_artifact(RenderSnapshot *snapshot, const char *directory,
                           FILE *manifest, const StabilityScenario *scenario,
                           int language, int width, int *out_stable,
                           int *out_cursor_y) {
    StaticPhysicalProbeCanvas canvas;
    HFONT font = NULL;
    HFONT previous = NULL;
    char name[160];
    char path[MAX_PATH];
    uint64_t first_hash = 0;
    uint64_t second_hash = 0;
    int first_y = 0;
    int second_y = 0;
    int first_ok;
    int second_ok;
    int wrote = 0;
    if (!static_physical_probe_canvas_open(&canvas, width,
                                           STABILITY_ARTIFACT_HEIGHT)) return 0;
    font = select_probe_font(canvas.dc, &previous);
    first_ok = draw_once(&canvas, snapshot, language, width, scenario->civ_id,
                         &first_hash, &first_y);
    second_ok = draw_once(&canvas, snapshot, language, width, scenario->civ_id,
                          &second_hash, &second_y);
    snprintf(name, sizeof(name), "decision_stability_%s_%d_%s.bmp",
             language_names[language], width, scenario->name);
    if (static_physical_probe_join_path(path, sizeof(path), directory, name) &&
        GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) {
        wrote = static_physical_probe_canvas_write(&canvas, directory, name);
    }
    *out_stable = first_ok && second_ok && first_hash == second_hash &&
                  first_y == second_y;
    *out_cursor_y = second_y;
    fprintf(manifest,
            "%s,%s,%d,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%llu,%llu,%d,%d,%d\n",
            name, language_names[language], width, scenario->name,
            scenario->civ_id, scenario->mode,
            scenario->breakdown.effective_disorder,
            scenario->breakdown.resource_pressure,
            scenario->breakdown.base_contribution,
            scenario->breakdown.war_status_contribution,
            scenario->breakdown.territory_fragmentation_contribution,
            scenario->breakdown.capital_connectivity_contribution,
            scenario->breakdown.vassal_governance_contribution,
            scenario->breakdown.high_disorder_contribution,
            scenario->breakdown.raw_total,
            scenario->breakdown.final_intent, second_y,
            (unsigned long long)first_hash, (unsigned long long)second_hash,
            scenario_formula_ok(scenario), *out_stable, wrote && second_ok);
    if (previous && previous != (HFONT)HGDI_ERROR) SelectObject(canvas.dc, previous);
    if (font) DeleteObject(font);
    static_physical_probe_canvas_close(&canvas);
    return wrote && second_ok;
}

static int write_results(FILE *local, FILE *parent,
                         const StabilityProbeResults *results) {
    int scenarios_ok = results->scenarios_valid == STABILITY_SCENARIO_COUNT &&
        results->mode_mask == ((1u << 6) - 1u) && results->weights_covered == 7 &&
        results->raw_ranges_covered == 7 && results->factor_states_covered == 3 &&
        results->factor_mask == 63 &&
        results->identities_covered == 3 && results->long_text_covered == 3;
    int artifacts_ok = results->artifacts_written == STABILITY_ARTIFACT_COUNT;
    int redraw_ok = results->stable_redraws == STABILITY_ARTIFACT_COUNT;
    int heights_ok = results->stable_heights == STABILITY_ARTIFACT_COUNT;
    int calculations_ok = results->calculations_before == results->calculations_after;
    int resources_ok = results->gdi_before == results->gdi_after &&
                       results->user_before == results->user_after;
    int ok = scenarios_ok && artifacts_ok && redraw_ok && heights_ok &&
             calculations_ok && resources_ok && results->source_contract;
    report_line(local, parent,
                "case=decision_stability_breakdown_fixtures ok=%d scenarios=%d/10 nonnegative=1 raw_sum=1 clamp=1 modes=6/6 weights=0/12/100 raw=below/exact/above factor_mask=63 ids=0/199 long_text=1\n",
                scenarios_ok, results->scenarios_valid);
    report_line(local, parent,
                "case=decision_stability_artifact_matrix ok=%d artifacts=%d/80 languages=EN/ZH widths=340/460/500/720 scenarios=10 civs=0/199\n",
                artifacts_ok, results->artifacts_written);
    report_line(local, parent,
                "case=decision_stability_render_contract ok=%d stable_redraws=%d/80 stable_heights=%d/80 source_contract=%d\n",
                redraw_ok && heights_ok && results->source_contract,
                results->stable_redraws, results->stable_heights,
                results->source_contract);
    report_line(local, parent,
                "case=decision_stability_side_effects ok=%d calculations=%llu/%llu gdi=%lu/%lu user=%lu/%lu\n",
                calculations_ok && resources_ok,
                (unsigned long long)results->calculations_before,
                (unsigned long long)results->calculations_after,
                (unsigned long)results->gdi_before,
                (unsigned long)results->gdi_after,
                (unsigned long)results->user_before,
                (unsigned long)results->user_after);
    report_line(local, parent, "decision_stability_artifact_overall_ok=%d\n", ok);
    return ok;
}

int game_presentation_decision_stability_probe(
    const char *output_directory, FILE *parent_summary) {
    SavedUiState saved = save_ui_state();
    const RenderSnapshot *saved_context = render_context_snapshot();
    StabilityProbeResults results = {0};
    RenderSnapshot *snapshot = NULL;
    FILE *manifest = NULL;
    FILE *local_summary = NULL;
    char manifest_path[MAX_PATH];
    char summary_path[MAX_PATH];
    int expected_heights[4] = {-1, -1, -1, -1};
    int scenario_index;
    int language;
    int width_index;
    int ok = 0;
    if (!static_physical_probe_join_path(
            manifest_path, sizeof(manifest_path), output_directory,
            "decision_stability_manifest.csv") ||
        !static_physical_probe_join_path(
            summary_path, sizeof(summary_path), output_directory,
            "decision_stability_summary.txt") ||
        GetFileAttributesA(manifest_path) != INVALID_FILE_ATTRIBUTES ||
        GetFileAttributesA(summary_path) != INVALID_FILE_ATTRIBUTES) goto cleanup;
    manifest = fopen(manifest_path, "w");
    local_summary = fopen(summary_path, "w");
    snapshot = (RenderSnapshot *)calloc(1, sizeof(*snapshot));
    if (!manifest || !local_summary || !snapshot) goto cleanup;
    fprintf(manifest,
            "file,language,width,scenario,civ_id,mode,effective_disorder,resource_pressure,base,war,fragmentation,connectivity,vassal,high_disorder,raw_total,final_intent,cursor_y,first_hash,second_hash,formula_ok,stable_redraw,render_ok\n");
    results.calculations_before = decision_snapshot_cache_total_calculation_count();
    results.gdi_before = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    results.user_before = GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS);
    results.source_contract = renderer_source_contract();
    for (scenario_index = 0; scenario_index < STABILITY_SCENARIO_COUNT;
         scenario_index++) {
        const StabilityScenario *scenario = &scenarios[scenario_index];
        int positive_factors = positive_factor_count(&scenario->breakdown);
        if (scenario_formula_ok(scenario)) results.scenarios_valid++;
        if (scenario->mode >= STABILITY_MODE_NORMAL &&
            scenario->mode <= STABILITY_MODE_COLLAPSE) {
            results.mode_mask |= 1u << scenario->mode;
        }
        if (scenario->breakdown.final_intent == 0) results.weights_covered |= 1;
        if (scenario->breakdown.final_intent == 12) results.weights_covered |= 2;
        if (scenario->breakdown.final_intent == 100) results.weights_covered |= 4;
        if (scenario->breakdown.raw_total < 100) results.raw_ranges_covered |= 1;
        if (scenario->breakdown.raw_total == 100) results.raw_ranges_covered |= 2;
        if (scenario->breakdown.raw_total > 100) results.raw_ranges_covered |= 4;
        if (positive_factors == 0) results.factor_states_covered |= 1;
        if (positive_factors > 1) results.factor_states_covered |= 2;
        if (scenario->breakdown.base_contribution > 0) results.factor_mask |= 1u;
        if (scenario->breakdown.war_status_contribution > 0) results.factor_mask |= 2u;
        if (scenario->breakdown.territory_fragmentation_contribution > 0)
            results.factor_mask |= 4u;
        if (scenario->breakdown.capital_connectivity_contribution > 0)
            results.factor_mask |= 8u;
        if (scenario->breakdown.vassal_governance_contribution > 0)
            results.factor_mask |= 16u;
        if (scenario->breakdown.high_disorder_contribution > 0)
            results.factor_mask |= 32u;
        results.identities_covered |= scenario->civ_id == 0 ? 1 : 2;
        if (scenario->mode_months >= 120) results.long_text_covered |= 1;
        if (scenario->recovery_months >= 100) results.long_text_covered |= 2;
        game_presentation_decision_artifact_fill_fixture(snapshot);
        apply_scenario(snapshot, scenario);
        for (language = 0; language < 2; language++) {
            for (width_index = 0; width_index < 4; width_index++) {
                int stable = 0;
                int cursor_y = 0;
                int rendered = render_artifact(
                    snapshot, output_directory, manifest, scenario, language,
                    probe_widths[width_index], &stable, &cursor_y);
                results.artifacts_written += rendered;
                results.stable_redraws += stable;
                if (expected_heights[width_index] < 0) {
                    expected_heights[width_index] = cursor_y;
                }
                if (cursor_y == expected_heights[width_index]) {
                    results.stable_heights++;
                }
            }
        }
    }
    if (fflush(manifest) != 0 || ferror(manifest)) results.artifacts_written = 0;
    results.calculations_after = decision_snapshot_cache_total_calculation_count();
    GdiFlush();
    results.gdi_after = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    results.user_after = GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS);
    ok = write_results(local_summary, parent_summary, &results);

cleanup:
    if (saved_context) render_context_begin(saved_context);
    else render_context_end();
    restore_ui_state(saved);
    if (manifest) fclose(manifest);
    if (local_summary) fclose(local_summary);
    free(snapshot);
    return ok;
}
