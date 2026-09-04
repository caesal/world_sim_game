#include "game/game_worldgen_aridity_response_matrix.h"

#include "game/game_worldgen_aridity_calibration_metrics.h"
#include "game/game_worldgen_aridity_response_metrics.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ARIDITY_OUTPUT_ENV "WORLD_SIM_ARIDITY_PROBE_DIR"

static const uint32_t BASELINE_SEEDS[] = {2026072301u};
static const uint32_t RESPONSE_SEEDS[] = {2026082301u, 2026082302u};
static const char *const ARTIFACT_MAP_NAMES[] = {
    "small", "medium", "large", "extreme"
};

static int run_baseline(
    const GameWorldgenAridityResponseOptions *options,
    const GameWorldgenAridityResponseDefaults *defaults,
    FILE *csv, uint64_t *matrix_hash, int *row_count,
    int *generation_failures, int *non_arid_failures,
    int *semantic_failures, int *hash_failures, int *state_failures) {
    int map_size;
    int case_index;
    int io_ok = game_worldgen_aridity_write_calibration_csv_header(csv);
    (void)options;
    for (map_size = 0; map_size < GAME_WORLDGEN_ARIDITY_MAP_COUNT; map_size++) {
        for (case_index = 0;
             case_index < GAME_WORLDGEN_ARIDITY_CASE_COUNT; case_index++) {
            const GameWorldgenAridityCase *matrix_case =
                &game_worldgen_aridity_cases[case_index];
            GameWorldgenAridityResult result;
            int row_ok;
            memset(&result, 0, sizeof(result));
            game_worldgen_aridity_response_reset_overrides();
            if (!game_worldgen_aridity_response_defaults_restored(defaults)) {
                (*state_failures)++;
                return 0;
            }
            row_ok = game_worldgen_aridity_run_world(
                BASELINE_SEEDS[0], map_size, matrix_case, &result);
            game_worldgen_aridity_response_reset_overrides();
            if (!game_worldgen_aridity_response_defaults_restored(defaults)) {
                (*state_failures)++;
                return 0;
            }
            *generation_failures += !result.generated;
            *non_arid_failures += result.generated && result.non_arid_count <= 0;
            *semantic_failures += result.oasis_semantic_errors != 0 ||
                result.oasis_count != result.oasis_reachable_count || !row_ok;
            *hash_failures += result.generated && result.physical_hash == 0;
            *matrix_hash = game_worldgen_aridity_hash_mix(
                *matrix_hash, BASELINE_SEEDS[0]);
            *matrix_hash = game_worldgen_aridity_hash_mix(
                *matrix_hash, (uint32_t)map_size);
            *matrix_hash = game_worldgen_aridity_hash_mix(
                *matrix_hash, result.physical_hash);
            io_ok &= game_worldgen_aridity_write_calibration_csv_row(
                csv, *row_count, BASELINE_SEEDS[0], map_size, matrix_case,
                &result);
            (*row_count)++;
        }
    }
    return io_ok;
}

static int write_artifact_row(
    FILE *manifest, const GameWorldgenAridityResponseOptions *options,
    const char *stem, uint32_t seed, int map_size, int case_index,
    const GameWorldgenAridityResponseResult *result,
    const WorldGenAridityCalibrationArtifactResult *artifacts) {
    return manifest && fprintf(manifest,
        "%u,%d,%s,%c,%d,%d,%016llx,%d,%s_geography.bmp,%016llx,"
        "%s_climate.bmp,%016llx\n",
        seed, map_size, game_worldgen_aridity_map_names[map_size],
        'A' + case_index, artifacts->width, artifacts->height,
        (unsigned long long)result->physical_hash, result->oasis_count, stem,
        (unsigned long long)artifacts->geography_hash, stem,
        (unsigned long long)artifacts->climate_hash) > 0 && options != NULL;
}

static int run_response(
    const GameWorldgenAridityResponseOptions *options,
    const GameWorldgenAridityResponseDefaults *defaults,
    FILE *csv, FILE *artifact_manifest, uint64_t *matrix_hash,
    int *row_count, int *generation_failures, int *non_arid_failures,
    int *semantic_failures, int *hash_failures, int *state_failures,
    int *projection_mismatches, int *artifact_count,
    int *artifact_failures) {
    const char *directory = getenv(ARIDITY_OUTPUT_ENV);
    char candidate_id[32];
    char pair_id[32];
    int active_drop_index =
        game_worldgen_aridity_response_oasis_drop_index(
            options->oasis_drop);
    int active_transition_index =
        game_worldgen_aridity_response_transition_margin_index(
            options->transition_margin);
    int seed_index;
    int map_size;
    int case_index;
    int io_ok = game_worldgen_aridity_response_write_csv_header(csv);
    if (!game_worldgen_aridity_response_candidate_id(
            options, candidate_id, sizeof(candidate_id)) ||
        !game_worldgen_aridity_response_pair_id(
            options, pair_id, sizeof(pair_id)) ||
        active_drop_index < 0 || active_transition_index < 0) return 0;
    for (seed_index = 0; seed_index < 2; seed_index++) {
        for (map_size = 0;
             map_size < GAME_WORLDGEN_ARIDITY_MAP_COUNT; map_size++) {
            for (case_index = 0;
                 case_index < GAME_WORLDGEN_ARIDITY_CASE_COUNT; case_index++) {
                const GameWorldgenAridityCase *matrix_case =
                    &game_worldgen_aridity_cases[case_index];
                GameWorldgenAridityResponseResult result;
                int emit = options->emit_artifacts && seed_index == 0 &&
                    (map_size == 0 || map_size == 3);
                int row_ok;
                memset(&result, 0, sizeof(result));
                game_worldgen_aridity_response_reset_overrides();
                if (!game_worldgen_aridity_response_defaults_restored(
                        defaults) ||
                    !game_worldgen_aridity_response_enable_override(options)) {
                    (*state_failures)++;
                    return 0;
                }
                if (emit) {
                    WorldGenAridityCalibrationArtifactResult artifacts;
                    char stem[160];
                    int length = directory && directory[0] ?
                        snprintf(stem, sizeof(stem),
                            "aridity_response_%s_%s_%u_%s_%c",
                            candidate_id, pair_id, RESPONSE_SEEDS[seed_index],
                            ARTIFACT_MAP_NAMES[map_size],
                            'A' + case_index) : -1;
                    row_ok = length > 0 && (size_t)length < sizeof(stem) &&
                        game_worldgen_aridity_response_run_world_with_artifacts(
                            RESPONSE_SEEDS[seed_index], map_size, matrix_case,
                            options, directory, stem, &result, &artifacts);
                    if (row_ok && write_artifact_row(
                            artifact_manifest, options, stem,
                            RESPONSE_SEEDS[seed_index], map_size, case_index,
                            &result, &artifacts)) {
                        *artifact_count += 2;
                    } else {
                        (*artifact_failures)++;
                    }
                } else {
                    row_ok = game_worldgen_aridity_response_run_world(
                        RESPONSE_SEEDS[seed_index], map_size, matrix_case,
                        options, &result);
                }
                if (!game_worldgen_aridity_response_override_matches(options)) {
                    (*state_failures)++;
                }
                game_worldgen_aridity_response_reset_overrides();
                if (!game_worldgen_aridity_response_defaults_restored(
                        defaults)) {
                    (*state_failures)++;
                    return 0;
                }
                *generation_failures += !result.generated;
                *non_arid_failures +=
                    result.generated && result.non_arid_count <= 0;
                *semantic_failures += result.oasis_semantic_errors != 0 ||
                    result.oasis_count != result.oasis_reachable_count ||
                    !row_ok;
                *hash_failures += result.generated && result.physical_hash == 0;
                *projection_mismatches += result.oasis_count !=
                    result.projected_visible_oasis_count
                        [active_drop_index][active_transition_index];
                *matrix_hash = game_worldgen_aridity_hash_mix(
                    *matrix_hash, RESPONSE_SEEDS[seed_index]);
                *matrix_hash = game_worldgen_aridity_hash_mix(
                    *matrix_hash, (uint32_t)map_size);
                *matrix_hash = game_worldgen_aridity_hash_mix(
                    *matrix_hash, result.physical_hash);
                io_ok &= game_worldgen_aridity_response_write_csv_row(
                    csv, *row_count, RESPONSE_SEEDS[seed_index], map_size,
                    matrix_case, &result);
                (*row_count)++;
            }
        }
    }
    return io_ok;
}

int game_worldgen_aridity_response_matrix_run(
    const GameWorldgenAridityResponseOptions *options) {
    GameWorldgenAridityResponseDefaults defaults;
    char csv_path[1024];
    char summary_path[1024];
    char artifact_manifest_path[1024];
    FILE *csv = NULL;
    FILE *summary = NULL;
    FILE *artifact_manifest = NULL;
    uint64_t matrix_hash = UINT64_C(1469598103934665603);
    int row_count = 0;
    int generation_failures = 0;
    int non_arid_failures = 0;
    int semantic_failures = 0;
    int hash_failures = 0;
    int state_failures = 0;
    int projection_mismatches = 0;
    int artifact_count = 0;
    int artifact_failures = 0;
    int expected_rows;
    int io_ok = 1;
    int overall_ok;
    int result_code = 2;
    const char *csv_name;
    if (!options || options->action ==
            GAME_WORLDGEN_ARIDITY_RESPONSE_ACTION_FORMULA) return 2;
    game_worldgen_aridity_response_reset_overrides();
    if (!game_worldgen_aridity_response_capture_defaults(&defaults)) {
        goto cleanup;
    }
    csv_name = options->action ==
        GAME_WORLDGEN_ARIDITY_RESPONSE_ACTION_BASELINE ?
        "aridity_matrix.csv" : "aridity_response_matrix.csv";
    if (!game_worldgen_aridity_output_path(
            csv_path, sizeof(csv_path), csv_name) ||
        !game_worldgen_aridity_output_path(
            summary_path, sizeof(summary_path),
            "aridity_response_summary.txt") ||
        !game_worldgen_aridity_output_path_available(csv_path) ||
        !game_worldgen_aridity_output_path_available(summary_path)) {
        goto cleanup;
    }
    csv = fopen(csv_path, "wb");
    summary = fopen(summary_path, "wb");
    if (!csv || !summary) goto cleanup;
    if (options->emit_artifacts) {
        if (!game_worldgen_aridity_output_path(
                artifact_manifest_path, sizeof(artifact_manifest_path),
                "aridity_response_artifacts_manifest.csv") ||
            !game_worldgen_aridity_output_path_available(
                artifact_manifest_path)) goto cleanup;
        artifact_manifest = fopen(artifact_manifest_path, "wb");
        if (!artifact_manifest) goto cleanup;
        io_ok &= fprintf(artifact_manifest,
            "seed,map_size,map_name,case_id,width,height,physical_hash,"
            "oasis_count,geography_file,geography_pixel_hash,"
            "climate_file,climate_pixel_hash\n") > 0;
    }
    if (options->action == GAME_WORLDGEN_ARIDITY_RESPONSE_ACTION_BASELINE) {
        io_ok &= run_baseline(
            options, &defaults, csv, &matrix_hash, &row_count,
            &generation_failures, &non_arid_failures, &semantic_failures,
            &hash_failures, &state_failures);
        expected_rows = 24;
    } else {
        io_ok &= run_response(
            options, &defaults, csv, artifact_manifest, &matrix_hash,
            &row_count, &generation_failures, &non_arid_failures,
            &semantic_failures, &hash_failures, &state_failures,
            &projection_mismatches, &artifact_count, &artifact_failures);
        expected_rows = 48;
    }
    overall_ok = row_count == expected_rows && generation_failures == 0 &&
        non_arid_failures == 0 && semantic_failures == 0 &&
        hash_failures == 0 && state_failures == 0 &&
        projection_mismatches == 0 && artifact_failures == 0 &&
        (!options->emit_artifacts || artifact_count == 24) && io_ok;
    io_ok &= fprintf(summary,
        "case=aridity_response action=%s worlds_generated=%d "
        "expected_worlds=%d generation_failures=%d non_arid_failures=%d "
        "semantic_failures=%d hash_failures=%d state_failures=%d "
        "projection_mismatches=%d artifact_count=%d artifact_failures=%d "
        "matrix_hash=%016llx overall_ok=%d\n",
        options->action_name, row_count, expected_rows, generation_failures,
        non_arid_failures, semantic_failures, hash_failures, state_failures,
        projection_mismatches, artifact_count, artifact_failures,
        (unsigned long long)matrix_hash, overall_ok) > 0;
    if (fflush(csv) != 0 || fflush(summary) != 0) io_ok = 0;
    if (artifact_manifest && fflush(artifact_manifest) != 0) io_ok = 0;
    if (fclose(csv) != 0) io_ok = 0;
    csv = NULL;
    if (fclose(summary) != 0) io_ok = 0;
    summary = NULL;
    if (artifact_manifest && fclose(artifact_manifest) != 0) io_ok = 0;
    artifact_manifest = NULL;
    if (!io_ok) goto cleanup;
    printf("worldgen aridity response matrix: %s\n", csv_path);
    printf("worldgen aridity response summary: %s\n", summary_path);
    result_code = overall_ok ? 0 : 1;

cleanup:
    if (csv) fclose(csv);
    if (summary) fclose(summary);
    if (artifact_manifest) fclose(artifact_manifest);
    game_worldgen_aridity_response_reset_overrides();
    return result_code;
}
