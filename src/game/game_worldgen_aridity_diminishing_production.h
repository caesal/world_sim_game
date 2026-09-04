#ifndef WORLD_SIM_GAME_WORLDGEN_ARIDITY_DIMINISHING_PRODUCTION_H
#define WORLD_SIM_GAME_WORLDGEN_ARIDITY_DIMINISHING_PRODUCTION_H

#include "game/game_worldgen_aridity_response_projection_histogram.h"

int game_worldgen_aridity_diminishing_production_options(
    GameWorldgenAridityProjectionOptions *options,
    char *pair_id, size_t pair_capacity);
int game_worldgen_aridity_diminishing_production_run_world(
    uint32_t seed, int map_size, const GameWorldgenAridityCase *matrix_case,
    const GameWorldgenAridityProjectionOptions *options,
    const char *output_directory, const char *artifact_stem,
    GameWorldgenAridityProjectionRow *row,
    WorldGenAridityCalibrationArtifactResult *artifact_result);

#endif
