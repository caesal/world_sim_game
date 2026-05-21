#ifndef WORLD_SIM_SDL_INPUT_H
#define WORLD_SIM_SDL_INPUT_H

#include "platform/sdl_render.h"

void sdl_input_handle_event(const SDL_Event *event, SDL_Window *window,
                            SdlMapRenderer *map, int *quit);

#endif
