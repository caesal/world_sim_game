#include "game/game_worldgen_aridity_response_metrics.h"

#include "world/world_gen.h"
#include "world/world_gen_aridity_response.h"
#include "world/world_gen_classify.h"
#include "world/world_gen_context.h"
#include "world/world_gen_moisture.h"

#include <string.h>

const int game_worldgen_aridity_response_oasis_drops[
    GAME_WORLDGEN_ARIDITY_RESPONSE_DROP_COUNT] = {0, 4, 8, 12, 16, 20};
const int game_worldgen_aridity_response_transition_margins[
    GAME_WORLDGEN_ARIDITY_RESPONSE_TRANSITION_COUNT] = {0, 5, 10, 15, 20, 25};

static int exact_index(const int *values, int count, int value) {
    int index;
    for (index = 0; index < count; index++) {
        if (values[index] == value) return index;
    }
    return -1;
}

int game_worldgen_aridity_response_oasis_drop_index(int value) {
    return exact_index(
        game_worldgen_aridity_response_oasis_drops,
        GAME_WORLDGEN_ARIDITY_RESPONSE_DROP_COUNT, value);
}

int game_worldgen_aridity_response_transition_margin_index(int value) {
    return exact_index(
        game_worldgen_aridity_response_transition_margins,
        GAME_WORLDGEN_ARIDITY_RESPONSE_TRANSITION_COUNT, value);
}

int game_worldgen_aridity_response_oasis_pair_valid(
    int oasis_drop, int transition_margin) {
    return game_worldgen_aridity_response_oasis_drop_index(oasis_drop) >= 0 &&
        game_worldgen_aridity_response_transition_margin_index(
            transition_margin) >= 0;
}

int game_worldgen_aridity_response_write_projected_csv_columns(FILE *file) {
    int drop;
    int transition;
    if (!file) return 0;
    for (drop = 0; drop < GAME_WORLDGEN_ARIDITY_RESPONSE_DROP_COUNT; drop++) {
        for (transition = 0;
             transition < GAME_WORLDGEN_ARIDITY_RESPONSE_TRANSITION_COUNT;
             transition++) {
            if (fprintf(file, ",projected_oasis_o%02d_t%02d",
                    game_worldgen_aridity_response_oasis_drops[drop],
                    game_worldgen_aridity_response_transition_margins[
                        transition]) <= 0) return 0;
        }
    }
    return 1;
}

int game_worldgen_aridity_response_write_projected_csv_values(
    FILE *file, const GameWorldgenAridityResponseResult *result) {
    int drop;
    int transition;
    if (!file || !result) return 0;
    for (drop = 0; drop < GAME_WORLDGEN_ARIDITY_RESPONSE_DROP_COUNT; drop++) {
        for (transition = 0;
             transition < GAME_WORLDGEN_ARIDITY_RESPONSE_TRANSITION_COUNT;
             transition++) {
            if (fprintf(file, ",%d",
                    result->projected_visible_oasis_count[drop][transition]) <=
                0) return 0;
        }
    }
    return 1;
}

static int is_arid_climate(Climate climate) {
    return climate == CLIMATE_DESERT || climate == CLIMATE_SEMI_ARID;
}

int game_worldgen_aridity_response_collect_metrics(
    const WorldGenContext *context,
    const GameWorldgenAridityResponseOptions *options,
    GameWorldgenAridityResponseResult *result) {
    int i;
    if (!context || !options || !result || context->tile_count <= 0) return 0;
    for (i = 0; i < context->tile_count; i++) {
        Geography geography;
        Climate climate;
        int channel;
        int predicate;
        int reachable;
        int drop_index;
        int transition_index;
        if (!context->land_mask[i]) continue;
        geography = (Geography)context->geography[i];
        climate = (Climate)context->climate[i];
        channel = (context->river_flags[i] & WORLD_GEN_RIVER_CHANNEL) != 0;
        result->land_count++;
        result->lake_count += geography == GEO_LAKE;
        result->terrestrial_count += geography != GEO_LAKE;
        result->desert_count += climate == CLIMATE_DESERT;
        result->semi_arid_count += climate == CLIMATE_SEMI_ARID;
        result->oasis_count += geography == GEO_OASIS;
        result->wetland_count += geography == GEO_WETLAND;
        result->river_channel_count += channel;
        result->arid_channel_count += is_arid_climate(climate) && channel;
        if (!game_worldgen_aridity_diminishing_oasis_histogram_note_tile(
                &result->diminishing_oasis_histogram, context, i, climate)) {
            return 0;
        }
        predicate = world_gen_classify_response_oasis_predicate_for_pair(
            context, i, climate, options->oasis_drop,
            options->transition_margin);
        reachable = world_gen_classify_response_visible_oasis_for_pair(
            context, i, climate, options->oasis_drop,
            options->transition_margin);
        result->oasis_predicate_count += predicate;
        result->oasis_reachable_count += reachable;
        if ((geography == GEO_OASIS) != reachable) {
            result->oasis_semantic_errors++;
        }
        if (geography == GEO_OASIS && !predicate) {
            result->oasis_semantic_errors++;
        }
        for (drop_index = 0;
             drop_index < GAME_WORLDGEN_ARIDITY_RESPONSE_DROP_COUNT;
             drop_index++) {
            for (transition_index = 0;
                 transition_index <
                     GAME_WORLDGEN_ARIDITY_RESPONSE_TRANSITION_COUNT;
                 transition_index++) {
                result->projected_visible_oasis_count
                    [drop_index][transition_index] +=
                    world_gen_classify_response_visible_oasis_for_pair(
                        context, i, climate,
                        game_worldgen_aridity_response_oasis_drops[drop_index],
                        game_worldgen_aridity_response_transition_margins[
                            transition_index]);
            }
        }
    }
    if (!game_worldgen_aridity_diminishing_oasis_histogram_finalize(
            &result->diminishing_oasis_histogram, context)) return 0;
    for (i = 0; i < GAME_WORLDGEN_ARIDITY_RESPONSE_DROP_COUNT; i++) {
        int transition_index;
        for (transition_index = 0;
             transition_index < GAME_WORLDGEN_ARIDITY_RESPONSE_TRANSITION_COUNT;
             transition_index++) {
            if (!game_worldgen_aridity_diminishing_oasis_histogram_crosscheck_legacy(
                    &result->diminishing_oasis_histogram,
                    game_worldgen_aridity_response_oasis_drops[i],
                    game_worldgen_aridity_response_transition_margins[
                        transition_index],
                    result->projected_visible_oasis_count[i][transition_index])) {
                return 0;
            }
        }
    }
    result->combined_arid_count = result->desert_count +
        result->semi_arid_count;
    result->non_arid_count = result->land_count - result->combined_arid_count;
    result->oasis_suppressed_count = result->oasis_predicate_count -
        result->oasis_reachable_count;
    return result->land_count > 0 && result->terrestrial_count > 0 &&
        result->non_arid_count > 0 && result->oasis_suppressed_count >= 0 &&
        result->oasis_semantic_errors == 0 &&
        result->oasis_count == result->oasis_reachable_count;
}

static int run_world_internal(
    uint32_t seed, int map_size, const GameWorldgenAridityCase *matrix_case,
    const GameWorldgenAridityResponseOptions *options,
    const char *output_directory, const char *artifact_stem,
    GameWorldgenAridityResponseResult *result,
    WorldGenAridityCalibrationArtifactResult *artifact_result) {
    WorldGenConfig config = DEFAULT_WORLD_GEN_CONFIG;
    WorldGenContext *context;
    WorldGenAridityResponseLimits limits;
    const WorldGenDiagnostics *diagnostics;
    int metrics_ok = 0;
    int artifact_ok = output_directory == NULL && artifact_stem == NULL;
    if (!matrix_case || !options || !result || !options->use_override ||
        map_size < 0 || map_size >= GAME_WORLDGEN_ARIDITY_MAP_COUNT ||
        !game_worldgen_aridity_response_override_matches(options) ||
        !world_gen_aridity_response_current_limits(
            matrix_case->moisture, matrix_case->drought,
            matrix_case->bias_desert, &limits)) return 0;
    memset(result, 0, sizeof(*result));
    result->drought_divisor = world_gen_moisture_drought_divisor();
    result->moisture_compression_span =
        world_gen_aridity_response_moisture_compression_span();
    result->oasis_drop = world_gen_aridity_response_oasis_drop();
    result->transition_margin =
        world_gen_aridity_response_transition_margin();
    result->combined_arid_limit = limits.combined_arid_limit;
    result->semi_arid_band = limits.semi_arid_band;
    result->desert_limit = limits.desert_limit;
    result->semi_arid_limit = limits.semi_arid_limit;
    result->oasis_limit = limits.oasis_limit;
    result->oasis_transition_limit = limits.oasis_transition_limit;
    config.moisture = matrix_case->moisture;
    config.drought = matrix_case->drought;
    config.bias_desert = matrix_case->bias_desert;
    config.seed = seed;
    config.random_seed = 0;
    worldgen_attempt_begin();
    context = world_gen_prepare_for_dimensions(
        &config, game_worldgen_aridity_map_widths[map_size],
        game_worldgen_aridity_map_heights[map_size]);
    result->generated = context != NULL;
    diagnostics = world_gen_last_diagnostics();
    if (context) {
        result->physical_hash = diagnostics ? diagnostics->physical_hash : 0;
        metrics_ok = game_worldgen_aridity_response_collect_metrics(
            context, options, result);
        if (output_directory && artifact_stem && artifact_result) {
            artifact_ok = game_worldgen_aridity_calibration_artifacts_write(
                context, output_directory, artifact_stem, artifact_result);
        }
        if (diagnostics) worldgen_attempt_note_elapsed(diagnostics->total_ms);
        world_gen_release_prepared(context);
    }
    result->ok = result->generated && result->physical_hash != 0 &&
        metrics_ok && artifact_ok;
    worldgen_attempt_finish(result->ok);
    worldgen_attempt_get(&result->attempt);
    return result->ok;
}

int game_worldgen_aridity_response_run_world(
    uint32_t seed, int map_size, const GameWorldgenAridityCase *matrix_case,
    const GameWorldgenAridityResponseOptions *options,
    GameWorldgenAridityResponseResult *result) {
    return run_world_internal(seed, map_size, matrix_case, options, NULL, NULL,
                              result, NULL);
}

int game_worldgen_aridity_response_run_world_with_artifacts(
    uint32_t seed, int map_size, const GameWorldgenAridityCase *matrix_case,
    const GameWorldgenAridityResponseOptions *options,
    const char *output_directory, const char *artifact_stem,
    GameWorldgenAridityResponseResult *result,
    WorldGenAridityCalibrationArtifactResult *artifact_result) {
    if (!output_directory || !artifact_stem || !artifact_result) return 0;
    memset(artifact_result, 0, sizeof(*artifact_result));
    return run_world_internal(seed, map_size, matrix_case, options,
                              output_directory, artifact_stem, result,
                              artifact_result);
}

static double share_of_land(int count, int land_count) {
    return land_count > 0 ? (double)count / (double)land_count : 0.0;
}

int game_worldgen_aridity_response_write_csv_header(FILE *file) {
    if (!file || fprintf(file,
        "row,seed,map_size,map_name,width,height,moisture,drought,bias_desert,"
        "combined_arid_limit,semi_arid_band,desert_limit,semi_arid_limit,"
        "oasis_limit,oasis_transition_limit,generated,failure_stage,"
        "failure_reason,physical_hash,land_count,terrestrial_count,lake_count,"
        "desert_count,semi_arid_count,combined_arid_count,non_arid_count,"
        "desert_share,semi_arid_share,combined_arid_share,non_arid_share,"
        "oasis_count,wetland_count,arid_channel_count,oasis_predicate_count,"
        "oasis_reachable_count,oasis_suppressed_count,oasis_semantic_errors,"
        "ok,drought_divisor,moisture_compression_span,oasis_drop,"
        "transition_margin,river_channel_count") <= 0) return 0;
    return game_worldgen_aridity_response_write_projected_csv_columns(file) &&
        fprintf(file, "\n") > 0;
}

int game_worldgen_aridity_response_write_csv_row(
    FILE *file, int row_index, uint32_t seed, int map_size,
    const GameWorldgenAridityCase *matrix_case,
    const GameWorldgenAridityResponseResult *result) {
    const char *failure_stage = result->generated ? "none" :
        worldgen_attempt_stage_name(result->attempt.last_failure_stage);
    const char *failure_reason = result->generated ? "none" :
        worldgen_failure_reason_name(result->attempt.last_failure_reason);
    int ok;
    if (!file || !matrix_case || !result) return 0;
    ok = fprintf(file,
        "%d,%u,%d,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s,%s,"
        "%016llx,%d,%d,%d,%d,%d,%d,%d,%.9f,%.9f,%.9f,%.9f,"
        "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
        row_index, seed, map_size, game_worldgen_aridity_map_names[map_size],
        game_worldgen_aridity_map_widths[map_size],
        game_worldgen_aridity_map_heights[map_size], matrix_case->moisture,
        matrix_case->drought, matrix_case->bias_desert,
        result->combined_arid_limit, result->semi_arid_band,
        result->desert_limit, result->semi_arid_limit, result->oasis_limit,
        result->oasis_transition_limit, result->generated, failure_stage,
        failure_reason, (unsigned long long)result->physical_hash,
        result->land_count, result->terrestrial_count, result->lake_count,
        result->desert_count, result->semi_arid_count,
        result->combined_arid_count, result->non_arid_count,
        share_of_land(result->desert_count, result->land_count),
        share_of_land(result->semi_arid_count, result->land_count),
        share_of_land(result->combined_arid_count, result->land_count),
        share_of_land(result->non_arid_count, result->land_count),
        result->oasis_count, result->wetland_count,
        result->arid_channel_count, result->oasis_predicate_count,
        result->oasis_reachable_count, result->oasis_suppressed_count,
        result->oasis_semantic_errors, result->ok, result->drought_divisor,
        result->moisture_compression_span, result->oasis_drop,
        result->transition_margin, result->river_channel_count) > 0;
    if (!ok) return 0;
    return game_worldgen_aridity_response_write_projected_csv_values(
            file, result) && fprintf(file, "\n") > 0;
}
