#include "render_internal.h"

typedef struct {
    HDC dc;
    HBITMAP bitmap;
    HBITMAP old_bitmap;
    unsigned int *pixels;
    int display;
    int revision;
    int valid;
} BaseMapSurfaceCache;

static BaseMapSurfaceCache base_surface_cache;

static void release_base_surface_cache(void) {
    if (base_surface_cache.dc && base_surface_cache.old_bitmap) {
        SelectObject(base_surface_cache.dc, base_surface_cache.old_bitmap);
    }
    if (base_surface_cache.bitmap) DeleteObject(base_surface_cache.bitmap);
    if (base_surface_cache.dc) DeleteDC(base_surface_cache.dc);
    memset(&base_surface_cache, 0, sizeof(base_surface_cache));
}

static int ensure_base_surface_cache(HDC hdc) {
    BITMAPINFO info;

    if (base_surface_cache.dc && base_surface_cache.bitmap && base_surface_cache.pixels) return 1;
    release_base_surface_cache();
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = MAP_W;
    info.bmiHeader.biHeight = -MAP_H;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    base_surface_cache.dc = CreateCompatibleDC(hdc);
    base_surface_cache.bitmap = CreateDIBSection(hdc, &info, DIB_RGB_COLORS,
                                                 (void **)&base_surface_cache.pixels, NULL, 0);
    if (!base_surface_cache.dc || !base_surface_cache.bitmap || !base_surface_cache.pixels) {
        release_base_surface_cache();
        return 0;
    }
    base_surface_cache.old_bitmap = SelectObject(base_surface_cache.dc, base_surface_cache.bitmap);
    return 1;
}

static void rebuild_base_surface_cache(void) {
    int x;
    int y;

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            COLORREF color = tile_display_color(x, y);
            base_surface_cache.pixels[y * MAP_W + x] =
                GetBValue(color) | (GetGValue(color) << 8) | (GetRValue(color) << 16);
        }
    }
    base_surface_cache.display = display_mode;
    base_surface_cache.revision = world_visual_revision;
    base_surface_cache.valid = 1;
}

void draw_crisp_map_surface(HDC hdc, MapLayout layout) {
    if (!ensure_base_surface_cache(hdc)) return;
    if (!base_surface_cache.valid || base_surface_cache.display != display_mode ||
        base_surface_cache.revision != world_visual_revision) {
        rebuild_base_surface_cache();
    }

    SetStretchBltMode(hdc, COLORONCOLOR);
    StretchBlt(hdc, layout.map_x, layout.map_y, layout.draw_w, layout.draw_h,
               base_surface_cache.dc, 0, 0, MAP_W, MAP_H, SRCCOPY);
}

static void draw_mountain_marker_if_needed(HDC hdc, MapLayout layout, int x, int y);

void draw_land_texture(HDC hdc, MapLayout layout, int x, int y) {
    int px = tile_left(layout, x);
    int py = tile_top(layout, y);
    int s = layout.tile_size;
    HBRUSH brush;
    COLORREF mark;

    if (!is_land(world[y][x].geography)) return;
    if (s < 12 || display_mode == DISPLAY_POLITICAL || ((x * 17 + y * 31) % 37) != 0) {
        draw_mountain_marker_if_needed(hdc, layout, x, y);
        return;
    }

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
    draw_mountain_marker_if_needed(hdc, layout, x, y);
}

static void draw_mountain_glyph(HDC hdc, int cx, int cy, int size) {
    HPEN pen = CreatePen(PS_SOLID, clamp(size / 7, 1, 3), RGB(34, 33, 28));
    HPEN old_pen = SelectObject(hdc, pen);
    HBRUSH old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    int half = size / 2;
    int small = size / 4;

    MoveToEx(hdc, cx - half, cy + small, NULL);
    LineTo(hdc, cx - small, cy - half);
    LineTo(hdc, cx + small, cy + small);
    MoveToEx(hdc, cx - small / 2, cy + small, NULL);
    LineTo(hdc, cx + small, cy - small);
    LineTo(hdc, cx + half, cy + small);
    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
}

static void draw_mountain_marker_if_needed(HDC hdc, MapLayout layout, int x, int y) {
    int seed = x * 47 + y * 83 + world[y][x].elevation * 5;
    int mountain = world[y][x].geography == GEO_MOUNTAIN;
    int hill = world[y][x].geography == GEO_HILL;
    int cx;
    int cy;
    int size;

    if ((!mountain && !hill) || layout.tile_size < 7) return;
    if (hill && (seed % 29) > 1) return;
    if (mountain && (seed % 19) > 2 && world[y][x].elevation < 78) return;
    if ((seed % 11) > 1 && layout.tile_size < 13) return;
    cx = (tile_left(layout, x) + tile_right(layout, x)) / 2 + (seed % 5 - 2) * layout.tile_size / 8;
    cy = (tile_top(layout, y) + tile_bottom(layout, y)) / 2 + (seed / 5 % 5 - 2) * layout.tile_size / 8;
    size = mountain ? clamp(layout.tile_size + world[y][x].elevation / 18, 9, 22) :
                      clamp(layout.tile_size + world[y][x].elevation / 28, 7, 16);
    draw_mountain_glyph(hdc, cx, cy, size);
}

static POINT river_screen_point(const RiverPath *river, int index, MapLayout layout) {
    RiverPoint point = river->points[index];
    POINT screen;
    int jitter = layout.tile_size >= 9 ? 1 : 0;
    int seed = point.x * 31 + point.y * 17 + index * 13 + river->order * 19;
    int jx = seed % 7 - 3;
    int jy = seed / 7 % 7 - 3;

    screen.x = (tile_left(layout, point.x) + tile_right(layout, point.x)) / 2 + jx * jitter / 4;
    screen.y = (tile_top(layout, point.y) + tile_bottom(layout, point.y)) / 2 + jy * jitter / 4;
    return screen;
}

static int river_pixel_width(const RiverPath *river, MapLayout layout) {
    int main_bonus = river->order >= 3 && layout.tile_size >= 8 ? 1 : 0;
    return clamp(river->width - 1 + main_bonus, 1, 2);
}

static void draw_river_path_stroke(HDC hdc, const RiverPath *river, MapLayout layout,
                                   int width, COLORREF color) {
    POINT points[MAX_RIVER_POINTS];
    HPEN pen;
    HPEN old_pen;
    int i;

    if (!river->active || river->point_count < 2) return;
    for (i = 0; i < river->point_count; i++) points[i] = river_screen_point(river, i, layout);
    pen = CreatePen(PS_SOLID, width, color);
    old_pen = SelectObject(hdc, pen);
    Polyline(hdc, points, river->point_count);
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
}

void draw_rivers(HDC hdc, RECT client, MapLayout layout) {
    int saved_dc;
    int i;

    if (river_path_count <= 0) return;
    saved_dc = SaveDC(hdc);
    IntersectClipRect(hdc, client.left, TOP_BAR_H, client.right - side_panel_w, client.bottom - BOTTOM_BAR_H);
    SetBkMode(hdc, TRANSPARENT);
    for (i = 0; i < river_path_count; i++) {
        int width = river_pixel_width(&river_paths[i], layout);
        draw_river_path_stroke(hdc, &river_paths[i], layout, width + 1, RGB(24, 88, 86));
    }
    for (i = 0; i < river_path_count; i++) {
        int width = river_pixel_width(&river_paths[i], layout);
        draw_river_path_stroke(hdc, &river_paths[i], layout, width, RGB(18, 152, 142));
    }
    RestoreDC(hdc, saved_dc);
}

static IconId city_stage_icon(const City *city) {
    if (city->capital) return ICON_CITY_CAPITAL;
    if (city->population >= 520) return ICON_CITY_STAGE;
    if (city->population >= 240) return ICON_CITY_TOWN;
    if (city->population >= 100) return ICON_CITY_VILLAGE;
    return ICON_CITY_OUTPOST;
}

static void draw_marker_ellipse(HDC hdc, RECT rect, COLORREF fill, COLORREF outline, int outline_width) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, outline_width, outline);
    HBRUSH old_brush = SelectObject(hdc, brush);
    HPEN old_pen = SelectObject(hdc, pen);

    Ellipse(hdc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    DeleteObject(pen);
    DeleteObject(brush);
}

static RECT centered_rect(int cx, int cy, int size) {
    RECT rect = {cx - size / 2, cy - size / 2, cx + size / 2, cy + size / 2};
    return rect;
}

static void draw_city_icon(HDC hdc, int cx, int cy, int size, IconId icon, int capital) {
    int backplate = clamp(size + (capital ? 10 : 6), 16, capital ? 34 : 28);
    int icon_size = clamp(size - 2, 10, capital ? 24 : 20);
    RECT plate = centered_rect(cx, cy, backplate);
    RECT icon_rect = centered_rect(cx, cy, icon_size);

    draw_marker_ellipse(hdc, plate, RGB(232, 218, 176), RGB(28, 27, 23), capital ? 3 : 2);
    if (capital) {
        RECT inner = plate;
        InflateRect(&inner, -4, -4);
        draw_marker_ellipse(hdc, inner, RGB(241, 229, 188), RGB(28, 27, 23), 2);
    }
    draw_icon(hdc, icon, icon_rect, RGB(22, 22, 20));
}

static void draw_harbor_marker(HDC hdc, int cx, int cy, int size) {
    int marker_size = clamp(size, 15, 26);
    int icon_size = clamp(marker_size - 4, 10, 20);
    RECT plate = centered_rect(cx, cy, marker_size);
    RECT icon_rect = centered_rect(cx, cy, icon_size);

    draw_marker_ellipse(hdc, plate, RGB(218, 226, 205), RGB(31, 50, 48), 2);
    draw_icon(hdc, ICON_HARBOR, icon_rect, RGB(24, 94, 116));
}

static void separate_harbor_from_city(int *px, int *py, int cx, int cy, int size) {
    int dx = *px - cx;
    int dy = *py - cy;
    int distance_sq = dx * dx + dy * dy;
    int min_distance = clamp(size + 10, 18, 34);

    if (distance_sq >= min_distance * min_distance) return;
    if (dx == 0 && dy == 0) {
        dx = 1;
        dy = -1;
    }
    *px += dx >= 0 ? min_distance / 2 : -min_distance / 2;
    *py += dy >= 0 ? min_distance / 2 : -min_distance / 2;
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
        draw_city_icon(hdc, cx, cy, clamp(s + (city->capital ? 8 : 4), 12, 26),
                       city_stage_icon(city), city->capital);
        if (city->port && city->port_x >= 0 && city->port_y >= 0) {
            int px = (tile_left(layout, city->port_x) + tile_right(layout, city->port_x)) / 2;
            int py = (tile_top(layout, city->port_y) + tile_bottom(layout, city->port_y)) / 2;
            separate_harbor_from_city(&px, &py, cx, cy, clamp(s + 8, 16, 26));
            draw_harbor_marker(hdc, px, py, clamp(s + 8, 16, 26));
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
