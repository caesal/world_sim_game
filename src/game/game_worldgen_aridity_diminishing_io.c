#include "game/game_worldgen_aridity_diminishing_io.h"

#include <stdint.h>

static int dryness_response(
    int drought, int bias_desert,
    const GameWorldgenAridityResponseOptions *response) {
    int64_t bias_numerator = (int64_t)bias_desert *
        response->desert_bias_span * 100;
    int64_t drought_numerator = (int64_t)drought *
        response->drought_classification_span * (100 - bias_desert);
    return (int)((bias_numerator + drought_numerator) / 10000);
}

static int write_climate_accounting_columns(FILE *file) {
    int climate;
    int source;
    int target;
    if (!file) return 0;
    for (climate = 0; climate < CLIMATE_COUNT; climate++) {
        if (fprintf(file, ",pre_climate_%02d", climate) <= 0) return 0;
    }
    for (climate = 0; climate < CLIMATE_COUNT; climate++) {
        if (fprintf(file, ",post_climate_%02d", climate) <= 0) return 0;
    }
    if (fprintf(file,
            ",refresh_eligible_count,refresh_changed_count,"
            "non_refresh_changed_count,transition_land_count,"
            "accounting_unexplained_count,climate_accounting_ok") <= 0) {
        return 0;
    }
    for (source = 0; source < CLIMATE_COUNT; source++) {
        for (target = 0; target < CLIMATE_COUNT; target++) {
            if (fprintf(file, ",nonrefresh_transition_%02d_%02d",
                    source, target) <= 0) return 0;
        }
    }
    for (source = 0; source < CLIMATE_COUNT; source++) {
        for (target = 0; target < CLIMATE_COUNT; target++) {
            if (fprintf(file, ",refresh_transition_%02d_%02d",
                    source, target) <= 0) return 0;
        }
    }
    return 1;
}

static int write_climate_accounting_values(
    FILE *file, const WorldGenAridityProjectionResult *projection) {
    int climate;
    int source;
    int target;
    if (!file || !projection) return 0;
    for (climate = 0; climate < CLIMATE_COUNT; climate++) {
        if (fprintf(file, ",%d", projection->pre_climate_count[climate]) <= 0) {
            return 0;
        }
    }
    for (climate = 0; climate < CLIMATE_COUNT; climate++) {
        if (fprintf(file, ",%d", projection->post_climate_count[climate]) <= 0) {
            return 0;
        }
    }
    if (fprintf(file, ",%d,%d,%d,%d,%d,%d",
            projection->refresh_eligible_count,
            projection->refresh_changed_count,
            projection->non_refresh_changed_count,
            projection->transition_accounted_count,
            projection->accounting_unexplained_count,
            projection->accounting_ok) <= 0) return 0;
    for (source = 0; source < CLIMATE_COUNT; source++) {
        for (target = 0; target < CLIMATE_COUNT; target++) {
            if (fprintf(file, ",%d", projection->transition_count
                    [WORLD_GEN_ARIDITY_TRANSITION_NON_REFRESH]
                    [source][target]) <= 0) return 0;
        }
    }
    for (source = 0; source < CLIMATE_COUNT; source++) {
        for (target = 0; target < CLIMATE_COUNT; target++) {
            if (fprintf(file, ",%d", projection->transition_count
                    [WORLD_GEN_ARIDITY_TRANSITION_REFRESH]
                    [source][target]) <= 0) return 0;
        }
    }
    return 1;
}

int game_worldgen_aridity_diminishing_write_actual_header(FILE *file) {
    if (!file || fprintf(file,
        "row,mode,candidate_id,pair_id,seed,map_size,map_name,width,height,"
        "case_id,moisture,drought,bias_desert,arid_base,desert_bias_span,"
        "drought_divisor,moisture_compression_span,"
        "drought_classification_span,dryness_response,oasis_drop,"
        "transition_margin,combined_arid_limit,semi_arid_band,desert_limit,"
        "semi_arid_limit,oasis_limit,oasis_transition_limit,generated,"
        "failure_stage,failure_reason,physical_hash,climate_input_hash,"
        "land_count,desert_count,semi_arid_count,combined_arid_count,"
        "non_arid_count,oasis_count,wetland_count,river_channel_count,"
        "oasis_semantic_errors,active_state_ok,restored_state_ok,ok") <= 0) {
        return 0;
    }
    return write_climate_accounting_columns(file) &&
        game_worldgen_aridity_response_write_projected_csv_columns(file) &&
        fprintf(file, "\n") > 0;
}

int game_worldgen_aridity_diminishing_write_actual_row(
    FILE *file, int row_index, const char *mode, const char *candidate_id,
    const char *pair_id, uint32_t seed, int map_size, int case_index,
    const GameWorldgenAridityCase *matrix_case,
    const GameWorldgenAridityProjectionOptions *options,
    const GameWorldgenAridityProjectionRow *row) {
    const GameWorldgenAridityResponseResult *actual;
    const GameWorldgenAridityResponseOptions *response;
    const char *stage;
    const char *reason;
    if (!file || !mode || !candidate_id || !pair_id || !matrix_case ||
        !options || !row || !row->has_actual || map_size < 0 ||
        map_size >= GAME_WORLDGEN_ARIDITY_MAP_COUNT || case_index < 0 ||
        case_index >= GAME_WORLDGEN_ARIDITY_CASE_COUNT) return 0;
    actual = &row->actual;
    response = &options->response;
    stage = row->generated ? "none" :
        worldgen_attempt_stage_name(row->attempt.last_failure_stage);
    reason = row->generated ? "none" :
        worldgen_failure_reason_name(row->attempt.last_failure_reason);
    if (fprintf(file,
        "%d,%s,%s,%s,%u,%d,%s,%d,%d,%c,%d,%d,%d,%d,%d,%d,%d,%d,"
        "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s,%s,%016llx,%016llx,%d,%d,"
        "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
        row_index, mode, candidate_id, pair_id, seed, map_size,
        game_worldgen_aridity_map_names[map_size],
        game_worldgen_aridity_map_widths[map_size],
        game_worldgen_aridity_map_heights[map_size], 'A' + case_index,
        matrix_case->moisture, matrix_case->drought,
        matrix_case->bias_desert, response->arid_base,
        response->desert_bias_span, response->drought_divisor,
        response->moisture_compression_span,
        response->drought_classification_span,
        dryness_response(matrix_case->drought, matrix_case->bias_desert,
                         response),
        response->oasis_drop, response->transition_margin,
        actual->combined_arid_limit, actual->semi_arid_band,
        actual->desert_limit, actual->semi_arid_limit, actual->oasis_limit,
        actual->oasis_transition_limit, row->generated, stage, reason,
        (unsigned long long)row->physical_hash,
        (unsigned long long)row->projection.climate_input_hash,
        actual->land_count, actual->desert_count, actual->semi_arid_count,
        actual->combined_arid_count, actual->non_arid_count,
        actual->oasis_count, actual->wetland_count,
        actual->river_channel_count, actual->oasis_semantic_errors,
        row->active_state_ok, row->restored_state_ok, row->ok) <= 0) {
        return 0;
    }
    return write_climate_accounting_values(file, &row->projection) &&
        game_worldgen_aridity_response_write_projected_csv_values(
            file, actual) && fprintf(file, "\n") > 0;
}
