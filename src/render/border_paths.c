#include "border_paths.h"

#include "render_map_internal.h"

typedef struct {
    float x;
    float y;
} MapFloatPoint;

#define CARTO_SCALE 4

static unsigned char mask_a[MAX_MAP_W * MAX_MAP_H];
static unsigned char mask_b[MAX_MAP_W * MAX_MAP_H];

static unsigned int premul(COLORREF color, int alpha) {
    unsigned int r = (unsigned int)(GetRValue(color) * alpha / 255);
    unsigned int g = (unsigned int)(GetGValue(color) * alpha / 255);
    unsigned int b = (unsigned int)(GetBValue(color) * alpha / 255);
    return b | (g << 8) | (r << 16) | ((unsigned int)alpha << 24);
}

static void blend_overlay_pixel(unsigned int *dst, COLORREF color, int alpha) {
    unsigned int old = *dst;
    int inv = 255 - alpha;
    int old_a = (int)(old >> 24);
    int b = GetBValue(color) * alpha / 255 + (int)(old & 255) * inv / 255;
    int g = GetGValue(color) * alpha / 255 + (int)((old >> 8) & 255) * inv / 255;
    int r = GetRValue(color) * alpha / 255 + (int)((old >> 16) & 255) * inv / 255;
    int a = alpha + old_a * inv / 255;
    *dst = (unsigned int)b | ((unsigned int)g << 8) | ((unsigned int)r << 16) | ((unsigned int)a << 24);
}

static unsigned int *begin_overlay(HDC hdc, HDC *memory_dc, HBITMAP *bitmap, HBITMAP *old_bitmap) {
    BITMAPINFO info;
    void *bits = NULL;

    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = MAP_W;
    info.bmiHeader.biHeight = -MAP_H;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    *memory_dc = CreateCompatibleDC(hdc);
    *bitmap = CreateDIBSection(hdc, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!*memory_dc || !*bitmap || !bits) {
        if (*bitmap) DeleteObject(*bitmap);
        if (*memory_dc) DeleteDC(*memory_dc);
        return NULL;
    }
    memset(bits, 0, MAP_W * MAP_H * sizeof(unsigned int));
    *old_bitmap = SelectObject(*memory_dc, *bitmap);
    return (unsigned int *)bits;
}

static void finish_overlay(HDC hdc, MapLayout layout, HDC memory_dc, HBITMAP bitmap, HBITMAP old_bitmap) {
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};

    SetStretchBltMode(hdc, HALFTONE);
    SetBrushOrgEx(hdc, 0, 0, NULL);
    AlphaBlend(hdc, layout.map_x, layout.map_y, layout.draw_w, layout.draw_h,
               memory_dc, 0, 0, MAP_W, MAP_H, blend);
    SelectObject(memory_dc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(memory_dc);
}

static unsigned int *begin_vector_layer(HDC hdc, HDC *memory_dc, HBITMAP *bitmap, HBITMAP *old_bitmap) {
    BITMAPINFO info;
    void *bits = NULL;

    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = MAP_W * CARTO_SCALE;
    info.bmiHeader.biHeight = -MAP_H * CARTO_SCALE;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    *memory_dc = CreateCompatibleDC(hdc);
    *bitmap = CreateDIBSection(hdc, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!*memory_dc || !*bitmap || !bits) {
        if (*bitmap) DeleteObject(*bitmap);
        if (*memory_dc) DeleteDC(*memory_dc);
        return NULL;
    }
    memset(bits, 0, (size_t)MAP_W * MAP_H * CARTO_SCALE * CARTO_SCALE * sizeof(unsigned int));
    *old_bitmap = SelectObject(*memory_dc, *bitmap);
    return (unsigned int *)bits;
}

static void apply_vector_alpha(unsigned int *pixels, int alpha) {
    int total = MAP_W * MAP_H * CARTO_SCALE * CARTO_SCALE;
    int i;

    for (i = 0; i < total; i++) {
        unsigned int rgb = pixels[i] & 0x00ffffff;
        if (rgb) {
            unsigned int b = (rgb & 255) * (unsigned int)alpha / 255;
            unsigned int g = ((rgb >> 8) & 255) * (unsigned int)alpha / 255;
            unsigned int r = ((rgb >> 16) & 255) * (unsigned int)alpha / 255;
            pixels[i] = b | (g << 8) | (r << 16) | ((unsigned int)alpha << 24);
        }
    }
}

static void finish_vector_layer(HDC hdc, MapLayout layout, HDC memory_dc, HBITMAP bitmap,
                                HBITMAP old_bitmap, unsigned int *pixels, int alpha) {
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};

    if (pixels) apply_vector_alpha(pixels, alpha);
    SetStretchBltMode(hdc, HALFTONE);
    SetBrushOrgEx(hdc, 0, 0, NULL);
    AlphaBlend(hdc, layout.map_x, layout.map_y, layout.draw_w, layout.draw_h,
               memory_dc, 0, 0, MAP_W * CARTO_SCALE, MAP_H * CARTO_SCALE, blend);
    SelectObject(memory_dc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(memory_dc);
}

static void build_owner_mask(int owner) {
    int x;
    int y;

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int active = world[y][x].owner == owner && owner >= 0 && owner < civ_count && civs[owner].alive;
            mask_a[y * MAP_W + x] = active ? 255 : 0;
        }
    }
}

static void build_province_mask(int province_id) {
    int x;
    int y;

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int active = province_id >= 0 && province_id < city_count && cities[province_id].alive &&
                         world[y][x].province_id == province_id;
            mask_a[y * MAP_W + x] = active ? 255 : 0;
        }
    }
}

static void build_land_mask(void) {
    int x;
    int y;

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) mask_a[y * MAP_W + x] = is_land(world[y][x].geography) ? 255 : 0;
    }
}

static void blur_once(const unsigned char *src, unsigned char *dst) {
    int x;
    int y;

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int sum = 0;
            int count = 0;
            int dy;
            int dx;

            for (dy = -1; dy <= 1; dy++) {
                int sy = clamp(y + dy, 0, MAP_H - 1);
                for (dx = -1; dx <= 1; dx++) {
                    int sx = clamp(x + dx, 0, MAP_W - 1);
                    sum += src[sy * MAP_W + sx];
                    count++;
                }
            }
            dst[y * MAP_W + x] = (unsigned char)(sum / count);
        }
    }
}

static void blur_current_mask(int passes) {
    int i;

    for (i = 0; i < passes; i++) {
        blur_once(mask_a, mask_b);
        memcpy(mask_a, mask_b, (size_t)MAP_W * MAP_H);
    }
}

static void filter_current_mask(int passes, int fill_threshold, int keep_threshold) {
    int pass;

    for (pass = 0; pass < passes; pass++) {
        int x;
        int y;

        for (y = 0; y < MAP_H; y++) {
            for (x = 0; x < MAP_W; x++) {
                int neighbors = 0;
                int dy;
                int dx;
                int active = mask_a[y * MAP_W + x] >= 128;

                for (dy = -1; dy <= 1; dy++) {
                    int sy = clamp(y + dy, 0, MAP_H - 1);
                    for (dx = -1; dx <= 1; dx++) {
                        int sx = clamp(x + dx, 0, MAP_W - 1);
                        if (mask_a[sy * MAP_W + sx] >= 128) neighbors++;
                    }
                }
                mask_b[y * MAP_W + x] = (active ? neighbors >= keep_threshold :
                                                   neighbors >= fill_threshold) ? 255 : 0;
            }
        }
        memcpy(mask_a, mask_b, (size_t)MAP_W * MAP_H);
    }
}

static void apply_mask_overlay(unsigned int *pixels, COLORREF color, int max_alpha) {
    int i;

    for (i = 0; i < MAP_W * MAP_H; i++) {
        int alpha = mask_a[i] * max_alpha / 255;
        if (alpha > 0) blend_overlay_pixel(&pixels[i], color, alpha);
    }
}

void draw_political_region_fills(HDC hdc, RECT client, MapLayout layout) {
    HDC memory_dc;
    HBITMAP bitmap;
    HBITMAP old_bitmap;
    unsigned int *pixels;
    int i;

    (void)client;
    if (display_mode != DISPLAY_POLITICAL) return;
    pixels = begin_overlay(hdc, &memory_dc, &bitmap, &old_bitmap);
    if (!pixels) return;
    for (i = 0; i < civ_count; i++) {
        if (!civs[i].alive) continue;
        build_owner_mask(i);
        blur_current_mask(1);
        apply_mask_overlay(pixels, civs[i].color, 58);
    }
    finish_overlay(hdc, layout, memory_dc, bitmap, old_bitmap);
}

void draw_coast_halo(HDC hdc, RECT client, MapLayout layout) {
    HDC memory_dc;
    HBITMAP bitmap;
    HBITMAP old_bitmap;
    unsigned int *pixels;
    int x;
    int y;

    (void)client;
    pixels = begin_overlay(hdc, &memory_dc, &bitmap, &old_bitmap);
    if (!pixels) return;
    build_land_mask();
    filter_current_mask(1, 6, 3);
    blur_current_mask(3);
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int idx = y * MAP_W + x;
            int alpha;
            if (is_land(world[y][x].geography)) continue;
            alpha = clamp(mask_a[idx] - 22, 0, 112) * 28 / 112;
            if (alpha > 0) pixels[idx] = premul(RGB(142, 190, 177), alpha);
        }
    }
    finish_overlay(hdc, layout, memory_dc, bitmap, old_bitmap);
}

static float iso_t(int a, int b) {
    if (a == b) return 0.5f;
    return (float)clamp((128 - a) * 1000 / (b - a), 0, 1000) / 1000.0f;
}

static MapFloatPoint edge_point(int x, int y, int edge, int v[4]) {
    MapFloatPoint p = {(float)x, (float)y};

    if (edge == 0) {
        p.x += iso_t(v[0], v[1]);
    } else if (edge == 1) {
        p.x += 1.0f;
        p.y += iso_t(v[1], v[2]);
    } else if (edge == 2) {
        p.x += iso_t(v[3], v[2]);
        p.y += 1.0f;
    } else {
        p.y += iso_t(v[0], v[3]);
    }
    return p;
}

static POINT map_to_layer(MapFloatPoint p) {
    POINT point;

    point.x = (int)(p.x * CARTO_SCALE);
    point.y = (int)(p.y * CARTO_SCALE);
    return point;
}

static int layer_pen_width_for_screen(MapLayout layout, int screen_width) {
    int source_width;

    if (layout.draw_w <= 0) return 1;
    source_width = screen_width * MAP_W * CARTO_SCALE / layout.draw_w;
    return clamp(source_width, 1, 12);
}

static void draw_iso_mask(HDC hdc, MapLayout layout, COLORREF color, int screen_width) {
    HPEN pen = CreatePen(PS_SOLID, layer_pen_width_for_screen(layout, screen_width), color);
    HPEN old_pen = SelectObject(hdc, pen);
    int x;
    int y;

    for (y = 0; y < MAP_H - 1; y++) {
        for (x = 0; x < MAP_W - 1; x++) {
            int v[4];
            int edges[4];
            int count = 0;
            int e;

            v[0] = mask_a[y * MAP_W + x];
            v[1] = mask_a[y * MAP_W + x + 1];
            v[2] = mask_a[(y + 1) * MAP_W + x + 1];
            v[3] = mask_a[(y + 1) * MAP_W + x];
            for (e = 0; e < 4; e++) {
                int a = v[e];
                int b = v[(e + 1) % 4];
                if ((a < 128 && b >= 128) || (a >= 128 && b < 128)) edges[count++] = e;
            }
            if (count >= 2) {
                MapFloatPoint a = edge_point(x, y, edges[0], v);
                MapFloatPoint b = edge_point(x, y, edges[1], v);
                POINT pa = map_to_layer(a);
                POINT pb = map_to_layer(b);
                MoveToEx(hdc, pa.x, pa.y, NULL);
                LineTo(hdc, pb.x, pb.y);
            }
            if (count == 4) {
                MapFloatPoint a = edge_point(x, y, edges[2], v);
                MapFloatPoint b = edge_point(x, y, edges[3], v);
                POINT pa = map_to_layer(a);
                POINT pb = map_to_layer(b);
                MoveToEx(hdc, pa.x, pa.y, NULL);
                LineTo(hdc, pb.x, pb.y);
            }
        }
    }
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
}

void draw_province_border_paths(HDC hdc, RECT client, MapLayout layout) {
    HDC memory_dc;
    HBITMAP bitmap;
    HBITMAP old_bitmap;
    unsigned int *pixels;
    int i;
    int width = 1;

    (void)client;
    if (layout.tile_size < 6) return;
    pixels = begin_vector_layer(hdc, &memory_dc, &bitmap, &old_bitmap);
    if (!pixels) return;
    SetBkMode(memory_dc, TRANSPARENT);
    for (i = 0; i < city_count; i++) {
        if (!cities[i].alive || cities[i].owner < 0) continue;
        build_province_mask(i);
        blur_current_mask(1);
        draw_iso_mask(memory_dc, layout, RGB(68, 66, 48), width);
    }
    finish_vector_layer(hdc, layout, memory_dc, bitmap, old_bitmap, pixels, 58);
}

void draw_country_border_paths(HDC hdc, RECT client, MapLayout layout) {
    HDC memory_dc;
    HBITMAP bitmap;
    HBITMAP old_bitmap;
    unsigned int *pixels;
    int i;
    int outer = layout.tile_size >= 7 ? 3 : 2;
    int inner = 1;

    (void)client;
    pixels = begin_vector_layer(hdc, &memory_dc, &bitmap, &old_bitmap);
    if (!pixels) return;
    SetBkMode(memory_dc, TRANSPARENT);
    for (i = 0; i < civ_count; i++) {
        if (!civs[i].alive) continue;
        build_owner_mask(i);
        filter_current_mask(1, 6, 3);
        blur_current_mask(2);
        draw_iso_mask(memory_dc, layout, RGB(42, 38, 31), outer);
        draw_iso_mask(memory_dc, layout, blend_color(civs[i].color, RGB(50, 40, 32), 20), inner);
    }
    finish_vector_layer(hdc, layout, memory_dc, bitmap, old_bitmap, pixels, 132);
}

void draw_coastline_paths(HDC hdc, RECT client, MapLayout layout) {
    HDC memory_dc;
    HBITMAP bitmap;
    HBITMAP old_bitmap;
    unsigned int *pixels;
    int outer = 2;
    int inner = 1;

    (void)client;
    pixels = begin_vector_layer(hdc, &memory_dc, &bitmap, &old_bitmap);
    if (!pixels) return;
    SetBkMode(memory_dc, TRANSPARENT);
    build_land_mask();
    filter_current_mask(1, 6, 3);
    blur_current_mask(2);
    draw_iso_mask(memory_dc, layout, RGB(24, 45, 39), outer);
    draw_iso_mask(memory_dc, layout, RGB(64, 112, 94), inner);
    finish_vector_layer(hdc, layout, memory_dc, bitmap, old_bitmap, pixels, 112);
}

void draw_map_grid_overlay(HDC hdc, RECT client, MapLayout layout) {
    int step = 100;
    int i;

    (void)client;
    for (i = 0; i <= MAP_W; i += step) {
        int sx = layout.map_x + i * layout.draw_w / MAP_W;
        RECT line = {sx, layout.map_y, sx + 1, layout.map_y + layout.draw_h};
        fill_rect_alpha(hdc, line, RGB(222, 211, 150), 42);
    }
    for (i = 0; i <= MAP_H; i += step) {
        int sy = layout.map_y + i * layout.draw_h / MAP_H;
        RECT line = {layout.map_x, sy, layout.map_x + layout.draw_w, sy + 1};
        fill_rect_alpha(hdc, line, RGB(222, 211, 150), 38);
    }
}
