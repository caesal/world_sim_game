#include "game/game_worldgen_aridity_diminishing_matrix.h"

#include "game/game_worldgen_aridity_calibration_metrics.h"
#include "game/game_worldgen_aridity_diminishing_io.h"
#include "game/game_worldgen_aridity_diminishing_oasis_histogram_io.h"
#include "game/game_worldgen_aridity_diminishing_production_matrix.h"
#include "game/game_worldgen_aridity_response_projection_histogram.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define DIM_OUTPUT_ENV "WORLD_SIM_ARIDITY_PROBE_DIR"
#define DIM_CANDIDATE_ID "a31_b04_d24_c12_r02"

static const uint32_t SCREEN_SEEDS[] = {2026082301u, 2026082302u};
static const uint32_t HOLDOUT_SEEDS[] = {2026082401u, 2026082402u};
static const uint32_t BASELINE_SEED = 2026072301u;
static const char *const MAP_STEMS[] = {
    "small", "medium", "large", "extreme"
};

typedef struct {
    int rows;
    int generation_failures;
    int hash_failures;
    int capture_failures;
    int semantic_failures;
    int state_failures;
    int projection_mismatches;
    int artifact_count;
    int artifact_failures;
    uint64_t matrix_hash;
} DiminishingSummary;

static int selected_oasis_matches_response(
    const GameWorldgenAridityResponseOptions *response,
    const GameWorldgenAridityProjectionRow *row);

static void note_projection_row(
    const GameWorldgenAridityProjectionRow *row,
    DiminishingSummary *summary) {
    summary->rows++;
    summary->generation_failures += !row->generated;
    summary->hash_failures += row->generated && row->physical_hash == 0;
    summary->capture_failures += !row->projection.complete ||
        row->projection.climate_input_hash == 0;
    summary->state_failures += !row->active_state_ok ||
        !row->restored_state_ok;
    summary->matrix_hash = game_worldgen_aridity_hash_mix(
        summary->matrix_hash, row->physical_hash);
    summary->matrix_hash = game_worldgen_aridity_hash_mix(
        summary->matrix_hash, row->projection.climate_input_hash);
}

static int run_baseline_or_production(
    const GameWorldgenAridityDiminishingOptions *options,
    const GameWorldgenAridityResponseDefaults *defaults, FILE *csv,
    DiminishingSummary *summary) {
    uint32_t baseline_seed = BASELINE_SEED;
    int seed_index;
    int map_size;
    int case_index;
    int io_ok = game_worldgen_aridity_write_calibration_csv_header(csv);
    for (seed_index = 0; seed_index < 1; seed_index++) {
        for (map_size = 0; map_size < GAME_WORLDGEN_ARIDITY_MAP_COUNT;
             map_size++) {
            for (case_index = 0;
                 case_index < GAME_WORLDGEN_ARIDITY_CASE_COUNT; case_index++) {
                GameWorldgenAridityResult row;
                uint32_t seed = baseline_seed;
                int row_ok;
                memset(&row, 0, sizeof(row));
                game_worldgen_aridity_diminishing_reset_all();
                summary->state_failures +=
                    !game_worldgen_aridity_response_defaults_restored(
                        defaults);
                row_ok = game_worldgen_aridity_run_world(
                    seed, map_size, &game_worldgen_aridity_cases[case_index],
                    &row);
                game_worldgen_aridity_diminishing_reset_all();
                summary->state_failures +=
                    !game_worldgen_aridity_response_defaults_restored(
                        defaults);
                summary->rows++;
                summary->generation_failures += !row.generated;
                summary->hash_failures += row.generated &&
                    row.physical_hash == 0;
                summary->semantic_failures += !row_ok ||
                    row.oasis_semantic_errors != 0;
                summary->matrix_hash = game_worldgen_aridity_hash_mix(
                    summary->matrix_hash, seed);
                summary->matrix_hash = game_worldgen_aridity_hash_mix(
                    summary->matrix_hash, row.physical_hash);
                io_ok &= game_worldgen_aridity_write_calibration_csv_row(
                    csv, summary->rows - 1, seed, map_size,
                    &game_worldgen_aridity_cases[case_index], &row);
            }
        }
    }
    return io_ok && options->action ==
        GAME_WORLDGEN_ARIDITY_DIMINISHING_ACTION_BASELINE;
}

static int run_carriers(
    const GameWorldgenAridityResponseDefaults *defaults, FILE *csv,
    DiminishingSummary *summary) {
    static const GameWorldgenAridityCase contexts[] = {
        {50, 0, 0}, {50, 100, 0}, {25, 100, 100}, {75, 100, 100}
    };
    static const char *const roles[] = {"AB", "CD", "E", "F"};
    int seed_index;
    int map_size;
    int context_index;
    int row_index = 0;
    int io_ok = game_worldgen_aridity_projection_write_carrier_header(csv);
    for (context_index = 0; context_index < 4; context_index++) {
        for (seed_index = 0; seed_index < 2; seed_index++) {
            for (map_size = 0; map_size < GAME_WORLDGEN_ARIDITY_MAP_COUNT;
                 map_size++) {
                GameWorldgenAridityProjectionRow row;
                char carrier_id[96];
                int named;
                int row_ok;
                memset(&row, 0, sizeof(row));
                named = snprintf(carrier_id, sizeof(carrier_id),
                    "d24_%s_m%02d_%u_%s", roles[context_index],
                    contexts[context_index].moisture,
                    SCREEN_SEEDS[seed_index], MAP_STEMS[map_size]);
                row_ok = named > 0 &&
                    (size_t)named < sizeof(carrier_id) &&
                    game_worldgen_aridity_projection_run_carrier(
                        SCREEN_SEEDS[seed_index], map_size,
                        &contexts[context_index], 24, defaults, &row);
                note_projection_row(&row, summary);
                summary->semantic_failures += !row_ok;
                io_ok &= game_worldgen_aridity_projection_write_carrier_row(
                    csv, row_index++, carrier_id, roles[context_index],
                    SCREEN_SEEDS[seed_index], map_size,
                    &contexts[context_index], 24, &row);
            }
        }
    }
    return io_ok && row_index == 32;
}

static int selected_oasis_matches_response(
    const GameWorldgenAridityResponseOptions *response,
    const GameWorldgenAridityProjectionRow *row) {
    int projected = -1;
    return response && row &&
        game_worldgen_aridity_diminishing_oasis_histogram_count(
            &row->actual.diminishing_oasis_histogram,
            response->oasis_drop, response->transition_margin, &projected) &&
        row->actual.oasis_count == projected;
}

static int write_artifact_row(
    FILE *file, const char *pair_id, const char *stem, uint32_t seed,
    int map_size, int case_index, const GameWorldgenAridityProjectionRow *row,
    const WorldGenAridityCalibrationArtifactResult *artifact) {
    return file && fprintf(file,
        "%s,%s,%u,%d,%s,%c,%d,%d,%016llx,%016llx,%d,%s_geography.bmp,"
        "%016llx,%s_climate.bmp,%016llx\n",
        DIM_CANDIDATE_ID, pair_id, seed, map_size,
        game_worldgen_aridity_map_names[map_size], 'A' + case_index,
        artifact->width, artifact->height,
        (unsigned long long)row->physical_hash,
        (unsigned long long)row->projection.climate_input_hash,
        row->actual.oasis_count, stem,
        (unsigned long long)artifact->geography_hash, stem,
        (unsigned long long)artifact->climate_hash) > 0;
}

static int run_actual(
    const GameWorldgenAridityDiminishingOptions *options,
    const GameWorldgenAridityResponseDefaults *defaults, FILE *csv,
    FILE *histogram,
    FILE *manifest, DiminishingSummary *summary) {
    const char *directory = getenv(DIM_OUTPUT_ENV);
    const uint32_t *seeds = options->action ==
        GAME_WORLDGEN_ARIDITY_DIMINISHING_ACTION_HOLDOUT ?
        HOLDOUT_SEEDS : SCREEN_SEEDS;
    int first_case = options->action ==
        GAME_WORLDGEN_ARIDITY_DIMINISHING_ACTION_OASIS ? 3 : 0;
    int case_count = options->action ==
        GAME_WORLDGEN_ARIDITY_DIMINISHING_ACTION_OASIS ? 2 : 6;
    char pair_id[32];
    int seed_index;
    int map_size;
    int relative_case;
    int row_index = 0;
    int io_ok = game_worldgen_aridity_diminishing_write_actual_header(csv) &&
        game_worldgen_aridity_diminishing_oasis_histogram_write_header(
            histogram);
    if (!game_worldgen_aridity_diminishing_pair_id(
            options, pair_id, sizeof(pair_id))) return 0;
    for (seed_index = 0; seed_index < 2; seed_index++) {
        for (map_size = 0; map_size < GAME_WORLDGEN_ARIDITY_MAP_COUNT;
             map_size++) {
            for (relative_case = 0; relative_case < case_count;
                 relative_case++) {
                int case_index = first_case + relative_case;
                GameWorldgenAridityProjectionRow row;
                WorldGenAridityCalibrationArtifactResult artifact;
                char stem[192];
                int emit = options->projection.emit_artifacts &&
                    seed_index == 0 && (map_size == 0 || map_size == 3);
                int named = emit && directory && directory[0] ?
                    snprintf(stem, sizeof(stem),
                        "dim_%s_%u_%s_%c",
                        pair_id, seeds[seed_index],
                        MAP_STEMS[map_size], 'A' + case_index) : 0;
                int row_ok;
                memset(&row, 0, sizeof(row));
                if (emit && (named <= 0 || (size_t)named >= sizeof(stem))) {
                    row_ok = 0;
                } else {
                    row_ok = game_worldgen_aridity_projection_run_actual(
                        seeds[seed_index], map_size,
                        &game_worldgen_aridity_cases[case_index],
                        &options->projection, defaults,
                        emit ? directory : NULL, emit ? stem : NULL, &row,
                        emit ? &artifact : NULL);
                }
                note_projection_row(&row, summary);
                summary->semantic_failures += !row_ok ||
                    row.actual.oasis_semantic_errors != 0;
                summary->projection_mismatches +=
                    !selected_oasis_matches_response(
                        &options->projection.response, &row);
                if (emit) {
                    if (row_ok && write_artifact_row(
                            manifest, pair_id, stem, seeds[seed_index],
                            map_size, case_index, &row, &artifact)) {
                        summary->artifact_count += 2;
                    } else {
                        summary->artifact_failures++;
                    }
                }
                io_ok &= game_worldgen_aridity_diminishing_write_actual_row(
                    csv, row_index++, options->action_name,
                    DIM_CANDIDATE_ID, pair_id, seeds[seed_index], map_size,
                    case_index, &game_worldgen_aridity_cases[case_index],
                    &options->projection, &row);
                io_ok &=
                    game_worldgen_aridity_diminishing_oasis_histogram_write_row(
                        histogram, row_index - 1, options->action_name,
                        DIM_CANDIDATE_ID, pair_id, seeds[seed_index], map_size,
                        case_index, &options->projection, &row);
            }
        }
    }
    return io_ok && row_index == (case_count * 8);
}

int game_worldgen_aridity_diminishing_matrix_run(
    const GameWorldgenAridityDiminishingOptions *options) {
    GameWorldgenAridityResponseDefaults defaults;
    GameWorldgenAridityDiminishingProductionSummary production = {0};
    DiminishingSummary result = {0};
    char csv_path[1024];
    char summary_path[1024];
    char manifest_path[1024];
    char histogram_path[1024];
    FILE *csv = NULL;
    FILE *summary = NULL;
    FILE *manifest = NULL;
    FILE *histogram = NULL;
    int expected_rows;
    int expected_artifacts = options && options->action ==
            GAME_WORLDGEN_ARIDITY_DIMINISHING_ACTION_PRODUCTION ? 48 :
        (options && options->projection.emit_artifacts ? 24 : 0);
    int io_ok = 1;
    int overall_ok;
    int result_code = 2;
    int needs_histogram;
    if (!options || options->action ==
            GAME_WORLDGEN_ARIDITY_DIMINISHING_ACTION_FORMULA) return 2;
    result.matrix_hash = UINT64_C(1469598103934665603);
    needs_histogram = options->action !=
            GAME_WORLDGEN_ARIDITY_DIMINISHING_ACTION_BASELINE &&
        options->action != GAME_WORLDGEN_ARIDITY_DIMINISHING_ACTION_CARRIERS;
    game_worldgen_aridity_diminishing_reset_all();
    if (!game_worldgen_aridity_projection_capture_defaults(&defaults) ||
        !game_worldgen_aridity_output_path(
            csv_path, sizeof(csv_path), options->action ==
                GAME_WORLDGEN_ARIDITY_DIMINISHING_ACTION_CARRIERS ?
                "aridity_diminishing_carriers.csv" :
                "aridity_diminishing_actual.csv") ||
        !game_worldgen_aridity_output_path(
            summary_path, sizeof(summary_path),
            "aridity_diminishing_summary.txt") ||
        !game_worldgen_aridity_output_path_available(csv_path) ||
        !game_worldgen_aridity_output_path_available(summary_path)) {
        goto cleanup;
    }
    csv = fopen(csv_path, "wb");
    summary = fopen(summary_path, "wb");
    if (!csv || !summary) goto cleanup;
    if (needs_histogram) {
        if (!game_worldgen_aridity_output_path(
                histogram_path, sizeof(histogram_path),
                "aridity_diminishing_oasis_histogram.csv") ||
            !game_worldgen_aridity_output_path_available(histogram_path)) {
            goto cleanup;
        }
        histogram = fopen(histogram_path, "wb");
        if (!histogram) goto cleanup;
    }
    if (expected_artifacts) {
        if (!game_worldgen_aridity_output_path(
                manifest_path, sizeof(manifest_path),
                "aridity_diminishing_artifacts_manifest.csv") ||
            !game_worldgen_aridity_output_path_available(manifest_path)) {
            goto cleanup;
        }
        manifest = fopen(manifest_path, "wb");
        if (!manifest) goto cleanup;
        io_ok &= fprintf(manifest,
            "candidate_id,pair_id,seed,map_size,map_name,case_id,width,height,"
            "physical_hash,climate_input_hash,oasis_count,geography_file,"
            "geography_pixel_hash,climate_file,climate_pixel_hash\n") > 0;
    }
    if (options->action == GAME_WORLDGEN_ARIDITY_DIMINISHING_ACTION_BASELINE) {
        io_ok &= run_baseline_or_production(
            options, &defaults, csv, &result);
        expected_rows = 24;
    } else if (options->action ==
               GAME_WORLDGEN_ARIDITY_DIMINISHING_ACTION_PRODUCTION) {
        io_ok &= game_worldgen_aridity_diminishing_production_matrix_run(
            options, csv, histogram, manifest, &production);
        result.rows += production.rows;
        result.generation_failures += production.generation_failures;
        result.hash_failures += production.hash_failures;
        result.capture_failures += production.capture_failures;
        result.semantic_failures += production.semantic_failures;
        result.state_failures += production.state_failures;
        result.projection_mismatches += production.projection_mismatches;
        result.artifact_count += production.artifact_count;
        result.artifact_failures += production.artifact_failures;
        result.matrix_hash = production.matrix_hash;
        expected_rows = 96;
    } else if (options->action ==
               GAME_WORLDGEN_ARIDITY_DIMINISHING_ACTION_CARRIERS) {
        io_ok &= run_carriers(&defaults, csv, &result);
        expected_rows = 32;
    } else {
        io_ok &= run_actual(
            options, &defaults, csv, histogram, manifest, &result);
        expected_rows = options->action ==
            GAME_WORLDGEN_ARIDITY_DIMINISHING_ACTION_OASIS ? 16 : 48;
    }
    overall_ok = result.rows == expected_rows &&
        result.generation_failures == 0 && result.hash_failures == 0 &&
        result.capture_failures == 0 && result.semantic_failures == 0 &&
        result.state_failures == 0 && result.projection_mismatches == 0 &&
        result.artifact_failures == 0 &&
        result.artifact_count == expected_artifacts && io_ok;
    io_ok &= fprintf(summary,
        "case=aridity_diminishing action=%s candidate_id=%s "
        "worlds_generated=%d expected_worlds=%d generation_failures=%d "
        "hash_failures=%d capture_failures=%d semantic_failures=%d "
        "state_failures=%d projection_mismatches=%d artifact_count=%d "
        "artifact_failures=%d matrix_hash=%016llx overall_ok=%d\n",
        options->action_name, DIM_CANDIDATE_ID, result.rows, expected_rows,
        result.generation_failures, result.hash_failures,
        result.capture_failures, result.semantic_failures,
        result.state_failures, result.projection_mismatches,
        result.artifact_count, result.artifact_failures,
        (unsigned long long)result.matrix_hash, overall_ok) > 0;
    if (fflush(csv) != 0 || fflush(summary) != 0 ||
        (histogram && fflush(histogram) != 0) ||
        (manifest && fflush(manifest) != 0)) io_ok = 0;
    if (fclose(csv) != 0) io_ok = 0;
    csv = NULL;
    if (fclose(summary) != 0) io_ok = 0;
    summary = NULL;
    if (manifest && fclose(manifest) != 0) io_ok = 0;
    manifest = NULL;
    if (histogram && fclose(histogram) != 0) io_ok = 0;
    histogram = NULL;
    if (!io_ok) goto cleanup;
    printf("worldgen aridity diminishing matrix: %s\n", csv_path);
    printf("worldgen aridity diminishing summary: %s\n", summary_path);
    if (needs_histogram) {
        printf("worldgen aridity diminishing oasis histogram: %s\n",
            histogram_path);
    }
    result_code = overall_ok ? 0 : 1;

cleanup:
    if (csv) fclose(csv);
    if (summary) fclose(summary);
    if (manifest) fclose(manifest);
    if (histogram) fclose(histogram);
    game_worldgen_aridity_diminishing_reset_all();
    return result_code;
}
