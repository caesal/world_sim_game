#include "game/game_worldgen_aridity_diminishing_production_matrix.h"

#include "game/game_worldgen_aridity_calibration_artifacts.h"
#include "game/game_worldgen_aridity_diminishing_io.h"
#include "game/game_worldgen_aridity_diminishing_oasis_histogram.h"
#include "game/game_worldgen_aridity_diminishing_oasis_histogram_io.h"
#include "game/game_worldgen_aridity_diminishing_production.h"
#include "game/game_worldgen_aridity_response_metrics.h"

#include <stdlib.h>
#include <string.h>

#define DIM_OUTPUT_ENV "WORLD_SIM_ARIDITY_PROBE_DIR"
#define DIM_CANDIDATE_ID "a31_b04_d24_c12_r02"

static const uint32_t SCREEN_SEEDS[] = {2026082301u, 2026082302u};
static const uint32_t HOLDOUT_SEEDS[] = {2026082401u, 2026082402u};
static const char *const MAP_STEMS[] = {
    "small", "medium", "large", "extreme"
};

static int selected_oasis_matches(
    const GameWorldgenAridityResponseOptions *response,
    const GameWorldgenAridityProjectionRow *row) {
    int projected = -1;
    return response && row &&
        game_worldgen_aridity_diminishing_oasis_histogram_count(
            &row->actual.diminishing_oasis_histogram,
            response->oasis_drop, response->transition_margin, &projected) &&
        row->actual.oasis_count == projected;
}

static void note_row(
    const GameWorldgenAridityProjectionRow *row,
    GameWorldgenAridityDiminishingProductionSummary *summary) {
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

static int run_row(
    const char *directory, const char *pair_id,
    const GameWorldgenAridityProjectionOptions *production,
    uint32_t seed, int seed_index, int map_size, int case_index, int row_index,
    FILE *csv, FILE *histogram, FILE *manifest,
    GameWorldgenAridityDiminishingProductionSummary *summary) {
    GameWorldgenAridityProjectionRow row;
    WorldGenAridityCalibrationArtifactResult artifact;
    char stem[192];
    int emit = seed_index == 0 && (map_size == 0 || map_size == 3);
    int named = emit ? snprintf(stem, sizeof(stem), "dim_%s_%u_%s_%c",
        pair_id, seed, MAP_STEMS[map_size], 'A' + case_index) : 0;
    int row_ok;
    int io_ok = 1;
    memset(&row, 0, sizeof(row));
    memset(&artifact, 0, sizeof(artifact));
    if (emit && (!directory || !directory[0] || named <= 0 ||
            (size_t)named >= sizeof(stem))) {
        row_ok = 0;
    } else {
        row_ok = game_worldgen_aridity_diminishing_production_run_world(
            seed, map_size, &game_worldgen_aridity_cases[case_index],
            production, emit ? directory : NULL, emit ? stem : NULL, &row,
            emit ? &artifact : NULL);
    }
    note_row(&row, summary);
    summary->semantic_failures += !row_ok ||
        row.actual.oasis_semantic_errors != 0;
    summary->projection_mismatches +=
        !selected_oasis_matches(&production->response, &row);
    if (emit) {
        if (row_ok && write_artifact_row(
                manifest, pair_id, stem, seed, map_size, case_index, &row,
                &artifact)) {
            summary->artifact_count += 2;
        } else {
            summary->artifact_failures++;
        }
    }
    io_ok &= game_worldgen_aridity_diminishing_write_actual_row(
        csv, row_index, "production", DIM_CANDIDATE_ID, pair_id, seed,
        map_size, case_index, &game_worldgen_aridity_cases[case_index],
        production, &row);
    io_ok &= game_worldgen_aridity_diminishing_oasis_histogram_write_row(
        histogram, row_index, "production", DIM_CANDIDATE_ID, pair_id, seed,
        map_size, case_index, production, &row);
    return io_ok;
}

int game_worldgen_aridity_diminishing_production_matrix_run(
    const GameWorldgenAridityDiminishingOptions *options, FILE *csv,
    FILE *histogram, FILE *manifest,
    GameWorldgenAridityDiminishingProductionSummary *summary) {
    GameWorldgenAridityProjectionOptions production;
    const char *directory = getenv(DIM_OUTPUT_ENV);
    char pair_id[32];
    int seed_group;
    int seed_index;
    int map_size;
    int case_index;
    int row_index = 0;
    int io_ok;
    if (!options || !csv || !histogram || !manifest || !summary) return 0;
    memset(summary, 0, sizeof(*summary));
    summary->matrix_hash = UINT64_C(1469598103934665603);
    io_ok = game_worldgen_aridity_diminishing_write_actual_header(csv) &&
        game_worldgen_aridity_diminishing_oasis_histogram_write_header(
            histogram);
    if (!options->production_no_override || !directory || !directory[0] ||
        !game_worldgen_aridity_diminishing_production_options(
            &production, pair_id, sizeof(pair_id))) return 0;
    for (seed_group = 0; seed_group < 2; seed_group++) {
        const uint32_t *seeds = seed_group ? HOLDOUT_SEEDS : SCREEN_SEEDS;
        for (seed_index = 0; seed_index < 2; seed_index++) {
            for (map_size = 0; map_size < GAME_WORLDGEN_ARIDITY_MAP_COUNT;
                 map_size++) {
                for (case_index = 0;
                     case_index < GAME_WORLDGEN_ARIDITY_CASE_COUNT;
                     case_index++) {
                    io_ok &= run_row(
                        directory, pair_id, &production, seeds[seed_index],
                        seed_index, map_size, case_index, row_index++, csv,
                        histogram, manifest, summary);
                }
            }
        }
    }
    return io_ok && row_index == 96;
}
