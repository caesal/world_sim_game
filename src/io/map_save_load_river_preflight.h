#ifndef WORLD_SIM_MAP_SAVE_LOAD_RIVER_PREFLIGHT_H
#define WORLD_SIM_MAP_SAVE_LOAD_RIVER_PREFLIGHT_H

#include "io/map_save_river_paths.h"

MapSaveRiverPathsStatus map_save_load_river_preflight(
    FILE *file, int map_w, int map_h, int river_path_count,
    MapSaveRiverPathsStage *stage);
const char *map_save_load_river_error_text(
    MapSaveRiverPathsStatus status, int chinese);

#endif
