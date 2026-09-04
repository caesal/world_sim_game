#ifndef WORLD_SIM_GAME_WORLDGEN_ARIDITY_DIMINISHING_OASIS_HISTOGRAM_IO_H
#define WORLD_SIM_GAME_WORLDGEN_ARIDITY_DIMINISHING_OASIS_HISTOGRAM_IO_H

#include "game/game_worldgen_aridity_response_projection_histogram.h"

int game_worldgen_aridity_diminishing_oasis_histogram_write_header(FILE *file);
int game_worldgen_aridity_diminishing_oasis_histogram_write_row(
    FILE *file, int row_index, const char *mode, const char *candidate_id,
    const char *pair_id, uint32_t seed, int map_size, int case_index,
    const GameWorldgenAridityProjectionOptions *options,
    const GameWorldgenAridityProjectionRow *row);

#endif
