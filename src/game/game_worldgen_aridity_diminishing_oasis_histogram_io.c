#include "game/game_worldgen_aridity_diminishing_oasis_histogram_io.h"

#include "game/game_worldgen_aridity_diminishing_oasis_histogram.h"

static int projection_prefix_below(
    const WorldGenAridityProjectionResult *projection, int exclusive_limit) {
    if (!projection || exclusive_limit <= 0) return 0;
    if (exclusive_limit > WORLD_GEN_ARIDITY_PROJECTION_BIN_COUNT) {
        exclusive_limit = WORLD_GEN_ARIDITY_PROJECTION_BIN_COUNT;
    }
    return projection->prefix[exclusive_limit - 1];
}

static int write_histogram_columns(FILE *file) {
    int value;
    int drop;
    int margin;
    for (value = 0;
         value < GAME_WORLDGEN_ARIDITY_DIMINISHING_MOISTURE_COUNT; value++) {
        if (fprintf(file, ",hist_%03d", value) <= 0) return 0;
    }
    for (value = 0;
         value < GAME_WORLDGEN_ARIDITY_DIMINISHING_MOISTURE_COUNT; value++) {
        if (fprintf(file, ",prefix_%03d", value) <= 0) return 0;
    }
    for (drop = 0; drop < GAME_WORLDGEN_ARIDITY_DIMINISHING_DROP_COUNT;
         drop++) {
        for (margin = 0;
             margin < GAME_WORLDGEN_ARIDITY_DIMINISHING_MARGIN_COUNT;
             margin++) {
            if (fprintf(file, ",projected_oasis_o%02d_t%02d",
                    drop, margin) <= 0) return 0;
        }
    }
    return 1;
}

int game_worldgen_aridity_diminishing_oasis_histogram_write_header(FILE *file) {
    if (!file || fprintf(file,
            "row,mode,candidate_id,pair_id,seed,map_size,map_name,case_id,"
            "drought,combined_arid_limit,desert_limit,semi_arid_limit,"
            "land_count,river_channel_count,eligible_count,histogram_total,"
            "prefix_total,oracle_pair_cases,oracle_bin_checks,"
            "oracle_mismatches,legacy_36_cases,legacy_36_mismatches,"
            "active_projected_oasis_count,projected_pre_desert_count,"
            "projected_pre_semi_arid_count,"
            "projected_pre_combined_arid_count,"
            "projected_pre_non_arid_count,projection_pre_identity_ok,"
            "histogram_ok") <= 0) return 0;
    return write_histogram_columns(file) && fprintf(file, "\n") > 0;
}

static int write_histogram_values(
    FILE *file,
    const GameWorldgenAridityDiminishingOasisHistogram *histogram) {
    int value;
    int drop;
    int margin;
    for (value = 0;
         value < GAME_WORLDGEN_ARIDITY_DIMINISHING_MOISTURE_COUNT; value++) {
        if (fprintf(file, ",%d", histogram->moisture_bins[value]) <= 0) {
            return 0;
        }
    }
    for (value = 0;
         value < GAME_WORLDGEN_ARIDITY_DIMINISHING_MOISTURE_COUNT; value++) {
        if (fprintf(file, ",%d", histogram->moisture_prefix[value]) <= 0) {
            return 0;
        }
    }
    for (drop = 0; drop < GAME_WORLDGEN_ARIDITY_DIMINISHING_DROP_COUNT;
         drop++) {
        for (margin = 0;
             margin < GAME_WORLDGEN_ARIDITY_DIMINISHING_MARGIN_COUNT;
             margin++) {
            if (fprintf(file, ",%d", histogram->projected_visible_oasis_count
                    [drop][margin]) <= 0) return 0;
        }
    }
    return 1;
}

int game_worldgen_aridity_diminishing_oasis_histogram_write_row(
    FILE *file, int row_index, const char *mode, const char *candidate_id,
    const char *pair_id, uint32_t seed, int map_size, int case_index,
    const GameWorldgenAridityProjectionOptions *options,
    const GameWorldgenAridityProjectionRow *row) {
    const GameWorldgenAridityResponseResult *actual;
    const GameWorldgenAridityDiminishingOasisHistogram *histogram;
    int histogram_total = 0;
    int projected_desert;
    int projected_combined;
    int projected_semi;
    int projected_non_arid;
    int projection_identity;
    int active_count = -1;
    int value;
    if (!file || !mode || !candidate_id || !pair_id || !options || !row ||
        !row->has_actual || map_size < 0 ||
        map_size >= GAME_WORLDGEN_ARIDITY_MAP_COUNT || case_index < 0 ||
        case_index >= GAME_WORLDGEN_ARIDITY_CASE_COUNT) return 0;
    actual = &row->actual;
    histogram = &actual->diminishing_oasis_histogram;
    for (value = 0;
         value < GAME_WORLDGEN_ARIDITY_DIMINISHING_MOISTURE_COUNT; value++) {
        histogram_total += histogram->moisture_bins[value];
    }
    projected_desert = projection_prefix_below(
        &row->projection, actual->desert_limit);
    projected_combined = projection_prefix_below(
        &row->projection, actual->semi_arid_limit);
    projected_semi = projected_combined - projected_desert;
    projected_non_arid = actual->land_count - projected_combined;
    projection_identity = row->projection.complete &&
        projected_desert == row->projection.pre_climate_count[CLIMATE_DESERT] &&
        projected_semi == row->projection.pre_climate_count[CLIMATE_SEMI_ARID] &&
        projected_combined == row->projection.pre_climate_count[CLIMATE_DESERT] +
            row->projection.pre_climate_count[CLIMATE_SEMI_ARID] &&
        projected_non_arid == actual->land_count -
            row->projection.pre_climate_count[CLIMATE_DESERT] -
            row->projection.pre_climate_count[CLIMATE_SEMI_ARID];
    if (!game_worldgen_aridity_diminishing_oasis_histogram_count(
            histogram, options->response.oasis_drop,
            options->response.transition_margin, &active_count)) return 0;
    if (fprintf(file,
            "%d,%s,%s,%s,%u,%d,%s,%c,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
            "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
            row_index, mode, candidate_id, pair_id, seed, map_size,
            game_worldgen_aridity_map_names[map_size], 'A' + case_index,
            game_worldgen_aridity_cases[case_index].drought,
            actual->combined_arid_limit, actual->desert_limit,
            actual->semi_arid_limit, actual->land_count,
            actual->river_channel_count, histogram->eligible_tile_count,
            histogram_total,
            histogram->moisture_prefix[
                GAME_WORLDGEN_ARIDITY_DIMINISHING_MOISTURE_COUNT - 1],
            histogram->oracle_case_count /
                GAME_WORLDGEN_ARIDITY_DIMINISHING_MOISTURE_COUNT,
            histogram->oracle_case_count, histogram->oracle_mismatch_count,
            histogram->legacy_crosscheck_case_count,
            histogram->legacy_crosscheck_mismatch_count, active_count,
            projected_desert, projected_semi, projected_combined,
            projected_non_arid, projection_identity, histogram->ok) <= 0) {
        return 0;
    }
    return write_histogram_values(file, histogram) && fprintf(file, "\n") > 0;
}
