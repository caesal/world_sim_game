#ifndef WORLD_SIM_GAME_WORLDGEN_ARIDITY_RESPONSE_METRICS_H
#define WORLD_SIM_GAME_WORLDGEN_ARIDITY_RESPONSE_METRICS_H

#include "core/worldgen_attempt.h"
#include "game/game_worldgen_aridity_calibration_artifacts.h"
#include "game/game_worldgen_aridity_calibration_metrics.h"
#include "game/game_worldgen_aridity_diminishing_oasis_histogram.h"
#include "game/game_worldgen_aridity_response_options.h"

#include <stdint.h>
#include <stdio.h>

struct WorldGenContext;

enum {
    GAME_WORLDGEN_ARIDITY_RESPONSE_DROP_COUNT = 6,
    GAME_WORLDGEN_ARIDITY_RESPONSE_TRANSITION_COUNT = 6
};

typedef struct {
    int generated;
    uint64_t physical_hash;
    WorldGenAttemptDiagnostics attempt;
    int drought_divisor;
    int moisture_compression_span;
    int oasis_drop;
    int transition_margin;
    int combined_arid_limit;
    int semi_arid_band;
    int desert_limit;
    int semi_arid_limit;
    int oasis_limit;
    int oasis_transition_limit;
    int land_count;
    int terrestrial_count;
    int lake_count;
    int desert_count;
    int semi_arid_count;
    int combined_arid_count;
    int non_arid_count;
    int oasis_count;
    int wetland_count;
    int river_channel_count;
    int arid_channel_count;
    int oasis_predicate_count;
    int oasis_reachable_count;
    int oasis_suppressed_count;
    int oasis_semantic_errors;
    int projected_visible_oasis_count
        [GAME_WORLDGEN_ARIDITY_RESPONSE_DROP_COUNT]
        [GAME_WORLDGEN_ARIDITY_RESPONSE_TRANSITION_COUNT];
    GameWorldgenAridityDiminishingOasisHistogram diminishing_oasis_histogram;
    int ok;
} GameWorldgenAridityResponseResult;

extern const int game_worldgen_aridity_response_oasis_drops[
    GAME_WORLDGEN_ARIDITY_RESPONSE_DROP_COUNT];
extern const int game_worldgen_aridity_response_transition_margins[
    GAME_WORLDGEN_ARIDITY_RESPONSE_TRANSITION_COUNT];

int game_worldgen_aridity_response_oasis_drop_index(int value);
int game_worldgen_aridity_response_transition_margin_index(int value);
int game_worldgen_aridity_response_oasis_pair_valid(
    int oasis_drop, int transition_margin);
int game_worldgen_aridity_response_write_projected_csv_columns(FILE *file);
int game_worldgen_aridity_response_write_projected_csv_values(
    FILE *file, const GameWorldgenAridityResponseResult *result);
int game_worldgen_aridity_response_collect_metrics(
    const struct WorldGenContext *context,
    const GameWorldgenAridityResponseOptions *options,
    GameWorldgenAridityResponseResult *result);
int game_worldgen_aridity_response_run_world(
    uint32_t seed, int map_size, const GameWorldgenAridityCase *matrix_case,
    const GameWorldgenAridityResponseOptions *options,
    GameWorldgenAridityResponseResult *result);
int game_worldgen_aridity_response_run_world_with_artifacts(
    uint32_t seed, int map_size, const GameWorldgenAridityCase *matrix_case,
    const GameWorldgenAridityResponseOptions *options,
    const char *output_directory, const char *artifact_stem,
    GameWorldgenAridityResponseResult *result,
    WorldGenAridityCalibrationArtifactResult *artifact_result);
int game_worldgen_aridity_response_write_csv_header(FILE *file);
int game_worldgen_aridity_response_write_csv_row(
    FILE *file, int row_index, uint32_t seed, int map_size,
    const GameWorldgenAridityCase *matrix_case,
    const GameWorldgenAridityResponseResult *result);

#endif
