#include "render.h"

#include "core/version.h"
#include "data/game_tables.h"
#include "sim/diplomacy.h"
#include "sim/war.h"
#include "world/world.h"

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
    ICON_STONE,
    ICON_CITY_CAPITAL,
    ICON_DISORDER,
    ICON_TERRITORY,
    ICON_MIGRATION,
    ICON_ECONOMY,
    ICON_PRODUCTION,
    ICON_INNOVATION,
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
typedef int (WINAPI *GdipCreateFromHDCProc)(HDC, void **);
typedef int (WINAPI *GdipDrawImageRectIProc)(void *, void *, int, int, int, int);
typedef int (WINAPI *GdipDeleteGraphicsProc)(void *);
typedef int (WINAPI *GdipDisposeImageProc)(void *);

static const char *ICON_PATHS[ICON_COUNT] = {
    "assets\\icons\\resource_ore.png",
    "assets\\icons\\metric_logistics.png",
    "assets\\icons\\resource_population.png",
    "assets\\icons\\metric_governance.png",
    "assets\\icons\\metric_cohesion.png",
    "assets\\icons\\resource_water.png",
    "assets\\icons\\metric_adaptation.png",
    "assets\\icons\\resource_water.png",
    "assets\\icons\\resource_population.png",
    "assets\\icons\\metric_governance.png",
    "assets\\icons\\resource_ore.png",
    "assets\\icons\\resource_food.png",
    "assets\\icons\\resource_livestock.png",
    "assets\\icons\\resource_wood.png",
    "assets\\icons\\resource_ore.png",
    "assets\\icons\\resource_stone.png",
    "assets\\icons\\metric_governance.png",
    "assets\\icons\\metric_adaptation.png",
    "assets\\icons\\metric_governance.png",
    "assets\\icons\\metric_logistics.png",
    "assets\\icons\\metric_economy.png",
    "assets\\icons\\metric_production.png",
    "assets\\icons\\metric_innovation.png"
};

static HMODULE gdiplus_module;
static ULONG_PTR gdiplus_token;
static GdiplusStartupProc gdiplus_startup;
static GdipCreateBitmapFromFileProc gdip_create_bitmap_from_file;
static GdipCreateFromHDCProc gdip_create_from_hdc;
static GdipDrawImageRectIProc gdip_draw_image_rect_i;
static GdipDeleteGraphicsProc gdip_delete_graphics;
static GdipDisposeImageProc gdip_dispose_image;
static void *icon_images[ICON_COUNT];
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

static void fill_rect_alpha(HDC hdc, RECT rect, COLORREF color, BYTE alpha) {
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    HDC memory_dc;
    BITMAPINFO info;
    void *bits = NULL;
    HBITMAP bitmap;
    HBITMAP old_bitmap;
    unsigned int pixel;
    unsigned int *pixels;
    int count;
    int i;
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, alpha, 0};

    if (width <= 0 || height <= 0) return;
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(hdc, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bitmap || !bits) {
        if (bitmap) DeleteObject(bitmap);
        fill_rect(hdc, rect, color);
        return;
    }

    pixel = (unsigned int)GetRValue(color) | ((unsigned int)GetGValue(color) << 8) |
            ((unsigned int)GetBValue(color) << 16);
    pixels = (unsigned int *)bits;
    count = width * height;
    for (i = 0; i < count; i++) pixels[i] = pixel;

    memory_dc = CreateCompatibleDC(hdc);
    old_bitmap = SelectObject(memory_dc, bitmap);
    AlphaBlend(hdc, rect.left, rect.top, width, height, memory_dc, 0, 0, width, height, blend);
    SelectObject(memory_dc, old_bitmap);
    DeleteDC(memory_dc);
    DeleteObject(bitmap);
}

static void draw_text_line(HDC hdc, int x, int y, const char *text, COLORREF color) {
    WCHAR wide_text[512];
    int len;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    len = MultiByteToWideChar(CP_UTF8, 0, text, -1, wide_text, (int)(sizeof(wide_text) / sizeof(wide_text[0])));
    if (len > 0) TextOutW(hdc, x, y, wide_text, len - 1);
}

static void draw_center_text(HDC hdc, RECT rect, const char *text, COLORREF color) {
    WCHAR wide_text[512];
    int len;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    len = MultiByteToWideChar(CP_UTF8, 0, text, -1, wide_text, (int)(sizeof(wide_text) / sizeof(wide_text[0])));
    if (len > 0) DrawTextW(hdc, wide_text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static const char *tr(const char *en, const char *zh) {
    return ui_language == UI_LANG_ZH ? zh : en;
}

static int point_in_rect_local(RECT rect, int x, int y) {
    return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
}

static int ensure_gdiplus(void) {
    GdiplusStartupInputLocal input;
    union { FARPROC raw; GdiplusStartupProc typed; } startup_proc;
    union { FARPROC raw; GdipCreateBitmapFromFileProc typed; } create_bitmap_proc;
    union { FARPROC raw; GdipCreateFromHDCProc typed; } create_graphics_proc;
    union { FARPROC raw; GdipDrawImageRectIProc typed; } draw_image_proc;
    union { FARPROC raw; GdipDeleteGraphicsProc typed; } delete_graphics_proc;
    union { FARPROC raw; GdipDisposeImageProc typed; } dispose_proc;

    if (gdiplus_token) return 1;
    if (!gdiplus_module) {
        gdiplus_module = LoadLibraryA("gdiplus.dll");
        if (!gdiplus_module) return 0;
        startup_proc.raw = GetProcAddress(gdiplus_module, "GdiplusStartup");
        create_bitmap_proc.raw = GetProcAddress(gdiplus_module, "GdipCreateBitmapFromFile");
        create_graphics_proc.raw = GetProcAddress(gdiplus_module, "GdipCreateFromHDC");
        draw_image_proc.raw = GetProcAddress(gdiplus_module, "GdipDrawImageRectI");
        delete_graphics_proc.raw = GetProcAddress(gdiplus_module, "GdipDeleteGraphics");
        dispose_proc.raw = GetProcAddress(gdiplus_module, "GdipDisposeImage");
        gdiplus_startup = startup_proc.typed;
        gdip_create_bitmap_from_file = create_bitmap_proc.typed;
        gdip_create_from_hdc = create_graphics_proc.typed;
        gdip_draw_image_rect_i = draw_image_proc.typed;
        gdip_delete_graphics = delete_graphics_proc.typed;
        gdip_dispose_image = dispose_proc.typed;
    }
    if (!gdiplus_startup || !gdip_create_bitmap_from_file || !gdip_create_from_hdc ||
        !gdip_draw_image_rect_i || !gdip_delete_graphics || !gdip_dispose_image) {
        return 0;
    }
    memset(&input, 0, sizeof(input));
    input.GdiplusVersion = 1;
    return gdiplus_startup(&gdiplus_token, &input, NULL) == 0;
}

static void *load_png_icon(const char *path) {
    WCHAR wide_path[MAX_PATH];
    void *image = NULL;

    if (!ensure_gdiplus()) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wide_path, MAX_PATH);
    if (gdip_create_bitmap_from_file(wide_path, &image) != 0 || !image) return NULL;
    return image;
}

static void *icon_image(IconId icon) {
    if (icon < 0 || icon >= ICON_COUNT) return NULL;
    if (!icon_load_attempted[icon]) {
        icon_images[icon] = load_png_icon(ICON_PATHS[icon]);
        icon_load_attempted[icon] = 1;
    }
    return icon_images[icon];
}

static void draw_icon(HDC hdc, IconId icon, RECT rect, COLORREF fallback) {
    void *image = icon_image(icon);
    void *graphics = NULL;

    if (!image || !gdip_create_from_hdc || gdip_create_from_hdc(hdc, &graphics) != 0 || !graphics) {
        RECT fallback_rect = {rect.left + 4, rect.top + 4, rect.right - 4, rect.bottom - 4};
        fill_rect(hdc, fallback_rect, fallback);
        return;
    }

    gdip_draw_image_rect_i(graphics, image, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
    gdip_delete_graphics(graphics);
}

static void draw_icon_text_line(HDC hdc, int x, int y, IconId icon, const char *text, COLORREF color) {
    RECT icon_rect = {x, y, x + 18, y + 18};
    draw_icon(hdc, icon, icon_rect, RGB(90, 110, 130));
    draw_text_line(hdc, x + 26, y, text, color);
}

static void format_metric_value(int value, char *buffer, size_t buffer_size) {
    if (value >= 1000000000) snprintf(buffer, buffer_size, "%.1fB", value / 1000000000.0);
    else if (value >= 1000000) snprintf(buffer, buffer_size, "%.1fM", value / 1000000.0);
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
    label_rect.right = rect.right - 48;
    draw_center_text(hdc, label_rect, label, RGB(176, 187, 197));
    value_rect.left = rect.right - 48;
    format_metric_value(value, text, sizeof(text));
    draw_center_text(hdc, value_rect, text, RGB(232, 238, 242));
    if (point_in_rect_local(rect, hover_x, hover_y)) *active_tooltip = tooltip_text;
}

static RECT metric_grid_rect(int x, int y, int box_w, int box_h, int index) {
    RECT rect;
    int col = index % 4;
    int row = index / 4;

    rect.left = x + col * (box_w + 8);
    rect.top = y + row * (box_h + 6);
    rect.right = rect.left + box_w;
    rect.bottom = rect.top + box_h;
    return rect;
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

static void draw_rivers(HDC hdc, RECT client, MapLayout layout) {
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

static void draw_borders(HDC hdc, RECT client, MapLayout layout) {
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

static void draw_city_icon(HDC hdc, int cx, int cy, int size, int capital, int port) {
    POINT roof[3];
    HBRUSH white_brush = CreateSolidBrush(capital ? RGB(255, 248, 210) : RGB(245, 245, 235));
    HBRUSH black_brush = CreateSolidBrush(RGB(20, 20, 20));
    HBRUSH port_brush = CreateSolidBrush(RGB(36, 135, 205));
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
    if (port) {
        RECT marker = {cx + size / 3, cy + size / 4, cx + size / 2 + 4, cy + size / 2 + 4};
        SelectObject(hdc, port_brush);
        Rectangle(hdc, marker.left, marker.top, marker.right, marker.bottom);
    }

    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    DeleteObject(white_brush);
    DeleteObject(black_brush);
    DeleteObject(port_brush);
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
                       city->capital,
                       city->port);
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
    const char *names_en[4] = {"All", "Climate", "Geography", "Political"};
    const char *names_zh[4] = {"全部", "气候", "地理", "政治"};
    int i;
    for (i = 0; i < 4; i++) {
        RECT button = get_mode_button_rect(client, i);
        int mode = MAP_DISPLAY_MODES[i];
        fill_rect(hdc, button, mode == display_mode ? RGB(49, 63, 76) : RGB(36, 46, 56));
        draw_center_text(hdc, button, tr(names_en[i], names_zh[i]), RGB(238, 243, 247));
    }
}

static void draw_top_bar(HDC hdc, RECT client) {
    RECT bar = {0, 0, client.right, TOP_BAR_H};
    RECT year_box = {client.right / 2 - 112, 9, client.right / 2 + 112, 50};
    RECT language_button = get_language_button_rect(client);
    char text[80];
    HFONT title_font = CreateFontA(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET,
                                   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                   DEFAULT_PITCH | FF_SWISS, "Microsoft YaHei UI");
    HFONT old_font;

    fill_rect(hdc, bar, RGB(52, 153, 216));
    fill_rect(hdc, year_box, RGB(42, 42, 42));
    snprintf(text, sizeof(text), "%s %d  %s %d", tr("Year", "年"), year, tr("Month", "月"), month);
    old_font = SelectObject(hdc, title_font);
    draw_center_text(hdc, year_box, text, RGB(199, 230, 107));
    SelectObject(hdc, old_font);
    DeleteObject(title_font);
    draw_text_line(hdc, 18, 20, WORLD_SIM_VERSION_LABEL, RGB(245, 250, 255));
    fill_rect(hdc, language_button, RGB(39, 81, 111));
    draw_center_text(hdc, language_button, ui_language == UI_LANG_ZH ? "中文" : "EN", RGB(245, 250, 255));
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
    const char *names_en[3] = {"Info", "Civ", "Map"};
    const char *names_zh[3] = {"信息", "文明", "地图"};
    int i;

    for (i = 0; i < 3; i++) {
        RECT tab = get_panel_tab_rect(client, i);
        fill_rect(hdc, tab, i == panel_tab ? RGB(56, 68, 80) : RGB(39, 47, 56));
        draw_center_text(hdc, tab, tr(names_en[i], names_zh[i]), RGB(238, 243, 247));
    }
}

static RECT setup_slider_rect(RECT client, int index) {
    RECT rect;
    int x = client.right - side_panel_w + FORM_X_PAD;
    int section_gap = index >= WORLD_SLIDER_BIAS_FOREST ? 34 : 0;
    rect.left = x + 158;
    rect.right = client.right - FORM_X_PAD - 8;
    rect.top = TOP_BAR_H + 292 + index * 34 + section_gap;
    rect.bottom = rect.top + 10;
    return rect;
}

static void draw_setup_slider(HDC hdc, RECT client, int index, const char *name, int value) {
    RECT track = setup_slider_rect(client, index);
    RECT fill = track;
    int label_x = client.right - side_panel_w + FORM_X_PAD;
    int knob_x = track.left + (track.right - track.left) * value / 100;
    RECT knob = {knob_x - 7, track.top - 8, knob_x + 7, track.top + 18};
    char text[80];
    char value_text[16];

    snprintf(text, sizeof(text), "%s", name);
    snprintf(value_text, sizeof(value_text), "%d", value);
    draw_text_line(hdc, label_x, track.top - 8, text, RGB(205, 214, 222));
    draw_text_line(hdc, track.left - 28, track.top - 8, value_text, RGB(232, 238, 244));
    fill_rect(hdc, track, RGB(82, 94, 104));
    fill.right = knob_x;
    fill_rect(hdc, fill, RGB(82, 136, 171));
    fill_rect(hdc, knob, RGB(232, 238, 244));
}

static void draw_info_tab(HDC hdc, RECT client, int x, int y, HFONT title_font, HFONT body_font) {
    char text[180];
    int owner = selected_tile_owner();
    int civ_id = selected_civ;
    int inner_w = side_panel_w - FORM_X_PAD * 2;
    int quad_w = (inner_w - 24) / 4;
    int metric_h = 30;
    const char *tooltip_text = NULL;

    if (owner >= 0 && owner < civ_count) civ_id = owner;
    if (civ_id >= 0 && civ_id < civ_count) {
        RECT swatch = {x, y + 3, x + 18, y + 21};
        CountrySummary country = summarize_country(civ_id);
        fill_rect(hdc, swatch, civs[civ_id].color);
        snprintf(text, sizeof(text), "%c  %.63s", civs[civ_id].symbol, civs[civ_id].name);
        SelectObject(hdc, title_font);
        draw_text_line(hdc, x + 28, y, text, RGB(245, 245, 245));
        y += 22;
        SelectObject(hdc, body_font);
        snprintf(text, sizeof(text), "%s: %s  %s %d", tr("Capital", "首都"), capital_name_for_civ(civ_id),
                 tr("Ports", "港口"), country.ports);
        draw_text_line(hdc, x + 28, y, text, RGB(180, 190, 198));
        y += 22;
        {
            RECT m = metric_grid_rect(x, y, quad_w, metric_h, 0);
            draw_metric_box(hdc, m, ICON_POPULATION, "POP", country.population, RGB(74, 112, 160), tr("Country population", "国家总人口"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 1);
            draw_metric_box(hdc, m, ICON_TERRITORY, "LAND", country.territory, RGB(88, 137, 83), tr("Total owned land tiles", "国家拥有土地格数"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 2);
            draw_metric_box(hdc, m, ICON_CITY_CAPITAL, "CITY", country.cities, RGB(154, 128, 74), tr("Total cities", "国家城市数量"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 3);
            draw_metric_box(hdc, m, ICON_DISORDER, "DIS", civs[civ_id].disorder, RGB(170, 73, 73), tr("Total disorder", "总混乱度"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 4);
            draw_metric_box(hdc, m, ICON_COUNTRY_DEFENSE, "GOV", civs[civ_id].governance, RGB(82, 114, 153), localized_text(CIVILIZATION_METRIC_RULES[CIV_METRIC_GOVERNANCE].ability, ui_language), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 5);
            draw_metric_box(hdc, m, ICON_CULTURE, "COH", civs[civ_id].cohesion, RGB(147, 105, 167), localized_text(CIVILIZATION_METRIC_RULES[CIV_METRIC_COHESION].ability, ui_language), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 6);
            draw_metric_box(hdc, m, ICON_PRODUCTION, "PROD", civs[civ_id].production, RGB(67, 128, 76), localized_text(CIVILIZATION_METRIC_RULES[CIV_METRIC_PRODUCTION].ability, ui_language), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 7);
            draw_metric_box(hdc, m, ICON_BATTLE, "MIL", civs[civ_id].military, RGB(158, 74, 62), localized_text(CIVILIZATION_METRIC_RULES[CIV_METRIC_MILITARY].ability, ui_language), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 8);
            draw_metric_box(hdc, m, ICON_ECONOMY, "COM", civs[civ_id].commerce, RGB(169, 134, 54), localized_text(CIVILIZATION_METRIC_RULES[CIV_METRIC_COMMERCE].ability, ui_language), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 9);
            draw_metric_box(hdc, m, ICON_MIGRATION, "LOG", civs[civ_id].logistics, RGB(82, 133, 87), localized_text(CIVILIZATION_METRIC_RULES[CIV_METRIC_LOGISTICS].ability, ui_language), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 10);
            draw_metric_box(hdc, m, ICON_INNOVATION, "INN", civs[civ_id].innovation, RGB(102, 128, 180), localized_text(CIVILIZATION_METRIC_RULES[CIV_METRIC_INNOVATION].ability, ui_language), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 11);
            draw_metric_box(hdc, m, ICON_HABITABILITY, "ADP", civs[civ_id].adaptation, RGB(116, 145, 94), tr("Dynamic adaptation from environment, resources, culture, and disorder", "由环境、资源、文化和混乱度动态决定的适应力"), &tooltip_text);
            y += 3 * (metric_h + 6) + 4;
        }
        draw_text_line(hdc, x, y, tr("Country Resources", "国家资源"), RGB(205, 214, 222));
        y += 22;
        {
            RECT m = metric_grid_rect(x, y, quad_w, metric_h, 0);
            draw_metric_box(hdc, m, ICON_FOOD, "FOOD", country.food, RGB(124, 154, 70), tr("Average country food", "国家平均粮食"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 1);
            draw_metric_box(hdc, m, ICON_LIVESTOCK, "HERD", country.livestock, RGB(126, 104, 70), tr("Average country livestock", "国家平均畜牧"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 2);
            draw_metric_box(hdc, m, ICON_WOOD, "WOOD", country.wood, RGB(67, 128, 76), tr("Average country wood", "国家平均木材"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 3);
            draw_metric_box(hdc, m, ICON_STONE, "STON", country.stone, RGB(130, 120, 104), tr("Average country stone", "国家平均石料"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 4);
            draw_metric_box(hdc, m, ICON_ORE, "ORE", country.minerals, RGB(130, 120, 104), tr("Average country ore", "国家平均矿石"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 5);
            draw_metric_box(hdc, m, ICON_WATER, "WATR", country.water, RGB(65, 126, 174), tr("Average country water", "国家平均水"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 6);
            draw_metric_box(hdc, m, ICON_POPULATION, "CAP", country.pop_capacity, RGB(74, 112, 160), tr("Average population capacity", "平均人口承载"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 7);
            draw_metric_box(hdc, m, ICON_ECONOMY, "CASH", country.money, RGB(169, 134, 54), tr("Average money potential", "平均金钱潜力"), &tooltip_text);
            y += 2 * (metric_h + 6) + 6;
        }
        draw_text_line(hdc, x, y, tr("Disorder Factors", "混乱因素"), RGB(205, 214, 222));
        y += 22;
        {
            RECT m = metric_grid_rect(x, y, quad_w, metric_h, 0);
            draw_metric_box(hdc, m, ICON_HABITABILITY, "RES", civs[civ_id].disorder_resource, RGB(170, 73, 73), tr("Resource pressure disorder", "资源压力混乱"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 1);
            draw_metric_box(hdc, m, ICON_DISORDER, "PLG", civs[civ_id].disorder_plague, RGB(170, 73, 73), tr("Plague disorder", "瘟疫混乱"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 2);
            draw_metric_box(hdc, m, ICON_MIGRATION, "MIG", civs[civ_id].disorder_migration, RGB(170, 73, 73), tr("Migration disorder", "迁徙混乱"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 3);
            draw_metric_box(hdc, m, ICON_COUNTRY_DEFENSE, "STAB", civs[civ_id].disorder_stability, RGB(82, 114, 153), tr("Stability pressure reduction", "稳定因素"), &tooltip_text);
            y += metric_h + 18;
        }
    } else {
        SelectObject(hdc, title_font);
        draw_text_line(hdc, x, y, tr("No Country Selected", "未选择国家"), RGB(245, 245, 245));
        SelectObject(hdc, body_font);
        y += 28;
        draw_text_line(hdc, x, y, tr("Select a country or one of its tiles.", "选择国家或它的地块。"), RGB(180, 190, 198));
        y += 42;
    }

    if (selected_x >= 0 && selected_y >= 0) {
        TerrainStats stats = tile_stats(selected_x, selected_y);
        int city_id = city_at(selected_x, selected_y);
        int region_id = city_for_tile(selected_x, selected_y);
        const char *province_name = region_id >= 0 ? cities[region_id].name : "Unorganized Land";
        int metric_food = stats.food;
        int metric_livestock = stats.livestock;
        int metric_wood = stats.wood;
        int metric_stone = stats.stone;
        int metric_ore = stats.minerals;
        int metric_water = stats.water;
        int metric_pop = stats.pop_capacity;
        int metric_money = stats.money;
        int metric_live = stats.habitability;
        int metric_attack = stats.attack;
        int metric_defense = stats.defense;
        int province_tiles = 0;
        int province_population = 0;

        if (region_id >= 0) {
            RegionSummary summary = summarize_city_region(region_id);
            metric_food = summary.food;
            metric_livestock = summary.livestock;
            metric_wood = summary.wood;
            metric_stone = summary.stone;
            metric_ore = summary.minerals;
            metric_water = summary.water;
            metric_pop = summary.pop_capacity;
            metric_money = summary.money;
            metric_live = summary.habitability;
            metric_attack = summary.attack;
            metric_defense = summary.defense;
            province_tiles = summary.tiles;
            province_population = summary.population;
        }

        SelectObject(hdc, title_font);
        draw_text_line(hdc, x, y, province_name, RGB(245, 245, 245));
        y += 26;
        SelectObject(hdc, body_font);
        snprintf(text, sizeof(text), "%s: %d, %d", tr("Tile", "地块"), selected_x, selected_y);
        draw_text_line(hdc, x, y, text, RGB(220, 225, 230)); y += 20;
        snprintf(text, sizeof(text), "%s: %s%s", tr("Geography", "地理"),
                 geography_name(world[selected_y][selected_x].geography),
                 world[selected_y][selected_x].river ? tr(" + River", " + 河流") : "");
        draw_icon_text_line(hdc, x, y, ICON_GEOGRAPHY, text, RGB(220, 225, 230)); y += 22;
        snprintf(text, sizeof(text), "%s: %s", tr("Climate", "气候"), climate_name(world[selected_y][selected_x].climate));
        draw_icon_text_line(hdc, x, y, ICON_CLIMATE, text, RGB(220, 225, 230)); y += 22;
        snprintf(text, sizeof(text), "%s: %s", tr("Ecology", "生态"), ecology_name(world[selected_y][selected_x].ecology));
        draw_text_line(hdc, x + 26, y, text, RGB(220, 225, 230)); y += 20;
        snprintf(text, sizeof(text), "%s: %s", tr("Resource", "资源点"), resource_name(world[selected_y][selected_x].resource));
        draw_text_line(hdc, x + 26, y, text, RGB(220, 225, 230)); y += 20;
        snprintf(text, sizeof(text), "%s %d  %s %d  %s %d",
                 tr("Elev", "海拔"),
                 world[selected_y][selected_x].elevation,
                 tr("Moist", "湿度"),
                 world[selected_y][selected_x].moisture,
                 tr("Temp", "温度"),
                 world[selected_y][selected_x].temperature);
        draw_text_line(hdc, x, y, text, RGB(180, 190, 198)); y += 20;
        if (region_id >= 0) {
            if (cities[region_id].owner >= 0 && cities[region_id].owner < civ_count) {
                snprintf(text, sizeof(text), "%s: %s", tr("Country", "国家"), civs[cities[region_id].owner].name);
                draw_text_line(hdc, x, y, text, RGB(180, 190, 198)); y += 20;
            }
            snprintf(text, sizeof(text), "%s %d  %s %d", tr("Tiles", "地块"), province_tiles, tr("Pop", "人口"), province_population);
            draw_text_line(hdc, x, y, text, RGB(180, 190, 198)); y += 22;
        } else {
            draw_text_line(hdc, x, y, tr("No province has been established here.", "这里还没有建立行省。"), RGB(180, 190, 198)); y += 22;
        }
        {
            RECT m = metric_grid_rect(x, y, quad_w, metric_h, 0);
            draw_metric_box(hdc, m, ICON_HABITABILITY, "LIVE", metric_live, RGB(116, 145, 94), tr("Habitability factors: food, livestock, wood, stone, ore, water, temperature", "宜居度：粮食、畜牧、木材、石料、矿石、水、温度综合"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 1);
            draw_metric_box(hdc, m, ICON_FOOD, "FOOD", metric_food, RGB(124, 154, 70), region_id >= 0 ? tr("Province average food", "行省平均粮食") : tr("Tile food", "地块粮食"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 2);
            draw_metric_box(hdc, m, ICON_LIVESTOCK, "HERD", metric_livestock, RGB(126, 104, 70), region_id >= 0 ? tr("Province average livestock", "行省平均畜牧") : tr("Tile livestock", "地块畜牧"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 3);
            draw_metric_box(hdc, m, ICON_WOOD, "WOOD", metric_wood, RGB(67, 128, 76), region_id >= 0 ? tr("Province average wood", "行省平均木材") : tr("Tile wood", "地块木材"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 4);
            draw_metric_box(hdc, m, ICON_STONE, "STON", metric_stone, RGB(130, 120, 104), region_id >= 0 ? tr("Province average stone", "行省平均石料") : tr("Tile stone", "地块石料"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 5);
            draw_metric_box(hdc, m, ICON_ORE, "ORE", metric_ore, RGB(130, 120, 104), region_id >= 0 ? tr("Province average ore", "行省平均矿石") : tr("Tile ore", "地块矿石"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 6);
            draw_metric_box(hdc, m, ICON_WATER, "WATR", metric_water, RGB(65, 126, 174), region_id >= 0 ? tr("Province average water", "行省平均水") : tr("Tile water", "地块水"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 7);
            draw_metric_box(hdc, m, ICON_ECONOMY, "CASH", metric_money, RGB(169, 134, 54), region_id >= 0 ? tr("Province average money potential", "行省平均金钱潜力") : tr("Tile money potential", "地块金钱潜力"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 8);
            draw_metric_box(hdc, m, ICON_POPULATION, "POP", metric_pop, RGB(74, 112, 160), region_id >= 0 ? tr("Province average population potential", "行省平均人口潜力") : tr("Tile population potential", "地块人口潜力"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 9);
            draw_metric_box(hdc, m, ICON_ATTACK, "ATK", metric_attack, RGB(158, 74, 62), region_id >= 0 ? tr("Province average attack modifier", "行省平均攻击修正") : tr("Tile attack modifier", "地块攻击修正"), &tooltip_text);
            m = metric_grid_rect(x, y, quad_w, metric_h, 10);
            draw_metric_box(hdc, m, ICON_TILE_DEFENSE, "DEF", metric_defense, RGB(80, 101, 148), region_id >= 0 ? tr("Province average defense modifier", "行省平均防御修正") : tr("Tile defense modifier", "地块防御修正"), &tooltip_text);
            y += 3 * (metric_h + 6) + 2;
        }
        snprintf(text, sizeof(text), "%s: LIVE %d FOOD %d HERD %d WOOD %d STON %d ORE %d WATER %d CASH %d",
                 tr("Province avg", "行省平均"), metric_live, metric_food, metric_livestock, metric_wood,
                 metric_stone, metric_ore, metric_water, metric_money);
        draw_text_line(hdc, x, y, text, RGB(145, 158, 168));
        y += 18;
        if (city_id >= 0) {
            snprintf(text, sizeof(text), "%s: %s%s  %s %d", tr("City", "城市"), cities[city_id].name,
                     cities[city_id].port ? tr(" Port", " 港口") : "",
                     tr("Pop", "人口"), cities[city_id].population);
            draw_text_line(hdc, x, y, text, RGB(220, 225, 230)); y += 20;
        }
    } else {
        SelectObject(hdc, title_font);
        draw_text_line(hdc, x, y, tr("No Province Selected", "未选择行省"), RGB(245, 245, 245));
        SelectObject(hdc, body_font);
        y += 28;
        draw_text_line(hdc, x, y, tr("Click a tile on the map.", "点击地图地块查看信息。"), RGB(180, 190, 198));
    }
    draw_tooltip(hdc, client, tooltip_text);
}

static void draw_civ_tab(HDC hdc, RECT client, int x, HFONT title_font, HFONT body_font) {
    int y = TOP_BAR_H + 58;
    int i;
    char text[180];

    SelectObject(hdc, title_font);
    draw_text_line(hdc, x, y, tr("Civilizations", "文明"), RGB(245, 245, 245));
    y += 30;
    SelectObject(hdc, body_font);
    for (i = 0; i < civ_count && y < client.bottom - 500; i++) {
        RECT swatch = {x, y + 3, x + 15, y + 18};
        fill_rect(hdc, swatch, civs[i].color);
        snprintf(text, sizeof(text), "%c  %.63s", civs[i].symbol, civs[i].name);
        draw_text_line(hdc, x + 24, y, text, i == selected_civ ? RGB(255, 238, 150) : RGB(238, 238, 238));
        y += 19;
        snprintf(text, sizeof(text), "Pop %d Land %d GOV%d COH%d PROD%d MIL%d",
                 civs[i].population, civs[i].territory, civs[i].governance,
                 civs[i].cohesion, civs[i].production, civs[i].military);
        draw_text_line(hdc, x + 24, y, text, RGB(175, 186, 196));
        y += 25;
    }

    if (selected_civ >= 0 && selected_civ < civ_count) {
        int relation_lines = 0;

        SelectObject(hdc, title_font);
        draw_text_line(hdc, x, y + 6, tr("Diplomacy", "外交"), RGB(245, 245, 245));
        y += 34;
        SelectObject(hdc, body_font);
        for (i = 0; i < civ_count && y < client.bottom - 300; i++) {
            DiplomacyRelation relation;
            ActiveWar active_war;
            char status_text[64];

            if (i == selected_civ) continue;
            relation = diplomacy_relation(selected_civ, i);
            if (relation.state == DIPLOMACY_NONE) continue;
            active_war = war_state_between(selected_civ, i);
            if (relation.state == DIPLOMACY_VASSAL && relation.overlord >= 0 && relation.overlord < civ_count) {
                if (relation.overlord == selected_civ) snprintf(status_text, sizeof(status_text), "%s", tr("Vassal", "附庸"));
                else snprintf(status_text, sizeof(status_text), "%s %c", tr("Vassal to", "附庸于"), civs[relation.overlord].symbol);
            } else {
                snprintf(status_text, sizeof(status_text), "%s", diplomacy_status_name(relation.state));
            }
            snprintf(text, sizeof(text), "%c %s  %s  REL %d TEN %d TRADE %d CON %d TRUCE %d",
                     civs[i].symbol, civs[i].name, status_text,
                     relation.relation_score, relation.border_tension, relation.trade_fit,
                     relation.resource_conflict, relation.truce_years_left);
            draw_text_line(hdc, x + 8, y, text, RGB(210, 220, 228));
            y += 20;
            if (active_war.active) {
                snprintf(text, sizeof(text), "War Years %d  Soldiers %d/%d  Losses %d/%d",
                         active_war.years, active_war.soldiers_a, active_war.soldiers_b,
                         active_war.casualties_a, active_war.casualties_b);
                draw_text_line(hdc, x + 18, y, text, RGB(196, 159, 142));
                y += 20;
            }
            relation_lines++;
        }
        if (relation_lines == 0) {
            draw_text_line(hdc, x + 8, y, tr("No known neighboring countries.", "暂无已接触邻国。"), RGB(160, 171, 180));
            y += 22;
        }
    }

    SelectObject(hdc, title_font);
    draw_text_line(hdc, x, client.bottom - 262, tr("Civilization Form", "文明表单"), RGB(245, 245, 245));
    SelectObject(hdc, body_font);
    draw_text_line(hdc, x, client.bottom - 235, tr("Name", "名字"), RGB(200, 210, 218));
    draw_text_line(hdc, x + 176, client.bottom - 235, tr("Symbol", "符号"), RGB(200, 210, 218));
    draw_text_line(hdc, x, client.bottom - 175, tr("Military", "军备"), RGB(200, 210, 218));
    draw_text_line(hdc, x + 82, client.bottom - 175, tr("Logistics", "后勤"), RGB(200, 210, 218));
    draw_text_line(hdc, x + 164, client.bottom - 175, tr("Governance", "治理"), RGB(200, 210, 218));
    draw_text_line(hdc, x + 246, client.bottom - 175, tr("Cohesion", "凝聚"), RGB(200, 210, 218));
    draw_text_line(hdc, x, client.bottom - 64, tr("F1 add. F2 apply. Select empty land before adding.", "F1 添加，F2 应用。添加前先选空地。"), RGB(160, 171, 180));
}

static void draw_map_tab(HDC hdc, RECT client, int x, HFONT title_font, HFONT body_font) {
    int y = TOP_BAR_H + 58;

    SelectObject(hdc, title_font);
    draw_text_line(hdc, x, y, tr("Map View", "地图视角"), RGB(245, 245, 245));
    SelectObject(hdc, body_font);
    draw_mode_buttons(hdc, client);

    y = TOP_BAR_H + 216;
    SelectObject(hdc, title_font);
    draw_text_line(hdc, x, y, tr("World Generation", "世界生成"), RGB(245, 245, 245));
    SelectObject(hdc, body_font);
    draw_text_line(hdc, x, y + 30, tr("Initial civilizations", "初始文明数量"), RGB(200, 210, 218));
    draw_setup_slider(hdc, client, WORLD_SLIDER_OCEAN, tr("Ocean", "海陆比例"), ocean_slider);
    draw_setup_slider(hdc, client, WORLD_SLIDER_CONTINENT, tr("Fragment", "大陆破碎"), continent_slider);
    draw_setup_slider(hdc, client, WORLD_SLIDER_RELIEF, tr("Relief", "地势起伏"), relief_slider);
    draw_setup_slider(hdc, client, WORLD_SLIDER_MOISTURE, tr("Moisture", "湿润度"), moisture_slider);
    draw_setup_slider(hdc, client, WORLD_SLIDER_DROUGHT, tr("Drought", "干旱度"), drought_slider);
    draw_setup_slider(hdc, client, WORLD_SLIDER_VEGETATION, tr("Vegetation", "植被密度"), vegetation_slider);
    draw_text_line(hdc, x, TOP_BAR_H + 496, tr("Advanced Terrain Bias", "高级地形偏好"), RGB(245, 245, 245));
    draw_setup_slider(hdc, client, WORLD_SLIDER_BIAS_FOREST, tr("Forest", "森林"), bias_forest_slider);
    draw_setup_slider(hdc, client, WORLD_SLIDER_BIAS_DESERT, tr("Desert", "沙漠"), bias_desert_slider);
    draw_setup_slider(hdc, client, WORLD_SLIDER_BIAS_MOUNTAIN, tr("Mountain", "山脉"), bias_mountain_slider);
    draw_setup_slider(hdc, client, WORLD_SLIDER_BIAS_WETLAND, tr("Wetland", "湿地"), bias_wetland_slider);
    draw_text_line(hdc, x, client.bottom - 64, tr("F5 rebuilds with these settings.", "F5 使用这些设置重建世界。"), RGB(160, 171, 180));
}

static void draw_side_panel(HDC hdc, RECT client) {
    int x = client.right - side_panel_w + 18;
    RECT panel = {client.right - side_panel_w, TOP_BAR_H, client.right, client.bottom};
    RECT divider = {client.right - side_panel_w - 3, TOP_BAR_H, client.right - side_panel_w + 3, client.bottom};
    HFONT title_font = CreateFontA(21, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET,
                                   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                   DEFAULT_PITCH | FF_SWISS, "Microsoft YaHei UI");
    HFONT body_font = CreateFontA(17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
                                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                  DEFAULT_PITCH | FF_SWISS, "Microsoft YaHei UI");
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

    snprintf(text, sizeof(text), "%s    %s: %s (%s/%s)    %s    F1 %s    F2 %s    F5 %s",
             tr("Space toggles run/pause", "空格开始/暂停"),
             tr("Speed", "速度"), SPEED_NAMES[speed_index], speed_seconds_text(speed_index), tr("month", "月"),
             tr("Wheel zoom", "滚轮缩放"), tr("add", "添加"), tr("apply", "应用"), tr("new world", "新世界"));
    draw_text_line(hdc, 216, client.bottom - 29, text, RGB(225, 230, 235));
}

static void draw_legend_item(HDC hdc, int x, int y, COLORREF color, const char *name) {
    RECT swatch = {x, y + 3, x + 16, y + 15};
    fill_rect(hdc, swatch, color);
    draw_text_line(hdc, x + 22, y, name, RGB(232, 238, 242));
}

static void draw_map_legend(HDC hdc, RECT client) {
    const Geography geographies[] = {
        GEO_OCEAN, GEO_COAST, GEO_PLAIN, GEO_HILL, GEO_MOUNTAIN, GEO_PLATEAU,
        GEO_BASIN, GEO_CANYON, GEO_VOLCANO, GEO_LAKE, GEO_BAY, GEO_DELTA,
        GEO_WETLAND, GEO_OASIS, GEO_ISLAND
    };
    const Climate climates[] = {
        CLIMATE_TROPICAL_RAINFOREST, CLIMATE_TROPICAL_MONSOON, CLIMATE_TROPICAL_SAVANNA,
        CLIMATE_DESERT, CLIMATE_SEMI_ARID, CLIMATE_MEDITERRANEAN, CLIMATE_OCEANIC,
        CLIMATE_TEMPERATE_MONSOON, CLIMATE_CONTINENTAL, CLIMATE_SUBARCTIC, CLIMATE_TUNDRA,
        CLIMATE_ICE_CAP, CLIMATE_ALPINE, CLIMATE_HIGHLAND_PLATEAU
    };
    int geo_count = (int)(sizeof(geographies) / sizeof(geographies[0]));
    int climate_count = (int)(sizeof(climates) / sizeof(climates[0]));
    int line_h = 20;
    int x;
    int y;
    int i;
    RECT box = get_map_legend_box_rect(client);
    RECT toggle = get_map_legend_toggle_rect(client);
    HBRUSH border_brush;

    if (IsRectEmpty(&box)) return;

    fill_rect_alpha(hdc, box, RGB(31, 37, 43), 218);
    border_brush = CreateSolidBrush(RGB(76, 92, 104));
    FrameRect(hdc, &box, border_brush);
    DeleteObject(border_brush);
    fill_rect_alpha(hdc, toggle, RGB(47, 58, 68), 236);
    draw_center_text(hdc, toggle, map_legend_collapsed ? "^" : "v", RGB(236, 242, 246));
    draw_text_line(hdc, box.left + 10, box.top + 8, "Map Legend", RGB(245, 245, 245));
    if (map_legend_collapsed) return;

    x = box.left + 10;
    y = box.top + 30;

    if (display_mode == DISPLAY_CLIMATE) {
        for (i = 0; i < climate_count; i++) {
            draw_legend_item(hdc, x, y + i * line_h, climate_color(climates[i]), climate_name(climates[i]));
        }
        return;
    }

    for (i = 0; i < geo_count; i++) {
        draw_legend_item(hdc, x, y + i * line_h, geography_color(geographies[i]), geography_name(geographies[i]));
    }
    if (display_mode == DISPLAY_ALL) {
        x = box.left + 190;
        for (i = 0; i < climate_count; i++) {
            draw_legend_item(hdc, x, y + i * line_h, climate_color(climates[i]), climate_name(climates[i]));
        }
    }
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
    if (layout.tile_size >= 1) {
        draw_rivers(hdc, client, layout);
        draw_borders(hdc, client, layout);
    }
    draw_cities(hdc, layout);
    draw_selected_tile(hdc, layout);
    draw_bottom_bar(hdc, client);
    draw_map_legend(hdc, client);
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
