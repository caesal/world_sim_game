#ifndef WORLD_SIM_GAME_WORLDGEN_H
#define WORLD_SIM_GAME_WORLDGEN_H

#include "world/world_gen.h"

#include <windows.h>

WorldGenConfig game_world_gen_config_from_globals(void);
uint64_t game_worldgen_retained_identity_hash(void);
void game_worldgen_validation_set_next_seed(const unsigned int *seed);
void game_clear_world_tiles(void);
void game_request_new_world_with_progress(HWND hwnd);
int game_worldgen_publish_and_prewarm(HWND hwnd);
int game_worldgen_service_pending_presentation(void);
int game_worldgen_snapshot_publish_pending(void);

#endif
