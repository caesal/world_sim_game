#ifndef WORLD_SIM_SDL_RENDER_UI_H
#define WORLD_SIM_SDL_RENDER_UI_H

#include "core/render_snapshot.h"
#include "platform/sdl_text.h"

void sdl_render_draw_ui(SDL_Renderer *renderer, SdlText *text,
                        const RenderSnapshot *snapshot, RECT client);

#endif
