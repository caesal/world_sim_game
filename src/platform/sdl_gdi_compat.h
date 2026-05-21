#ifndef WORLD_SIM_SDL_GDI_COMPAT_H
#define WORLD_SIM_SDL_GDI_COMPAT_H

#include "platform/platform_types.h"
#include "platform/sdl_text.h"

HDC sdl_gdi_begin(SDL_Renderer *renderer, SdlText *text, RECT client);
void sdl_gdi_end(HDC hdc);

#endif
