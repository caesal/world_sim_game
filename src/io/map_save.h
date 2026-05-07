#ifndef WORLD_SIM_MAP_SAVE_H
#define WORLD_SIM_MAP_SAVE_H

#include <windows.h>

int ensure_map_save_folder(void);
int save_current_map(HWND hwnd);
int load_map_from_file(HWND hwnd);

#endif
