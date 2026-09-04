#ifndef WORLD_SIM_GAME_WORLDGEN_ARIDITY_RESPONSE_PROJECTION_HISTOGRAM_H
#define WORLD_SIM_GAME_WORLDGEN_ARIDITY_RESPONSE_PROJECTION_HISTOGRAM_H

#include "core/worldgen_attempt.h"
#include "game/game_worldgen_aridity_calibration_artifacts.h"
#include "game/game_worldgen_aridity_calibration_metrics.h"
#include "game/game_worldgen_aridity_response_metrics.h"
#include "game/game_worldgen_aridity_response_projection_options.h"
#include "world/world_gen_aridity_projection.h"

#include <stdint.h>
#include <stdio.h>

typedef struct {
    int generated;
    uint64_t physical_hash;
    WorldGenAttemptDiagnostics attempt;
    WorldGenAridityProjectionResult projection;
    GameWorldgenAridityResponseResult actual;
    int has_actual;
    int active_state_ok;
    int restored_state_ok;
    int artifact_ok;
    int ok;
} GameWorldgenAridityProjectionRow;

int game_worldgen_aridity_projection_run_carrier(
    uint32_t seed, int map_size, const GameWorldgenAridityCase *matrix_case,
    int drought_divisor, const GameWorldgenAridityResponseDefaults *defaults,
    GameWorldgenAridityProjectionRow *row);
int game_worldgen_aridity_projection_run_actual(
    uint32_t seed, int map_size, const GameWorldgenAridityCase *matrix_case,
    const GameWorldgenAridityProjectionOptions *options,
    const GameWorldgenAridityResponseDefaults *defaults,
    const char *output_directory, const char *artifact_stem,
    GameWorldgenAridityProjectionRow *row,
    WorldGenAridityCalibrationArtifactResult *artifact_result);
int game_worldgen_aridity_projection_write_carrier_header(FILE *file);
int game_worldgen_aridity_projection_write_carrier_row(
    FILE *file, int row_index, const char *carrier_id,
    const char *source_cases, uint32_t seed, int map_size,
    const GameWorldgenAridityCase *matrix_case, int drought_divisor,
    const GameWorldgenAridityProjectionRow *row);
int game_worldgen_aridity_projection_write_actual_header(FILE *file);
int game_worldgen_aridity_projection_write_actual_row(
    FILE *file, int row_index, const char *mode, const char *candidate_id,
    const char *pair_id, uint32_t seed, int map_size, int case_index,
    const GameWorldgenAridityCase *matrix_case,
    const GameWorldgenAridityProjectionOptions *options,
    const GameWorldgenAridityProjectionRow *row);

#endif
