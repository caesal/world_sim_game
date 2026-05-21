#include "platform/sdl_gdi_compat_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static struct SdlGdiObject stock_null_brush = {
    SDL_GDI_BRUSH, 1, 0, 0, 0, 0, 0, 1, RGB(0, 0, 0), NULL
};
static struct SdlGdiObject stock_gray_brush = {
    SDL_GDI_BRUSH, 1, 0, 0, 0, 0, 0, 0, RGB(128, 128, 128), NULL
};
static struct SdlGdiObject default_brush = {
    SDL_GDI_BRUSH, 1, 0, 0, 0, 0, 0, 0, RGB(255, 255, 255), NULL
};
static struct SdlGdiObject default_pen = {
    SDL_GDI_PEN, 1, 0, 0, 1, 0, 0, 0, RGB(255, 255, 255), NULL
};
static struct SdlGdiObject default_font = {
    SDL_GDI_FONT, 1, 0, 0, 15, 0, FW_NORMAL, 0, RGB(0, 0, 0), NULL
};
static struct SdlGdiContext root_context;

static SDL_Rect sdl_rect_from_rect(RECT rect) {
    SDL_Rect out = {rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top};
    return out;
}

static RECT normalize_rect(RECT rect) {
    RECT out = rect;
    if (out.left > out.right) { int t = out.left; out.left = out.right; out.right = t; }
    if (out.top > out.bottom) { int t = out.top; out.top = out.bottom; out.bottom = t; }
    return out;
}

static RECT intersect_rect(RECT a, RECT b) {
    RECT out = {max(a.left, b.left), max(a.top, b.top), min(a.right, b.right), min(a.bottom, b.bottom)};
    if (out.right < out.left) out.right = out.left;
    if (out.bottom < out.top) out.bottom = out.top;
    return out;
}

static int clip_contains(HDC hdc, int x, int y) {
    return !hdc || (x >= hdc->clip.left && x < hdc->clip.right && y >= hdc->clip.top && y < hdc->clip.bottom);
}

static void set_draw_color(SDL_Renderer *renderer, COLORREF color, int alpha) {
    SDL_SetRenderDrawColor(renderer, GetRValue(color), GetGValue(color), GetBValue(color), (Uint8)alpha);
}

static void apply_clip(HDC hdc) {
    if (!hdc || hdc->memory || !hdc->renderer) return;
    if (IsRectEmpty(&hdc->clip)) SDL_SetRenderClipRect(hdc->renderer, NULL);
    else {
        SDL_Rect clip = sdl_rect_from_rect(hdc->clip);
        SDL_SetRenderClipRect(hdc->renderer, &clip);
    }
}

HDC sdl_gdi_begin(SDL_Renderer *renderer, SdlText *text, RECT client) {
    memset(&root_context, 0, sizeof(root_context));
    root_context.renderer = renderer;
    root_context.text = text;
    root_context.client = client;
    root_context.clip = client;
    root_context.text_color = RGB(255, 255, 255);
    root_context.brush = &default_brush;
    root_context.pen = &default_pen;
    root_context.font = &default_font;
    apply_clip(&root_context);
    return &root_context;
}

void sdl_gdi_end(HDC hdc) {
    if (hdc && hdc->renderer) SDL_SetRenderClipRect(hdc->renderer, NULL);
}

int InflateRect(RECT *rect, int dx, int dy) {
    if (!rect) return 0;
    rect->left -= dx; rect->right += dx; rect->top -= dy; rect->bottom += dy;
    return 1;
}

HBRUSH CreateSolidBrush(COLORREF color) {
    HBRUSH brush = (HBRUSH)calloc(1, sizeof(*brush));
    if (!brush) return NULL;
    brush->type = SDL_GDI_BRUSH;
    brush->color = color;
    return brush;
}

HPEN CreatePen(int style, int width, COLORREF color) {
    HPEN pen = (HPEN)calloc(1, sizeof(*pen));
    (void)style;
    if (!pen) return NULL;
    pen->type = SDL_GDI_PEN;
    pen->width = max(1, width);
    pen->color = color;
    return pen;
}

HFONT CreateFontW(int height, int width, int escapement, int orientation, int weight,
                  BOOL italic, BOOL underline, BOOL strikeout, DWORD charset,
                  DWORD out_precision, DWORD clip_precision, DWORD quality,
                  DWORD pitch_and_family, const WCHAR *face_name) {
    HFONT font = (HFONT)calloc(1, sizeof(*font));
    (void)width; (void)escapement; (void)orientation; (void)italic; (void)underline;
    (void)strikeout; (void)charset; (void)out_precision; (void)clip_precision;
    (void)quality; (void)pitch_and_family; (void)face_name;
    if (!font) return NULL;
    font->type = SDL_GDI_FONT;
    font->width = abs(height);
    font->weight = weight;
    return font;
}

HDC CreateCompatibleDC(HDC hdc) {
    HDC out = (HDC)calloc(1, sizeof(*out));
    if (!out) return NULL;
    if (hdc) {
        out->renderer = hdc->renderer;
        out->text = hdc->text;
        out->client = hdc->client;
    }
    out->clip = out->client;
    out->text_color = RGB(255, 255, 255);
    out->brush = &default_brush;
    out->pen = &default_pen;
    out->font = &default_font;
    out->memory = 1;
    return out;
}

HBITMAP CreateCompatibleBitmap(HDC hdc, int width, int height) {
    BITMAPINFO info;
    void *bits = NULL;
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    return CreateDIBSection(hdc, &info, DIB_RGB_COLORS, &bits, NULL, 0);
}

HBITMAP CreateDIBSection(HDC hdc, const BITMAPINFO *info, UINT usage,
                         void **bits, void *section, DWORD offset) {
    int width;
    int height;
    HBITMAP bitmap;
    (void)hdc; (void)usage; (void)section; (void)offset;
    if (!info) return NULL;
    width = abs(info->bmiHeader.biWidth);
    height = abs(info->bmiHeader.biHeight);
    bitmap = (HBITMAP)calloc(1, sizeof(*bitmap));
    if (!bitmap) return NULL;
    bitmap->type = SDL_GDI_BITMAP;
    bitmap->width = width;
    bitmap->height = height;
    bitmap->pixels = (unsigned int *)calloc((size_t)width * (size_t)height, sizeof(unsigned int));
    if (!bitmap->pixels) { free(bitmap); return NULL; }
    if (bits) *bits = bitmap->pixels;
    return bitmap;
}

HRGN CreateRectRgn(int left, int top, int right, int bottom) {
    HRGN region = (HRGN)calloc(1, sizeof(*region));
    if (!region) return NULL;
    region->type = SDL_GDI_REGION;
    region->x = left;
    region->y = top;
    region->width = right - left;
    region->height = bottom - top;
    return region;
}

HGDIOBJ SelectObject(HDC hdc, HGDIOBJ object) {
    HGDIOBJ old = NULL;
    if (!hdc || !object) return NULL;
    if (object->type == SDL_GDI_BRUSH) { old = (HGDIOBJ)hdc->brush; hdc->brush = (HBRUSH)object; }
    else if (object->type == SDL_GDI_PEN) { old = (HGDIOBJ)hdc->pen; hdc->pen = (HPEN)object; }
    else if (object->type == SDL_GDI_FONT) { old = (HGDIOBJ)hdc->font; hdc->font = (HFONT)object; }
    else if (object->type == SDL_GDI_BITMAP) { old = (HGDIOBJ)hdc->bitmap; hdc->bitmap = (HBITMAP)object; hdc->clip = (RECT){0, 0, object->width, object->height}; }
    return old;
}

HGDIOBJ GetStockObject(int object_id) {
    if (object_id == NULL_BRUSH) return (HGDIOBJ)&stock_null_brush;
    if (object_id == GRAY_BRUSH) return (HGDIOBJ)&stock_gray_brush;
    return (HGDIOBJ)&default_brush;
}

int DeleteObject(HGDIOBJ object) {
    if (!object || object->stock) return 0;
    if (object->type == SDL_GDI_BITMAP) free(object->pixels);
    free(object);
    return 1;
}

int DeleteDC(HDC hdc) {
    if (!hdc || hdc == &root_context) return 0;
    free(hdc);
    return 1;
}

static void fill_memory_rect(HDC hdc, RECT rect, COLORREF color) {
    int x, y;
    unsigned int pixel = ((unsigned int)GetRValue(color) << 16) |
                         ((unsigned int)GetGValue(color) << 8) | GetBValue(color);
    if (!hdc || !hdc->bitmap || !hdc->bitmap->pixels) return;
    rect = intersect_rect(normalize_rect(rect), hdc->clip);
    for (y = rect.top; y < rect.bottom; y++)
        for (x = rect.left; x < rect.right; x++)
            hdc->bitmap->pixels[y * hdc->bitmap->width + x] = pixel;
}

int FillRect(HDC hdc, const RECT *rect, HBRUSH brush) {
    SDL_FRect fr;
    RECT draw_rect;
    if (!hdc || !rect || !brush || brush->is_null) return 0;
    draw_rect = normalize_rect(*rect);
    if (hdc->memory) { fill_memory_rect(hdc, draw_rect, brush->color); return 1; }
    apply_clip(hdc);
    fr.x = (float)draw_rect.left; fr.y = (float)draw_rect.top;
    fr.w = (float)(draw_rect.right - draw_rect.left); fr.h = (float)(draw_rect.bottom - draw_rect.top);
    set_draw_color(hdc->renderer, brush->color, 255);
    SDL_RenderFillRect(hdc->renderer, &fr);
    return 1;
}

int FrameRect(HDC hdc, const RECT *rect, HBRUSH brush) {
    SDL_FRect fr;
    if (!hdc || !rect || !brush || hdc->memory) return 0;
    apply_clip(hdc);
    fr.x = (float)rect->left; fr.y = (float)rect->top;
    fr.w = (float)(rect->right - rect->left); fr.h = (float)(rect->bottom - rect->top);
    set_draw_color(hdc->renderer, brush->color, 255);
    SDL_RenderRect(hdc->renderer, &fr);
    return 1;
}

int SetBkMode(HDC hdc, int mode) { (void)hdc; return mode; }
COLORREF SetTextColor(HDC hdc, COLORREF color) { COLORREF old = hdc ? hdc->text_color : 0; if (hdc) hdc->text_color = color; return old; }

static unsigned int pixel_to_argb(unsigned int pixel, int alpha) {
    return ((unsigned int)alpha << 24) | ((pixel >> 16) & 0xff) << 16 |
           ((pixel >> 8) & 0xff) << 8 | (pixel & 0xff);
}

int BitBlt(HDC dst, int x, int y, int width, int height, HDC src, int sx, int sy, DWORD rop) {
    SDL_Texture *texture;
    unsigned int *tmp;
    int row, col;
    SDL_FRect dr;
    (void)rop;
    if (!dst || !src || !src->bitmap || !src->bitmap->pixels || width <= 0 || height <= 0) return 0;
    if (dst->memory) return 0;
    tmp = (unsigned int *)malloc((size_t)width * (size_t)height * sizeof(unsigned int));
    if (!tmp) return 0;
    for (row = 0; row < height; row++) {
        for (col = 0; col < width; col++) {
            int px = sx + col, py = sy + row;
            unsigned int p = 0;
            if (px >= 0 && py >= 0 && px < src->bitmap->width && py < src->bitmap->height)
                p = src->bitmap->pixels[py * src->bitmap->width + px];
            tmp[row * width + col] = pixel_to_argb(p, 255);
        }
    }
    texture = SDL_CreateTexture(dst->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, width, height);
    if (!texture) { free(tmp); return 0; }
    SDL_UpdateTexture(texture, NULL, tmp, width * (int)sizeof(unsigned int));
    dr.x = (float)x; dr.y = (float)y; dr.w = (float)width; dr.h = (float)height;
    apply_clip(dst);
    SDL_RenderTexture(dst->renderer, texture, NULL, &dr);
    SDL_DestroyTexture(texture);
    free(tmp);
    return 1;
}

int TransparentBlt(HDC dst, int x, int y, int width, int height, HDC src,
                   int sx, int sy, int sw, int sh, COLORREF transparent) {
    (void)transparent; (void)sw; (void)sh;
    return BitBlt(dst, x, y, width, height, src, sx, sy, SRCCOPY);
}

int AlphaBlend(HDC dst, int x, int y, int width, int height, HDC src,
               int sx, int sy, int sw, int sh, BLENDFUNCTION blend) {
    (void)sx; (void)sy; (void)sw; (void)sh;
    if (dst && dst->renderer) SDL_SetRenderDrawBlendMode(dst->renderer, SDL_BLENDMODE_BLEND);
    if (src && src->bitmap && src->bitmap->pixels) {
        BitBlt(dst, x, y, width, height, src, 0, 0, SRCCOPY);
    }
    if (dst && dst->renderer) SDL_SetRenderDrawBlendMode(dst->renderer, SDL_BLENDMODE_NONE);
    (void)blend;
    return 1;
}

int SaveDC(HDC hdc) {
    SdlGdiSavedState *state;
    if (!hdc || hdc->stack_count >= 16) return 0;
    state = &hdc->stack[hdc->stack_count++];
    state->clip = hdc->clip; state->text_color = hdc->text_color;
    state->brush = hdc->brush; state->pen = hdc->pen; state->font = hdc->font; state->pen_pos = hdc->pen_pos;
    return hdc->stack_count;
}

int RestoreDC(HDC hdc, int saved) {
    SdlGdiSavedState *state;
    if (!hdc || saved <= 0 || saved > hdc->stack_count) return 0;
    hdc->stack_count = saved - 1;
    state = &hdc->stack[hdc->stack_count];
    hdc->clip = state->clip; hdc->text_color = state->text_color;
    hdc->brush = state->brush; hdc->pen = state->pen; hdc->font = state->font; hdc->pen_pos = state->pen_pos;
    apply_clip(hdc);
    return 1;
}

int IntersectClipRect(HDC hdc, int left, int top, int right, int bottom) {
    RECT rect = {left, top, right, bottom};
    if (!hdc) return 0;
    hdc->clip = intersect_rect(hdc->clip, rect);
    apply_clip(hdc);
    return 1;
}

int SelectClipRgn(HDC hdc, HRGN region) {
    int left;
    int top;
    if (!hdc) return 0;
    if (!region) {
        hdc->clip = hdc->client;
        apply_clip(hdc);
        return 1;
    }
    left = region->x;
    top = region->y;
    hdc->clip = (RECT){left, top, left + region->width, top + region->height};
    apply_clip(hdc);
    return 1;
}

int SetStretchBltMode(HDC hdc, int mode) { (void)hdc; return mode; }
int SetBrushOrgEx(HDC hdc, int x, int y, void *point) { (void)hdc; (void)x; (void)y; (void)point; return 1; }

int MoveToEx(HDC hdc, int x, int y, POINT *old_point) {
    if (!hdc) return 0;
    if (old_point) *old_point = hdc->pen_pos;
    hdc->pen_pos = (POINT){x, y};
    return 1;
}

int LineTo(HDC hdc, int x, int y) {
    if (!hdc || hdc->memory) return 0;
    apply_clip(hdc);
    set_draw_color(hdc->renderer, hdc->pen ? hdc->pen->color : RGB(255, 255, 255), 255);
    SDL_RenderLine(hdc->renderer, (float)hdc->pen_pos.x, (float)hdc->pen_pos.y, (float)x, (float)y);
    hdc->pen_pos = (POINT){x, y};
    return 1;
}

int Polyline(HDC hdc, const POINT *points, int count) {
    int i;
    if (!hdc || !points || count <= 0) return 0;
    MoveToEx(hdc, points[0].x, points[0].y, NULL);
    for (i = 1; i < count; i++) LineTo(hdc, points[i].x, points[i].y);
    return 1;
}

int Rectangle(HDC hdc, int left, int top, int right, int bottom) {
    RECT rect = {left, top, right, bottom};
    if (!hdc) return 0;
    if (hdc->brush && !hdc->brush->is_null) FillRect(hdc, &rect, hdc->brush);
    if (hdc->pen) FrameRect(hdc, &rect, (HBRUSH)hdc->pen);
    return 1;
}

int Ellipse(HDC hdc, int left, int top, int right, int bottom) {
    int cx = (left + right) / 2, cy = (top + bottom) / 2;
    int rx = max(1, (right - left) / 2), ry = max(1, (bottom - top) / 2);
    int x, y;
    if (!hdc || hdc->memory) return 0;
    apply_clip(hdc);
    if (hdc->brush && !hdc->brush->is_null) {
        set_draw_color(hdc->renderer, hdc->brush->color, 255);
        for (y = top; y < bottom; y++)
            for (x = left; x < right; x++)
                if (clip_contains(hdc, x, y) && ((x - cx) * (x - cx) * ry * ry + (y - cy) * (y - cy) * rx * rx <= rx * rx * ry * ry))
                    SDL_RenderPoint(hdc->renderer, (float)x, (float)y);
    }
    if (hdc->pen) {
        double a;
        set_draw_color(hdc->renderer, hdc->pen->color, 255);
        for (a = 0.0; a < 6.28318; a += 0.04)
            SDL_RenderPoint(hdc->renderer, (float)(cx + cos(a) * rx), (float)(cy + sin(a) * ry));
    }
    return 1;
}

BOOL InvalidateRect(HWND hwnd, const RECT *rect, BOOL erase) { (void)hwnd; (void)rect; (void)erase; return TRUE; }
HWND SetCapture(HWND hwnd) { return hwnd; }
BOOL ReleaseCapture(void) { return TRUE; }
short GetKeyState(int key) {
    (void)key;
    return 0;
}
