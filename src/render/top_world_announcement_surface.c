#include "render/top_world_announcement_surface.h"
#include "render/top_world_announcement_resources.h"

#include <string.h>

#define ANNOUNCEMENT_ICON_SIZE 24
#define ANNOUNCEMENT_TRANSPARENT_KEY RGB(1, 2, 3)

static HDC surface_dc;
static HBITMAP surface_bitmap;
static HBITMAP surface_default_bitmap;
static int surface_width;
static int surface_height;
static unsigned int *surface_bits;
static int surface_valid;
static RECT surface_band;
static TopWorldAnnouncementSurfaceKey surface_key;
static HDC overlay_dc;
static HBITMAP overlay_bitmap;
static HBITMAP overlay_default_bitmap;
static unsigned int *overlay_bits;
static HDC base_dc;
static HBITMAP base_bitmap;
static HBITMAP base_default_bitmap;
static int base_width;
static int base_height;
static unsigned int *base_bits;
static int base_valid;
static RECT base_band;
static TopWorldAnnouncementUnderlayKey base_key;
static HDC composed_dc;
static HBITMAP composed_bitmap;
static HBITMAP composed_default_bitmap;
static unsigned int *composed_bits;
static HDC alpha_dc;
static HBITMAP alpha_bitmap;
static COLORREF alpha_color = CLR_INVALID;
static unsigned int *alpha_bits;
static HBRUSH transparent_brush;
static int composite_prewarmed;

static int same_rect(RECT a, RECT b) {
    return a.left == b.left && a.top == b.top &&
           a.right == b.right && a.bottom == b.bottom;
}

static HBITMAP create_top_down_dib(HDC target, int width, int height,
                                   unsigned int **bits) {
    BITMAPINFO info;
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    return CreateDIBSection(target, &info, DIB_RGB_COLORS, (void **)bits, NULL, 0);
}

static unsigned int dib_rgb(COLORREF color) {
    return (unsigned int)GetBValue(color) |
           ((unsigned int)GetGValue(color) << 8) |
           ((unsigned int)GetRValue(color) << 16);
}

static int ensure_bitmap(HDC target, int width, int height) {
    HBITMAP bitmap;
    HBITMAP overlay;
    HBITMAP previous;
    unsigned int *bits = NULL;
    unsigned int *alpha_overlay_bits = NULL;
    if (surface_dc && surface_bitmap &&
        overlay_dc && overlay_bitmap &&
        surface_width >= width && surface_height >= height) return 1;
    if (!surface_dc) surface_dc = CreateCompatibleDC(target);
    if (!overlay_dc) overlay_dc = CreateCompatibleDC(target);
    if (!surface_dc || !overlay_dc) return 0;
    bitmap = create_top_down_dib(target, width, height, &bits);
    overlay = create_top_down_dib(target, width, height, &alpha_overlay_bits);
    if (!bitmap || !overlay || !bits || !alpha_overlay_bits) {
        if (bitmap) DeleteObject(bitmap);
        if (overlay) DeleteObject(overlay);
        return 0;
    }
    if (surface_bitmap) {
        SelectObject(surface_dc, surface_default_bitmap);
        DeleteObject(surface_bitmap);
    }
    if (overlay_bitmap) {
        SelectObject(overlay_dc, overlay_default_bitmap);
        DeleteObject(overlay_bitmap);
    }
    previous = SelectObject(surface_dc, bitmap);
    if (!surface_default_bitmap) surface_default_bitmap = previous;
    previous = SelectObject(overlay_dc, overlay);
    if (!overlay_default_bitmap) overlay_default_bitmap = previous;
    surface_bitmap = bitmap;
    overlay_bitmap = overlay;
    surface_bits = bits;
    overlay_bits = alpha_overlay_bits;
    surface_width = width;
    surface_height = height;
    surface_valid = 0;
    return 1;
}

static void compile_alpha_overlay(int width, int height) {
    unsigned int key = dib_rgb(ANNOUNCEMENT_TRANSPARENT_KEY);
    unsigned int background =
        ((unsigned int)((34 * 191 + 127) / 255)) |
        ((unsigned int)((30 * 191 + 127) / 255) << 8) |
        ((unsigned int)((24 * 191 + 127) / 255) << 16) |
        (191u << 24);
    int x;
    int y;
    GdiFlush();
    for (y = 0; y < height; y++) {
        unsigned int *source = surface_bits + y * surface_width;
        unsigned int *target = overlay_bits + y * surface_width;
        for (x = 0; x < width; x++) {
            unsigned int pixel = source[x] & 0x00ffffffu;
            target[x] = pixel == key ? background : pixel | 0xff000000u;
        }
    }
}

static int ensure_base_bitmap(HDC target, int width, int height) {
    HBITMAP bitmap;
    HBITMAP composed;
    HBITMAP previous;
    unsigned int *bits = NULL;
    unsigned int *finished_bits = NULL;
    if (base_dc && base_bitmap && composed_dc && composed_bitmap &&
        base_width >= width && base_height >= height) return 1;
    if (!base_dc) base_dc = CreateCompatibleDC(target);
    if (!composed_dc) composed_dc = CreateCompatibleDC(target);
    if (!base_dc || !composed_dc) return 0;
    bitmap = create_top_down_dib(target, width, height, &bits);
    composed = create_top_down_dib(target, width, height, &finished_bits);
    if (!bitmap || !composed || !bits || !finished_bits) {
        if (bitmap) DeleteObject(bitmap);
        if (composed) DeleteObject(composed);
        return 0;
    }
    if (base_bitmap) {
        SelectObject(base_dc, base_default_bitmap);
        DeleteObject(base_bitmap);
    }
    if (composed_bitmap) {
        SelectObject(composed_dc, composed_default_bitmap);
        DeleteObject(composed_bitmap);
    }
    previous = SelectObject(base_dc, bitmap);
    if (!base_default_bitmap) base_default_bitmap = previous;
    previous = SelectObject(composed_dc, composed);
    if (!composed_default_bitmap) composed_default_bitmap = previous;
    base_bitmap = bitmap;
    composed_bitmap = composed;
    base_bits = bits;
    composed_bits = finished_bits;
    base_width = width;
    base_height = height;
    base_valid = 0;
    return 1;
}

static void compose_overlay_over_underlay(int width, int height) {
    int x;
    int y;
    GdiFlush();
    for (y = 0; y < height; y++) {
        unsigned int *underlay = base_bits + y * base_width;
        unsigned int *overlay = overlay_bits + y * surface_width;
        unsigned int *finished = composed_bits + y * base_width;
        for (x = 0; x < width; x++) {
            unsigned int source = overlay[x];
            unsigned int alpha = source >> 24;
            unsigned int inverse = 255 - alpha;
            unsigned int target = underlay[x];
            unsigned int blue = (source & 255u) +
                (((target & 255u) * inverse + 127u) / 255u);
            unsigned int green = ((source >> 8) & 255u) +
                ((((target >> 8) & 255u) * inverse + 127u) / 255u);
            unsigned int red = ((source >> 16) & 255u) +
                ((((target >> 16) & 255u) * inverse + 127u) / 255u);
            finished[x] = min(blue, 255u) | (min(green, 255u) << 8) |
                          (min(red, 255u) << 16) | 0xff000000u;
        }
    }
}

int top_world_announcement_surface_begin(HDC target, RECT band,
                                         const TopWorldAnnouncementSurfaceKey *key,
                                         HDC *surface) {
    int width = band.right - band.left;
    int height = band.bottom - band.top;
    if (!target || !key || !surface || width <= 0 || height <= 0) return -1;
    if (!ensure_bitmap(target, width, height)) {
        *surface = target;
        return -1;
    }
    if (surface_valid && same_rect(surface_band, band) &&
        memcmp(&surface_key, key, sizeof(*key)) == 0) {
        *surface = surface_dc;
        return 0;
    }
    SetWindowOrgEx(surface_dc, 0, 0, NULL);
    if (!transparent_brush) transparent_brush = CreateSolidBrush(ANNOUNCEMENT_TRANSPARENT_KEY);
    if (!transparent_brush) {
        *surface = target;
        return -1;
    }
    FillRect(surface_dc, &(RECT){0, 0, width, height}, transparent_brush);
    SetWindowOrgEx(surface_dc, band.left, band.top, NULL);
    surface_band = band;
    surface_key = *key;
    surface_valid = 0;
    *surface = surface_dc;
    return 1;
}

void top_world_announcement_surface_finish(void) {
    int width = surface_band.right - surface_band.left;
    int height = surface_band.bottom - surface_band.top;
    if (!surface_dc || !surface_bitmap || !overlay_dc || !overlay_bitmap) return;
    SetWindowOrgEx(surface_dc, 0, 0, NULL);
    compile_alpha_overlay(width, height);
    surface_valid = 1;
}

void top_world_announcement_surface_blit(HDC target, RECT band) {
    int width = band.right - band.left;
    int height = band.bottom - band.top;
    if (!target || !surface_valid || !overlay_dc || !same_rect(surface_band, band)) return;
    if (!ensure_base_bitmap(target, width, height)) return;
    if (!base_valid || !same_rect(base_band, band)) {
        SetWindowOrgEx(base_dc, 0, 0, NULL);
        BitBlt(base_dc, 0, 0, width, height, target, band.left, band.top, SRCCOPY);
    }
    compose_overlay_over_underlay(width, height);
    BitBlt(target, band.left, band.top, width, height, composed_dc, 0, 0, SRCCOPY);
}

void top_world_announcement_surface_invalidate(void) {
    surface_valid = 0;
}

int top_world_announcement_surface_capture_underlay(
    HDC target, RECT band, const TopWorldAnnouncementUnderlayKey *key) {
    int width = band.right - band.left;
    int height = band.bottom - band.top;
    if (!target || !key || width <= 0 || height <= 0) return 0;
    if (!ensure_base_bitmap(target, width, height)) return 0;
    SetWindowOrgEx(base_dc, 0, 0, NULL);
    BitBlt(base_dc, 0, 0, width, height, target, band.left, band.top, SRCCOPY);
    base_band = band;
    base_key = *key;
    base_valid = 1;
    return 1;
}

int top_world_announcement_surface_underlay_matches(
    RECT band, const TopWorldAnnouncementUnderlayKey *key) {
    return key && base_valid && same_rect(base_band, band) &&
           memcmp(&base_key, key, sizeof(*key)) == 0;
}

int top_world_announcement_surface_restore_underlay(
    HDC target, RECT band, const TopWorldAnnouncementUnderlayKey *key) {
    if (!target || !top_world_announcement_surface_underlay_matches(band, key)) return 0;
    BitBlt(target, band.left, band.top, band.right - band.left, band.bottom - band.top,
           base_dc, 0, 0, SRCCOPY);
    return 1;
}

void top_world_announcement_surface_invalidate_underlay(void) {
    base_valid = 0;
}

void top_world_announcement_surface_prewarm(HDC target, RECT band) {
    int width = band.right - band.left;
    int height = band.bottom - band.top;
    RECT local = {0, 0, width, height};
    if (!target || width <= 0 || height <= 0) return;
    if (!ensure_bitmap(target, width, height) ||
        !ensure_base_bitmap(target, width, height)) return;
    if (!transparent_brush) transparent_brush = CreateSolidBrush(ANNOUNCEMENT_TRANSPARENT_KEY);
    if (!transparent_brush || composite_prewarmed) return;
    SetWindowOrgEx(surface_dc, 0, 0, NULL);
    SetWindowOrgEx(base_dc, 0, 0, NULL);
    FillRect(surface_dc, &local, transparent_brush);
    PatBlt(base_dc, 0, 0, width, height, BLACKNESS);
    compile_alpha_overlay(width, height);
    compose_overlay_over_underlay(width, height);
    BitBlt(base_dc, 0, 0, width, height, composed_dc, 0, 0, SRCCOPY);
    composite_prewarmed = 1;
    surface_valid = 0;
    base_valid = 0;
}

void top_world_announcement_surface_draw_icon(HDC target, RECT band,
                                              IconId icon, COLORREF fallback) {
    RECT rect = {band.left + 9, band.top + 8,
                 band.left + 9 + ANNOUNCEMENT_ICON_SIZE,
                 band.top + 8 + ANNOUNCEMENT_ICON_SIZE};
    top_world_announcement_draw_cached_icon(target, icon, rect, fallback);
}

void top_world_announcement_surface_fill_alpha(HDC target, RECT rect,
                                               COLORREF color, BYTE alpha) {
    BITMAPINFO info;
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, alpha, 0};
    HBITMAP previous;
    if (!target || rect.right <= rect.left || rect.bottom <= rect.top) return;
    if (!alpha_dc) alpha_dc = CreateCompatibleDC(target);
    if (!alpha_dc) return;
    if (!alpha_bitmap) {
        memset(&info, 0, sizeof(info));
        info.bmiHeader.biSize = sizeof(info.bmiHeader);
        info.bmiHeader.biWidth = 1;
        info.bmiHeader.biHeight = -1;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        alpha_bitmap = CreateDIBSection(target, &info, DIB_RGB_COLORS,
                                        (void **)&alpha_bits, NULL, 0);
        if (!alpha_bitmap || !alpha_bits) {
            if (alpha_bitmap) DeleteObject(alpha_bitmap);
            alpha_bitmap = NULL;
            alpha_bits = NULL;
            return;
        }
        previous = SelectObject(alpha_dc, alpha_bitmap);
        (void)previous;
    }
    if (alpha_color != color) {
        *alpha_bits = (unsigned int)GetBValue(color) |
                      ((unsigned int)GetGValue(color) << 8) |
                      ((unsigned int)GetRValue(color) << 16);
        alpha_color = color;
    }
    AlphaBlend(target, rect.left, rect.top, rect.right - rect.left,
               rect.bottom - rect.top, alpha_dc, 0, 0, 1, 1, blend);
}
