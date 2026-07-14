#ifndef WORLD_SIM_MAP_SAVE_PLAGUE_H
#define WORLD_SIM_MAP_SAVE_PLAGUE_H

#include <stdio.h>

#define MAP_SAVE_PLAGUE_BLOCK_VERSION 2

int map_save_plague_write(FILE *file);
int map_save_plague_read(FILE *file);

#endif
