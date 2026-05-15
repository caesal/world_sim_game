#ifndef WORLD_SIM_GAME_WORLDGEN_H
#define WORLD_SIM_GAME_WORLDGEN_H

#include "world/world_gen.h"

#include <windows.h>

WorldGenConfig game_world_gen_config_from_globals(void);
void game_clear_world_tiles(void);
void game_request_new_world_with_progress(HWND hwnd);

#endif
