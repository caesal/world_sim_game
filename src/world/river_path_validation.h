#ifndef WORLD_SIM_RIVER_PATH_VALIDATION_H
#define WORLD_SIM_RIVER_PATH_VALIDATION_H

#include "core/world_types.h"

#include <stddef.h>

int river_path_count_limit(int map_w, int map_h);
int river_path_count_valid(int count, int map_w, int map_h);
size_t river_path_storage_byte_limit(void);
int river_path_storage_bytes_checked(int count, size_t *out_bytes);
int river_paths_validate(const RiverPath *paths, int count,
                         int map_w, int map_h);

#endif
