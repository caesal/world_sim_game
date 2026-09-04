#include "game/game_worldgen_aridity_calibration_matrix.h"

#include "game/game_worldgen_aridity_calibration_metrics.h"
#include "game/game_worldgen_aridity_calibration_options.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ARIDITY_OUTPUT_ENV "WORLD_SIM_ARIDITY_PROBE_DIR"

static const char *const ARTIFACT_MAP_NAMES[] = {
    "small", "medium", "large", "extreme"
};

int game_worldgen_aridity_calibration_matrix_run(void) {
    GameWorldgenAridityCalibrationOptions options;
    GameWorldgenAridityCalibrationDefaults defaults;
    char candidate_id[64];
    char csv_path[1024];
    char summary_path[1024];
    char artifact_manifest_path[1024];
    FILE *csv = NULL;
    FILE *summary = NULL;
    FILE *artifact_manifest = NULL;
    uint64_t matrix_hash = UINT64_C(1469598103934665603);
    int row_index = 0;
    int generation_failures = 0;
    int non_arid_failures = 0;
    int semantic_failures = 0;
    int hash_failures = 0;
    int zero_oasis_drought_worlds = 0;
    int artifact_count = 0;
    int artifact_failures = 0;
    int state_failures = 0;
    int io_ok = 1;
    int seed_index;
    int map_size;
    int case_index;
    int result_code = 2;
    game_worldgen_aridity_calibration_reset_overrides();
    if (!game_worldgen_aridity_calibration_capture_defaults(&defaults) ||
        !game_worldgen_aridity_calibration_options_parse(&options) ||
        !game_worldgen_aridity_calibration_candidate_id(
            &options, candidate_id, sizeof(candidate_id))) goto cleanup;
    if (!game_worldgen_aridity_output_path(
            csv_path, sizeof(csv_path), "aridity_matrix.csv") ||
        !game_worldgen_aridity_output_path(
            summary_path, sizeof(summary_path),
            "aridity_calibration_summary.txt") ||
        !game_worldgen_aridity_output_path_available(csv_path) ||
        !game_worldgen_aridity_output_path_available(summary_path)) goto cleanup;
    csv = fopen(csv_path, "wb");
    summary = fopen(summary_path, "wb");
    if (!csv || !summary) goto cleanup;
    if (options.stage == GAME_WORLDGEN_ARIDITY_STAGE_QUALIFICATION) {
        io_ok &= game_worldgen_aridity_write_csv_header(csv);
    } else {
        io_ok &= game_worldgen_aridity_write_calibration_csv_header(csv);
    }
    if (options.stage == GAME_WORLDGEN_ARIDITY_STAGE_FINAL) {
        if (!game_worldgen_aridity_output_path(
                artifact_manifest_path, sizeof(artifact_manifest_path),
                "aridity_artifacts_manifest.csv") ||
            !game_worldgen_aridity_output_path_available(
                artifact_manifest_path)) goto cleanup;
        artifact_manifest = fopen(artifact_manifest_path, "wb");
        if (!artifact_manifest) goto cleanup;
        io_ok &= fprintf(artifact_manifest,
            "seed,map_size,map_name,case_id,width,height,physical_hash,"
            "oasis_count,geography_file,geography_pixel_hash,"
            "climate_file,climate_pixel_hash\n") > 0;
    }
    for (seed_index = 0; seed_index < options.seed_count; seed_index++) {
        for (map_size = 0; map_size < GAME_WORLDGEN_ARIDITY_MAP_COUNT;
             map_size++) {
            for (case_index = 0;
                 case_index < GAME_WORLDGEN_ARIDITY_CASE_COUNT; case_index++) {
                const GameWorldgenAridityCase *matrix_case =
                    &game_worldgen_aridity_cases[case_index];
                GameWorldgenAridityResult result;
                int emit_artifacts =
                    options.stage == GAME_WORLDGEN_ARIDITY_STAGE_FINAL &&
                    options.seeds[seed_index] == 2026082201u;
                int row_ok;
                memset(&result, 0, sizeof(result));
                game_worldgen_aridity_calibration_reset_overrides();
                if (!game_worldgen_aridity_calibration_defaults_restored(
                        &defaults)) {
                    state_failures++;
                    goto cleanup;
                }
                if (options.use_override &&
                    !game_worldgen_aridity_calibration_enable_override(
                        &options)) {
                    state_failures++;
                    goto cleanup;
                }
                if (emit_artifacts) {
                    const char *directory = getenv(ARIDITY_OUTPUT_ENV);
                    WorldGenAridityCalibrationArtifactResult artifacts;
                    char stem[160];
                    int stem_length = directory && directory[0] ?
                        snprintf(stem, sizeof(stem),
                                 "aridity_final_%u_%s_%c",
                                 options.seeds[seed_index],
                                 ARTIFACT_MAP_NAMES[map_size],
                                 'A' + case_index) : -1;
                    int stem_ok = stem_length > 0 &&
                        (size_t)stem_length < sizeof(stem);
                    row_ok = stem_ok &&
                        game_worldgen_aridity_run_world_with_artifacts(
                            options.seeds[seed_index], map_size, matrix_case,
                            directory, stem, &result, &artifacts);
                    if (row_ok) {
                        io_ok &= fprintf(artifact_manifest,
                            "%u,%d,%s,%c,%d,%d,%016llx,%d,"
                            "%s_geography.bmp,%016llx,%s_climate.bmp,%016llx\n",
                            options.seeds[seed_index], map_size,
                            game_worldgen_aridity_map_names[map_size],
                            'A' + case_index, artifacts.width,
                            artifacts.height,
                            (unsigned long long)result.physical_hash,
                            result.oasis_count, stem,
                            (unsigned long long)artifacts.geography_hash,
                            stem,
                            (unsigned long long)artifacts.climate_hash) > 0;
                        artifact_count += 2;
                    } else {
                        artifact_failures++;
                    }
                } else {
                    row_ok = game_worldgen_aridity_run_world(
                        options.seeds[seed_index], map_size, matrix_case,
                        &result);
                }
                if (options.use_override &&
                    !game_worldgen_aridity_calibration_override_matches(
                        &options)) state_failures++;
                game_worldgen_aridity_calibration_reset_overrides();
                if (!game_worldgen_aridity_calibration_defaults_restored(
                        &defaults)) {
                    state_failures++;
                    goto cleanup;
                }
                generation_failures += !result.generated;
                non_arid_failures +=
                    result.generated && result.non_arid_count <= 0;
                semantic_failures += result.oasis_semantic_errors != 0 ||
                    result.oasis_count != result.oasis_reachable_count;
                hash_failures += result.generated && result.physical_hash == 0;
                zero_oasis_drought_worlds += matrix_case->drought == 100 &&
                    result.oasis_count == 0;
                matrix_hash = game_worldgen_aridity_hash_mix(
                    matrix_hash, options.seeds[seed_index]);
                matrix_hash = game_worldgen_aridity_hash_mix(
                    matrix_hash, (uint32_t)map_size);
                matrix_hash = game_worldgen_aridity_hash_mix(
                    matrix_hash, (uint32_t)matrix_case->moisture);
                matrix_hash = game_worldgen_aridity_hash_mix(
                    matrix_hash, (uint32_t)matrix_case->drought);
                matrix_hash = game_worldgen_aridity_hash_mix(
                    matrix_hash, (uint32_t)matrix_case->bias_desert);
                matrix_hash = game_worldgen_aridity_hash_mix(
                    matrix_hash, result.physical_hash);
                if (options.stage == GAME_WORLDGEN_ARIDITY_STAGE_QUALIFICATION) {
                    io_ok &= game_worldgen_aridity_write_csv_row(
                        csv, row_index, options.seeds[seed_index], map_size,
                        matrix_case, &result);
                } else {
                    io_ok &= game_worldgen_aridity_write_calibration_csv_row(
                        csv, row_index, options.seeds[seed_index], map_size,
                        matrix_case, &result);
                }
                row_index++;
                if (!row_ok) semantic_failures +=
                    result.generated && result.non_arid_count > 0 &&
                    result.physical_hash != 0 &&
                    result.oasis_semantic_errors == 0;
            }
        }
    }
    {
        int expected_rows = options.seed_count *
            GAME_WORLDGEN_ARIDITY_MAP_COUNT *
            GAME_WORLDGEN_ARIDITY_CASE_COUNT;
        int structural_ok = row_index == expected_rows &&
            generation_failures == 0 && non_arid_failures == 0 &&
            semantic_failures == 0 && hash_failures == 0 &&
            state_failures == 0 && artifact_failures == 0 &&
            (options.stage != GAME_WORLDGEN_ARIDITY_STAGE_FINAL ||
             artifact_count == 48) && io_ok;
        io_ok &= fprintf(summary,
            "case=aridity_calibration stage=%s mode=%s candidate=%s "
            "oasis_transition_margin=%d "
            "seeds=%d map_sizes=4 configurations=6 worlds=%d "
            "generation_failures=%d non_arid_structural_failures=%d "
            "semantic_failures=%d hash_failures=%d "
            "zero_oasis_drought_worlds=%d state_failures=%d "
            "artifact_count=%d artifact_failures=%d "
            "default_drought_divisor=%d default_desert_base=%d "
            "default_desert_bias_span=%d default_semi_arid_width=%d "
            "default_oasis_transition_margin=%d "
            "matrix_hash=%016llx structural_ok=%d\n",
            options.stage_name, options.mode_name, candidate_id,
            options.oasis_transition_margin, options.seed_count, row_index,
            generation_failures, non_arid_failures, semantic_failures,
            hash_failures, zero_oasis_drought_worlds, state_failures,
            artifact_count, artifact_failures,
            defaults.drought_divisor, defaults.desert_base,
            defaults.desert_bias_span, defaults.semi_arid_width,
            defaults.oasis_transition_margin,
            (unsigned long long)matrix_hash, structural_ok) > 0;
        if (fflush(csv) != 0 || fflush(summary) != 0) io_ok = 0;
        if (artifact_manifest && fflush(artifact_manifest) != 0) io_ok = 0;
        if (fclose(csv) != 0) io_ok = 0;
        csv = NULL;
        if (fclose(summary) != 0) io_ok = 0;
        summary = NULL;
        if (artifact_manifest && fclose(artifact_manifest) != 0) io_ok = 0;
        artifact_manifest = NULL;
        if (!io_ok) goto cleanup;
        printf("worldgen aridity calibration matrix: %s\n", csv_path);
        printf("worldgen aridity calibration summary: %s\n", summary_path);
        result_code = structural_ok ? 0 : 1;
    }
cleanup:
    if (csv) fclose(csv);
    if (summary) fclose(summary);
    if (artifact_manifest) fclose(artifact_manifest);
    game_worldgen_aridity_calibration_reset_overrides();
    return result_code;
}
