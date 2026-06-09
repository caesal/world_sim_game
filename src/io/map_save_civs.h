#ifndef WORLD_SIM_MAP_SAVE_CIVS_H
#define WORLD_SIM_MAP_SAVE_CIVS_H

#include <stdio.h>

int map_save_read_civilizations(FILE *file, int save_version, int count);
void map_save_normalize_loaded_civilizations(int save_version);

#endif
