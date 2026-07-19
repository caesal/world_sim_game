#include "io/map_save_load_river_preflight.h"

#include "io/map_save_world_physical.h"
#include "world/river_path_validation.h"

#include <limits.h>
#include <stdint.h>

const char *map_save_load_river_error_text(
    MapSaveRiverPathsStatus status, int chinese) {
    if (status == MAP_SAVE_RIVER_PATHS_TOO_LARGE) {
        return chinese ? "地图的河流数据超出支持上限或已损坏。" :
            "The map's river data exceeds the supported limit or is corrupted.";
    }
    if (status == MAP_SAVE_RIVER_PATHS_ALLOCATION_FAILED) {
        return chinese ? "无法为地图河流数据分配内存。" :
            "Could not allocate memory for the map's river data.";
    }
    if (status == MAP_SAVE_RIVER_PATHS_PREFIX_INVALID) {
        return chinese ? "地图的物理世界数据不兼容或已损坏。" :
            "The map's physical-world data is incompatible or corrupted.";
    }
    return chinese ? "地图的河流数据不兼容或已损坏。" :
        "The map's river data is incompatible or corrupted.";
}

MapSaveRiverPathsStatus map_save_load_river_preflight(
    FILE *file, int map_w, int map_h, int river_path_count,
    MapSaveRiverPathsStage *stage) {
    MapSaveRiverPathsStatus status;
    uint64_t tile_count;
    uint64_t world_bytes;
    size_t river_bytes;
    long body_offset;
    int restored;

    if (!file || !stage || stage->paths) {
        return MAP_SAVE_RIVER_PATHS_INVALID_ARGUMENT;
    }
    if (!river_path_count_valid(river_path_count, map_w, map_h) ||
        !river_path_storage_bytes_checked(river_path_count, &river_bytes)) {
        return MAP_SAVE_RIVER_PATHS_TOO_LARGE;
    }
    (void)river_bytes;
    body_offset = ftell(file);
    if (body_offset < 0) return MAP_SAVE_RIVER_PATHS_POSITION_ERROR;
    tile_count = (uint64_t)(unsigned int)map_w *
                 (uint64_t)(unsigned int)map_h;
    world_bytes = tile_count * sizeof(Tile);
    if (world_bytes > (uint64_t)LONG_MAX ||
        world_bytes > (uint64_t)(LONG_MAX - body_offset) ||
        fseek(file, body_offset + (long)world_bytes, SEEK_SET) != 0) {
        return MAP_SAVE_RIVER_PATHS_PREFIX_INVALID;
    }
    if (!map_save_world_physical_validate(file, map_w, map_h) ||
        !map_save_world_physical_skip(file, map_w, map_h)) {
        (void)fseek(file, body_offset, SEEK_SET);
        return MAP_SAVE_RIVER_PATHS_PREFIX_INVALID;
    }
    status = map_save_river_paths_stage_read(
        file, river_path_count, map_w, map_h, stage);
    restored = fseek(file, body_offset, SEEK_SET) == 0;
    if (!restored) {
        map_save_river_paths_stage_release(stage);
        return MAP_SAVE_RIVER_PATHS_POSITION_ERROR;
    }
    return status;
}
