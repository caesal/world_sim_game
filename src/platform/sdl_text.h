#ifndef WORLD_SIM_SDL_TEXT_H
#define WORLD_SIM_SDL_TEXT_H

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

typedef struct {
    TTF_Font *title;
    TTF_Font *body;
    TTF_Font *small;
} SdlText;

int sdl_text_init(SdlText *text, char *error, int error_size);
void sdl_text_shutdown(SdlText *text);
void sdl_text_draw(SDL_Renderer *renderer, TTF_Font *font, int x, int y,
                   SDL_Color color, const char *text);

#endif
