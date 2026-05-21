#ifndef WORLD_SIM_PLATFORM_SDL_UI_COMMON_H
#define WORLD_SIM_PLATFORM_SDL_UI_COMMON_H

#include "platform/platform_types.h"
#include "platform/sdl_text.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <stdio.h>
#include <string.h>

static inline SDL_Color sdl_ui_color(unsigned char r, unsigned char g, unsigned char b) {
    SDL_Color color = {r, g, b, 255};
    return color;
}

static inline SDL_FRect sdl_ui_rect(RECT rect) {
    SDL_FRect out = {(float)rect.left, (float)rect.top,
                     (float)(rect.right - rect.left), (float)(rect.bottom - rect.top)};
    return out;
}

static inline void sdl_ui_fill(SDL_Renderer *renderer, RECT rect, SDL_Color color) {
    SDL_FRect out = sdl_ui_rect(rect);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, &out);
}

static inline void sdl_ui_stroke(SDL_Renderer *renderer, RECT rect, SDL_Color color) {
    SDL_FRect out = sdl_ui_rect(rect);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderRect(renderer, &out);
}

static inline int sdl_ui_text_width(TTF_Font *font, const char *text) {
    int w = 0;
    int h = 0;
    if (!font || !text) return 0;
    TTF_GetStringSize(font, text, strlen(text), &w, &h);
    return w;
}

static inline void sdl_ui_text_at(SDL_Renderer *renderer, SdlText *text, TTF_Font *font,
                                  int x, int y, SDL_Color color, const char *value) {
    sdl_text_draw(renderer, font ? font : text->body, x, y, color, value);
}

static inline void sdl_ui_text_center(SDL_Renderer *renderer, SdlText *text, TTF_Font *font,
                                      RECT rect, SDL_Color color, const char *value) {
    int w = sdl_ui_text_width(font ? font : text->body, value);
    int x = rect.left + ((rect.right - rect.left) - w) / 2;
    int y = rect.top + ((rect.bottom - rect.top) - 18) / 2;
    sdl_ui_text_at(renderer, text, font, x, y, color, value);
}

static inline void sdl_ui_panel_line(SDL_Renderer *renderer, SdlText *text, int x, int *y,
                                     const char *label, const char *value) {
    char line[256];
    snprintf(line, sizeof(line), "%s%s%s", label, value && value[0] ? ": " : "", value ? value : "");
    sdl_ui_text_at(renderer, text, text->small, x, *y, sdl_ui_color(204, 214, 222), line);
    *y += 22;
}

#endif
