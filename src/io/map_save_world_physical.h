#ifndef WORLD_SIM_MAP_SAVE_WORLD_PHYSICAL_H
#define WORLD_SIM_MAP_SAVE_WORLD_PHYSICAL_H

#include <stdio.h>

#define MAP_SAVE_WORLD_PHYSICAL_BLOCK_VERSION 2

int map_save_world_physical_write(FILE *file, int map_w, int map_h);
int map_save_world_physical_validate(FILE *file,
                                     int expected_map_w, int expected_map_h);
int map_save_world_physical_read(FILE *file, int expected_map_w, int expected_map_h);
int map_save_world_physical_skip(FILE *file,
                                 int expected_map_w, int expected_map_h);

#endif
