#ifndef WORLD_SIM_GAME_WORLDGEN_ARIDITY_DIMINISHING_IO_H
#define WORLD_SIM_GAME_WORLDGEN_ARIDITY_DIMINISHING_IO_H

#include "game/game_worldgen_aridity_response_projection_histogram.h"

int game_worldgen_aridity_diminishing_write_actual_header(FILE *file);
int game_worldgen_aridity_diminishing_write_actual_row(
    FILE *file, int row_index, const char *mode, const char *candidate_id,
    const char *pair_id, uint32_t seed, int map_size, int case_index,
    const GameWorldgenAridityCase *matrix_case,
    const GameWorldgenAridityProjectionOptions *options,
    const GameWorldgenAridityProjectionRow *row);

#endif
