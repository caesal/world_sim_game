#include "render/worldgen_ui_assets.h"
#include "render/worldgen_ui_surface_cache.h"

#include <string.h>

#define WORLDGEN_UI_ASSET_BASE "assets\\worldgen_ui\\"
#define GDIP_UNIT_PIXEL 2

typedef struct {
    unsigned int GdiplusVersion;
    void *DebugEventCallback;
    BOOL SuppressBackgroundThread;
    BOOL SuppressExternalCodecs;
} GdiplusStartupInputLocal;

typedef int (WINAPI *GdiplusStartupProc)(
    ULONG_PTR *, const GdiplusStartupInputLocal *, void *);
typedef void (WINAPI *GdiplusShutdownProc)(ULONG_PTR);
typedef int (WINAPI *GdipCreateBitmapFromFileProc)(const WCHAR *, void **);
typedef int (WINAPI *GdipGetImageDimensionProc)(void *, unsigned int *);
typedef int (WINAPI *GdipCreateFromHDCProc)(HDC, void **);
typedef int (WINAPI *GdipDrawImageRectRectIProc)(
    void *, void *, int, int, int, int, int, int, int, int, int,
    void *, void *, void *);
typedef int (WINAPI *GdipDeleteGraphicsProc)(void *);
typedef int (WINAPI *GdipDisposeImageProc)(void *);

typedef struct {
    void *image;
    unsigned int width;
    unsigned int height;
    int attempted;
} WorldgenUiAssetCache;

static const WorldgenUiAssetInfo ASSET_INFO[WORLDGEN_UI_ASSET_COUNT] = {
    {WORLDGEN_UI_ASSET_BASE "landmass_ocean_xy.png", 1254, 1254, 1},
    {WORLDGEN_UI_ASSET_BASE "relief_profile.png", 2048, 768, 1},
    {WORLDGEN_UI_ASSET_BASE "climate_biome_envelope.png", 1536, 1024, 1},
    {WORLDGEN_UI_ASSET_BASE "river_density_atlas.png", 2172, 724,
     WORLDGEN_UI_ATLAS_SLOT_COUNT},
    {WORLDGEN_UI_ASSET_BASE "natural_region_scale_atlas.png", 2048, 768,
     WORLDGEN_UI_ATLAS_SLOT_COUNT}
};

static HMODULE gdiplus_module;
static ULONG_PTR gdiplus_token;
static int gdiplus_attempted;
static GdiplusStartupProc gdiplus_startup;
static GdiplusShutdownProc gdiplus_shutdown;
static GdipCreateBitmapFromFileProc gdip_create_bitmap_from_file;
static GdipGetImageDimensionProc gdip_get_image_width;
static GdipGetImageDimensionProc gdip_get_image_height;
static GdipCreateFromHDCProc gdip_create_from_hdc;
static GdipDrawImageRectRectIProc gdip_draw_image_rect_rect_i;
static GdipDeleteGraphicsProc gdip_delete_graphics;
static GdipDisposeImageProc gdip_dispose_image;
static WorldgenUiAssetCache asset_cache[WORLDGEN_UI_ASSET_COUNT];
static WorldgenUiAssetDiagnostics asset_diagnostics[WORLDGEN_UI_ASSET_COUNT];
static int validation_force_missing[WORLDGEN_UI_ASSET_COUNT];

static int asset_valid(WorldgenUiAssetId asset) {
    return asset >= 0 && asset < WORLDGEN_UI_ASSET_COUNT;
}

static void clear_gdiplus_procs(void) {
    gdiplus_startup = NULL;
    gdiplus_shutdown = NULL;
    gdip_create_bitmap_from_file = NULL;
    gdip_get_image_width = NULL;
    gdip_get_image_height = NULL;
    gdip_create_from_hdc = NULL;
    gdip_draw_image_rect_rect_i = NULL;
    gdip_delete_graphics = NULL;
    gdip_dispose_image = NULL;
}

static int ensure_gdiplus(void) {
    GdiplusStartupInputLocal input;
    union { FARPROC raw; GdiplusStartupProc typed; } startup_proc;
    union { FARPROC raw; GdiplusShutdownProc typed; } shutdown_proc;
    union { FARPROC raw; GdipCreateBitmapFromFileProc typed; } bitmap_proc;
    union { FARPROC raw; GdipGetImageDimensionProc typed; } width_proc;
    union { FARPROC raw; GdipGetImageDimensionProc typed; } height_proc;
    union { FARPROC raw; GdipCreateFromHDCProc typed; } graphics_proc;
    union { FARPROC raw; GdipDrawImageRectRectIProc typed; } draw_proc;
    union { FARPROC raw; GdipDeleteGraphicsProc typed; } delete_proc;
    union { FARPROC raw; GdipDisposeImageProc typed; } dispose_proc;

    if (gdiplus_token) return 1;
    if (gdiplus_attempted) return 0;
    gdiplus_attempted = 1;
    gdiplus_module = LoadLibraryA("gdiplus.dll");
    if (!gdiplus_module) return 0;
    startup_proc.raw = GetProcAddress(gdiplus_module, "GdiplusStartup");
    shutdown_proc.raw = GetProcAddress(gdiplus_module, "GdiplusShutdown");
    bitmap_proc.raw = GetProcAddress(gdiplus_module,
                                     "GdipCreateBitmapFromFile");
    width_proc.raw = GetProcAddress(gdiplus_module, "GdipGetImageWidth");
    height_proc.raw = GetProcAddress(gdiplus_module, "GdipGetImageHeight");
    graphics_proc.raw = GetProcAddress(gdiplus_module, "GdipCreateFromHDC");
    draw_proc.raw = GetProcAddress(gdiplus_module,
                                   "GdipDrawImageRectRectI");
    delete_proc.raw = GetProcAddress(gdiplus_module, "GdipDeleteGraphics");
    dispose_proc.raw = GetProcAddress(gdiplus_module, "GdipDisposeImage");
    gdiplus_startup = startup_proc.typed;
    gdiplus_shutdown = shutdown_proc.typed;
    gdip_create_bitmap_from_file = bitmap_proc.typed;
    gdip_get_image_width = width_proc.typed;
    gdip_get_image_height = height_proc.typed;
    gdip_create_from_hdc = graphics_proc.typed;
    gdip_draw_image_rect_rect_i = draw_proc.typed;
    gdip_delete_graphics = delete_proc.typed;
    gdip_dispose_image = dispose_proc.typed;
    if (!gdiplus_startup || !gdiplus_shutdown ||
        !gdip_create_bitmap_from_file || !gdip_get_image_width ||
        !gdip_get_image_height || !gdip_create_from_hdc ||
        !gdip_draw_image_rect_rect_i || !gdip_delete_graphics ||
        !gdip_dispose_image) {
        clear_gdiplus_procs();
        FreeLibrary(gdiplus_module);
        gdiplus_module = NULL;
        return 0;
    }
    memset(&input, 0, sizeof(input));
    input.GdiplusVersion = 1;
    if (gdiplus_startup(&gdiplus_token, &input, NULL) != 0) {
        gdiplus_token = 0;
        clear_gdiplus_procs();
        FreeLibrary(gdiplus_module);
        gdiplus_module = NULL;
        return 0;
    }
    return 1;
}

static void dispose_unretained_image(WorldgenUiAssetId asset, void *image) {
    if (!image || !gdip_dispose_image) return;
    gdip_dispose_image(image);
    asset_diagnostics[asset].bitmap_releases++;
}

static void *load_asset(WorldgenUiAssetId asset) {
    WorldgenUiAssetCache *cache;
    WorldgenUiAssetDiagnostics *diagnostics;
    WCHAR wide_path[MAX_PATH];
    void *image = NULL;
    unsigned int width = 0;
    unsigned int height = 0;

    if (!asset_valid(asset)) return NULL;
    cache = &asset_cache[asset];
    diagnostics = &asset_diagnostics[asset];
    if (cache->attempted) return cache->image;
    cache->attempted = 1;
    diagnostics->attempts++;
    if (validation_force_missing[asset]) return NULL;
    if (!ensure_gdiplus()) return NULL;
    if (!MultiByteToWideChar(CP_UTF8, 0, ASSET_INFO[asset].path, -1,
                             wide_path, MAX_PATH)) return NULL;
    if (gdip_create_bitmap_from_file(wide_path, &image) != 0 || !image) {
        return NULL;
    }
    diagnostics->decodes++;
    diagnostics->bitmap_allocations++;
    if (gdip_get_image_width(image, &width) != 0 ||
        gdip_get_image_height(image, &height) != 0 ||
        width == 0 || height == 0) {
        dispose_unretained_image(asset, image);
        return NULL;
    }
    cache->image = image;
    cache->width = width;
    cache->height = height;
    return cache->image;
}

static RECT aspect_fit_rect(RECT dst, int source_width, int source_height) {
    RECT fit = dst;
    int box_width = dst.right - dst.left;
    int box_height = dst.bottom - dst.top;
    int draw_width;
    int draw_height;

    if (box_width <= 0 || box_height <= 0 ||
        source_width <= 0 || source_height <= 0) {
        return (RECT){dst.left, dst.top, dst.left, dst.top};
    }
    draw_width = box_width;
    draw_height = (int)((long long)box_width * source_height / source_width);
    if (draw_height > box_height) {
        draw_height = box_height;
        draw_width = (int)((long long)box_height * source_width /
                           source_height);
    }
    fit.left = dst.left + (box_width - draw_width) / 2;
    fit.top = dst.top + (box_height - draw_height) / 2;
    fit.right = fit.left + draw_width;
    fit.bottom = fit.top + draw_height;
    return fit;
}

static void draw_neutral_fallback(HDC hdc, RECT dst,
                                  int source_width, int source_height) {
    RECT fit = aspect_fit_rect(dst, source_width, source_height);
    HGDIOBJ old_brush;
    HGDIOBJ old_pen;
    COLORREF old_brush_color;
    COLORREF old_pen_color;

    if (!hdc || fit.right <= fit.left || fit.bottom <= fit.top) return;
    old_brush = SelectObject(hdc, GetStockObject(DC_BRUSH));
    old_pen = SelectObject(hdc, GetStockObject(DC_PEN));
    old_brush_color = SetDCBrushColor(hdc, RGB(48, 56, 58));
    old_pen_color = SetDCPenColor(hdc, RGB(92, 104, 106));
    Rectangle(hdc, fit.left, fit.top, fit.right, fit.bottom);
    MoveToEx(hdc, fit.left + 4, fit.top + 4, NULL);
    LineTo(hdc, fit.right - 4, fit.bottom - 4);
    MoveToEx(hdc, fit.right - 4, fit.top + 4, NULL);
    LineTo(hdc, fit.left + 4, fit.bottom - 4);
    SetDCPenColor(hdc, old_pen_color);
    SetDCBrushColor(hdc, old_brush_color);
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
}

typedef struct {
    WorldgenUiAssetId asset;
    int source_x;
    int source_y;
    int source_width;
    int source_height;
} WorldgenUiSurfaceContext;

static int render_source_surface(HDC hdc, int width, int height,
                                 void *context_value) {
    WorldgenUiSurfaceContext *context = context_value;
    WorldgenUiAssetCache *cache = &asset_cache[context->asset];
    void *graphics = NULL;
    int ok;
    if (!cache->image || !hdc || width <= 0 || height <= 0) return 0;
    if (gdip_create_from_hdc(hdc, &graphics) != 0 || !graphics) return 0;
    ok = gdip_draw_image_rect_rect_i(
        graphics, cache->image, 0, 0, width, height,
        context->source_x, context->source_y,
        context->source_width, context->source_height,
        GDIP_UNIT_PIXEL, NULL, NULL, NULL) == 0;
    gdip_delete_graphics(graphics);
    return ok;
}

static int draw_source_fit(HDC hdc, WorldgenUiAssetId asset, int variant,
                           RECT dst, int source_x, int source_y,
                           int source_width, int source_height) {
    RECT fit = aspect_fit_rect(dst, source_width, source_height);
    WorldgenUiSurfaceContext context = {
        asset, source_x, source_y, source_width, source_height
    };
    if (!asset_cache[asset].image || !hdc || fit.right <= fit.left ||
        fit.bottom <= fit.top) return 0;
    return worldgen_ui_surface_cache_draw(hdc, asset, variant, fit,
                                          render_source_surface, &context);
}

const WorldgenUiAssetInfo *worldgen_ui_assets_info(
    WorldgenUiAssetId asset) {
    return asset_valid(asset) ? &ASSET_INFO[asset] : NULL;
}

int worldgen_ui_assets_preload_all(void) {
    int all_loaded = 1;
    int i;
    for (i = 0; i < WORLDGEN_UI_ASSET_COUNT; i++) {
        if (!load_asset((WorldgenUiAssetId)i)) all_loaded = 0;
    }
    return all_loaded;
}

int worldgen_ui_assets_is_loaded(WorldgenUiAssetId asset) {
    return asset_valid(asset) && asset_cache[asset].image != NULL;
}

int worldgen_ui_assets_dimensions(WorldgenUiAssetId asset,
                                  int *width, int *height) {
    WorldgenUiAssetCache *cache;
    if (!load_asset(asset)) return 0;
    cache = &asset_cache[asset];
    if (width) *width = (int)cache->width;
    if (height) *height = (int)cache->height;
    return 1;
}

int worldgen_ui_assets_draw_fit(HDC hdc, WorldgenUiAssetId asset, RECT dst) {
    const WorldgenUiAssetInfo *info = worldgen_ui_assets_info(asset);
    WorldgenUiAssetDiagnostics *diagnostics;
    WorldgenUiAssetCache *cache;
    int ok;

    if (!info || !hdc || dst.right <= dst.left || dst.bottom <= dst.top) {
        return 0;
    }
    diagnostics = &asset_diagnostics[asset];
    diagnostics->draw_calls++;
    if (!load_asset(asset)) {
        diagnostics->fallback_draws++;
        draw_neutral_fallback(hdc, dst, info->expected_width,
                              info->expected_height);
        return 0;
    }
    cache = &asset_cache[asset];
    ok = draw_source_fit(hdc, asset, -1, dst, 0, 0,
                         (int)cache->width, (int)cache->height);
    if (!ok) {
        diagnostics->fallback_draws++;
        draw_neutral_fallback(hdc, dst, (int)cache->width,
                              (int)cache->height);
    }
    return ok;
}

int worldgen_ui_assets_draw_slot_fit(HDC hdc, WorldgenUiAssetId asset,
                                     int slot, RECT dst) {
    const WorldgenUiAssetInfo *info = worldgen_ui_assets_info(asset);
    WorldgenUiAssetDiagnostics *diagnostics;
    WorldgenUiAssetCache *cache;
    int source_left;
    int source_right;
    int ok;

    if (!info || !hdc || dst.right <= dst.left || dst.bottom <= dst.top) {
        return 0;
    }
    diagnostics = &asset_diagnostics[asset];
    diagnostics->draw_calls++;
    if (info->slot_count != WORLDGEN_UI_ATLAS_SLOT_COUNT ||
        slot < 0 || slot >= info->slot_count || !load_asset(asset)) {
        diagnostics->fallback_draws++;
        draw_neutral_fallback(hdc, dst,
                              info->expected_width / max(1, info->slot_count),
                              info->expected_height);
        return 0;
    }
    cache = &asset_cache[asset];
    source_left = (int)((long long)cache->width * slot /
                        info->slot_count);
    source_right = (int)((long long)cache->width * (slot + 1) /
                         info->slot_count);
    ok = draw_source_fit(hdc, asset, slot, dst, source_left, 0,
                         source_right - source_left, (int)cache->height);
    if (!ok) {
        diagnostics->fallback_draws++;
        draw_neutral_fallback(hdc, dst, source_right - source_left,
                              (int)cache->height);
    }
    return ok;
}

int worldgen_ui_assets_get_diagnostics(
    WorldgenUiAssetId asset, WorldgenUiAssetDiagnostics *out) {
    WorldgenUiSurfaceStats surfaces;
    if (!asset_valid(asset) || !out) return 0;
    *out = asset_diagnostics[asset];
    worldgen_ui_surface_cache_get_stats(asset, &surfaces);
    out->surface_builds = surfaces.builds;
    out->surface_hits = surfaces.hits;
    out->surface_allocations = surfaces.allocations;
    out->surface_releases = surfaces.releases;
    out->loaded = asset_cache[asset].image != NULL;
    out->width = (int)asset_cache[asset].width;
    out->height = (int)asset_cache[asset].height;
    return 1;
}

void worldgen_ui_assets_get_totals(WorldgenUiAssetDiagnostics *out) {
    int i;
    if (!out) return;
    memset(out, 0, sizeof(*out));
    for (i = 0; i < WORLDGEN_UI_ASSET_COUNT; i++) {
        WorldgenUiAssetDiagnostics item;
        worldgen_ui_assets_get_diagnostics((WorldgenUiAssetId)i, &item);
        out->attempts += item.attempts;
        out->decodes += item.decodes;
        out->draw_calls += item.draw_calls;
        out->fallback_draws += item.fallback_draws;
        out->bitmap_allocations += item.bitmap_allocations;
        out->bitmap_releases += item.bitmap_releases;
        out->surface_builds += item.surface_builds;
        out->surface_hits += item.surface_hits;
        out->surface_allocations += item.surface_allocations;
        out->surface_releases += item.surface_releases;
        out->loaded += item.loaded;
    }
}

void worldgen_ui_assets_release(void) {
    int i;
    worldgen_ui_surface_cache_release();
    for (i = 0; i < WORLDGEN_UI_ASSET_COUNT; i++) {
        if (asset_cache[i].image && gdip_dispose_image) {
            gdip_dispose_image(asset_cache[i].image);
            asset_diagnostics[i].bitmap_releases++;
        }
        memset(&asset_cache[i], 0, sizeof(asset_cache[i]));
    }
    if (gdiplus_token && gdiplus_shutdown) {
        gdiplus_shutdown(gdiplus_token);
    }
    gdiplus_token = 0;
    clear_gdiplus_procs();
    if (gdiplus_module) FreeLibrary(gdiplus_module);
    gdiplus_module = NULL;
    gdiplus_attempted = 0;
}

void worldgen_ui_assets_reset_for_tests(void) {
    worldgen_ui_assets_release();
    worldgen_ui_surface_cache_reset_for_tests();
    memset(asset_diagnostics, 0, sizeof(asset_diagnostics));
    memset(validation_force_missing, 0, sizeof(validation_force_missing));
}

void worldgen_ui_assets_validation_force_missing(
    WorldgenUiAssetId asset, int force_missing) {
    if (!asset_valid(asset)) return;
    force_missing = force_missing != 0;
    if (validation_force_missing[asset] == force_missing) return;
    worldgen_ui_surface_cache_release();
    if (asset_cache[asset].image && gdip_dispose_image) {
        gdip_dispose_image(asset_cache[asset].image);
        asset_diagnostics[asset].bitmap_releases++;
    }
    memset(&asset_cache[asset], 0, sizeof(asset_cache[asset]));
    validation_force_missing[asset] = force_missing;
}
