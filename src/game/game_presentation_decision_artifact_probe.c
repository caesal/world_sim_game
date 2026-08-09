#include "game/game_presentation_decision_artifact_probe.h"

#include "game/game_presentation_decision_artifact_fixture.h"
#include "game/game_presentation_static_physical_artifacts.h"
#include "render/panel_country_decision.h"
#include "render/panel_view_model_cache_keys.h"
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

#define DECISION_ARTIFACT_HEIGHT 900
#define DECISION_ARTIFACT_COUNT 64

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
    int artifacts_written;
    int render_contracts;
    int stability_headers;
    int distinct_pressures;
    int stable_keys;
    int revised_keys;
    int source_contract;
    uint64_t calculation_before;
    uint64_t calculation_after;
} DecisionArtifactResults;

static const int probe_widths[] = {340, 460, 500, 720};
static const int probe_civs[] = {0, 199};
static const char *language_names[] = {"en", "zh"};
static const char *subtab_names[] = {
    "overview", "expansion", "war", "stability"
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

static int ensure_directory_tree(const char *directory) {
    char path[MAX_PATH];
    size_t length;
    size_t i;
    if (!directory || !directory[0]) return 0;
    length = strlen(directory);
    if (length >= sizeof(path)) return 0;
    memcpy(path, directory, length + 1);
    for (i = 0; i < length; i++) {
        DWORD error;
        if (path[i] != '/' && path[i] != '\\') continue;
        if (i == 0 || (i == 2 && path[1] == ':')) continue;
        path[i] = '\0';
        if (!CreateDirectoryA(path, NULL)) {
            error = GetLastError();
            if (error != ERROR_ALREADY_EXISTS) return 0;
        }
        path[i] = directory[i];
    }
    if (CreateDirectoryA(path, NULL)) return 1;
    return GetLastError() == ERROR_ALREADY_EXISTS;
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

static void set_probe_ui(int language, int width, int civ_id, int subtab) {
    selected_civ = civ_id;
    ui_language = language;
    side_panel_w = width;
    side_panel_collapsed = 0;
    panel_tab = PANEL_COUNTRY;
    country_detail_subtab = COUNTRY_DETAIL_DECISION;
    country_decision_subtab = subtab;
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

static int stability_snapshot_matches(const DecisionSnapshot *decision) {
    const DecisionStabilityBreakdown *b = &decision->stability_breakdown;
    int raw = b->base_contribution + b->war_status_contribution +
              b->territory_fragmentation_contribution +
              b->capital_connectivity_contribution +
              b->vassal_governance_contribution +
              b->high_disorder_contribution;
    return b->raw_total == raw && b->final_intent == clamp(raw, 0, 100) &&
           decision->stability_weight == b->final_intent;
}

static int render_artifact(RenderSnapshot *snapshot, const char *directory,
                           FILE *manifest, int language, int width, int civ_id,
                           int subtab, uint64_t *out_hash, int *out_header) {
    StaticPhysicalProbeCanvas canvas;
    const RenderSnapshot *previous_context = render_context_snapshot();
    UiCursor cursor;
    HFONT font = NULL;
    HFONT previous = NULL;
    char name[128];
    const DecisionSnapshot *decision = &snapshot->civs[civ_id].decision;
    int header_ok = subtab != COUNTRY_DECISION_STABILITY;
    int visible;
    int wrote;
    if (!static_physical_probe_canvas_open(
            &canvas, width, DECISION_ARTIFACT_HEIGHT)) return 0;
    set_probe_ui(language, width, civ_id, subtab);
    font = select_probe_font(canvas.dc, &previous);
    fill_rect(canvas.dc, (RECT){0, 0, width, DECISION_ARTIFACT_HEIGHT},
              ui_theme_color(UI_COLOR_PANEL));
    cursor = ui_cursor(8, 8, width - 16, DECISION_ARTIFACT_HEIGHT - 8);
    render_context_begin(snapshot);
    draw_country_decision_tab(canvas.dc, &cursor, civ_id);
    if (previous_context) render_context_begin(previous_context);
    else render_context_end();
    GdiFlush();
    *out_hash = static_physical_probe_canvas_hash(&canvas);
    if (subtab == COUNTRY_DECISION_STABILITY) {
        header_ok = stability_snapshot_matches(decision);
    }
    snprintf(name, sizeof(name), "decision_%s_%d_%s_%s.bmp",
             language_names[language], width,
             civ_id == 0 ? "early_0" : "late_199", subtab_names[subtab]);
    wrote = static_physical_probe_canvas_write(&canvas, directory, name);
    visible = cursor.y > 42 && cursor.y <= DECISION_ARTIFACT_HEIGHT - 8 &&
              *out_hash != 0;
    fprintf(manifest, "%s,%s,%d,%d,%s,%llu,%llu,%d,%d,%d\n",
            name, language_names[language], width, civ_id,
            subtab_names[subtab],
            (unsigned long long)decision->published_revision,
            (unsigned long long)*out_hash, cursor.y, header_ok,
            wrote && visible);
    if (previous && previous != (HFONT)HGDI_ERROR) {
        SelectObject(canvas.dc, previous);
    }
    if (font) DeleteObject(font);
    static_physical_probe_canvas_close(&canvas);
    *out_header = header_ok;
    return wrote && visible;
}

static int renderer_source_lacks_loading_text(void) {
    const char *paths[] = {
        "src/render/panel_country_decision.c",
        "src/render/panel_country_decision_extra.c",
        "src/render/panel_country_detail.c",
        "src/core/render_snapshot_civs.c"
    };
    const char *forbidden[] = {"Updat" "ing", "更新" "中"};
    char buffer[4096];
    int i;
    for (i = 0; i < (int)(sizeof(paths) / sizeof(paths[0])); i++) {
        FILE *file = fopen(paths[i], "rb");
        size_t used = 0;
        if (!file) return 0;
        while (!feof(file)) {
            size_t read_count = fread(buffer + used, 1,
                                      sizeof(buffer) - 1 - used, file);
            int j;
            used += read_count;
            buffer[used] = '\0';
            for (j = 0; j < (int)(sizeof(forbidden) / sizeof(forbidden[0])); j++) {
                if (strstr(buffer, forbidden[j])) {
                    fclose(file);
                    return 0;
                }
            }
            if (used > 16) {
                memmove(buffer, buffer + used - 16, 16);
                used = 16;
            }
            if (read_count == 0) break;
        }
        fclose(file);
    }
    return 1;
}

static void verify_cache_keys(RenderSnapshot *snapshot,
                              DecisionArtifactResults *results) {
    RECT client = {0, 0, 1152, 900};
    int civ_index;
    int subtab;
    for (civ_index = 0; civ_index < 2; civ_index++) {
        int civ_id = probe_civs[civ_index];
        SnapshotCiv *civ = &snapshot->civs[civ_id];
        int old_desire = civ->decision.war_desire;
        uint64_t old_revision = civ->decision.published_revision;
        for (subtab = 0; subtab < COUNTRY_DECISION_SUBTAB_COUNT; subtab++) {
            unsigned int ui_base;
            unsigned int data_base;
            unsigned int ui_same;
            unsigned int data_same;
            unsigned int data_payload;
            unsigned int data_revision;
            set_probe_ui(UI_LANG_EN, 460, civ_id, subtab);
            ui_base = panel_view_model_cache_ui_key(
                client, PANEL_CACHE_COUNTRY_DETAIL);
            data_base = panel_view_model_cache_data_key(
                snapshot, PANEL_CACHE_COUNTRY_DETAIL);
            ui_same = panel_view_model_cache_ui_key(
                client, PANEL_CACHE_COUNTRY_DETAIL);
            data_same = panel_view_model_cache_data_key(
                snapshot, PANEL_CACHE_COUNTRY_DETAIL);
            civ->decision.war_desire = old_desire + 9;
            data_payload = panel_view_model_cache_data_key(
                snapshot, PANEL_CACHE_COUNTRY_DETAIL);
            civ->decision.war_desire = old_desire;
            if (ui_base == ui_same && data_base == data_same &&
                data_base == data_payload) results->stable_keys++;
            civ->decision.published_revision = old_revision + 1;
            data_revision = panel_view_model_cache_data_key(
                snapshot, PANEL_CACHE_COUNTRY_DETAIL);
            civ->decision.published_revision = old_revision;
            if (data_revision != data_base) results->revised_keys++;
        }
    }
}

static int write_results(FILE *local, FILE *parent,
                         const DecisionArtifactResults *results) {
    int artifact_ok = results->artifacts_written == DECISION_ARTIFACT_COUNT;
    int render_ok = results->render_contracts == DECISION_ARTIFACT_COUNT;
    int snapshots_ok = results->stability_headers == 16;
    int pressure_ok = results->distinct_pressures == 16;
    int keys_ok = results->stable_keys == 8 && results->revised_keys == 8;
    int calculations_ok = results->calculation_before ==
                          results->calculation_after;
    int ok = artifact_ok && render_ok && snapshots_ok && pressure_ok && keys_ok &&
             calculations_ok && results->source_contract;
    report_line(local, parent,
                "case=decision_artifact_matrix ok=%d artifacts=%d/64 render_contracts=%d/64 languages=EN/ZH widths=340/460/500/720 civs=0/199 subtabs=4\n",
                artifact_ok && render_ok, results->artifacts_written,
                results->render_contracts);
    report_line(local, parent,
                "case=decision_stability_snapshot_contract ok=%d coherent_snapshots=%d/16 distinct_pressure=%d/16\n",
                snapshots_ok && pressure_ok, results->stability_headers,
                results->distinct_pressures);
    report_line(local, parent,
                "case=decision_panel_cache_keys ok=%d unchanged_reuse=%d/8 revision_invalidations=%d/8\n",
                keys_ok, results->stable_keys, results->revised_keys);
    report_line(local, parent,
                "case=decision_render_side_effects ok=%d calculations=%llu/%llu no_loading_source=%d\n",
                calculations_ok && results->source_contract,
                (unsigned long long)results->calculation_before,
                (unsigned long long)results->calculation_after,
                results->source_contract);
    report_line(local, parent, "decision_artifact_overall_ok=%d\n", ok);
    return ok;
}

int game_presentation_decision_artifact_probe(
    const char *output_directory, FILE *parent_summary) {
    SavedUiState saved = save_ui_state();
    const RenderSnapshot *saved_context = render_context_snapshot();
    DecisionArtifactResults results = {0};
    RenderSnapshot *snapshot = NULL;
    FILE *manifest = NULL;
    FILE *local_summary = NULL;
    char manifest_path[MAX_PATH];
    char summary_path[MAX_PATH];
    uint64_t hashes[2][4][2][4] = {{{{0}}}};
    int language;
    int width_index;
    int civ_index;
    int subtab;
    int matrix_distinct = 1;
    int ok = 0;

    if (!ensure_directory_tree(output_directory) ||
        !static_physical_probe_join_path(
            manifest_path, sizeof(manifest_path), output_directory,
            "decision_presentation_manifest.csv") ||
        !static_physical_probe_join_path(
            summary_path, sizeof(summary_path), output_directory,
            "decision_presentation_summary.txt")) goto cleanup;
    manifest = fopen(manifest_path, "w");
    local_summary = fopen(summary_path, "w");
    snapshot = (RenderSnapshot *)calloc(1, sizeof(*snapshot));
    if (!manifest || !local_summary || !snapshot) {
        report_line(local_summary, parent_summary,
                    "case=decision_artifact_setup ok=0 directory=%s\n",
                    output_directory ? output_directory : "(null)");
        goto cleanup;
    }
    fprintf(manifest,
            "file,language,width,civ_id,subtab,published_revision,pixel_hash,cursor_y,stability_snapshot_ok,render_ok\n");
    game_presentation_decision_artifact_fill_fixture(snapshot);
    results.calculation_before =
        decision_snapshot_cache_total_calculation_count();
    results.source_contract = renderer_source_lacks_loading_text();
    for (language = 0; language < 2; language++) {
        for (width_index = 0; width_index < 4; width_index++) {
            int width = probe_widths[width_index];
            for (civ_index = 0; civ_index < 2; civ_index++) {
                int civ_id = probe_civs[civ_index];
                for (subtab = 0; subtab < 4; subtab++) {
                    int header_ok = 0;
                    int rendered = render_artifact(
                        snapshot, output_directory, manifest, language, width,
                        civ_id, subtab,
                        &hashes[language][width_index][civ_index][subtab],
                        &header_ok);
                    results.artifacts_written += rendered;
                    results.render_contracts += rendered;
                    if (subtab == COUNTRY_DECISION_STABILITY) {
                        results.stability_headers += header_ok;
                        results.distinct_pressures +=
                            snapshot->civs[civ_id].decision.stability_weight !=
                            snapshot->civs[civ_id].decision.stability_pressure;
                    }
                }
                for (subtab = 0; subtab < 4; subtab++) {
                    int other;
                    for (other = subtab + 1; other < 4; other++) {
                        if (hashes[language][width_index][civ_index][subtab] ==
                            hashes[language][width_index][civ_index][other]) {
                            matrix_distinct = 0;
                        }
                    }
                }
            }
        }
    }
    if (fflush(manifest) != 0 || ferror(manifest)) {
        results.artifacts_written = 0;
    }
    if (!matrix_distinct) results.render_contracts = 0;
    verify_cache_keys(snapshot, &results);
    results.calculation_after =
        decision_snapshot_cache_total_calculation_count();
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
