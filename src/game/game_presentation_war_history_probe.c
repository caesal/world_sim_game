#include "game/game_presentation_war_history_probe.h"

#include "game/game_presentation_static_physical_artifacts.h"
#include "game/game_presentation_war_history_fixture.h"
#include "game/game_presentation_war_history_visual_contract_probe.h"
#include "render/panel_country_diplomacy.h"
#include "render/panel_country_diplomacy_result.h"
#include "render/panel_country_diplomacy_war_history.h"
#include "render/panel_view_model_cache_keys.h"
#include "render/render_context.h"
#include "sim/diplomacy.h"
#include "ui/ui_theme.h"
#include "ui/ui_types.h"
#include "ui/ui_widgets.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WAR_HISTORY_ARTIFACT_HEIGHT 1320
#define WAR_HISTORY_ARTIFACT_COUNT \
    (WAR_HISTORY_PRESENTATION_CASE_COUNT * 2 * 4)

typedef struct {
    int selected_civ;
    int ui_language;
    int side_panel_w;
    int side_panel_collapsed;
    int panel_tab;
    int country_detail_subtab;
    int country_diplomacy_view;
    int display_mode;
    int selected_alliance_id;
    int hover_x;
    int hover_y;
    int diplomacy_scroll;
} SavedUiState;

typedef struct {
    int artifacts;
    int stable_redraws;
    int geometry_contracts;
    int fixture_contracts;
    int history_heights;
    int case_coverage;
    int formatter_terms;
    int cache_contracts;
    int source_contract;
    int distinct_matrix;
} WarHistoryProbeResults;

typedef struct {
    int result;
    int perspective;
    int tone;
    const char *en;
    const char *zh;
} ResultTermCase;

static const int artifact_widths[] = {340, 460, 500, 720};
static const char *language_slugs[] = {"en", "zh"};

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
    saved.country_diplomacy_view = country_diplomacy_view;
    saved.display_mode = display_mode;
    saved.selected_alliance_id = selected_alliance_id;
    saved.hover_x = hover_x;
    saved.hover_y = hover_y;
    saved.diplomacy_scroll =
        country_detail_scroll_offsets[COUNTRY_DETAIL_DIPLOMACY];
    return saved;
}

static void restore_ui_state(SavedUiState saved) {
    selected_civ = saved.selected_civ;
    ui_language = saved.ui_language;
    side_panel_w = saved.side_panel_w;
    side_panel_collapsed = saved.side_panel_collapsed;
    panel_tab = saved.panel_tab;
    country_detail_subtab = saved.country_detail_subtab;
    country_diplomacy_view = saved.country_diplomacy_view;
    display_mode = saved.display_mode;
    selected_alliance_id = saved.selected_alliance_id;
    hover_x = saved.hover_x;
    hover_y = saved.hover_y;
    country_detail_scroll_offsets[COUNTRY_DETAIL_DIPLOMACY] =
        saved.diplomacy_scroll;
}

static void set_probe_ui(int language, int width, int civ_id,
                         DiplomacyView view) {
    selected_civ = civ_id;
    ui_language = language;
    side_panel_w = width;
    side_panel_collapsed = 0;
    panel_tab = PANEL_COUNTRY;
    country_detail_subtab = COUNTRY_DETAIL_DIPLOMACY;
    country_diplomacy_view = view;
    display_mode = DISPLAY_POLITICAL;
    selected_alliance_id = -1;
    hover_x = -10000;
    hover_y = -10000;
    country_detail_scroll_offsets[COUNTRY_DETAIL_DIPLOMACY] = 0;
}

static HFONT select_probe_font(HDC dc, HFONT *previous) {
    HFONT font = CreateFontW(
        17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    *previous = font ? (HFONT)SelectObject(dc, font) : NULL;
    return font;
}

static uint64_t draw_artifact_once(StaticPhysicalProbeCanvas *canvas,
                                   const RenderSnapshot *snapshot,
                                   int civ_id, int *out_cursor_y) {
    const RenderSnapshot *previous = render_context_snapshot();
    RECT viewport = {8, 8, canvas->width - 8, canvas->height - 8};
    UiCursor cursor = ui_cursor(8, 8, canvas->width - 16,
                                canvas->height - 8);
    fill_rect(canvas->dc,
              (RECT){0, 0, canvas->width, canvas->height},
              ui_theme_color(UI_COLOR_PANEL));
    render_context_begin(snapshot);
    draw_country_diplomacy_tab(canvas->dc, &cursor, viewport, 0, civ_id);
    if (previous) render_context_begin(previous);
    else render_context_end();
    GdiFlush();
    if (out_cursor_y) *out_cursor_y = cursor.y;
    return static_physical_probe_canvas_hash(canvas);
}

static int history_text_fits(HDC dc, int panel_width, int card_width,
                             const SnapshotCiv *civ) {
    int i;
    for (i = 0; i < civ->war_history.count; i++) {
        if (!country_diplomacy_war_history_record_text_fits(
                dc, panel_width, card_width,
                &civ->war_history.records[i])) return 0;
    }
    return 1;
}

static int render_artifact(RenderSnapshot *snapshot, const char *directory,
                           FILE *manifest, int language, int width,
                           int case_index, int *out_cursor_y,
                           uint64_t *out_hash) {
    const WarHistoryPresentationCase *spec =
        game_presentation_war_history_case(case_index);
    StaticPhysicalProbeCanvas canvas;
    SnapshotCiv *civ = &snapshot->civs[spec->selected_civ];
    HFONT font = NULL;
    HFONT previous = NULL;
    char name[128];
    uint64_t repeat_hash;
    int repeat_y;
    int history_height;
    int expected_height;
    int wrote;
    int contract;
    if (!static_physical_probe_canvas_open(
            &canvas, width, WAR_HISTORY_ARTIFACT_HEIGHT)) return 0;
    set_probe_ui(language, width, spec->selected_civ, DIPLOMACY_VIEW_WAR);
    font = select_probe_font(canvas.dc, &previous);
    *out_hash = draw_artifact_once(&canvas, snapshot, spec->selected_civ,
                                   out_cursor_y);
    snprintf(name, sizeof(name), "war_history_%s_%d_%s.bmp",
             language_slugs[language], width, spec->slug);
    wrote = static_physical_probe_canvas_write(&canvas, directory, name);
    static_physical_probe_canvas_clear(&canvas);
    repeat_hash = draw_artifact_once(&canvas, snapshot, spec->selected_civ,
                                     &repeat_y);
    history_height = country_diplomacy_war_history_height(civ);
    expected_height = spec->history_count > 0 ?
                      31 + spec->history_count *
                          country_diplomacy_war_history_card_height(width, 17) +
                      (spec->history_count - 1) * 6 : 0;
    contract = wrote && *out_hash != 0 && *out_hash == repeat_hash &&
               *out_cursor_y == repeat_y &&
               *out_cursor_y < WAR_HISTORY_ARTIFACT_HEIGHT - 8 &&
               history_height == expected_height &&
               history_text_fits(canvas.dc, width, width - 24, civ) &&
               game_presentation_war_history_fixture_contract(
                   snapshot, case_index);
    fprintf(manifest,
            "%s,%s,%d,%s,%d,%d,%d,%d,%d,%d,%llu,%llu,%llu,%d,%d,%d\n",
            name, language_slugs[language], width, spec->slug,
            spec->selected_civ, spec->history_count, spec->lead_result,
            spec->lead_settlement, spec->lead_beneficiary,
            spec->active_war,
            (unsigned long long)civ->war_history.revision,
            (unsigned long long)*out_hash,
            (unsigned long long)repeat_hash, *out_cursor_y,
            history_height, contract);
    if (previous && previous != (HFONT)HGDI_ERROR) {
        SelectObject(canvas.dc, previous);
    }
    if (font) DeleteObject(font);
    static_physical_probe_canvas_close(&canvas);
    return contract;
}

static int exact_result_terms(void) {
    static const ResultTermCase terms[] = {
        {DIP_LAST_WAR_INTERRUPTED, 0, UI_COLOR_ACCENT,
         "Front Severed", "战线中断"},
        {DIP_LAST_WAR_FRONT_SEVERED, 0, UI_COLOR_ACCENT,
         "Front Severed", "战线中断"},
        {DIP_LAST_WAR_NEGOTIATED_TRUCE, 0, UI_COLOR_ACCENT,
         "Negotiated Truce", "议和停战"},
        {DIP_LAST_WAR_OFFENSIVE_HALTED, 1, UI_COLOR_ACCENT,
         "Offensive Halted", "攻势中止"},
        {DIP_LAST_WAR_OFFENSIVE_HALTED, -1, UI_COLOR_ACCENT,
         "Enemy Offensive Halted", "对方攻势中止"},
        {DIP_LAST_WAR_SURRENDER, 1, UI_COLOR_GOOD,
         "Surrender Win", "受降胜利"},
        {DIP_LAST_WAR_SURRENDER, -1, UI_COLOR_DANGER,
         "Surrender", "投降战败"},
        {DIP_LAST_WAR_DECISIVE, 1, UI_COLOR_GOOD,
         "Military Win", "军事胜利"},
        {DIP_LAST_WAR_MILITARY, -1, UI_COLOR_DANGER,
         "Military Defeat", "军事战败"}
    };
    int saved_language = ui_language;
    int local = 101;
    int opponent = 202;
    int passed = 0;
    int i;
    for (i = 0; i < (int)(sizeof(terms) / sizeof(terms[0])); i++) {
        int winner = terms[i].perspective > 0 ? local :
                     terms[i].perspective < 0 ? opponent : 0;
        int loser = terms[i].perspective > 0 ? opponent :
                    terms[i].perspective < 0 ? local : 0;
        COLORREF color = panel_country_diplomacy_result_color(
            local, winner, loser, terms[i].result);
        int en_ok;
        int zh_ok;
        ui_language = UI_LANG_EN;
        en_ok = strcmp(panel_country_diplomacy_result_text(
                           local, winner, loser, terms[i].result),
                       terms[i].en) == 0;
        ui_language = UI_LANG_ZH;
        zh_ok = strcmp(panel_country_diplomacy_result_text(
                           local, winner, loser, terms[i].result),
                       terms[i].zh) == 0;
        if (en_ok && zh_ok && color == ui_theme_color(terms[i].tone)) {
            passed += 2;
        }
    }
    ui_language = saved_language;
    return passed;
}

static int renderer_source_contract(void) {
    return game_presentation_war_history_visual_contract_source_ok();
}

static int cache_key_contract(RenderSnapshot *snapshot, int case_index) {
    const WarHistoryPresentationCase *spec =
        game_presentation_war_history_case(case_index);
    SnapshotCiv *selected = &snapshot->civs[spec->selected_civ];
    SnapshotCiv *unrelated = &snapshot->civs[spec->selected_civ == 0 ? 199 : 0];
    uint64_t revision = selected->war_history.revision;
    int casualties = selected->war_history.records[0].local_casualties;
    unsigned int base;
    unsigned int same;
    unsigned int payload;
    unsigned int revised;
    unsigned int unrelated_changed;
    unsigned int peace_base;
    unsigned int peace_history_changed;
    int passed = 0;
    set_probe_ui(UI_LANG_EN, 500, spec->selected_civ, DIPLOMACY_VIEW_WAR);
    base = panel_view_model_cache_data_key(
        snapshot, PANEL_CACHE_COUNTRY_DETAIL);
    same = panel_view_model_cache_data_key(
        snapshot, PANEL_CACHE_COUNTRY_DETAIL);
    selected->war_history.records[0].local_casualties++;
    payload = panel_view_model_cache_data_key(
        snapshot, PANEL_CACHE_COUNTRY_DETAIL);
    selected->war_history.records[0].local_casualties = casualties;
    selected->war_history.revision++;
    revised = panel_view_model_cache_data_key(
        snapshot, PANEL_CACHE_COUNTRY_DETAIL);
    selected->war_history.revision = revision;
    unrelated->war_history.revision++;
    unrelated_changed = panel_view_model_cache_data_key(
        snapshot, PANEL_CACHE_COUNTRY_DETAIL);
    unrelated->war_history.revision--;
    set_probe_ui(UI_LANG_EN, 500, spec->selected_civ, DIPLOMACY_VIEW_PEACE);
    peace_base = panel_view_model_cache_data_key(
        snapshot, PANEL_CACHE_COUNTRY_DETAIL);
    selected->war_history.revision++;
    peace_history_changed = panel_view_model_cache_data_key(
        snapshot, PANEL_CACHE_COUNTRY_DETAIL);
    selected->war_history.revision = revision;
    passed += base == same;
    passed += base == payload;
    passed += base != revised;
    passed += base == unrelated_changed;
    passed += peace_base == peace_history_changed;
    return passed;
}

static int write_results(FILE *local, FILE *parent,
                         const WarHistoryProbeResults *results) {
    int matrix_ok = results->artifacts == WAR_HISTORY_ARTIFACT_COUNT &&
                    results->stable_redraws == WAR_HISTORY_ARTIFACT_COUNT &&
                    results->geometry_contracts == WAR_HISTORY_ARTIFACT_COUNT &&
                    results->distinct_matrix;
    int fixture_ok =
        results->fixture_contracts == WAR_HISTORY_PRESENTATION_CASE_COUNT &&
        results->history_heights == WAR_HISTORY_PRESENTATION_CASE_COUNT &&
        results->case_coverage;
    int semantics_ok = results->formatter_terms == 18;
    int cache_ok = results->cache_contracts == 5;
    int ok = matrix_ok && fixture_ok && semantics_ok && cache_ok &&
             results->source_contract;
    report_line(local, parent,
                "case=war_history_presentation_matrix ok=%d artifacts=%d/80 stable_redraws=%d/80 geometry=%d/80 languages=EN/ZH widths=340/460/500/720 cases=10\n",
                matrix_ok, results->artifacts, results->stable_redraws,
                results->geometry_contracts);
    report_line(local, parent,
                "case=war_history_fixture_contract ok=%d cases=%d/10 fixed_heights=%d/10 counts=0/1/2/3 local_left=1 newest_first=1 active_and_inactive=1\n",
                fixture_ok, results->fixture_contracts,
                results->history_heights);
    report_line(local, parent,
                "case=war_history_result_terms ok=%d exact_en_zh=%d/18 semantic_colors=1 interrupted=1\n",
                semantics_ok, results->formatter_terms);
    report_line(local, parent,
                "case=war_history_cache_keys ok=%d contracts=%d/5 stable=1 payload_revision_owned=1 selected_revision_invalidates=1 unrelated_reuses=1 nonwar_reuses=1\n",
                cache_ok, results->cache_contracts);
    report_line(local, parent,
                "case=war_history_renderer_boundary ok=%d snapshot_only=1 no_loading_text=1 army_history_active_order=1\n",
                results->source_contract);
    report_line(local, parent, "war_history_presentation_overall_ok=%d\n", ok);
    return ok;
}

int game_presentation_war_history_probe(
    const char *output_directory, FILE *parent_summary) {
    SavedUiState saved = save_ui_state();
    const RenderSnapshot *saved_context = render_context_snapshot();
    WarHistoryProbeResults results = {0};
    RenderSnapshot *snapshot = NULL;
    FILE *manifest = NULL;
    FILE *local_summary = NULL;
    char manifest_path[MAX_PATH];
    char summary_path[MAX_PATH];
    uint64_t hashes[2][4][WAR_HISTORY_PRESENTATION_CASE_COUNT] = {{{0}}};
    int expected_y[4][WAR_HISTORY_PRESENTATION_CASE_COUNT];
    int case_index;
    int language;
    int width_index;
    int ok = 0;
    for (width_index = 0; width_index < 4; width_index++) {
        for (case_index = 0;
             case_index < WAR_HISTORY_PRESENTATION_CASE_COUNT;
             case_index++) expected_y[width_index][case_index] = -1;
    }
    if (!static_physical_probe_join_path(
            manifest_path, sizeof(manifest_path), output_directory,
            "war_history_presentation_manifest.csv") ||
        !static_physical_probe_join_path(
            summary_path, sizeof(summary_path), output_directory,
            "war_history_presentation_summary.txt")) goto cleanup;
    manifest = fopen(manifest_path, "w");
    local_summary = fopen(summary_path, "w");
    snapshot = (RenderSnapshot *)calloc(1, sizeof(*snapshot));
    if (!manifest || !local_summary || !snapshot) {
        report_line(local_summary, parent_summary,
                    "case=war_history_presentation_setup ok=0 directory=%s\n",
                    output_directory ? output_directory : "(null)");
        goto cleanup;
    }
    fprintf(manifest,
            "file,language,width,case,selected_civ,count,lead_result,settlement,beneficiary,active_war,history_revision,pixel_hash,repeat_hash,cursor_y,history_height,render_ok\n");
    for (case_index = 0; case_index < WAR_HISTORY_PRESENTATION_CASE_COUNT;
         case_index++) {
        const WarHistoryPresentationCase *spec =
            game_presentation_war_history_case(case_index);
        int expected_height = spec->history_count > 0 ?
                              31 + spec->history_count *
                                  country_diplomacy_war_history_card_height(
                                      500, 17) +
                              (spec->history_count - 1) * 6 : 0;
        game_presentation_war_history_fill_fixture(snapshot, case_index);
        results.fixture_contracts +=
            game_presentation_war_history_fixture_contract(
                snapshot, case_index);
        set_probe_ui(UI_LANG_EN, 500, spec->selected_civ,
                     DIPLOMACY_VIEW_WAR);
        results.history_heights +=
            country_diplomacy_war_history_height(
                &snapshot->civs[spec->selected_civ]) == expected_height;
        for (language = 0; language < 2; language++) {
            for (width_index = 0; width_index < 4; width_index++) {
                int cursor_y = -1;
                int rendered = render_artifact(
                    snapshot, output_directory, manifest, language,
                    artifact_widths[width_index], case_index, &cursor_y,
                    &hashes[language][width_index][case_index]);
                results.artifacts += rendered;
                results.stable_redraws += rendered;
                if (expected_y[width_index][case_index] < 0) {
                    expected_y[width_index][case_index] = cursor_y;
                }
                results.geometry_contracts +=
                    rendered && cursor_y ==
                        expected_y[width_index][case_index];
            }
        }
    }
    results.distinct_matrix = 1;
    for (language = 0; language < 2; language++) {
        for (width_index = 0; width_index < 4; width_index++) {
            int a;
            int b;
            for (a = 0; a < WAR_HISTORY_PRESENTATION_CASE_COUNT; a++) {
                for (b = a + 1; b < WAR_HISTORY_PRESENTATION_CASE_COUNT; b++) {
                    if (hashes[language][width_index][a] ==
                        hashes[language][width_index][b]) {
                        results.distinct_matrix = 0;
                    }
                }
            }
        }
    }
    results.formatter_terms = exact_result_terms();
    results.case_coverage = game_presentation_war_history_matrix_contract();
    game_presentation_war_history_fill_fixture(snapshot, 3);
    results.cache_contracts = cache_key_contract(snapshot, 3);
    results.source_contract = renderer_source_contract();
    if (fflush(manifest) != 0 || ferror(manifest)) results.artifacts = 0;
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
