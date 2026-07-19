#ifndef WORLD_SIM_MAP_SAVE_RIVER_PATHS_H
#define WORLD_SIM_MAP_SAVE_RIVER_PATHS_H

#include "core/world_types.h"

#include <stddef.h>
#include <stdio.h>

typedef enum {
    MAP_SAVE_RIVER_PATHS_OK = 0,
    MAP_SAVE_RIVER_PATHS_INVALID_ARGUMENT,
    MAP_SAVE_RIVER_PATHS_TOO_LARGE,
    MAP_SAVE_RIVER_PATHS_ALLOCATION_FAILED,
    MAP_SAVE_RIVER_PATHS_TRUNCATED,
    MAP_SAVE_RIVER_PATHS_INVALID_PAYLOAD,
    MAP_SAVE_RIVER_PATHS_POSITION_ERROR,
    MAP_SAVE_RIVER_PATHS_PREFIX_INVALID
} MapSaveRiverPathsStatus;

typedef struct {
    RiverPath *paths;
    int count;
    size_t bytes;
    long file_offset;
} MapSaveRiverPathsStage;

int map_save_river_paths_allocate(RiverPath **out, int count,
                                  int map_w, int map_h);
int map_save_river_paths_write(FILE *file, const RiverPath *paths, int count,
                               int map_w, int map_h);
int map_save_river_paths_read(FILE *file, RiverPath *paths, int count,
                              int map_w, int map_h);
void map_save_river_paths_stage_release(MapSaveRiverPathsStage *stage);
MapSaveRiverPathsStatus map_save_river_paths_stage_read(
    FILE *file, int count, int map_w, int map_h,
    MapSaveRiverPathsStage *stage);
int map_save_river_paths_stage_advance(
    FILE *file, const MapSaveRiverPathsStage *stage);

#endif
