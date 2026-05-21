#ifndef WORLD_SIM_SDL_RENDER_H
#define WORLD_SIM_SDL_RENDER_H

#include "platform/sdl_text.h"

typedef struct {
    SDL_Texture *map_texture;
    int texture_w;
    int texture_h;
    int texture_mode;
    int dirty;
    unsigned int texture_revision;
} SdlMapRenderer;

void sdl_render_init(SdlMapRenderer *map);
void sdl_render_shutdown(SdlMapRenderer *map);
void sdl_render_invalidate_map(SdlMapRenderer *map);
int sdl_render_draw(SdlMapRenderer *map, SDL_Renderer *renderer, SdlText *text,
                    int window_w, int window_h, int mode, int running, int months_per_tick);
int sdl_render_draw_backbuffer(SdlMapRenderer *map, SDL_Renderer *renderer, SdlText *text,
                               int window_w, int window_h, int mode, int running,
                               int months_per_tick);

#endif
