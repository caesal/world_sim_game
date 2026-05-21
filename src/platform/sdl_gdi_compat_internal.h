#ifndef WORLD_SIM_SDL_GDI_COMPAT_INTERNAL_H
#define WORLD_SIM_SDL_GDI_COMPAT_INTERNAL_H

#include "platform/sdl_gdi_compat.h"

typedef enum {
    SDL_GDI_BRUSH,
    SDL_GDI_PEN,
    SDL_GDI_FONT,
    SDL_GDI_BITMAP,
    SDL_GDI_REGION
} SdlGdiObjectType;

struct SdlGdiObject {
    SdlGdiObjectType type;
    int stock;
    int x;
    int y;
    int width;
    int height;
    int weight;
    int is_null;
    COLORREF color;
    unsigned int *pixels;
};

typedef struct {
    RECT clip;
    COLORREF text_color;
    HBRUSH brush;
    HPEN pen;
    HFONT font;
    POINT pen_pos;
} SdlGdiSavedState;

struct SdlGdiContext {
    SDL_Renderer *renderer;
    SdlText *text;
    RECT client;
    RECT clip;
    COLORREF text_color;
    HBRUSH brush;
    HPEN pen;
    HFONT font;
    HBITMAP bitmap;
    POINT pen_pos;
    SdlGdiSavedState stack[16];
    int stack_count;
    int memory;
};

TTF_Font *sdl_gdi_font_for(HDC hdc);
int sdl_gdi_wide_to_utf8(const WCHAR *src, int len, char *dst, int dst_len);
void sdl_gdi_draw_text_utf8(HDC hdc, TTF_Font *font, int x, int y, const char *utf8);

#endif
