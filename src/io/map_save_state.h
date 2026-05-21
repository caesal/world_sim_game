#ifndef WORLD_SIM_MAP_SAVE_STATE_H
#define WORLD_SIM_MAP_SAVE_STATE_H

#include <stdio.h>

int map_save_write_dynamic_state(FILE *file);
int map_save_read_dynamic_state(FILE *file, int save_version);

#endif
