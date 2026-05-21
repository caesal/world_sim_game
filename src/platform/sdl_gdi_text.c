#include "platform/sdl_gdi_compat_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SDL_Color sdl_color(COLORREF c) {
    SDL_Color out = {GetRValue(c), GetGValue(c), GetBValue(c), 255};
    return out;
}

TTF_Font *sdl_gdi_font_for(HDC hdc) {
    int height = hdc && hdc->font ? abs(hdc->font->width) : 15;
    if (!hdc || !hdc->text) return NULL;
    if (height >= 20 || (hdc->font && hdc->font->weight >= FW_BOLD)) return hdc->text->title;
    if (height <= 14) return hdc->text->small;
    return hdc->text->body;
}

int sdl_gdi_wide_to_utf8(const WCHAR *src, int len, char *dst, int dst_len) {
    int i = 0;
    if (!dst || dst_len <= 0) return 0;
    if (!src) {
        dst[0] = '\0';
        return 0;
    }
    if (len < 0) {
        while (src[i] && i + 1 < dst_len) {
            dst[i] = (char)(src[i] & 0xff);
            i++;
        }
    } else {
        while (i < len && i + 1 < dst_len) {
            dst[i] = (char)(src[i] & 0xff);
            i++;
        }
    }
    dst[i] = '\0';
    return i;
}

void sdl_gdi_draw_text_utf8(HDC hdc, TTF_Font *font, int x, int y, const char *utf8) {
    SDL_Surface *surface;
    SDL_Texture *texture;
    SDL_FRect dst;

    if (!hdc || !hdc->renderer || !font || !utf8 || !utf8[0]) return;
    surface = TTF_RenderText_Blended(font, utf8, strlen(utf8), sdl_color(hdc->text_color));
    if (!surface) return;
    texture = SDL_CreateTextureFromSurface(hdc->renderer, surface);
    if (!texture) {
        SDL_DestroySurface(surface);
        return;
    }
    dst.x = (float)x;
    dst.y = (float)y;
    dst.w = (float)surface->w;
    dst.h = (float)surface->h;
    SDL_RenderTexture(hdc->renderer, texture, NULL, &dst);
    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);
}

int MultiByteToWideChar(UINT code_page, DWORD flags, const char *src, int src_len,
                        WCHAR *dst, int dst_len) {
    int i;
    int len;
    (void)code_page;
    (void)flags;
    if (!src) return 0;
    len = src_len < 0 ? (int)strlen(src) + 1 : src_len;
    if (!dst || dst_len <= 0) return len;
    for (i = 0; i < len && i < dst_len; i++) dst[i] = (unsigned char)src[i];
    if (i == dst_len) dst[dst_len - 1] = 0;
    return i;
}

int TextOutW(HDC hdc, int x, int y, const WCHAR *text, int len) {
    char utf8[1024];
    sdl_gdi_wide_to_utf8(text, len, utf8, sizeof(utf8));
    sdl_gdi_draw_text_utf8(hdc, sdl_gdi_font_for(hdc), x, y, utf8);
    return 1;
}

int GetTextExtentPoint32W(HDC hdc, const WCHAR *text, int len, SIZE *size) {
    char utf8[1024];
    TTF_Font *font = sdl_gdi_font_for(hdc);
    int w = 0;
    int h = 0;

    if (!size) return 0;
    sdl_gdi_wide_to_utf8(text, len, utf8, sizeof(utf8));
    if (font && utf8[0]) TTF_GetStringSize(font, utf8, strlen(utf8), &w, &h);
    size->cx = w;
    size->cy = h;
    return 1;
}

int DrawTextW(HDC hdc, const WCHAR *text, int len, RECT *rect, UINT format) {
    char utf8[1024];
    SIZE size;
    int x;
    int y;

    if (!rect) return 0;
    sdl_gdi_wide_to_utf8(text, len, utf8, sizeof(utf8));
    GetTextExtentPoint32W(hdc, text, len, &size);
    if (format & DT_CALCRECT) {
        int max_w = max(1, rect->right - rect->left);
        int lines = (format & DT_WORDBREAK) ? max(1, (size.cx + max_w - 1) / max_w) : 1;
        rect->right = rect->left + min(size.cx, max_w);
        rect->bottom = rect->top + max(size.cy, 18) * lines;
        return rect->bottom - rect->top;
    }
    x = rect->left;
    if (format & DT_CENTER) x = rect->left + (rect->right - rect->left - size.cx) / 2;
    else if (format & DT_RIGHT) x = rect->right - size.cx;
    y = (format & DT_VCENTER) ? rect->top + (rect->bottom - rect->top - size.cy) / 2 : rect->top;
    sdl_gdi_draw_text_utf8(hdc, sdl_gdi_font_for(hdc), x, y, utf8);
    return 1;
}

int DrawTextA(HDC hdc, const char *text, int len, RECT *rect, UINT format) {
    WCHAR wide[1024];
    MultiByteToWideChar(CP_UTF8, 0, text ? text : "", len < 0 ? -1 : len, wide,
                        (int)(sizeof(wide) / sizeof(wide[0])));
    return DrawTextW(hdc, wide, -1, rect, format);
}

int MessageBoxW(HWND hwnd, const WCHAR *text, const WCHAR *title, UINT flags) {
    char text_utf8[2048];
    char title_utf8[256];
    (void)hwnd;
    (void)flags;
    sdl_gdi_wide_to_utf8(text, -1, text_utf8, sizeof(text_utf8));
    sdl_gdi_wide_to_utf8(title, -1, title_utf8, sizeof(title_utf8));
    fprintf(stderr, "%s: %s\n", title_utf8, text_utf8);
    return 0;
}
