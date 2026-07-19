#include "render/render_ocean_assets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OCEAN_MOTIF_ASSET_MAX 32
#define OCEAN_ASSET_BASE "assets\\ocean_decoration_reference\\"
#define OCEAN_TEXTURE_PATH OCEAN_ASSET_BASE "ocean_texture_reference.png"
#define OCEAN_MOTIF_MANIFEST OCEAN_ASSET_BASE "motifs_manifest.tsv"
#define OCEAN_TEXTURE_TILE_PX 760

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

static HMODULE gdiplus_module;
static ULONG_PTR gdiplus_token;
static GdiplusStartupProc gdiplus_startup;
static GdipCreateBitmapFromFileProc gdip_create_bitmap_from_file;
static GdipCreateFromHDCProc gdip_create_from_hdc;
static GdipDrawImageRectIProc gdip_draw_image_rect_i;
static GdipDeleteGraphicsProc gdip_delete_graphics;
static GdipDisposeImageProc gdip_dispose_image;
static void *texture_image;
static int texture_attempted;

typedef struct {
    char id[32];
    char path[MAX_PATH];
    OceanMotifAssetInfo info;
    void *image;
    int load_attempted;
} OceanMotifAsset;

static OceanMotifAsset motif_assets[OCEAN_MOTIF_ASSET_MAX];
static int motif_count;
static int manifest_attempted;
static int manifest_loaded;

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
        !gdip_draw_image_rect_i || !gdip_delete_graphics || !gdip_dispose_image) return 0;
    input.GdiplusVersion = 1;
    input.DebugEventCallback = NULL;
    input.SuppressBackgroundThread = FALSE;
    input.SuppressExternalCodecs = FALSE;
    return gdiplus_startup(&gdiplus_token, &input, NULL) == 0;
}

static void *load_png(const char *path) {
    WCHAR wide_path[MAX_PATH];
    void *image = NULL;
    if (!ensure_gdiplus()) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wide_path, MAX_PATH);
    if (gdip_create_bitmap_from_file(wide_path, &image) != 0 || !image) return NULL;
    return image;
}

static void *load_texture(void) {
    return load_png(OCEAN_TEXTURE_PATH);
}

static void *texture(void) {
    if (!texture_attempted) {
        texture_image = load_texture();
        texture_attempted = 1;
    }
    return texture_image;
}

int ocean_assets_texture_ready(void) {
    return texture() != NULL;
}

int ocean_assets_texture_loaded(void) {
    return texture_attempted && texture_image != NULL;
}

static int draw_texture_tiled(HDC hdc, RECT rect, int tile_px, void *image) {
    void *graphics = NULL;
    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;
    int x, y, saved_dc;
    int ok = 1;
    if (!image || w <= 0 || h <= 0 || tile_px <= 0) return 0;
    saved_dc = SaveDC(hdc);
    IntersectClipRect(hdc, rect.left, rect.top, rect.right, rect.bottom);
    if (gdip_create_from_hdc(hdc, &graphics) != 0 || !graphics) {
        RestoreDC(hdc, saved_dc);
        return 0;
    }
    x = rect.left - tile_px;
    while (x + tile_px < rect.left) x += tile_px;
    y = rect.top - tile_px;
    while (y + tile_px < rect.top) y += tile_px;
    for (; y < rect.bottom; y += tile_px) {
        for (x = rect.left - tile_px;
             x + tile_px < rect.left; x += tile_px) {
        }
        for (; x < rect.right; x += tile_px) {
            if (gdip_draw_image_rect_i(graphics, image, x, y,
                                       tile_px, tile_px) != 0) ok = 0;
        }
    }
    gdip_delete_graphics(graphics);
    RestoreDC(hdc, saved_dc);
    return ok;
}

int ocean_assets_draw_texture_tiled(HDC hdc, RECT rect, int tile_px) {
    return draw_texture_tiled(hdc, rect, tile_px, texture());
}

int ocean_assets_draw_texture_tiled_loaded(HDC hdc, RECT rect, int tile_px) {
    return draw_texture_tiled(
        hdc, rect, tile_px,
        ocean_assets_texture_loaded() ? texture_image : NULL);
}

int ocean_assets_draw_texture(HDC hdc, RECT rect) {
    return ocean_assets_draw_texture_tiled(hdc, rect, OCEAN_TEXTURE_TILE_PX);
}

int ocean_assets_texture_tile_px(void) { return OCEAN_TEXTURE_TILE_PX; }

static int yes_field(const char *s) {
    return s && (strcmp(s, "yes") == 0 || strcmp(s, "1") == 0 || strcmp(s, "true") == 0);
}

static void trim_line(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) s[--n] = '\0';
}

static int parse_manifest_row(char *line) {
    char *fields[14];
    int i;
    OceanMotifAsset *asset;
    if (motif_count >= OCEAN_MOTIF_ASSET_MAX) return 0;
    trim_line(line);
    fields[0] = strtok(line, "\t");
    for (i = 1; i < 14; i++) fields[i] = strtok(NULL, "\t");
    if (!fields[0] || !fields[1] || strcmp(fields[0], "id") == 0) return 0;
    asset = &motif_assets[motif_count];
    memset(asset, 0, sizeof(*asset));
    strncpy(asset->id, fields[0], sizeof(asset->id) - 1);
    snprintf(asset->path, sizeof(asset->path), "%s%s", OCEAN_ASSET_BASE, fields[1]);
    for (i = 0; asset->path[i]; i++) {
        if (asset->path[i] == '/') asset->path[i] = '\\';
    }
    asset->info.id = asset->id;
    asset->info.default_w = fields[2] ? atoi(fields[2]) : 96;
    asset->info.default_h = fields[3] ? atoi(fields[3]) : 80;
    asset->info.footprint_w = fields[4] ? atoi(fields[4]) : asset->info.default_w;
    asset->info.footprint_h = fields[5] ? atoi(fields[5]) : asset->info.default_h;
    asset->info.min_coast_clear_tiles = fields[6] ? atoi(fields[6]) : 6;
    asset->info.min_label_clear_px = fields[7] ? atoi(fields[7]) : 72;
    asset->info.allow_interior = yes_field(fields[8]);
    asset->info.allow_exterior = yes_field(fields[9]);
    asset->info.weight = fields[10] ? atoi(fields[10]) : 1;
    asset->info.mirror_allowed = yes_field(fields[12]);
    if (asset->info.weight < 1) asset->info.weight = 1;
    if (asset->info.default_w < 12) asset->info.default_w = 12;
    if (asset->info.default_h < 12) asset->info.default_h = 12;
    motif_count++;
    return 1;
}

static void ensure_manifest(void) {
    FILE *file;
    char line[512];
    if (manifest_attempted) return;
    manifest_attempted = 1;
    file = fopen(OCEAN_MOTIF_MANIFEST, "r");
    if (!file) return;
    while (fgets(line, sizeof(line), file)) parse_manifest_row(line);
    fclose(file);
    manifest_loaded = motif_count > 0;
}

int ocean_assets_motif_count(void) {
    ensure_manifest();
    return motif_count;
}

int ocean_assets_manifest_loaded(void) {
    return manifest_attempted && manifest_loaded;
}

int ocean_assets_motif_count_loaded(void) {
    return motif_count;
}

const OceanMotifAssetInfo *ocean_assets_motif_info(int index) {
    ensure_manifest();
    if (index < 0 || index >= motif_count) return NULL;
    return &motif_assets[index].info;
}

const OceanMotifAssetInfo *ocean_assets_motif_info_loaded(int index) {
    if (!manifest_attempted || index < 0 || index >= motif_count) return NULL;
    return &motif_assets[index].info;
}

static void *motif_image(int index) {
    OceanMotifAsset *asset;
    ensure_manifest();
    if (index < 0 || index >= motif_count) return NULL;
    asset = &motif_assets[index];
    if (!asset->load_attempted) {
        asset->image = load_png(asset->path);
        asset->load_attempted = 1;
    }
    return asset->image;
}

int ocean_assets_motifs_ready(void) {
    int i;
    ensure_manifest();
    if (!manifest_loaded) return 0;
    for (i = 0; i < motif_count; i++) {
        if (!motif_image(i)) return 0;
    }
    return 1;
}

int ocean_assets_motifs_loaded(void) {
    int i;
    if (!manifest_attempted || !manifest_loaded) return 0;
    for (i = 0; i < motif_count; i++) {
        if (!motif_assets[i].load_attempted || !motif_assets[i].image) return 0;
    }
    return 1;
}

int ocean_assets_draw_motif(HDC hdc, int index, RECT dst) {
    void *graphics = NULL;
    void *image = motif_image(index);
    int ok;
    if (!image || dst.right <= dst.left || dst.bottom <= dst.top) return 0;
    if (gdip_create_from_hdc(hdc, &graphics) != 0 || !graphics) return 0;
    ok = gdip_draw_image_rect_i(graphics, image, dst.left, dst.top,
                                dst.right - dst.left,
                                dst.bottom - dst.top) == 0;
    gdip_delete_graphics(graphics);
    return ok;
}
