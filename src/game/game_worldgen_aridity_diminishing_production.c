#include "game/game_worldgen_aridity_diminishing_production.h"

#include "game/game_worldgen_aridity_diminishing_options.h"
#include "game/game_worldgen_aridity_diminishing_oasis_histogram.h"
#include "game/game_worldgen_aridity_calibration_artifacts.h"
#include "game/game_worldgen_aridity_response_metrics.h"
#include "world/world_gen.h"
#include "world/world_gen_aridity_projection.h"
#include "world/world_gen_aridity_response.h"
#include "world/world_gen_classify.h"
#include "world/world_gen_context.h"
#include "world/world_gen_moisture.h"

#include <stdio.h>
#include <string.h>

static int production_defaults_match(void) {
    return !world_gen_moisture_validation_drought_divisor_active() &&
        !world_gen_classify_validation_aridity_active() &&
        !world_gen_aridity_response_validation_active() &&
        world_gen_moisture_drought_divisor() == 24 &&
        world_gen_aridity_response_arid_base() == 31 &&
        world_gen_aridity_response_desert_bias_span() == 4 &&
        world_gen_aridity_response_drought_classification_span() == 2 &&
        world_gen_aridity_response_moisture_compression_span() == 12 &&
        world_gen_aridity_response_oasis_drop() == 20 &&
        world_gen_aridity_response_transition_margin() == 2;
}

int game_worldgen_aridity_diminishing_production_options(
    GameWorldgenAridityProjectionOptions *options,
    char *pair_id, size_t pair_capacity) {
    GameWorldgenAridityResponseOptions *response;
    int written;
    game_worldgen_aridity_diminishing_reset_all();
    if (!options || !pair_id || pair_capacity == 0 ||
        !production_defaults_match()) return 0;
    memset(options, 0, sizeof(*options));
    response = &options->response;
    options->action = GAME_WORLDGEN_ARIDITY_PROJECTION_ACTION_CONFIRM;
    options->action_name = "production";
    options->has_candidate = 1;
    options->has_oasis_pair = 1;
    response->action = GAME_WORLDGEN_ARIDITY_RESPONSE_ACTION_CONFIRM;
    response->action_name = "production";
    response->use_override = 0;
    response->arid_base = world_gen_aridity_response_arid_base();
    response->desert_bias_span =
        world_gen_aridity_response_desert_bias_span();
    response->drought_divisor = world_gen_moisture_drought_divisor();
    response->moisture_compression_span =
        world_gen_aridity_response_moisture_compression_span();
    response->drought_classification_span =
        world_gen_aridity_response_drought_classification_span();
    response->oasis_drop = world_gen_aridity_response_oasis_drop();
    response->transition_margin =
        world_gen_aridity_response_transition_margin();
    written = snprintf(pair_id, pair_capacity, "o%02d_t%02d",
        response->oasis_drop, response->transition_margin);
    return written > 0 && (size_t)written < pair_capacity;
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

static int projection_ok(
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

static int prepare_actual(
    const GameWorldgenAridityCase *matrix_case,
    GameWorldgenAridityResponseResult *actual) {
    WorldGenAridityResponseLimits limits;
    if (!matrix_case || !actual ||
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

int game_worldgen_aridity_diminishing_production_run_world(
    uint32_t seed, int map_size, const GameWorldgenAridityCase *matrix_case,
    const GameWorldgenAridityProjectionOptions *options,
    const char *output_directory, const char *artifact_stem,
    GameWorldgenAridityProjectionRow *row,
    WorldGenAridityCalibrationArtifactResult *artifact_result) {
    WorldGenConfig config;
    WorldGenContext *context = NULL;
    const WorldGenDiagnostics *diagnostics;
    int artifact_requested = output_directory || artifact_stem ||
        artifact_result;
    int capture_ok = 0;
    int metrics_ok = 0;
    int preliminary_ok;
    if (artifact_result) memset(artifact_result, 0, sizeof(*artifact_result));
    if (!matrix_case || !options || !row || options->response.use_override ||
        map_size < 0 || map_size >= GAME_WORLDGEN_ARIDITY_MAP_COUNT ||
        (artifact_requested && (!output_directory || !output_directory[0] ||
         !artifact_stem || !artifact_stem[0] || !artifact_result))) return 0;
    memset(row, 0, sizeof(*row));
    row->has_actual = 1;
    row->artifact_ok = !artifact_requested;
    game_worldgen_aridity_diminishing_reset_all();
    if (!production_defaults_match() ||
        !world_gen_aridity_projection_validation_enable() ||
        !world_gen_aridity_projection_validation_matches() ||
        !prepare_actual(matrix_case, &row->actual)) goto cleanup;
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
            &row->projection) && projection_ok(context, &row->projection);
        metrics_ok = game_worldgen_aridity_response_collect_metrics(
            context, &options->response, &row->actual);
        if (artifact_requested && metrics_ok && capture_ok) {
            row->artifact_ok =
                game_worldgen_aridity_calibration_artifacts_write(
                    context, output_directory, artifact_stem,
                    artifact_result);
        }
        if (diagnostics) worldgen_attempt_note_elapsed(diagnostics->total_ms);
    }
    row->active_state_ok = production_defaults_match() &&
        world_gen_aridity_projection_validation_active() &&
        world_gen_aridity_projection_validation_matches();
    row->actual.ok = row->generated && row->physical_hash != 0 && metrics_ok;
    preliminary_ok = row->actual.ok && capture_ok && row->active_state_ok &&
        row->artifact_ok;
    if (context) world_gen_release_prepared(context);
    context = NULL;
    worldgen_attempt_finish(preliminary_ok);
    worldgen_attempt_get(&row->attempt);
    row->actual.attempt = row->attempt;

cleanup:
    if (context) world_gen_release_prepared(context);
    game_worldgen_aridity_diminishing_reset_all();
    row->restored_state_ok = production_defaults_match() &&
        !world_gen_aridity_projection_validation_active();
    row->ok = row->actual.ok && capture_ok && row->active_state_ok &&
        row->restored_state_ok && row->artifact_ok;
    return row->ok;
}
