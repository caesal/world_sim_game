#include "game/game_worldgen_aridity_response_projection_histogram.h"

#include "world/world_gen.h"
#include "world/world_gen_aridity_response.h"
#include "world/world_gen_context.h"
#include "world/world_gen_moisture.h"

#include <string.h>

static int map_size_valid(int map_size) {
    return map_size >= 0 && map_size < GAME_WORLDGEN_ARIDITY_MAP_COUNT;
}

static void configure_world(
    WorldGenConfig *config, uint32_t seed,
    const GameWorldgenAridityCase *matrix_case) {
    *config = DEFAULT_WORLD_GEN_CONFIG;
    config->moisture = matrix_case->moisture;
    config->drought = matrix_case->drought;
    config->bias_desert = matrix_case->bias_desert;
    config->seed = seed;
    config->random_seed = 0;
}

static int projection_result_ok(
    const WorldGenContext *context,
    const WorldGenAridityProjectionResult *projection) {
    int prefix = 0;
    int bin;
    if (!context || !projection || !projection->complete ||
        !projection->accounting_ok ||
        projection->climate_input_hash == 0 ||
        projection->width != context->width ||
        projection->height != context->height ||
        projection->master_seed != context->master_seed ||
        projection->land_count <= 0 || projection->eligible_count < 0 ||
        projection->eligible_count > projection->land_count) return 0;
    for (bin = 0; bin < WORLD_GEN_ARIDITY_PROJECTION_BIN_COUNT; bin++) {
        if (projection->histogram[bin] < 0) return 0;
        prefix += projection->histogram[bin];
        if (projection->prefix[bin] != prefix) return 0;
    }
    return prefix == projection->eligible_count;
}

static int prepare_actual_result(
    const GameWorldgenAridityCase *matrix_case,
    const GameWorldgenAridityProjectionOptions *options,
    GameWorldgenAridityResponseResult *actual) {
    WorldGenAridityResponseLimits limits;
    if (!matrix_case || !options || !actual ||
        !world_gen_aridity_response_current_limits(
            matrix_case->moisture, matrix_case->drought,
            matrix_case->bias_desert, &limits)) return 0;
    memset(actual, 0, sizeof(*actual));
    actual->drought_divisor = world_gen_moisture_drought_divisor();
    actual->moisture_compression_span =
        world_gen_aridity_response_moisture_compression_span();
    actual->oasis_drop = world_gen_aridity_response_oasis_drop();
    actual->transition_margin =
        world_gen_aridity_response_transition_margin();
    actual->combined_arid_limit = limits.combined_arid_limit;
    actual->semi_arid_band = limits.semi_arid_band;
    actual->desert_limit = limits.desert_limit;
    actual->semi_arid_limit = limits.semi_arid_limit;
    actual->oasis_limit = limits.oasis_limit;
    actual->oasis_transition_limit = limits.oasis_transition_limit;
    return 1;
}

int game_worldgen_aridity_projection_run_carrier(
    uint32_t seed, int map_size, const GameWorldgenAridityCase *matrix_case,
    int drought_divisor, const GameWorldgenAridityResponseDefaults *defaults,
    GameWorldgenAridityProjectionRow *row) {
    WorldGenConfig config;
    WorldGenContext *context = NULL;
    const WorldGenDiagnostics *diagnostics;
    int capture_ok = 0;
    int preliminary_ok;
    if (!matrix_case || !defaults || !row || !map_size_valid(map_size)) {
        return 0;
    }
    memset(row, 0, sizeof(*row));
    game_worldgen_aridity_projection_reset_all();
    if (!game_worldgen_aridity_projection_defaults_restored(defaults) ||
        !game_worldgen_aridity_projection_enable_carrier(drought_divisor)) {
        goto cleanup;
    }
    configure_world(&config, seed, matrix_case);
    worldgen_attempt_begin();
    context = world_gen_prepare_for_dimensions(
        &config, game_worldgen_aridity_map_widths[map_size],
        game_worldgen_aridity_map_heights[map_size]);
    row->generated = context != NULL;
    diagnostics = world_gen_last_diagnostics();
    if (context) {
        row->physical_hash = diagnostics ? diagnostics->physical_hash : 0;
        capture_ok = world_gen_aridity_projection_get_result(
            &row->projection) &&
            projection_result_ok(context, &row->projection);
        if (diagnostics) worldgen_attempt_note_elapsed(diagnostics->total_ms);
    }
    row->active_state_ok =
        game_worldgen_aridity_projection_carrier_matches(drought_divisor);
    preliminary_ok = row->generated && row->physical_hash != 0 &&
        capture_ok && row->active_state_ok;
    if (context) world_gen_release_prepared(context);
    context = NULL;
    worldgen_attempt_finish(preliminary_ok);
    worldgen_attempt_get(&row->attempt);

cleanup:
    if (context) world_gen_release_prepared(context);
    game_worldgen_aridity_projection_reset_all();
    row->restored_state_ok =
        game_worldgen_aridity_projection_defaults_restored(defaults);
    row->artifact_ok = 1;
    row->ok = row->generated && row->physical_hash != 0 && capture_ok &&
        row->active_state_ok && row->restored_state_ok;
    return row->ok;
}

int game_worldgen_aridity_projection_run_actual(
    uint32_t seed, int map_size, const GameWorldgenAridityCase *matrix_case,
    const GameWorldgenAridityProjectionOptions *options,
    const GameWorldgenAridityResponseDefaults *defaults,
    const char *output_directory, const char *artifact_stem,
    GameWorldgenAridityProjectionRow *row,
    WorldGenAridityCalibrationArtifactResult *artifact_result) {
    WorldGenConfig config;
    WorldGenContext *context = NULL;
    const WorldGenDiagnostics *diagnostics;
    int capture_ok = 0;
    int metrics_ok = 0;
    int preliminary_ok;
    int wants_artifact = output_directory && artifact_stem && artifact_result;
    if (!matrix_case || !options || !defaults || !row ||
        !options->has_candidate || !map_size_valid(map_size)) return 0;
    memset(row, 0, sizeof(*row));
    if (artifact_result) memset(artifact_result, 0, sizeof(*artifact_result));
    row->has_actual = 1;
    row->artifact_ok = !wants_artifact;
    game_worldgen_aridity_projection_reset_all();
    if (!game_worldgen_aridity_projection_defaults_restored(defaults) ||
        !game_worldgen_aridity_projection_enable_actual(options) ||
        !prepare_actual_result(matrix_case, options, &row->actual)) {
        goto cleanup;
    }
    configure_world(&config, seed, matrix_case);
    worldgen_attempt_begin();
    context = world_gen_prepare_for_dimensions(
        &config, game_worldgen_aridity_map_widths[map_size],
        game_worldgen_aridity_map_heights[map_size]);
    row->generated = context != NULL;
    row->actual.generated = row->generated;
    diagnostics = world_gen_last_diagnostics();
    if (context) {
        row->physical_hash = diagnostics ? diagnostics->physical_hash : 0;
        row->actual.physical_hash = row->physical_hash;
        capture_ok = world_gen_aridity_projection_get_result(
            &row->projection) &&
            projection_result_ok(context, &row->projection);
        metrics_ok = game_worldgen_aridity_response_collect_metrics(
            context, &options->response, &row->actual);
        if (wants_artifact) {
            row->artifact_ok =
                game_worldgen_aridity_calibration_artifacts_write(
                    context, output_directory, artifact_stem,
                    artifact_result);
        }
        if (diagnostics) worldgen_attempt_note_elapsed(diagnostics->total_ms);
    }
    row->active_state_ok =
        game_worldgen_aridity_projection_actual_matches(options);
    row->actual.ok = row->generated && row->physical_hash != 0 &&
        metrics_ok && row->artifact_ok;
    preliminary_ok = row->actual.ok && capture_ok && row->active_state_ok;
    if (context) world_gen_release_prepared(context);
    context = NULL;
    worldgen_attempt_finish(preliminary_ok);
    worldgen_attempt_get(&row->attempt);
    row->actual.attempt = row->attempt;

cleanup:
    if (context) world_gen_release_prepared(context);
    game_worldgen_aridity_projection_reset_all();
    row->restored_state_ok =
        game_worldgen_aridity_projection_defaults_restored(defaults);
    row->ok = row->actual.ok && capture_ok && row->active_state_ok &&
        row->restored_state_ok;
    return row->ok;
}

int game_worldgen_aridity_projection_write_carrier_header(FILE *file) {
    int bin;
    if (!file || fprintf(file,
        "row,carrier_id,carrier_role,seed,map_size,map_name,width,height,"
        "moisture,drought,bias_desert,drought_divisor,generated,"
        "failure_stage,failure_reason,physical_hash,climate_input_hash,"
        "master_seed,land_count,arid_eligible_count,active_state_ok,"
        "restored_state_ok,ok") <= 0) return 0;
    for (bin = 0; bin < WORLD_GEN_ARIDITY_PROJECTION_BIN_COUNT; bin++) {
        if (fprintf(file, ",hist_%03d", bin) <= 0) return 0;
    }
    for (bin = 0; bin < WORLD_GEN_ARIDITY_PROJECTION_BIN_COUNT; bin++) {
        if (fprintf(file, ",prefix_%03d", bin) <= 0) return 0;
    }
    return fprintf(file, "\n") > 0;
}

int game_worldgen_aridity_projection_write_carrier_row(
    FILE *file, int row_index, const char *carrier_id,
    const char *source_cases, uint32_t seed, int map_size,
    const GameWorldgenAridityCase *matrix_case, int drought_divisor,
    const GameWorldgenAridityProjectionRow *row) {
    const char *stage;
    const char *reason;
    int bin;
    if (!file || !carrier_id || !source_cases || !matrix_case || !row ||
        !map_size_valid(map_size)) return 0;
    stage = row->generated ? "none" :
        worldgen_attempt_stage_name(row->attempt.last_failure_stage);
    reason = row->generated ? "none" :
        worldgen_failure_reason_name(row->attempt.last_failure_reason);
    if (fprintf(file,
        "%d,%s,%s,%u,%d,%s,%d,%d,%d,%d,%d,%d,%d,%s,%s,%016llx,"
        "%016llx,%u,%d,%d,%d,%d,%d",
        row_index, carrier_id, source_cases, seed, map_size,
        game_worldgen_aridity_map_names[map_size],
        game_worldgen_aridity_map_widths[map_size],
        game_worldgen_aridity_map_heights[map_size], matrix_case->moisture,
        matrix_case->drought, matrix_case->bias_desert, drought_divisor,
        row->generated, stage, reason,
        (unsigned long long)row->physical_hash,
        (unsigned long long)row->projection.climate_input_hash,
        row->projection.master_seed, row->projection.land_count,
        row->projection.eligible_count, row->active_state_ok,
        row->restored_state_ok, row->ok) <= 0) return 0;
    for (bin = 0; bin < WORLD_GEN_ARIDITY_PROJECTION_BIN_COUNT; bin++) {
        if (fprintf(file, ",%d", row->projection.histogram[bin]) <= 0) {
            return 0;
        }
    }
    for (bin = 0; bin < WORLD_GEN_ARIDITY_PROJECTION_BIN_COUNT; bin++) {
        if (fprintf(file, ",%d", row->projection.prefix[bin]) <= 0) {
            return 0;
        }
    }
    return fprintf(file, "\n") > 0;
}

int game_worldgen_aridity_projection_write_actual_header(FILE *file) {
    if (!file || fprintf(file,
        "row,mode,candidate_id,pair_id,seed,map_size,map_name,width,height,"
        "case_id,moisture,drought,bias_desert,arid_base,desert_bias_span,"
        "drought_divisor,moisture_compression_span,oasis_drop,"
        "transition_margin,combined_arid_limit,semi_arid_band,desert_limit,"
        "semi_arid_limit,oasis_limit,oasis_transition_limit,generated,"
        "failure_stage,failure_reason,physical_hash,climate_input_hash,"
        "projection_land_count,projection_eligible_count,land_count,"
        "terrestrial_count,lake_count,desert_count,semi_arid_count,"
        "combined_arid_count,non_arid_count,oasis_count,wetland_count,"
        "river_channel_count,arid_channel_count,oasis_predicate_count,"
        "oasis_reachable_count,oasis_suppressed_count,oasis_semantic_errors") <=
        0) return 0;
    return game_worldgen_aridity_response_write_projected_csv_columns(file) &&
        fprintf(file,
            ",active_state_ok,restored_state_ok,artifact_ok,ok\n") > 0;
}

int game_worldgen_aridity_projection_write_actual_row(
    FILE *file, int row_index, const char *mode, const char *candidate_id,
    const char *pair_id, uint32_t seed, int map_size, int case_index,
    const GameWorldgenAridityCase *matrix_case,
    const GameWorldgenAridityProjectionOptions *options,
    const GameWorldgenAridityProjectionRow *row) {
    const GameWorldgenAridityResponseResult *actual;
    const char *stage;
    const char *reason;
    if (!file || !mode || !candidate_id || !pair_id || !matrix_case ||
        !options || !row || !row->has_actual || !map_size_valid(map_size) ||
        case_index < 0 || case_index >= GAME_WORLDGEN_ARIDITY_CASE_COUNT) {
        return 0;
    }
    actual = &row->actual;
    stage = row->generated ? "none" :
        worldgen_attempt_stage_name(row->attempt.last_failure_stage);
    reason = row->generated ? "none" :
        worldgen_failure_reason_name(row->attempt.last_failure_reason);
    if (fprintf(file,
        "%d,%s,%s,%s,%u,%d,%s,%d,%d,%c,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
        "%d,%d,%d,%d,%d,%d,%d,%s,%s,%016llx,%016llx,%d,%d,%d,%d,%d,"
        "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
        row_index, mode, candidate_id, pair_id, seed, map_size,
        game_worldgen_aridity_map_names[map_size],
        game_worldgen_aridity_map_widths[map_size],
        game_worldgen_aridity_map_heights[map_size], 'A' + case_index,
        matrix_case->moisture, matrix_case->drought,
        matrix_case->bias_desert, options->response.arid_base,
        options->response.desert_bias_span,
        options->response.drought_divisor,
        options->response.moisture_compression_span,
        options->response.oasis_drop, options->response.transition_margin,
        actual->combined_arid_limit, actual->semi_arid_band,
        actual->desert_limit, actual->semi_arid_limit, actual->oasis_limit,
        actual->oasis_transition_limit, row->generated, stage, reason,
        (unsigned long long)row->physical_hash,
        (unsigned long long)row->projection.climate_input_hash,
        row->projection.land_count, row->projection.eligible_count,
        actual->land_count, actual->terrestrial_count, actual->lake_count,
        actual->desert_count, actual->semi_arid_count,
        actual->combined_arid_count, actual->non_arid_count,
        actual->oasis_count, actual->wetland_count,
        actual->river_channel_count, actual->arid_channel_count,
        actual->oasis_predicate_count, actual->oasis_reachable_count,
        actual->oasis_suppressed_count, actual->oasis_semantic_errors) <= 0) {
        return 0;
    }
    if (!game_worldgen_aridity_response_write_projected_csv_values(
            file, actual)) return 0;
    return fprintf(file, ",%d,%d,%d,%d\n", row->active_state_ok,
        row->restored_state_ok, row->artifact_ok, row->ok) > 0;
}
