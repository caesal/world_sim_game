#include "game/game_worldgen_aridity_calibration_metrics.h"

#include "world/world_gen.h"
#include "world/world_gen_aridity_response.h"
#include "world/world_gen_classify.h"
#include "world/world_gen_context.h"
#include "world/world_gen_moisture.h"

#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define ARIDITY_OUTPUT_ENV "WORLD_SIM_ARIDITY_PROBE_DIR"

const int game_worldgen_aridity_map_widths[
    GAME_WORLDGEN_ARIDITY_MAP_COUNT] = {576, 720, 864, 1152};
const int game_worldgen_aridity_map_heights[
    GAME_WORLDGEN_ARIDITY_MAP_COUNT] = {400, 500, 600, 800};
const char *const game_worldgen_aridity_map_names[
    GAME_WORLDGEN_ARIDITY_MAP_COUNT] = {
        "Small", "Medium", "Large", "Extreme"
    };
const GameWorldgenAridityCase game_worldgen_aridity_cases[
    GAME_WORLDGEN_ARIDITY_CASE_COUNT] = {
        {50, 0, 0}, {50, 0, 100}, {50, 100, 0},
        {50, 100, 100}, {25, 100, 100}, {75, 100, 100}
    };
const int game_worldgen_aridity_oasis_transition_margins[
    GAME_WORLDGEN_ARIDITY_OASIS_MARGIN_COUNT] = {0, 5, 10, 15, 20, 25, 30};

uint64_t game_worldgen_aridity_hash_mix(uint64_t hash, uint64_t value) {
    int byte_index;
    for (byte_index = 0; byte_index < 8; byte_index++) {
        hash ^= (uint8_t)(value >> (byte_index * 8));
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

int game_worldgen_aridity_output_path(char *path, size_t capacity,
                                      const char *name) {
    const char *directory = getenv(ARIDITY_OUTPUT_ENV);
    const char *separator;
    size_t length;
    int written;
    if (!path || capacity == 0 || !name || !name[0]) return 0;
    if (!directory || !directory[0]) {
        CreateDirectoryA("logs", NULL);
        directory = "logs";
    }
    length = strlen(directory);
    separator = length > 0 &&
        (directory[length - 1] == '/' || directory[length - 1] == '\\')
        ? "" : "/";
    written = snprintf(path, capacity, "%s%s%s", directory, separator, name);
    return written > 0 && (size_t)written < capacity;
}

int game_worldgen_aridity_output_path_available(const char *path) {
    return path && GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES;
}

static int is_arid_climate(Climate climate) {
    return climate == CLIMATE_DESERT || climate == CLIMATE_SEMI_ARID;
}

static int response_semantics_active(void) {
    return world_gen_aridity_response_validation_active() ||
        !world_gen_classify_validation_aridity_active();
}

static int collect_metrics(const WorldGenContext *context,
                           GameWorldgenAridityResult *result) {
    int oasis_transition_margin;
    int oasis_drop = 0;
    int response_semantics;
    int i;
    if (!context || !result || context->tile_count <= 0) return 0;
    response_semantics = response_semantics_active();
    oasis_transition_margin = response_semantics
        ? world_gen_aridity_response_transition_margin()
        : world_gen_oasis_transition_margin();
    if (response_semantics) {
        oasis_drop = world_gen_aridity_response_oasis_drop();
    }
    for (i = 0; i < context->tile_count; i++) {
        Geography geography;
        Climate climate;
        int is_land;
        int arid;
        int channel;
        int predicate;
        int reachable = 0;
        int margin_index;
        is_land = context->land_mask[i] != 0;
        geography = (Geography)context->geography[i];
        climate = (Climate)context->climate[i];
        if (!is_land) continue;
        result->land_count++;
        result->lake_count += geography == GEO_LAKE;
        result->terrestrial_count += geography != GEO_LAKE;
        result->desert_count += climate == CLIMATE_DESERT;
        result->semi_arid_count += climate == CLIMATE_SEMI_ARID;
        result->oasis_count += geography == GEO_OASIS;
        result->wetland_count += geography == GEO_WETLAND;
        arid = is_arid_climate(climate);
        channel = (context->river_flags[i] & WORLD_GEN_RIVER_CHANNEL) != 0;
        predicate = response_semantics
            ? world_gen_classify_response_oasis_predicate_for_pair(
                context, i, climate, oasis_drop, oasis_transition_margin)
            : world_gen_classify_oasis_predicate_for_margin(
                context, i, climate, oasis_transition_margin);
        result->river_channel_count += channel;
        result->arid_channel_count += arid && channel;
        result->oasis_predicate_count += predicate;
        reachable = response_semantics
            ? world_gen_classify_response_visible_oasis_for_pair(
                context, i, climate, oasis_drop, oasis_transition_margin)
            : world_gen_classify_visible_oasis_for_margin(
                context, i, climate, oasis_transition_margin);
        for (margin_index = 0;
             margin_index < GAME_WORLDGEN_ARIDITY_OASIS_MARGIN_COUNT;
             margin_index++) {
            result->projected_visible_oasis_count[margin_index] +=
                response_semantics
                    ? world_gen_classify_response_visible_oasis_for_pair(
                        context, i, climate, oasis_drop,
                        game_worldgen_aridity_oasis_transition_margins[
                            margin_index])
                    : world_gen_classify_visible_oasis_for_margin(
                        context, i, climate,
                        game_worldgen_aridity_oasis_transition_margins[
                            margin_index]);
        }
        result->oasis_reachable_count += reachable;
        if ((geography == GEO_OASIS) != reachable) {
            result->oasis_semantic_errors++;
        }
        if (geography == GEO_OASIS && !predicate) {
            result->oasis_semantic_errors++;
        }
    }
    result->combined_arid_count = result->desert_count +
        result->semi_arid_count;
    result->non_arid_count = result->land_count - result->combined_arid_count;
    result->oasis_suppressed_count = result->oasis_predicate_count -
        result->oasis_reachable_count;
    return result->land_count > 0 && result->terrestrial_count > 0 &&
        result->combined_arid_count >= 0 && result->non_arid_count > 0 &&
        result->oasis_suppressed_count >= 0 &&
        result->oasis_semantic_errors == 0 &&
        result->oasis_count == result->oasis_reachable_count;
}

static int run_world_internal(
    uint32_t seed, int map_size, const GameWorldgenAridityCase *matrix_case,
    const char *output_directory, const char *artifact_stem,
    GameWorldgenAridityResult *result,
    WorldGenAridityCalibrationArtifactResult *artifact_result) {
    WorldGenConfig config = DEFAULT_WORLD_GEN_CONFIG;
    WorldGenContext *context;
    WorldGenAridityResponseLimits response_limits;
    const WorldGenDiagnostics *diagnostics;
    int response_semantics;
    int metrics_ok = 0;
    int artifact_ok = output_directory == NULL && artifact_stem == NULL;
    if (!matrix_case || !result || map_size < 0 ||
        map_size >= GAME_WORLDGEN_ARIDITY_MAP_COUNT) return 0;
    memset(result, 0, sizeof(*result));
    response_semantics = response_semantics_active();
    result->drought_divisor = world_gen_moisture_drought_divisor();
    if (response_semantics) {
        if (!world_gen_aridity_response_current_limits(
                matrix_case->moisture, matrix_case->drought,
                matrix_case->bias_desert, &response_limits)) return 0;
        result->desert_base = world_gen_aridity_response_arid_base();
        result->desert_bias_span =
            world_gen_aridity_response_desert_bias_span();
        result->semi_arid_width = response_limits.semi_arid_band;
        result->oasis_transition_margin =
            world_gen_aridity_response_transition_margin();
        result->desert_limit = response_limits.desert_limit;
        result->semi_arid_limit = response_limits.semi_arid_limit;
        result->oasis_limit = response_limits.oasis_limit;
        result->oasis_transition_limit =
            response_limits.oasis_transition_limit;
    } else {
        result->desert_base = world_gen_desert_base();
        result->desert_bias_span = world_gen_desert_bias_span();
        result->semi_arid_width = world_gen_semi_arid_width();
        result->oasis_transition_margin = world_gen_oasis_transition_margin();
        result->desert_limit = world_gen_desert_moisture_limit(
            matrix_case->bias_desert);
        result->semi_arid_limit = world_gen_semi_arid_moisture_limit(
            matrix_case->bias_desert);
        result->oasis_limit =
            world_gen_oasis_moisture_limit(matrix_case->drought);
        result->oasis_transition_limit =
            world_gen_oasis_transition_moisture_limit(
                matrix_case->bias_desert, matrix_case->drought);
    }
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
        metrics_ok = collect_metrics(context, result);
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

int game_worldgen_aridity_run_world(
    uint32_t seed, int map_size, const GameWorldgenAridityCase *matrix_case,
    GameWorldgenAridityResult *result) {
    return run_world_internal(seed, map_size, matrix_case, NULL, NULL,
                              result, NULL);
}

int game_worldgen_aridity_run_world_with_artifacts(
    uint32_t seed, int map_size, const GameWorldgenAridityCase *matrix_case,
    const char *output_directory, const char *artifact_stem,
    GameWorldgenAridityResult *result,
    WorldGenAridityCalibrationArtifactResult *artifact_result) {
    if (!output_directory || !artifact_stem || !artifact_result) return 0;
    memset(artifact_result, 0, sizeof(*artifact_result));
    return run_world_internal(seed, map_size, matrix_case, output_directory,
                              artifact_stem, result, artifact_result);
}

static double share_of_land(int count, int land_count) {
    return land_count > 0 ? (double)count / (double)land_count : 0.0;
}

int game_worldgen_aridity_write_csv_header(FILE *file) {
    return file && fprintf(file,
        "row,seed,map_size,map_name,width,height,moisture,drought,bias_desert,"
        "desert_limit,semi_arid_limit,oasis_limit,generated,failure_stage,"
        "failure_reason,physical_hash,land_count,terrestrial_count,lake_count,"
        "desert_count,semi_arid_count,combined_arid_count,non_arid_count,"
        "desert_share,semi_arid_share,combined_arid_share,non_arid_share,"
        "oasis_count,wetland_count,arid_channel_count,oasis_predicate_count,"
        "oasis_reachable_count,oasis_suppressed_count,oasis_semantic_errors,ok\n") > 0;
}

int game_worldgen_aridity_write_csv_row(
    FILE *file, int row_index, uint32_t seed, int map_size,
    const GameWorldgenAridityCase *matrix_case,
    const GameWorldgenAridityResult *result) {
    const char *failure_stage = result->generated ? "none" :
        worldgen_attempt_stage_name(result->attempt.last_failure_stage);
    const char *failure_reason = result->generated ? "none" :
        worldgen_failure_reason_name(result->attempt.last_failure_reason);
    return fprintf(file,
        "%d,%u,%d,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s,%s,%016llx,"
        "%d,%d,%d,%d,%d,%d,%d,%.9f,%.9f,%.9f,%.9f,"
        "%d,%d,%d,%d,%d,%d,%d,%d\n",
        row_index, seed, map_size, game_worldgen_aridity_map_names[map_size],
        game_worldgen_aridity_map_widths[map_size],
        game_worldgen_aridity_map_heights[map_size], matrix_case->moisture,
        matrix_case->drought, matrix_case->bias_desert,
        result->desert_limit, result->semi_arid_limit, result->oasis_limit,
        result->generated, failure_stage, failure_reason,
        (unsigned long long)result->physical_hash, result->land_count,
        result->terrestrial_count, result->lake_count, result->desert_count,
        result->semi_arid_count, result->combined_arid_count,
        result->non_arid_count,
        share_of_land(result->desert_count, result->land_count),
        share_of_land(result->semi_arid_count, result->land_count),
        share_of_land(result->combined_arid_count, result->land_count),
        share_of_land(result->non_arid_count, result->land_count),
        result->oasis_count, result->wetland_count,
        result->arid_channel_count, result->oasis_predicate_count,
        result->oasis_reachable_count, result->oasis_suppressed_count,
        result->oasis_semantic_errors, result->ok) > 0;
}

int game_worldgen_aridity_write_calibration_csv_header(FILE *file) {
    return file && fprintf(file,
        "row,seed,map_size,map_name,width,height,moisture,drought,bias_desert,"
        "desert_limit,semi_arid_limit,oasis_limit,generated,failure_stage,"
        "failure_reason,physical_hash,land_count,terrestrial_count,lake_count,"
        "desert_count,semi_arid_count,combined_arid_count,non_arid_count,"
        "desert_share,semi_arid_share,combined_arid_share,non_arid_share,"
        "oasis_count,wetland_count,arid_channel_count,oasis_predicate_count,"
        "oasis_reachable_count,oasis_suppressed_count,oasis_semantic_errors,ok,"
        "drought_divisor,desert_base,desert_bias_span,semi_arid_width,"
        "oasis_transition_margin,oasis_transition_limit,river_channel_count,"
        "projected_visible_oasis_margin_0,projected_visible_oasis_margin_5,"
        "projected_visible_oasis_margin_10,projected_visible_oasis_margin_15,"
        "projected_visible_oasis_margin_20,projected_visible_oasis_margin_25,"
        "projected_visible_oasis_margin_30\n") > 0;
}

int game_worldgen_aridity_write_calibration_csv_row(
    FILE *file, int row_index, uint32_t seed, int map_size,
    const GameWorldgenAridityCase *matrix_case,
    const GameWorldgenAridityResult *result) {
    const char *failure_stage = result->generated ? "none" :
        worldgen_attempt_stage_name(result->attempt.last_failure_stage);
    const char *failure_reason = result->generated ? "none" :
        worldgen_failure_reason_name(result->attempt.last_failure_reason);
    return fprintf(file,
        "%d,%u,%d,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s,%s,%016llx,"
        "%d,%d,%d,%d,%d,%d,%d,%.9f,%.9f,%.9f,%.9f,"
        "%d,%d,%d,%d,%d,%d,%d,%d,"
        "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
        row_index, seed, map_size, game_worldgen_aridity_map_names[map_size],
        game_worldgen_aridity_map_widths[map_size],
        game_worldgen_aridity_map_heights[map_size], matrix_case->moisture,
        matrix_case->drought, matrix_case->bias_desert,
        result->desert_limit, result->semi_arid_limit, result->oasis_limit,
        result->generated, failure_stage, failure_reason,
        (unsigned long long)result->physical_hash, result->land_count,
        result->terrestrial_count, result->lake_count, result->desert_count,
        result->semi_arid_count, result->combined_arid_count,
        result->non_arid_count,
        share_of_land(result->desert_count, result->land_count),
        share_of_land(result->semi_arid_count, result->land_count),
        share_of_land(result->combined_arid_count, result->land_count),
        share_of_land(result->non_arid_count, result->land_count),
        result->oasis_count, result->wetland_count,
        result->arid_channel_count, result->oasis_predicate_count,
        result->oasis_reachable_count, result->oasis_suppressed_count,
        result->oasis_semantic_errors, result->ok,
        result->drought_divisor, result->desert_base,
        result->desert_bias_span, result->semi_arid_width,
        result->oasis_transition_margin, result->oasis_transition_limit,
        result->river_channel_count,
        result->projected_visible_oasis_count[0],
        result->projected_visible_oasis_count[1],
        result->projected_visible_oasis_count[2],
        result->projected_visible_oasis_count[3],
        result->projected_visible_oasis_count[4],
        result->projected_visible_oasis_count[5],
        result->projected_visible_oasis_count[6]) > 0;
}
