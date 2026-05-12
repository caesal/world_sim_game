#ifndef WORLD_SIM_MAP_SAVE_REGIONS_H
#define WORLD_SIM_MAP_SAVE_REGIONS_H

#include <stdio.h>

int map_save_write_natural_regions(FILE *file);
int map_save_read_natural_regions(FILE *file, int save_version);

#endif
