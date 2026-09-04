#ifndef WORLD_SIM_GAME_WORLDGEN_ARIDITY_DIMINISHING_PRODUCTION_MATRIX_H
#define WORLD_SIM_GAME_WORLDGEN_ARIDITY_DIMINISHING_PRODUCTION_MATRIX_H

#include "game/game_worldgen_aridity_diminishing_options.h"

#include <stdint.h>
#include <stdio.h>

typedef struct {
    int rows;
    int generation_failures;
    int hash_failures;
    int capture_failures;
    int semantic_failures;
    int state_failures;
    int projection_mismatches;
    int artifact_count;
    int artifact_failures;
    uint64_t matrix_hash;
} GameWorldgenAridityDiminishingProductionSummary;

int game_worldgen_aridity_diminishing_production_matrix_run(
    const GameWorldgenAridityDiminishingOptions *options, FILE *csv,
    FILE *histogram, FILE *manifest,
    GameWorldgenAridityDiminishingProductionSummary *summary);

#endif
