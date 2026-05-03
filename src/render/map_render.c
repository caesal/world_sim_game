#include "render_internal.h"

void draw_crisp_map_surface(HDC hdc, MapLayout layout) {
    BITMAPINFO info;
    unsigned int *pixels;
    HBITMAP bitmap;
    HDC surface_dc;
    HBITMAP old_bitmap;
    int x;
    int y;

    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = MAP_W;
    info.bmiHeader.biHeight = -MAP_H;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    surface_dc = CreateCompatibleDC(hdc);
    bitmap = CreateDIBSection(hdc, &info, DIB_RGB_COLORS, (void **)&pixels, NULL, 0);
    if (!surface_dc || !bitmap || !pixels) {
        if (bitmap) DeleteObject(bitmap);
        if (surface_dc) DeleteDC(surface_dc);
        return;
    }
    old_bitmap = SelectObject(surface_dc, bitmap);

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            COLORREF color = tile_display_color(x, y);
            pixels[y * MAP_W + x] = GetBValue(color) | (GetGValue(color) << 8) | (GetRValue(color) << 16);
        }
    }

    SetStretchBltMode(hdc, COLORONCOLOR);
    StretchBlt(hdc, layout.map_x, layout.map_y, layout.draw_w, layout.draw_h,
               surface_dc, 0, 0, MAP_W, MAP_H, SRCCOPY);
    SelectObject(surface_dc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(surface_dc);
}

void draw_land_texture(HDC hdc, MapLayout layout, int x, int y) {
    int px = tile_left(layout, x);
    int py = tile_top(layout, y);
    int s = layout.tile_size;
    HBRUSH brush;
    COLORREF mark;

    if (!is_land(world[y][x].geography) || s < 12) return;
    if (display_mode == DISPLAY_POLITICAL) return;
    if (((x * 17 + y * 31) % 37) != 0) return;

    mark = world[y][x].climate == CLIMATE_TROPICAL_RAINFOREST ? RGB(24, 105, 42) :
           world[y][x].climate == CLIMATE_OCEANIC ? RGB(35, 113, 58) :
           world[y][x].climate == CLIMATE_TEMPERATE_MONSOON ? RGB(55, 142, 58) :
           world[y][x].climate == CLIMATE_DESERT ? RGB(235, 163, 72) :
           world[y][x].climate == CLIMATE_ICE_CAP ? RGB(245, 252, 255) :
           world[y][x].geography == GEO_WETLAND ? RGB(61, 113, 76) :
           world[y][x].geography == GEO_MOUNTAIN ? RGB(84, 76, 68) :
           world[y][x].geography == GEO_HILL ? RGB(123, 113, 79) :
           RGB(122, 176, 79);
    brush = CreateSolidBrush(mark);
    SelectObject(hdc, brush);
    SelectObject(hdc, GetStockObject(NULL_PEN));
    Ellipse(hdc, px + s / 4, py + s / 5, px + s, py + s * 4 / 5);
    DeleteObject(brush);
}

void draw_rivers(HDC hdc, RECT client, MapLayout layout) {
    int min_x;
    int max_x;
    int min_y;
    int max_y;
    int y;
    int x;
    HPEN river_pen = CreatePen(PS_SOLID, layout.tile_size >= 7 ? 2 : 1, RGB(38, 150, 225));
    HPEN old_pen = SelectObject(hdc, river_pen);

    if (!visible_tile_bounds(client, layout, &min_x, &max_x, &min_y, &max_y)) {
        SelectObject(hdc, old_pen);
        DeleteObject(river_pen);
        return;
    }

    for (y = min_y; y <= max_y; y++) {
        for (x = min_x; x <= max_x; x++) {
            int cx;
            int cy;
            int s;
            if (!world[y][x].river) continue;
            cx = (tile_left(layout, x) + tile_right(layout, x)) / 2;
            cy = (tile_top(layout, y) + tile_bottom(layout, y)) / 2;
            s = layout.tile_size / 2;
            MoveToEx(hdc, cx - s, cy, NULL);
            LineTo(hdc, cx + s, cy);
            if (y + 1 < MAP_H && world[y + 1][x].river) {
                MoveToEx(hdc, cx, cy, NULL);
                LineTo(hdc, cx, (tile_top(layout, y + 1) + tile_bottom(layout, y + 1)) / 2);
            }
            if (x + 1 < MAP_W && world[y][x + 1].river) {
                MoveToEx(hdc, cx, cy, NULL);
                LineTo(hdc, (tile_left(layout, x + 1) + tile_right(layout, x + 1)) / 2, cy);
            }
            if (x + 1 < MAP_W && y + 1 < MAP_H && world[y + 1][x + 1].river) {
                MoveToEx(hdc, cx, cy, NULL);
                LineTo(hdc, (tile_left(layout, x + 1) + tile_right(layout, x + 1)) / 2,
                       (tile_top(layout, y + 1) + tile_bottom(layout, y + 1)) / 2);
            }
            if (x > 0 && y + 1 < MAP_H && world[y + 1][x - 1].river) {
                MoveToEx(hdc, cx, cy, NULL);
                LineTo(hdc, (tile_left(layout, x - 1) + tile_right(layout, x - 1)) / 2,
                       (tile_top(layout, y + 1) + tile_bottom(layout, y + 1)) / 2);
            }
        }
    }
    SelectObject(hdc, old_pen);
    DeleteObject(river_pen);
}

static unsigned int premultiplied_pixel(COLORREF color, int alpha) {
    int r = GetRValue(color) * alpha / 255;
    int g = GetGValue(color) * alpha / 255;
    int b = GetBValue(color) * alpha / 255;
    return (unsigned int)b | ((unsigned int)g << 8) | ((unsigned int)r << 16) | ((unsigned int)alpha << 24);
}

static void put_border_pixel(unsigned int *pixels, int width, int height, int x, int y, COLORREF color, int alpha) {
    unsigned int *target;
    int current_alpha;

    if (x < 0 || x >= width || y < 0 || y >= height) return;
    target = &pixels[y * width + x];
    current_alpha = (int)(*target >> 24);
    if (alpha >= current_alpha) *target = premultiplied_pixel(color, alpha);
}

static void draw_border_vertical(unsigned int *pixels, int width, int height, int x, int y0, int y1,
                                 COLORREF color, int alpha, int line_width) {
    int y;
    int w;

    for (y = y0; y <= y1; y++) {
        for (w = 0; w < line_width; w++) {
            put_border_pixel(pixels, width, height, x + w, y, color, alpha);
            if (line_width > 1) put_border_pixel(pixels, width, height, x - w, y, color, alpha / 2);
        }
    }
}

static void draw_border_horizontal(unsigned int *pixels, int width, int height, int x0, int x1, int y,
                                   COLORREF color, int alpha, int line_width) {
    int x;
    int w;

    for (x = x0; x <= x1; x++) {
        for (w = 0; w < line_width; w++) {
            put_border_pixel(pixels, width, height, x, y + w, color, alpha);
            if (line_width > 1) put_border_pixel(pixels, width, height, x, y - w, color, alpha / 2);
        }
    }
}

static void draw_tile_border_edges(unsigned int *pixels, int width, int height, int x, int y,
                                   int scale, COLORREF color, int alpha, int line_width,
                                   int left, int right, int top, int bottom) {
    int sx0 = x * scale;
    int sy0 = y * scale;
    int sx1 = sx0 + scale - 1;
    int sy1 = sy0 + scale - 1;
    int right_x = sx1 + 1 >= width ? width - 1 : sx1 + 1;
    int bottom_y = sy1 + 1 >= height ? height - 1 : sy1 + 1;

    if (left) draw_border_vertical(pixels, width, height, sx0, sy0, sy1, color, alpha, line_width);
    if (right) draw_border_vertical(pixels, width, height, right_x, sy0, sy1, color, alpha, line_width);
    if (top) draw_border_horizontal(pixels, width, height, sx0, sx1, sy0, color, alpha, line_width);
    if (bottom) draw_border_horizontal(pixels, width, height, sx0, sx1, bottom_y, color, alpha, line_width);
}

void draw_borders(HDC hdc, RECT client, MapLayout layout) {
    enum { BORDER_SCALE = 4 };
    const int border_w = MAP_W * BORDER_SCALE;
    const int border_h = MAP_H * BORDER_SCALE;
    BITMAPINFO info;
    unsigned int *pixels;
    HBITMAP bitmap;
    HDC surface_dc;
    HBITMAP old_bitmap;
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    int x;
    int y;

    (void)client;
    if (layout.draw_w <= 0 || layout.draw_h <= 0) return;
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = border_w;
    info.bmiHeader.biHeight = -border_h;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    surface_dc = CreateCompatibleDC(hdc);
    bitmap = CreateDIBSection(hdc, &info, DIB_RGB_COLORS, (void **)&pixels, NULL, 0);
    if (!surface_dc || !bitmap || !pixels) {
        if (bitmap) DeleteObject(bitmap);
        if (surface_dc) DeleteDC(surface_dc);
        return;
    }
    memset(pixels, 0, (size_t)border_w * border_h * sizeof(*pixels));
    old_bitmap = SelectObject(surface_dc, bitmap);

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int province = city_for_tile(x, y);
            int left = x == 0 || city_for_tile(x - 1, y) != province;
            int right = x == MAP_W - 1 || city_for_tile(x + 1, y) != province;
            int top = y == 0 || city_for_tile(x, y - 1) != province;
            int bottom = y == MAP_H - 1 || city_for_tile(x, y + 1) != province;

            if (province < 0) continue;
            draw_tile_border_edges(pixels, border_w, border_h, x, y,
                                   BORDER_SCALE, RGB(54, 63, 52), 135, 1, left, right, top, bottom);
        }
    }

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int owner = world[y][x].owner;
            int left = x == 0 || world[y][x - 1].owner != owner;
            int right = x == MAP_W - 1 || world[y][x + 1].owner != owner;
            int top = y == 0 || world[y - 1][x].owner != owner;
            int bottom = y == MAP_H - 1 || world[y + 1][x].owner != owner;

            if (owner < 0) continue;
            draw_tile_border_edges(pixels, border_w, border_h, x, y,
                                   BORDER_SCALE, RGB(22, 26, 22), 225, 2, left, right, top, bottom);
        }
    }

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int left = x == 0 || !is_land(world[y][x - 1].geography);
            int right = x == MAP_W - 1 || !is_land(world[y][x + 1].geography);
            int top = y == 0 || !is_land(world[y - 1][x].geography);
            int bottom = y == MAP_H - 1 || !is_land(world[y + 1][x].geography);

            if (!is_land(world[y][x].geography)) continue;
            draw_tile_border_edges(pixels, border_w, border_h, x, y,
                                   BORDER_SCALE, RGB(31, 72, 63), 190, 1, left, right, top, bottom);
        }
    }

    SetStretchBltMode(hdc, HALFTONE);
    AlphaBlend(hdc, layout.map_x, layout.map_y, layout.draw_w, layout.draw_h,
               surface_dc, 0, 0, border_w, border_h, blend);
    SelectObject(surface_dc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(surface_dc);
}

static IconId city_stage_icon(const City *city) {
    if (city->capital) return ICON_CITY_CAPITAL;
    if (city->population >= 520) return ICON_CITY_STAGE;
    if (city->population >= 240) return ICON_CITY_TOWN;
    if (city->population >= 100) return ICON_CITY_VILLAGE;
    return ICON_CITY_OUTPOST;
}

static void draw_city_icon(HDC hdc, int cx, int cy, int size, IconId icon) {
    RECT rect = {cx - size / 2, cy - size / 2, cx + size / 2, cy + size / 2};
    draw_icon(hdc, icon, rect, RGB(20, 20, 20));
}

void draw_cities(HDC hdc, MapLayout layout) {
    int i;
    int s = layout.tile_size;

    for (i = 0; i < city_count; i++) {
        City *city = &cities[i];
        int cx;
        int cy;
        if (!city->alive || city->owner < 0 || city->owner >= civ_count || !civs[city->owner].alive) continue;
        cx = (tile_left(layout, city->x) + tile_right(layout, city->x)) / 2;
        cy = (tile_top(layout, city->y) + tile_bottom(layout, city->y)) / 2;
        draw_city_icon(hdc, cx, cy, clamp(s + (city->capital ? 12 : 8), 14, 30), city_stage_icon(city));
        if (city->port && city->port_x >= 0 && city->port_y >= 0) {
            int px = (tile_left(layout, city->port_x) + tile_right(layout, city->port_x)) / 2;
            int py = (tile_top(layout, city->port_y) + tile_bottom(layout, city->port_y)) / 2;
            RECT port_rect = {px - 8, py - 8, px + 8, py + 8};
            draw_icon(hdc, ICON_HARBOR, port_rect, RGB(36, 135, 205));
        }
    }
}

void draw_selected_tile(HDC hdc, MapLayout layout) {
    RECT rect;
    HPEN pen;
    HPEN old_pen;
    HBRUSH old_brush;

    if (selected_x < 0 || selected_y < 0) return;
    rect.left = tile_left(layout, selected_x);
    rect.top = tile_top(layout, selected_y);
    rect.right = tile_right(layout, selected_x);
    rect.bottom = tile_bottom(layout, selected_y);

    pen = CreatePen(PS_SOLID, 3, RGB(255, 255, 255));
    old_pen = SelectObject(hdc, pen);
    old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
}
