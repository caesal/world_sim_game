#include "game/game_worldgen_aridity_response_projection_matrix.h"

#include "game/game_worldgen_aridity_calibration_metrics.h"
#include "game/game_worldgen_aridity_response_matrix.h"
#include "game/game_worldgen_aridity_response_projection_histogram.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define PROJECTION_OUTPUT_ENV "WORLD_SIM_ARIDITY_PROBE_DIR"

static const uint32_t PROJECTION_SEEDS[] = {2026082301u, 2026082302u};
static const int PROJECTION_DIVISORS[] = {6, 8, 10, 12, 14, 16, 20, 24};
static const char *const MAP_FILE_NAMES[] = {
    "small", "medium", "large", "extreme"
};

typedef struct {
    int row_count;
    int generation_failures;
    int hash_failures;
    int capture_failures;
    int semantic_failures;
    int state_failures;
    int projection_mismatches;
    int artifact_count;
    int artifact_failures;
    uint64_t matrix_hash;
} ProjectionMatrixSummary;

static int run_legacy_baseline(void) {
    GameWorldgenAridityResponseOptions legacy;
    memset(&legacy, 0, sizeof(legacy));
    legacy.action = GAME_WORLDGEN_ARIDITY_RESPONSE_ACTION_BASELINE;
    legacy.action_name = "baseline";
    return game_worldgen_aridity_response_matrix_run(&legacy);
}

static void note_row(
    const GameWorldgenAridityProjectionRow *row,
    ProjectionMatrixSummary *summary) {
    summary->row_count++;
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

static int run_carriers(
    const GameWorldgenAridityResponseDefaults *defaults, FILE *csv,
    ProjectionMatrixSummary *summary) {
    const GameWorldgenAridityCase neutral = {50, 0, 0};
    static const GameWorldgenAridityCase drought_cases[] = {
        {50, 100, 0}, {25, 100, 100}, {75, 100, 100}
    };
    static const char *const source_cases[] = {"CD", "E", "F"};
    int row_index = 0;
    int seed_index;
    int map_size;
    int divisor_index;
    int context_index;
    int io_ok = game_worldgen_aridity_projection_write_carrier_header(csv);
    for (seed_index = 0; seed_index < 2; seed_index++) {
        for (map_size = 0; map_size < GAME_WORLDGEN_ARIDITY_MAP_COUNT;
             map_size++) {
            GameWorldgenAridityProjectionRow row;
            char carrier_id[96];
            memset(&row, 0, sizeof(row));
            int named = snprintf(carrier_id, sizeof(carrier_id),
                "neutral_%u_%s", PROJECTION_SEEDS[seed_index],
                MAP_FILE_NAMES[map_size]);
            int row_ok = named > 0 && (size_t)named < sizeof(carrier_id) &&
                game_worldgen_aridity_projection_run_carrier(
                    PROJECTION_SEEDS[seed_index], map_size, &neutral, 24,
                    defaults, &row);
            note_row(&row, summary);
            summary->semantic_failures += !row_ok;
            io_ok &= game_worldgen_aridity_projection_write_carrier_row(
                csv, row_index++, carrier_id, "AB",
                PROJECTION_SEEDS[seed_index], map_size, &neutral, 24, &row);
        }
    }
    for (divisor_index = 0; divisor_index < 8; divisor_index++) {
        for (context_index = 0; context_index < 3; context_index++) {
            for (seed_index = 0; seed_index < 2; seed_index++) {
                for (map_size = 0;
                     map_size < GAME_WORLDGEN_ARIDITY_MAP_COUNT; map_size++) {
                    const GameWorldgenAridityCase *matrix_case =
                        &drought_cases[context_index];
                    GameWorldgenAridityProjectionRow row;
                    char carrier_id[96];
                    memset(&row, 0, sizeof(row));
                    int named = snprintf(carrier_id, sizeof(carrier_id),
                        "d%02d_m%02d_%u_%s",
                        PROJECTION_DIVISORS[divisor_index],
                        matrix_case->moisture, PROJECTION_SEEDS[seed_index],
                        MAP_FILE_NAMES[map_size]);
                    int row_ok = named > 0 &&
                        (size_t)named < sizeof(carrier_id) &&
                        game_worldgen_aridity_projection_run_carrier(
                            PROJECTION_SEEDS[seed_index], map_size,
                            matrix_case, PROJECTION_DIVISORS[divisor_index],
                            defaults, &row);
                    note_row(&row, summary);
                    summary->semantic_failures += !row_ok;
                    io_ok &=
                        game_worldgen_aridity_projection_write_carrier_row(
                            csv, row_index++, carrier_id,
                            source_cases[context_index],
                            PROJECTION_SEEDS[seed_index], map_size,
                            matrix_case, PROJECTION_DIVISORS[divisor_index],
                            &row);
                }
            }
        }
    }
    return io_ok && row_index == 200;
}

static int selected_projection_matches(
    const GameWorldgenAridityProjectionOptions *options,
    const GameWorldgenAridityProjectionRow *row) {
    int drop_index = game_worldgen_aridity_response_oasis_drop_index(
        options->response.oasis_drop);
    int transition_index =
        game_worldgen_aridity_response_transition_margin_index(
            options->response.transition_margin);
    return drop_index >= 0 && transition_index >= 0 &&
        row->actual.oasis_count ==
            row->actual.projected_visible_oasis_count
                [drop_index][transition_index];
}

static int write_artifact_manifest_row(
    FILE *file, const char *candidate_id, const char *pair_id,
    const char *stem, uint32_t seed, int map_size, int case_index,
    const GameWorldgenAridityProjectionRow *row,
    const WorldGenAridityCalibrationArtifactResult *artifact) {
    return file && fprintf(file,
        "%s,%s,%u,%d,%s,%c,%d,%d,%016llx,%016llx,%d,%s_geography.bmp,"
        "%016llx,%s_climate.bmp,%016llx\n",
        candidate_id, pair_id, seed, map_size,
        game_worldgen_aridity_map_names[map_size], 'A' + case_index,
        artifact->width, artifact->height,
        (unsigned long long)row->physical_hash,
        (unsigned long long)row->projection.climate_input_hash,
        row->actual.oasis_count, stem,
        (unsigned long long)artifact->geography_hash, stem,
        (unsigned long long)artifact->climate_hash) > 0;
}

static int run_actual(
    const GameWorldgenAridityProjectionOptions *options,
    const GameWorldgenAridityResponseDefaults *defaults, FILE *csv,
    FILE *artifact_manifest, ProjectionMatrixSummary *summary) {
    const char *directory = getenv(PROJECTION_OUTPUT_ENV);
    char candidate_id[64];
    char pair_id[32] = "all";
    int confirm = options->action ==
        GAME_WORLDGEN_ARIDITY_PROJECTION_ACTION_CONFIRM;
    int first_case = confirm ? 0 : 3;
    int case_count = confirm ? GAME_WORLDGEN_ARIDITY_CASE_COUNT : 2;
    int seed_index;
    int map_size;
    int relative_case;
    int row_index = 0;
    int io_ok = game_worldgen_aridity_projection_write_actual_header(csv);
    if (!game_worldgen_aridity_projection_candidate_id(
            options, candidate_id, sizeof(candidate_id))) return 0;
    if (!game_worldgen_aridity_projection_pair_id(
            options, pair_id, sizeof(pair_id))) return 0;
    for (seed_index = 0; seed_index < 2; seed_index++) {
        for (map_size = 0; map_size < GAME_WORLDGEN_ARIDITY_MAP_COUNT;
             map_size++) {
            for (relative_case = 0; relative_case < case_count;
                 relative_case++) {
                int case_index = first_case + relative_case;
                const GameWorldgenAridityCase *matrix_case =
                    &game_worldgen_aridity_cases[case_index];
                GameWorldgenAridityProjectionRow row;
                WorldGenAridityCalibrationArtifactResult artifact;
                char stem[192];
                int emit = confirm && seed_index == 0 &&
                    (map_size == 0 || map_size == 3);
                int named = emit && directory && directory[0] ?
                    snprintf(stem, sizeof(stem),
                        "aridity_response_projection_%s_%s_%u_%s_%c",
                        candidate_id, pair_id,
                        PROJECTION_SEEDS[seed_index],
                        MAP_FILE_NAMES[map_size], 'A' + case_index) : 0;
                int row_ok;
                if (emit && (named <= 0 || (size_t)named >= sizeof(stem))) {
                    memset(&row, 0, sizeof(row));
                    row_ok = 0;
                } else {
                    row_ok = game_worldgen_aridity_projection_run_actual(
                        PROJECTION_SEEDS[seed_index], map_size, matrix_case,
                        options, defaults, emit ? directory : NULL,
                        emit ? stem : NULL, &row, emit ? &artifact : NULL);
                }
                note_row(&row, summary);
                summary->semantic_failures += !row_ok ||
                    row.actual.oasis_semantic_errors != 0;
                summary->projection_mismatches +=
                    !selected_projection_matches(options, &row);
                if (emit) {
                    if (row_ok && write_artifact_manifest_row(
                            artifact_manifest, candidate_id, pair_id, stem,
                            PROJECTION_SEEDS[seed_index], map_size,
                            case_index, &row, &artifact)) {
                        summary->artifact_count += 2;
                    } else {
                        summary->artifact_failures++;
                    }
                }
                io_ok &= game_worldgen_aridity_projection_write_actual_row(
                    csv, row_index++, confirm ? "confirm" : "oasis",
                    candidate_id, pair_id, PROJECTION_SEEDS[seed_index],
                    map_size, case_index, matrix_case, options, &row);
            }
        }
    }
    return io_ok && row_index == (confirm ? 48 : 16);
}

int game_worldgen_aridity_response_projection_matrix_run(
    const GameWorldgenAridityProjectionOptions *options) {
    GameWorldgenAridityResponseDefaults defaults;
    ProjectionMatrixSummary result = {0};
    char csv_path[1024];
    char summary_path[1024];
    char artifact_path[1024];
    FILE *csv = NULL;
    FILE *summary = NULL;
    FILE *artifact_manifest = NULL;
    const char *csv_name;
    int expected_rows;
    int io_ok = 1;
    int overall_ok;
    int result_code = 2;
    if (!options || options->action ==
            GAME_WORLDGEN_ARIDITY_PROJECTION_ACTION_FORMULA) return 2;
    if (options->action ==
            GAME_WORLDGEN_ARIDITY_PROJECTION_ACTION_BASELINE) {
        return run_legacy_baseline();
    }
    result.matrix_hash = UINT64_C(1469598103934665603);
    game_worldgen_aridity_projection_reset_all();
    if (!game_worldgen_aridity_projection_capture_defaults(&defaults)) {
        goto cleanup;
    }
    csv_name = options->action ==
        GAME_WORLDGEN_ARIDITY_PROJECTION_ACTION_CARRIERS ?
        "aridity_response_projection_carriers.csv" :
        "aridity_response_projection_actual.csv";
    if (!game_worldgen_aridity_output_path(
            csv_path, sizeof(csv_path), csv_name) ||
        !game_worldgen_aridity_output_path(
            summary_path, sizeof(summary_path),
            "aridity_response_projection_summary.txt") ||
        !game_worldgen_aridity_output_path_available(csv_path) ||
        !game_worldgen_aridity_output_path_available(summary_path)) {
        goto cleanup;
    }
    csv = fopen(csv_path, "wb");
    summary = fopen(summary_path, "wb");
    if (!csv || !summary) goto cleanup;
    if (options->emit_artifacts) {
        if (!game_worldgen_aridity_output_path(
                artifact_path, sizeof(artifact_path),
                "aridity_response_projection_artifacts_manifest.csv") ||
            !game_worldgen_aridity_output_path_available(artifact_path)) {
            goto cleanup;
        }
        artifact_manifest = fopen(artifact_path, "wb");
        if (!artifact_manifest) goto cleanup;
        io_ok &= fprintf(artifact_manifest,
            "candidate_id,pair_id,seed,map_size,map_name,case_id,width,height,"
            "physical_hash,climate_input_hash,oasis_count,geography_file,"
            "geography_pixel_hash,climate_file,climate_pixel_hash\n") > 0;
    }
    if (options->action ==
            GAME_WORLDGEN_ARIDITY_PROJECTION_ACTION_CARRIERS) {
        io_ok &= run_carriers(&defaults, csv, &result);
        expected_rows = 200;
    } else {
        io_ok &= run_actual(
            options, &defaults, csv, artifact_manifest, &result);
        expected_rows = options->action ==
            GAME_WORLDGEN_ARIDITY_PROJECTION_ACTION_CONFIRM ? 48 : 16;
    }
    overall_ok = result.row_count == expected_rows &&
        result.generation_failures == 0 && result.hash_failures == 0 &&
        result.capture_failures == 0 && result.semantic_failures == 0 &&
        result.state_failures == 0 && result.projection_mismatches == 0 &&
        result.artifact_failures == 0 &&
        (!options->emit_artifacts || result.artifact_count == 24) && io_ok;
    io_ok &= fprintf(summary,
        "case=aridity_response_projection action=%s worlds_generated=%d "
        "expected_worlds=%d generation_failures=%d hash_failures=%d "
        "capture_failures=%d semantic_failures=%d state_failures=%d "
        "projection_mismatches=%d artifact_count=%d artifact_failures=%d "
        "matrix_hash=%016llx overall_ok=%d\n",
        options->action_name, result.row_count, expected_rows,
        result.generation_failures, result.hash_failures,
        result.capture_failures, result.semantic_failures,
        result.state_failures, result.projection_mismatches,
        result.artifact_count, result.artifact_failures,
        (unsigned long long)result.matrix_hash, overall_ok) > 0;
    if (fflush(csv) != 0 || fflush(summary) != 0) io_ok = 0;
    if (artifact_manifest && fflush(artifact_manifest) != 0) io_ok = 0;
    if (fclose(csv) != 0) io_ok = 0;
    csv = NULL;
    if (fclose(summary) != 0) io_ok = 0;
    summary = NULL;
    if (artifact_manifest && fclose(artifact_manifest) != 0) io_ok = 0;
    artifact_manifest = NULL;
    if (!io_ok) goto cleanup;
    printf("worldgen aridity projection matrix: %s\n", csv_path);
    printf("worldgen aridity projection summary: %s\n", summary_path);
    result_code = overall_ok ? 0 : 1;

cleanup:
    if (csv) fclose(csv);
    if (summary) fclose(summary);
    if (artifact_manifest) fclose(artifact_manifest);
    game_worldgen_aridity_projection_reset_all();
    return result_code;
}
