#include "render.h"

#include "../core/version.h"
#include "../world/world.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef enum {
    ICON_BATTLE,
    ICON_EXPANSION,
    ICON_POPULATION,
    ICON_COUNTRY_DEFENSE,
    ICON_CULTURE,
    ICON_WATER,
    ICON_GEOGRAPHY,
    ICON_CLIMATE,
    ICON_HABITABILITY,
    ICON_TILE_DEFENSE,
    ICON_ATTACK,
    ICON_FOOD,
    ICON_LIVESTOCK,
    ICON_WOOD,
    ICON_ORE,
    ICON_CITY_CAPITAL,
    ICON_DISORDER,
    ICON_TERRITORY,
    ICON_MIGRATION,
    ICON_ECONOMY,
    ICON_COUNT
} IconId;

typedef struct {
    unsigned int GdiplusVersion;
    void *DebugEventCallback;
    BOOL SuppressBackgroundThread;
    BOOL SuppressExternalCodecs;
} GdiplusStartupInputLocal;

typedef int (WINAPI *GdiplusStartupProc)(ULONG_PTR *, const GdiplusStartupInputLocal *, void *);
typedef int (WINAPI *GdipCreateBitmapFromFileProc)(const WCHAR *, void **);
typedef int (WINAPI *GdipCreateHBITMAPFromBitmapProc)(void *, HBITMAP *, unsigned int);
typedef int (WINAPI *GdipDisposeImageProc)(void *);

static const char *ICON_PATHS[ICON_COUNT] = {
    "assets\\icons\\da4cfdad-bd6a-42c8-b000-26f9e1701a04.png",
    "assets\\icons\\f5dd2d6c-49fd-4360-8880-82c2c7947606.png",
    "assets\\icons\\84e4f335-63ca-43bf-a468-d8377a2f0d93.png",
    "assets\\icons\\2575440b-6437-4710-9f9e-7d468c96c387.png",
    "assets\\icons\\549db0c0-dce6-4735-9cdf-ef6227b95624.png",
    "assets\\icons\\c8bdab75-378e-4028-925f-92bac56c2cbc.png",
    "assets\\icons\\64370779-f76e-4d64-bf0d-14917f10af4c.png",
    "assets\\icons\\ed02970b-d72c-44fa-86c3-db0faf3fb6e2.png",
    "assets\\icons\\aa96daaa-1065-4065-9f69-be6d4a8d2bf4.png",
    "assets\\icons\\07ecac66-980b-40e8-96ec-c5de1cffac89.png",
    "assets\\icons\\d550bb66-b977-470d-b353-3749411e7da3.png",
    "assets\\icons\\646747fb-7988-404b-9028-ec7212972a2f.png",
    "assets\\icons\\d0c0d0b9-04d9-4c67-8088-d86cff8398d7.png",
    "assets\\icons\\a65bd62d-9257-4e4a-b101-b87deb871acc.png",
    "assets\\icons\\b1c1848f-5923-459f-af35-2eb77592ed58.png",
    "assets\\icons\\f5dd2d6c-49fd-4360-8880-82c2c7947606.png",
    "assets\\icons\\135b0de7-31bb-4df1-8f48-26e5d0d0ac30.png",
    "assets\\icons\\fd38cff2-4ab8-433c-b5e1-3ebe57ffcc21.png",
    "assets\\icons\\277ee907-1171-4b34-b1d3-756f4710c737.png",
    "assets\\icons\\7176f3a9-9e25-41c5-95c6-1575a234b251.png"
};

static HMODULE gdiplus_module;
static ULONG_PTR gdiplus_token;
static GdiplusStartupProc gdiplus_startup;
static GdipCreateBitmapFromFileProc gdip_create_bitmap_from_file;
static GdipCreateHBITMAPFromBitmapProc gdip_create_hbitmap_from_bitmap;
static GdipDisposeImageProc gdip_dispose_image;
static HBITMAP icon_bitmaps[ICON_COUNT];
static int icon_load_attempted[ICON_COUNT];

MapLayout get_map_layout(RECT client) {
    MapLayout layout;
    int available_w = (client.right - client.left) - side_panel_w - 36;
    int available_h = (client.bottom - client.top) - TOP_BAR_H - BOTTOM_BAR_H - 24;
    int fit_w = available_w;
    int fit_h = fit_w * MAP_H / MAP_W;

    if (fit_h > available_h) {
        fit_h = available_h;
        fit_w = fit_h * MAP_W / MAP_H;
    }
    layout.draw_w = clamp(fit_w * map_zoom_percent / 100, MAP_W / 2, MAP_W * 10);
    layout.draw_h = clamp(fit_h * map_zoom_percent / 100, MAP_H / 2, MAP_H * 10);
    layout.tile_size = clamp(layout.draw_w / MAP_W, 1, 42);
    layout.map_x = 18 + map_offset_x;
    layout.map_y = TOP_BAR_H + 10 + map_offset_y;
    return layout;
}

static void fill_rect(HDC hdc, RECT rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
}

static void draw_text_line(HDC hdc, int x, int y, const char *text, COLORREF color) {
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    TextOutA(hdc, x, y, text, (int)strlen(text));
}

static void draw_center_text(HDC hdc, RECT rect, const char *text, COLORREF color) {
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    DrawTextA(hdc, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static int point_in_rect_local(RECT rect, int x, int y) {
    return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
}

static int ensure_gdiplus(void) {
    GdiplusStartupInputLocal input;
    union { FARPROC raw; GdiplusStartupProc typed; } startup_proc;
    union { FARPROC raw; GdipCreateBitmapFromFileProc typed; } create_bitmap_proc;
    union { FARPROC raw; GdipCreateHBITMAPFromBitmapProc typed; } create_hbitmap_proc;
    union { FARPROC raw; GdipDisposeImageProc typed; } dispose_proc;

    if (gdiplus_token) return 1;
    if (!gdiplus_module) {
        gdiplus_module = LoadLibraryA("gdiplus.dll");
        if (!gdiplus_module) return 0;
        startup_proc.raw = GetProcAddress(gdiplus_module, "GdiplusStartup");
        create_bitmap_proc.raw = GetProcAddress(gdiplus_module, "GdipCreateBitmapFromFile");
        create_hbitmap_proc.raw = GetProcAddress(gdiplus_module, "GdipCreateHBITMAPFromBitmap");
        dispose_proc.raw = GetProcAddress(gdiplus_module, "GdipDisposeImage");
        gdiplus_startup = startup_proc.typed;
        gdip_create_bitmap_from_file = create_bitmap_proc.typed;
        gdip_create_hbitmap_from_bitmap = create_hbitmap_proc.typed;
        gdip_dispose_image = dispose_proc.typed;
    }
    if (!gdiplus_startup || !gdip_create_bitmap_from_file || !gdip_create_hbitmap_from_bitmap || !gdip_dispose_image) return 0;
    memset(&input, 0, sizeof(input));
    input.GdiplusVersion = 1;
    return gdiplus_startup(&gdiplus_token, &input, NULL) == 0;
}

static HBITMAP load_png_icon(const char *path) {
    WCHAR wide_path[MAX_PATH];
    void *bitmap = NULL;
    HBITMAP hbitmap = NULL;

    if (!ensure_gdiplus()) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wide_path, MAX_PATH);
    if (gdip_create_bitmap_from_file(wide_path, &bitmap) != 0 || !bitmap) return NULL;
    gdip_create_hbitmap_from_bitmap(bitmap, &hbitmap, 0x00000000);
    gdip_dispose_image(bitmap);
    return hbitmap;
}

static HBITMAP icon_bitmap(IconId icon) {
    if (icon < 0 || icon >= ICON_COUNT) return NULL;
    if (!icon_load_attempted[icon]) {
        icon_bitmaps[icon] = load_png_icon(ICON_PATHS[icon]);
        icon_load_attempted[icon] = 1;
    }
    return icon_bitmaps[icon];
}

static void draw_icon(HDC hdc, IconId icon, RECT rect, COLORREF fallback) {
    HBITMAP bitmap = icon_bitmap(icon);
    BITMAP info;
    HDC memory_dc;
    HBITMAP old_bitmap;
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};

    if (!bitmap) {
        RECT fallback_rect = {rect.left + 4, rect.top + 4, rect.right - 4, rect.bottom - 4};
        fill_rect(hdc, fallback_rect, fallback);
        return;
    }

    GetObject(bitmap, sizeof(info), &info);
    memory_dc = CreateCompatibleDC(hdc);
    old_bitmap = SelectObject(memory_dc, bitmap);
    SetStretchBltMode(hdc, HALFTONE);
    AlphaBlend(hdc, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
               memory_dc, 0, 0, info.bmWidth, info.bmHeight, blend);
    SelectObject(memory_dc, old_bitmap);
    DeleteDC(memory_dc);
}

static void draw_icon_text_line(HDC hdc, int x, int y, IconId icon, const char *text, COLORREF color) {
    RECT icon_rect = {x, y, x + 18, y + 18};
    draw_icon(hdc, icon, icon_rect, RGB(90, 110, 130));
    draw_text_line(hdc, x + 26, y, text, color);
}

static void format_metric_value(int value, char *buffer, size_t buffer_size) {
    if (value >= 1000000) snprintf(buffer, buffer_size, "%dM", value / 1000000);
    else if (value >= 10000) snprintf(buffer, buffer_size, "%dK", value / 1000);
    else snprintf(buffer, buffer_size, "%d", value);
}

static void draw_metric_box(HDC hdc, RECT rect, IconId icon, const char *label, int value, COLORREF accent,
                            const char *tooltip_text, const char **active_tooltip) {
    char text[32];
    RECT accent_rect = rect;
    RECT icon_rect = rect;
    RECT label_rect = rect;
    RECT value_rect = rect;

    fill_rect(hdc, rect, RGB(39, 47, 56));
    accent_rect.right = accent_rect.left + 4;
    fill_rect(hdc, accent_rect, accent);
    icon_rect.left += 8;
    icon_rect.top += 3;
    icon_rect.right = icon_rect.left + 22;
    icon_rect.bottom -= 3;
    draw_icon(hdc, icon, icon_rect, accent);
    label_rect.left = rect.left + 36;
    label_rect.right = rect.right - 34;
    draw_center_text(hdc, label_rect, label, RGB(176, 187, 197));
    value_rect.left = rect.right - 34;
    format_metric_value(value, text, sizeof(text));
    draw_center_text(hdc, value_rect, text, RGB(232, 238, 242));
    if (point_in_rect_local(rect, hover_x, hover_y)) *active_tooltip = tooltip_text;
}

static void draw_tooltip(HDC hdc, RECT client, const char *tooltip_text) {
    RECT tip;
    SIZE text_size;
    int panel_left = client.right - side_panel_w;
    int width;
    int height = 26;

    if (!tooltip_text || hover_x < 0 || hover_y < 0) return;
    GetTextExtentPoint32A(hdc, tooltip_text, (int)strlen(tooltip_text), &text_size);
    width = clamp(text_size.cx + 24, 120, side_panel_w - FORM_X_PAD * 2);
    tip.left = hover_x + 14;
    tip.top = hover_y + 18;
    tip.right = tip.left + width;
    tip.bottom = tip.top + height;
    if (tip.right > client.right - FORM_X_PAD) {
        tip.right = client.right - FORM_X_PAD;
        tip.left = tip.right - width;
    }
    if (tip.left < panel_left + FORM_X_PAD) {
        tip.left = panel_left + FORM_X_PAD;
        tip.right = tip.left + width;
    }
    if (tip.bottom > client.bottom - 10) {
        tip.bottom = hover_y - 12;
        tip.top = tip.bottom - height;
    }
    if (tip.top < TOP_BAR_H + 8) {
        tip.top = TOP_BAR_H + 8;
        tip.bottom = tip.top + height;
    }
    fill_rect(hdc, tip, RGB(54, 62, 70));
    draw_center_text(hdc, tip, tooltip_text, RGB(236, 241, 245));
}

static int visible_tile_bounds(RECT client, MapLayout layout, int *min_x, int *max_x, int *min_y, int *max_y) {
    int view_left = client.left;
    int view_top = TOP_BAR_H;
    int view_right = client.right - side_panel_w;
    int view_bottom = client.bottom - BOTTOM_BAR_H;

    if (layout.draw_w <= 0 || layout.draw_h <= 0) return 0;
    if (view_right <= layout.map_x || view_bottom <= layout.map_y ||
        view_left >= layout.map_x + layout.draw_w || view_top >= layout.map_y + layout.draw_h) {
        return 0;
    }

    *min_x = clamp((view_left - layout.map_x) * MAP_W / layout.draw_w - 2, 0, MAP_W - 1);
    *max_x = clamp((view_right - layout.map_x) * MAP_W / layout.draw_w + 2, 0, MAP_W - 1);
    *min_y = clamp((view_top - layout.map_y) * MAP_H / layout.draw_h - 2, 0, MAP_H - 1);
    *max_y = clamp((view_bottom - layout.map_y) * MAP_H / layout.draw_h + 2, 0, MAP_H - 1);
    return *min_x <= *max_x && *min_y <= *max_y;
}

static COLORREF tile_display_color(int x, int y) {
    int owner = world[y][x].owner;
    COLORREF base;

    switch (display_mode) {
        case DISPLAY_CLIMATE:
            base = climate_color(world[y][x].climate);
            break;
        case DISPLAY_GEOGRAPHY:
            base = blend_color(geography_color(world[y][x].geography), overview_color(x, y), 35);
            break;
        case DISPLAY_POLITICAL:
            base = overview_color(x, y);
            if (owner >= 0 && owner < civ_count && civs[owner].alive) {
                base = blend_color(base, civs[owner].color, 74);
            }
            break;
        case DISPLAY_ALL:
            base = overview_color(x, y);
            if (owner >= 0 && owner < civ_count && civs[owner].alive) {
                base = blend_color(base, civs[owner].color, 38);
            }
            break;
        case DISPLAY_OVERVIEW:
        default:
            base = overview_color(x, y);
            break;
    }
    return base;
}

static int tile_left(MapLayout layout, int x) {
    return layout.map_x + x * layout.draw_w / MAP_W;
}

static int tile_right(MapLayout layout, int x) {
    return layout.map_x + (x + 1) * layout.draw_w / MAP_W;
}

static int tile_top(MapLayout layout, int y) {
    return layout.map_y + y * layout.draw_h / MAP_H;
}

static int tile_bottom(MapLayout layout, int y) {
    return layout.map_y + (y + 1) * layout.draw_h / MAP_H;
}

static void draw_crisp_map_surface(HDC hdc, MapLayout layout) {
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

static void draw_land_texture(HDC hdc, MapLayout layout, int x, int y) {
    int px = tile_left(layout, x);
    int py = tile_top(layout, y);
    int s = layout.tile_size;
    HBRUSH brush;
    COLORREF mark;

    if (!is_land(world[y][x].geography) || s < 12) return;
    if (display_mode == DISPLAY_POLITICAL) return;
    if (((x * 17 + y * 31) % 37) != 0) return;

    mark = world[y][x].climate == CLIMATE_FOREST ? RGB(35, 113, 38) :
           world[y][x].climate == CLIMATE_DESERT ? RGB(235, 163, 72) :
           world[y][x].climate == CLIMATE_SNOWFIELD ? RGB(245, 252, 255) :
           world[y][x].climate == CLIMATE_SWAMP ? RGB(61, 113, 76) :
           world[y][x].geography == GEO_MOUNTAIN ? RGB(84, 76, 68) :
           world[y][x].geography == GEO_HILL ? RGB(123, 113, 79) :
           RGB(122, 176, 79);
    brush = CreateSolidBrush(mark);
    SelectObject(hdc, brush);
    SelectObject(hdc, GetStockObject(NULL_PEN));
    Ellipse(hdc, px + s / 4, py + s / 5, px + s, py + s * 4 / 5);
    DeleteObject(brush);
}

static void draw_rivers(HDC hdc, RECT client, MapLayout layout) {
    int min_x;
    int max_x;
    int min_y;
    int max_y;
    int y;
    int x;
    HPEN river_pen = CreatePen(PS_SOLID, layout.tile_size >= 10 ? 2 : 1, RGB(57, 139, 218));
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
        }
    }
    SelectObject(hdc, old_pen);
    DeleteObject(river_pen);
}

static void draw_borders(HDC hdc, RECT client, MapLayout layout) {
    int min_x;
    int max_x;
    int min_y;
    int max_y;
    int x;
    int y;
    HPEN country_pen = CreatePen(PS_SOLID, layout.tile_size >= 8 ? 3 : 2, RGB(28, 30, 28));
    HPEN province_pen = CreatePen(PS_SOLID, 1, RGB(73, 78, 68));
    HPEN coast_pen = CreatePen(PS_SOLID, layout.tile_size >= 8 ? 3 : 2, RGB(36, 65, 57));
    HPEN old_pen = SelectObject(hdc, province_pen);

    if (!visible_tile_bounds(client, layout, &min_x, &max_x, &min_y, &max_y)) {
        SelectObject(hdc, old_pen);
        DeleteObject(country_pen);
        DeleteObject(province_pen);
        DeleteObject(coast_pen);
        return;
    }

    for (y = min_y; y <= max_y; y++) {
        for (x = min_x; x <= max_x; x++) {
            int owner = world[y][x].owner;
            int province = city_for_tile(x, y);
            int px = tile_left(layout, x);
            int py = tile_top(layout, y);
            int right = tile_right(layout, x);
            int bottom = tile_bottom(layout, y);

            if (owner < 0 || province < 0) continue;
            if (x < MAP_W - 1 && world[y][x + 1].owner == owner &&
                city_for_tile(x + 1, y) >= 0 && city_for_tile(x + 1, y) != province) {
                MoveToEx(hdc, right, py, NULL);
                LineTo(hdc, right, bottom);
            }
            if (y < MAP_H - 1 && world[y + 1][x].owner == owner &&
                city_for_tile(x, y + 1) >= 0 && city_for_tile(x, y + 1) != province) {
                MoveToEx(hdc, px, bottom, NULL);
                LineTo(hdc, right, bottom);
            }
        }
    }

    SelectObject(hdc, country_pen);
    for (y = min_y; y <= max_y; y++) {
        for (x = min_x; x <= max_x; x++) {
            int owner = world[y][x].owner;
            int px = tile_left(layout, x);
            int py = tile_top(layout, y);
            int right = tile_right(layout, x);
            int bottom = tile_bottom(layout, y);

            if (owner < 0) continue;
            if (x == 0 || world[y][x - 1].owner != owner) { MoveToEx(hdc, px, py, NULL); LineTo(hdc, px, bottom); }
            if (x == MAP_W - 1 || world[y][x + 1].owner != owner) { MoveToEx(hdc, right, py, NULL); LineTo(hdc, right, bottom); }
            if (y == 0 || world[y - 1][x].owner != owner) { MoveToEx(hdc, px, py, NULL); LineTo(hdc, right, py); }
            if (y == MAP_H - 1 || world[y + 1][x].owner != owner) { MoveToEx(hdc, px, bottom, NULL); LineTo(hdc, right, bottom); }
        }
    }

    SelectObject(hdc, coast_pen);
    for (y = min_y; y <= max_y; y++) {
        for (x = min_x; x <= max_x; x++) {
            int px = tile_left(layout, x);
            int py = tile_top(layout, y);
            int right = tile_right(layout, x);
            int bottom = tile_bottom(layout, y);

            if (!is_land(world[y][x].geography)) continue;
            if (x == 0 || !is_land(world[y][x - 1].geography)) { MoveToEx(hdc, px, py, NULL); LineTo(hdc, px, bottom); }
            if (x == MAP_W - 1 || !is_land(world[y][x + 1].geography)) { MoveToEx(hdc, right, py, NULL); LineTo(hdc, right, bottom); }
            if (y == 0 || !is_land(world[y - 1][x].geography)) { MoveToEx(hdc, px, py, NULL); LineTo(hdc, right, py); }
            if (y == MAP_H - 1 || !is_land(world[y + 1][x].geography)) { MoveToEx(hdc, px, bottom, NULL); LineTo(hdc, right, bottom); }
        }
    }

    SelectObject(hdc, old_pen);
    DeleteObject(country_pen);
    DeleteObject(province_pen);
    DeleteObject(coast_pen);
}

static void draw_city_icon(HDC hdc, int cx, int cy, int size, int capital) {
    POINT roof[3];
    HBRUSH white_brush = CreateSolidBrush(capital ? RGB(255, 248, 210) : RGB(245, 245, 235));
    HBRUSH black_brush = CreateSolidBrush(RGB(20, 20, 20));
    HPEN black_pen = CreatePen(PS_SOLID, 2, RGB(20, 20, 20));
    HPEN old_pen = SelectObject(hdc, black_pen);
    HBRUSH old_brush = SelectObject(hdc, white_brush);

    Rectangle(hdc, cx - size / 2, cy, cx + size / 2, cy + size / 2);
    roof[0].x = cx - size / 2 - 2; roof[0].y = cy;
    roof[1].x = cx; roof[1].y = cy - size / 2;
    roof[2].x = cx + size / 2 + 2; roof[2].y = cy;
    SelectObject(hdc, black_brush);
    Polygon(hdc, roof, 3);
    SelectObject(hdc, white_brush);
    Rectangle(hdc, cx - size / 10, cy + size / 5, cx + size / 8, cy + size / 2);

    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    DeleteObject(white_brush);
    DeleteObject(black_brush);
    DeleteObject(black_pen);
}

static void draw_cities(HDC hdc, MapLayout layout) {
    int i;
    int s = layout.tile_size;

    for (i = 0; i < city_count; i++) {
        City *city = &cities[i];
        if (!city->alive || city->owner < 0 || city->owner >= civ_count || !civs[city->owner].alive) continue;
        draw_city_icon(hdc,
                       (tile_left(layout, city->x) + tile_right(layout, city->x)) / 2,
                       (tile_top(layout, city->y) + tile_bottom(layout, city->y)) / 2,
                       clamp(s + (city->capital ? 7 : 4), 8, 18),
                       city->capital);
    }
}

static void draw_selected_tile(HDC hdc, MapLayout layout) {
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

static void draw_mode_buttons(HDC hdc, RECT client) {
    int i;
    for (i = 0; i < 4; i++) {
        RECT button = get_mode_button_rect(client, i);
        int mode = MAP_DISPLAY_MODES[i];
        fill_rect(hdc, button, mode == display_mode ? RGB(49, 63, 76) : RGB(36, 46, 56));
        draw_center_text(hdc, button, MAP_DISPLAY_NAMES[i], RGB(238, 243, 247));
    }
}

static void draw_top_bar(HDC hdc, RECT client) {
    RECT bar = {0, 0, client.right, TOP_BAR_H};
    RECT year_box = {client.right / 2 - 112, 9, client.right / 2 + 112, 50};
    char text[80];
    HFONT title_font = CreateFontA(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET,
                                   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                   DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    HFONT old_font;

    fill_rect(hdc, bar, RGB(52, 153, 216));
    fill_rect(hdc, year_box, RGB(42, 42, 42));
    snprintf(text, sizeof(text), "Year %d  Month %d", year, month);
    old_font = SelectObject(hdc, title_font);
    draw_center_text(hdc, year_box, text, RGB(199, 230, 107));
    SelectObject(hdc, old_font);
    DeleteObject(title_font);
    draw_text_line(hdc, 18, 20, WORLD_SIM_VERSION_LABEL, RGB(245, 250, 255));
}

static int selected_tile_owner(void) {
    if (selected_x < 0 || selected_y < 0) return -1;
    return world[selected_y][selected_x].owner;
}

static const char *capital_name_for_civ(int civ_id) {
    int city_id;

    if (civ_id < 0 || civ_id >= civ_count) return "None";
    city_id = civs[civ_id].capital_city;
    if (city_id < 0 || city_id >= city_count || !cities[city_id].alive) return "None";
    return cities[city_id].name;
}

static void draw_panel_tabs(HDC hdc, RECT client) {
    const char *names[3] = {"Info", "Civ", "Map"};
    int i;

    for (i = 0; i < 3; i++) {
        RECT tab = get_panel_tab_rect(client, i);
        fill_rect(hdc, tab, i == panel_tab ? RGB(56, 68, 80) : RGB(39, 47, 56));
        draw_center_text(hdc, tab, names[i], RGB(238, 243, 247));
    }
}

static RECT setup_slider_rect(RECT client, int index) {
    RECT rect;
    int x = client.right - side_panel_w + FORM_X_PAD;
    rect.left = x + 90;
    rect.right = client.right - FORM_X_PAD;
    rect.top = TOP_BAR_H + 315 + index * 48;
    rect.bottom = rect.top + 8;
    return rect;
}

static void draw_setup_slider(HDC hdc, RECT client, int index, const char *name, int value) {
    RECT track = setup_slider_rect(client, index);
    int label_x = client.right - side_panel_w + FORM_X_PAD;
    int knob_x = track.left + (track.right - track.left) * value / 100;
    RECT knob = {knob_x - 6, track.top - 7, knob_x + 6, track.top + 15};
    char text[80];

    snprintf(text, sizeof(text), "%s %d", name, value);
    draw_text_line(hdc, label_x, track.top - 8, text, RGB(205, 214, 222));
    fill_rect(hdc, track, RGB(92, 104, 114));
    fill_rect(hdc, knob, RGB(232, 238, 244));
}

static void draw_info_tab(HDC hdc, RECT client, int x, int y, HFONT title_font, HFONT body_font) {
    char text[180];
    int owner = selected_tile_owner();
    int civ_id = selected_civ;
    int city_total = 0;
    int inner_w = side_panel_w - FORM_X_PAD * 2;
    int quad_w = (inner_w - 24) / 4;
    int i;
    const char *tooltip_text = NULL;

    SelectObject(hdc, title_font);
    draw_text_line(hdc, x, y, "Selected Country", RGB(245, 245, 245));
    y += 28;
    SelectObject(hdc, body_font);
    if (owner >= 0 && owner < civ_count) civ_id = owner;
    if (civ_id >= 0 && civ_id < civ_count) {
        RECT swatch = {x, y + 3, x + 18, y + 21};
        for (i = 0; i < city_count; i++) {
            if (cities[i].alive && cities[i].owner == civ_id) city_total++;
        }
        fill_rect(hdc, swatch, civs[civ_id].color);
        snprintf(text, sizeof(text), "%c  %s", civs[civ_id].symbol, civs[civ_id].name);
        draw_text_line(hdc, x + 28, y, text, RGB(245, 245, 245));
        y += 22;
        snprintf(text, sizeof(text), "Capital: %s", capital_name_for_civ(civ_id));
        draw_text_line(hdc, x + 28, y, text, RGB(180, 190, 198));
        y += 28;
        {
            RECT m = {x, y, x + quad_w, y + 28};
            draw_metric_box(hdc, m, ICON_POPULATION, "POP", civs[civ_id].population, RGB(74, 112, 160), "Total country population", &tooltip_text);
            m.left += quad_w + 8; m.right += quad_w + 8;
            draw_metric_box(hdc, m, ICON_TERRITORY, "LAND", civs[civ_id].territory, RGB(88, 137, 83), "Total owned land tiles", &tooltip_text);
            m.left += quad_w + 8; m.right += quad_w + 8;
            draw_metric_box(hdc, m, ICON_CITY_CAPITAL, "CITY", city_total, RGB(154, 128, 74), "Total cities", &tooltip_text);
            m.left += quad_w + 8; m.right += quad_w + 8;
            draw_metric_box(hdc, m, ICON_DISORDER, "DIS", civs[civ_id].disorder, RGB(170, 73, 73), "Disorder: resource stress, war losses, migration pressure", &tooltip_text);
            y += 34;
            m.left = x; m.right = x + quad_w;
            draw_metric_box(hdc, m, ICON_BATTLE, "BAT", civs[civ_id].aggression, RGB(158, 74, 62), "Aggression", &tooltip_text);
            m.left += quad_w + 8; m.right += quad_w + 8;
            draw_metric_box(hdc, m, ICON_EXPANSION, "EXP", civs[civ_id].expansion, RGB(82, 133, 87), "Expansion", &tooltip_text);
            m.left += quad_w + 8; m.right += quad_w + 8;
            draw_metric_box(hdc, m, ICON_COUNTRY_DEFENSE, "DEF", civs[civ_id].defense, RGB(80, 101, 148), "Defense", &tooltip_text);
            m.left += quad_w + 8; m.right += quad_w + 8;
            draw_metric_box(hdc, m, ICON_CULTURE, "CUL", civs[civ_id].culture, RGB(147, 105, 167), "Culture cohesion", &tooltip_text);
            y += 38;
        }
    } else {
        draw_text_line(hdc, x, y, "Select a country or one of its tiles.", RGB(180, 190, 198));
        y += 42;
    }

    SelectObject(hdc, title_font);
    draw_text_line(hdc, x, y, "Selected Tile", RGB(245, 245, 245));
    y += 28;
    SelectObject(hdc, body_font);
    if (selected_x >= 0 && selected_y >= 0) {
        TerrainStats stats = tile_stats(selected_x, selected_y);
        int city_id = city_at(selected_x, selected_y);
        int region_id = city_for_tile(selected_x, selected_y);

        snprintf(text, sizeof(text), "Position: %d, %d", selected_x, selected_y);
        draw_text_line(hdc, x, y, text, RGB(220, 225, 230)); y += 20;
        snprintf(text, sizeof(text), "Geography: %s%s", geography_name(world[selected_y][selected_x].geography),
                 world[selected_y][selected_x].river ? " + River" : "");
        draw_icon_text_line(hdc, x, y, ICON_GEOGRAPHY, text, RGB(220, 225, 230)); y += 22;
        snprintf(text, sizeof(text), "Climate: %s", climate_name(world[selected_y][selected_x].climate));
        draw_icon_text_line(hdc, x, y, ICON_CLIMATE, text, RGB(220, 225, 230)); y += 22;
        snprintf(text, sizeof(text), "Elev %d Moist %d Temp %d",
                 world[selected_y][selected_x].elevation,
                 world[selected_y][selected_x].moisture,
                 world[selected_y][selected_x].temperature);
        draw_text_line(hdc, x, y, text, RGB(180, 190, 198)); y += 20;
        {
            RECT m = {x, y, x + quad_w, y + 26};
            draw_metric_box(hdc, m, ICON_FOOD, "FOOD", stats.food, RGB(124, 154, 70), "Tile food", &tooltip_text);
            m.left += quad_w + 8; m.right += quad_w + 8;
            draw_metric_box(hdc, m, ICON_LIVESTOCK, "HERD", stats.livestock, RGB(126, 104, 70), "Tile livestock", &tooltip_text);
            m.left += quad_w + 8; m.right += quad_w + 8;
            draw_metric_box(hdc, m, ICON_WOOD, "WOOD", stats.wood, RGB(67, 128, 76), "Tile wood", &tooltip_text);
            m.left += quad_w + 8; m.right += quad_w + 8;
            draw_metric_box(hdc, m, ICON_ORE, "ORE", stats.minerals, RGB(130, 120, 104), "Tile ore", &tooltip_text);
            y += 30;
            m.left = x; m.right = x + quad_w;
            draw_metric_box(hdc, m, ICON_WATER, "WATR", stats.water, RGB(65, 126, 174), "Tile water", &tooltip_text);
            m.left += quad_w + 8; m.right += quad_w + 8;
            draw_metric_box(hdc, m, ICON_HABITABILITY, "LIVE", stats.habitability, RGB(116, 145, 94), "Tile habitability", &tooltip_text);
            m.left += quad_w + 8; m.right += quad_w + 8;
            draw_metric_box(hdc, m, ICON_ATTACK, "ATK", stats.attack, RGB(158, 74, 62), "Tile attack modifier", &tooltip_text);
            m.left += quad_w + 8; m.right += quad_w + 8;
            draw_metric_box(hdc, m, ICON_TILE_DEFENSE, "DEF", stats.defense, RGB(80, 101, 148), "Tile defense modifier", &tooltip_text);
            y += 34;
        }
        if (city_id >= 0) {
            snprintf(text, sizeof(text), "City: %s  Pop %d", cities[city_id].name, cities[city_id].population);
            draw_text_line(hdc, x, y, text, RGB(220, 225, 230)); y += 20;
            if (cities[city_id].owner >= 0 && cities[city_id].owner < civ_count) {
                snprintf(text, sizeof(text), "Country: %s%s", civs[cities[city_id].owner].name,
                         cities[city_id].capital ? "  Capital" : "");
                draw_text_line(hdc, x, y, text, RGB(180, 190, 198)); y += 20;
            }
        }
        if (region_id >= 0) {
            RegionSummary summary = summarize_city_region(region_id);
            snprintf(text, sizeof(text), "Province: %s", cities[region_id].name);
            draw_text_line(hdc, x, y, text, RGB(220, 225, 230)); y += 20;
            if (cities[region_id].owner >= 0 && cities[region_id].owner < civ_count) {
                snprintf(text, sizeof(text), "Country: %s", civs[cities[region_id].owner].name);
                draw_text_line(hdc, x, y, text, RGB(180, 190, 198)); y += 20;
                snprintf(text, sizeof(text), "Capital: %s", capital_name_for_civ(cities[region_id].owner));
                draw_text_line(hdc, x, y, text, RGB(180, 190, 198)); y += 20;
            }
            snprintf(text, sizeof(text), "Tiles %d  Pop %d", summary.tiles, summary.population);
            draw_text_line(hdc, x, y, text, RGB(180, 190, 198)); y += 22;
            {
                RECT m = {x, y, x + quad_w, y + 26};
                draw_metric_box(hdc, m, ICON_FOOD, "FOOD", summary.food, RGB(124, 154, 70), "Province average food", &tooltip_text);
                m.left += quad_w + 8; m.right += quad_w + 8;
                draw_metric_box(hdc, m, ICON_LIVESTOCK, "HERD", summary.livestock, RGB(126, 104, 70), "Province average livestock", &tooltip_text);
                m.left += quad_w + 8; m.right += quad_w + 8;
                draw_metric_box(hdc, m, ICON_WOOD, "WOOD", summary.wood, RGB(67, 128, 76), "Province average wood", &tooltip_text);
                m.left += quad_w + 8; m.right += quad_w + 8;
                draw_metric_box(hdc, m, ICON_ORE, "ORE", summary.minerals, RGB(130, 120, 104), "Province average ore", &tooltip_text);
                y += 30;
                m.left = x; m.right = x + quad_w;
                draw_metric_box(hdc, m, ICON_WATER, "WATR", summary.water, RGB(65, 126, 174), "Province average water", &tooltip_text);
                m.left += quad_w + 8; m.right += quad_w + 8;
                draw_metric_box(hdc, m, ICON_HABITABILITY, "LIVE", summary.habitability, RGB(116, 145, 94), "Province average habitability", &tooltip_text);
                m.left += quad_w + 8; m.right += quad_w + 8;
                draw_metric_box(hdc, m, ICON_ATTACK, "ATK", summary.attack, RGB(158, 74, 62), "Province average attack modifier", &tooltip_text);
                m.left += quad_w + 8; m.right += quad_w + 8;
                draw_metric_box(hdc, m, ICON_TILE_DEFENSE, "DEF", summary.defense, RGB(80, 101, 148), "Province average defense modifier", &tooltip_text);
            }
        }
    } else {
        draw_text_line(hdc, x, y, "Click a tile on the map.", RGB(180, 190, 198));
    }
    draw_tooltip(hdc, client, tooltip_text);
}

static void draw_civ_tab(HDC hdc, RECT client, int x, HFONT title_font, HFONT body_font) {
    int y = TOP_BAR_H + 58;
    int i;
    char text[180];

    SelectObject(hdc, title_font);
    draw_text_line(hdc, x, y, "Civilizations", RGB(245, 245, 245));
    y += 30;
    SelectObject(hdc, body_font);
    for (i = 0; i < civ_count && y < client.bottom - 320; i++) {
        RECT swatch = {x, y + 3, x + 15, y + 18};
        fill_rect(hdc, swatch, civs[i].color);
        snprintf(text, sizeof(text), "%c  %.63s", civs[i].symbol, civs[i].name);
        draw_text_line(hdc, x + 24, y, text, i == selected_civ ? RGB(255, 238, 150) : RGB(238, 238, 238));
        y += 19;
        snprintf(text, sizeof(text), "Pop %d  Land %d  A%d E%d D%d C%d",
                 civs[i].population, civs[i].territory, civs[i].aggression,
                 civs[i].expansion, civs[i].defense, civs[i].culture);
        draw_text_line(hdc, x + 24, y, text, RGB(175, 186, 196));
        y += 25;
    }

    SelectObject(hdc, title_font);
    draw_text_line(hdc, x, client.bottom - 262, "Civilization Form", RGB(245, 245, 245));
    SelectObject(hdc, body_font);
    draw_text_line(hdc, x, client.bottom - 235, "Name", RGB(200, 210, 218));
    draw_text_line(hdc, x + 176, client.bottom - 235, "Symbol", RGB(200, 210, 218));
    draw_text_line(hdc, x, client.bottom - 175, "Aggression", RGB(200, 210, 218));
    draw_text_line(hdc, x + 82, client.bottom - 175, "Expansion", RGB(200, 210, 218));
    draw_text_line(hdc, x + 164, client.bottom - 175, "Defense", RGB(200, 210, 218));
    draw_text_line(hdc, x + 246, client.bottom - 175, "Culture", RGB(200, 210, 218));
    draw_text_line(hdc, x, client.bottom - 64, "F1 add. F2 apply. Select empty land before adding.", RGB(160, 171, 180));
}

static void draw_map_tab(HDC hdc, RECT client, int x, HFONT title_font, HFONT body_font) {
    int y = TOP_BAR_H + 58;

    SelectObject(hdc, title_font);
    draw_text_line(hdc, x, y, "Map View", RGB(245, 245, 245));
    SelectObject(hdc, body_font);
    draw_mode_buttons(hdc, client);

    y = TOP_BAR_H + 230;
    SelectObject(hdc, title_font);
    draw_text_line(hdc, x, y, "World Generation", RGB(245, 245, 245));
    SelectObject(hdc, body_font);
    draw_text_line(hdc, x, y + 30, "Initial civilizations", RGB(200, 210, 218));
    draw_setup_slider(hdc, client, 0, "Ocean", ocean_slider);
    draw_setup_slider(hdc, client, 1, "Mountains", mountain_slider);
    draw_setup_slider(hdc, client, 2, "Desert", desert_slider);
    draw_setup_slider(hdc, client, 3, "Forest", forest_slider);
    draw_setup_slider(hdc, client, 4, "Wetland", wetland_slider);
    draw_text_line(hdc, x, client.bottom - 64, "F5 rebuilds with these settings.", RGB(160, 171, 180));
}

static void draw_side_panel(HDC hdc, RECT client) {
    int x = client.right - side_panel_w + 18;
    RECT panel = {client.right - side_panel_w, TOP_BAR_H, client.right, client.bottom};
    RECT divider = {client.right - side_panel_w - 3, TOP_BAR_H, client.right - side_panel_w + 3, client.bottom};
    HFONT title_font = CreateFontA(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET,
                                   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                   DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    HFONT body_font = CreateFontA(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
                                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                  DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    HFONT old_font;

    fill_rect(hdc, panel, RGB(31, 37, 43));
    fill_rect(hdc, divider, RGB(71, 82, 92));
    draw_panel_tabs(hdc, client);

    old_font = SelectObject(hdc, title_font);
    if (panel_tab == PANEL_INFO) draw_info_tab(hdc, client, x, TOP_BAR_H + 58, title_font, body_font);
    else if (panel_tab == PANEL_CIV) draw_civ_tab(hdc, client, x, title_font, body_font);
    else draw_map_tab(hdc, client, x, title_font, body_font);

    SelectObject(hdc, old_font);
    DeleteObject(title_font);
    DeleteObject(body_font);
}

static void draw_bottom_bar(HDC hdc, RECT client) {
    RECT bar = {0, client.bottom - BOTTOM_BAR_H, client.right - side_panel_w, client.bottom};
    RECT play = get_play_button_rect(client);
    char text[256];
    int i;

    fill_rect(hdc, bar, RGB(30, 34, 38));
    fill_rect(hdc, play, auto_run ? RGB(68, 88, 104) : RGB(42, 58, 72));
    draw_center_text(hdc, play, auto_run ? "||" : ">", RGB(245, 248, 250));

    for (i = 0; i < 3; i++) {
        RECT button = get_speed_button_rect(client, i);
        fill_rect(hdc, button, i == speed_index ? RGB(84, 110, 132) : RGB(42, 58, 72));
        draw_center_text(hdc, button, i == 0 ? ">" : (i == 1 ? ">>" : ">>>"), RGB(235, 240, 244));
    }

    snprintf(text, sizeof(text), "Space toggles run/pause    Click speed: %s (%s/month)    Wheel zoom    F1 add    F2 apply    F5 new world",
             SPEED_NAMES[speed_index], speed_seconds_text(speed_index));
    draw_text_line(hdc, 216, client.bottom - 29, text, RGB(225, 230, 235));
}

static void render_world(HDC hdc, RECT client) {
    int min_x;
    int max_x;
    int min_y;
    int max_y;
    int y;
    int x;
    MapLayout layout = get_map_layout(client);

    fill_rect(hdc, client, RGB(79, 160, 215));
    draw_top_bar(hdc, client);
    draw_crisp_map_surface(hdc, layout);
    if (layout.tile_size >= 12 && visible_tile_bounds(client, layout, &min_x, &max_x, &min_y, &max_y)) {
        for (y = min_y; y <= max_y; y++) {
            for (x = min_x; x <= max_x; x++) draw_land_texture(hdc, layout, x, y);
        }
    }
    if (layout.tile_size >= 2) {
        draw_rivers(hdc, client, layout);
        draw_borders(hdc, client, layout);
    }
    draw_cities(hdc, layout);
    draw_selected_tile(hdc, layout);
    draw_bottom_bar(hdc, client);
    draw_side_panel(hdc, client);
}

void paint_window(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT client;
    HDC mem_dc;
    HBITMAP bitmap;
    HBITMAP old_bitmap;

    GetClientRect(hwnd, &client);
    mem_dc = CreateCompatibleDC(hdc);
    bitmap = CreateCompatibleBitmap(hdc, client.right - client.left, client.bottom - client.top);
    old_bitmap = SelectObject(mem_dc, bitmap);
    render_world(mem_dc, client);
    BitBlt(hdc, 0, 0, client.right - client.left, client.bottom - client.top, mem_dc, 0, 0, SRCCOPY);
    SelectObject(mem_dc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(mem_dc);
    EndPaint(hwnd, &ps);
}
