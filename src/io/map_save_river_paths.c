#include "io/map_save_river_paths.h"

#include "world/river_path_validation.h"

#include <limits.h>
#include <stddef.h>
#include <stdlib.h>

_Static_assert(sizeof(int) == 4, "save-v20 integer contract changed");
_Static_assert(MAX_RIVER_POINTS == 96, "save-v20 river point limit changed");
_Static_assert(sizeof(RiverPoint) == 8, "save-v20 RiverPoint contract changed");
_Static_assert(offsetof(RiverPath, points) == 20,
               "save-v20 RiverPath prefix changed");
_Static_assert(sizeof(RiverPath) == 788,
               "save-v20 RiverPath contract changed");

int map_save_river_paths_allocate(RiverPath **out, int count,
                                  int map_w, int map_h) {
    size_t bytes;
    if (!out || *out || !river_path_count_valid(count, map_w, map_h) ||
        !river_path_storage_bytes_checked(count, &bytes)) return 0;
    if (count == 0) return 1;
    *out = (RiverPath *)calloc(1, bytes);
    return *out != NULL;
}

int map_save_river_paths_write(FILE *file, const RiverPath *paths, int count,
                               int map_w, int map_h) {
    if (!file || !river_paths_validate(paths, count, map_w, map_h)) return 0;
    return count == 0 ||
           fwrite(paths, sizeof(*paths), (size_t)count, file) == (size_t)count;
}

int map_save_river_paths_read(FILE *file, RiverPath *paths, int count,
                              int map_w, int map_h) {
    if (!file || !river_path_count_valid(count, map_w, map_h) ||
        (count > 0 && !paths)) return 0;
    if (count > 0 &&
        fread(paths, sizeof(*paths), (size_t)count, file) != (size_t)count) return 0;
    return river_paths_validate(paths, count, map_w, map_h);
}

void map_save_river_paths_stage_release(MapSaveRiverPathsStage *stage) {
    if (!stage) return;
    free(stage->paths);
    stage->paths = NULL;
    stage->count = 0;
    stage->bytes = 0;
    stage->file_offset = -1;
}

MapSaveRiverPathsStatus map_save_river_paths_stage_read(
    FILE *file, int count, int map_w, int map_h,
    MapSaveRiverPathsStage *stage) {
    size_t bytes;
    long offset;
    if (!file || !stage || stage->paths) {
        return MAP_SAVE_RIVER_PATHS_INVALID_ARGUMENT;
    }
    if (!river_path_count_valid(count, map_w, map_h) ||
        !river_path_storage_bytes_checked(count, &bytes)) {
        return MAP_SAVE_RIVER_PATHS_TOO_LARGE;
    }
    offset = ftell(file);
    if (offset < 0) return MAP_SAVE_RIVER_PATHS_POSITION_ERROR;
    if (count > 0) {
        stage->paths = (RiverPath *)malloc(bytes);
        if (!stage->paths) return MAP_SAVE_RIVER_PATHS_ALLOCATION_FAILED;
        if (fread(stage->paths, sizeof(*stage->paths), (size_t)count, file) !=
            (size_t)count) {
            map_save_river_paths_stage_release(stage);
            return MAP_SAVE_RIVER_PATHS_TRUNCATED;
        }
    }
    stage->count = count;
    stage->bytes = bytes;
    stage->file_offset = offset;
    if (!river_paths_validate(stage->paths, count, map_w, map_h)) {
        map_save_river_paths_stage_release(stage);
        return MAP_SAVE_RIVER_PATHS_INVALID_PAYLOAD;
    }
    return MAP_SAVE_RIVER_PATHS_OK;
}

int map_save_river_paths_stage_advance(
    FILE *file, const MapSaveRiverPathsStage *stage) {
    long offset;
    if (!file || !stage || stage->file_offset < 0 ||
        stage->bytes > (size_t)LONG_MAX) return 0;
    offset = ftell(file);
    if (offset != stage->file_offset ||
        stage->bytes > (size_t)(LONG_MAX - offset)) return 0;
    return fseek(file, (long)stage->bytes, SEEK_CUR) == 0;
}
