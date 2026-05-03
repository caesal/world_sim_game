#include "icons.h"

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
    "assets\\icons\\metric_military.png",
    "assets\\icons\\metric_territory.png",
    "assets\\icons\\resource_population.png",
    "assets\\icons\\metric_defense.png",
    "assets\\icons\\metric_cohesion.png",
    "assets\\icons\\resource_water.png",
    "assets\\icons\\map_geography.png",
    "assets\\icons\\map_climate.png",
    "assets\\icons\\resource_population.png",
    "assets\\icons\\metric_defense.png",
    "assets\\icons\\metric_attack.png",
    "assets\\icons\\resource_food.png",
    "assets\\icons\\resource_livestock.png",
    "assets\\icons\\resource_wood.png",
    "assets\\icons\\resource_ore.png",
    "assets\\icons\\resource_stone.png",
    "assets\\icons\\city_capital.png",
    "assets\\icons\\metric_disorder.png",
    "assets\\icons\\metric_territory.png",
    "assets\\icons\\metric_logistics.png",
    "assets\\icons\\metric_economy.png",
    "assets\\icons\\metric_production.png",
    "assets\\icons\\metric_innovation.png",
    "assets\\icons\\city_stage_outpost.png",
    "assets\\icons\\city_stage_village.png",
    "assets\\icons\\city_stage_town.png",
    "assets\\icons\\city_stage_city.png",
    "assets\\icons\\city_harbor.png"
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

static void fill_icon_fallback(HDC hdc, RECT rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
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
    input.GdiplusVersion = 1;
    input.DebugEventCallback = NULL;
    input.SuppressBackgroundThread = FALSE;
    input.SuppressExternalCodecs = FALSE;
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

void draw_icon(HDC hdc, IconId icon, RECT rect, COLORREF fallback) {
    void *image = icon_image(icon);
    void *graphics = NULL;

    if (!image || !gdip_create_from_hdc || gdip_create_from_hdc(hdc, &graphics) != 0 || !graphics) {
        RECT fallback_rect = {rect.left + 4, rect.top + 4, rect.right - 4, rect.bottom - 4};
        fill_icon_fallback(hdc, fallback_rect, fallback);
        return;
    }
    gdip_draw_image_rect_i(graphics, image, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
    gdip_delete_graphics(graphics);
}
