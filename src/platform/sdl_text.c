#include "platform/sdl_text.h"

#include <stdio.h>
#include <string.h>

static int file_exists(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    fclose(file);
    return 1;
}

static const char *first_existing_font(void) {
    static const char *candidates[] = {
        "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
        "/System/Library/Fonts/Hiragino Sans GB.ttc",
        "/System/Library/Fonts/STHeiti Medium.ttc",
        "/System/Library/Fonts/SFNS.ttf",
        "C:/Windows/Fonts/msyh.ttc",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf"
    };
    int i;

    for (i = 0; i < (int)(sizeof(candidates) / sizeof(candidates[0])); i++) {
        if (file_exists(candidates[i])) return candidates[i];
    }
    return NULL;
}

static TTF_Font *open_font_size(const char *path, float size) {
    return path ? TTF_OpenFont(path, size) : NULL;
}

int sdl_text_init(SdlText *text, char *error, int error_size) {
    const char *path;

    if (!text) return 0;
    memset(text, 0, sizeof(*text));
    if (!TTF_Init()) {
        if (error && error_size > 0) snprintf(error, (size_t)error_size, "TTF_Init: %s", SDL_GetError());
        return 0;
    }

    path = first_existing_font();
    text->title = open_font_size(path, 22.0f);
    text->body = open_font_size(path, 15.0f);
    text->small = open_font_size(path, 13.0f);
    if (!text->title || !text->body || !text->small) {
        if (error && error_size > 0) {
            snprintf(error, (size_t)error_size, "TTF_OpenFont: %s", SDL_GetError());
        }
        sdl_text_shutdown(text);
        return 0;
    }
    return 1;
}

void sdl_text_shutdown(SdlText *text) {
    if (!text) return;
    if (text->title) TTF_CloseFont(text->title);
    if (text->body) TTF_CloseFont(text->body);
    if (text->small) TTF_CloseFont(text->small);
    text->title = NULL;
    text->body = NULL;
    text->small = NULL;
    TTF_Quit();
}

void sdl_text_draw(SDL_Renderer *renderer, TTF_Font *font, int x, int y,
                   SDL_Color color, const char *text) {
    SDL_Surface *surface;
    SDL_Texture *texture;
    SDL_FRect dst;

    if (!renderer || !font || !text || !text[0]) return;
    surface = TTF_RenderText_Blended(font, text, strlen(text), color);
    if (!surface) return;
    texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        SDL_DestroySurface(surface);
        return;
    }
    dst.x = (float)x;
    dst.y = (float)y;
    dst.w = (float)surface->w;
    dst.h = (float)surface->h;
    SDL_RenderTexture(renderer, texture, NULL, &dst);
    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);
}
