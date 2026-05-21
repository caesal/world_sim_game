#ifndef WORLD_SIM_SDL_WORLD_H
#define WORLD_SIM_SDL_WORLD_H

void sdl_world_reset_blank(void);
void sdl_world_generate_default(int civs, int map_size);
int sdl_world_step_months(int months);

#endif
