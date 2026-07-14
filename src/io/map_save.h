#ifndef WORLD_SIM_MAP_SAVE_H
#define WORLD_SIM_MAP_SAVE_H

#include <windows.h>

int ensure_map_save_folder(void);
int map_save_current_version(void);
int map_save_version_supported(int version);
int map_save_probe_fog_header_roundtrip(int value);
int save_current_map(HWND hwnd);
int load_map_from_file(HWND hwnd);

#endif
