#ifndef WORLD_SIM_GAME_WORLDGEN_H
#define WORLD_SIM_GAME_WORLDGEN_H

#include "core/worldgen_progress.h"
#include "world/world_gen.h"

WorldGenConfig game_world_gen_config_from_globals(void);
void game_clear_world_tiles(void);
void game_start_blank_world(void);
void game_request_new_world_with_callbacks(WorldGenProgressRepaintFn repaint, void *user_data);

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
void game_request_new_world_with_progress(HWND hwnd);
#endif

#endif
