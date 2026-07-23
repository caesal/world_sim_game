#include "render/worldgen_ui_surface_cache.h"

#include <string.h>

#define WORLDGEN_UI_SURFACE_CAPACITY 32
#define WORLDGEN_UI_SURFACE_KEY_COUNT 16

typedef struct {
    HDC memory_dc;
    HBITMAP bitmap;
    HGDIOBJ previous_bitmap;
    int asset_key;
    int variant_key;
    int width;
    int height;
    unsigned int last_used;
    int occupied;
} WorldgenUiSurfaceEntry;

static WorldgenUiSurfaceEntry entries[WORLDGEN_UI_SURFACE_CAPACITY];
static WorldgenUiSurfaceStats stats[WORLDGEN_UI_SURFACE_KEY_COUNT];
static unsigned int use_clock;

static int valid_key(int key) {
    return key >= 0 && key < WORLDGEN_UI_SURFACE_KEY_COUNT;
}

static void release_entry(WorldgenUiSurfaceEntry *entry) {
    if (!entry || !entry->occupied) return;
    if (entry->memory_dc && entry->previous_bitmap)
        SelectObject(entry->memory_dc, entry->previous_bitmap);
    if (entry->bitmap) DeleteObject(entry->bitmap);
    if (entry->memory_dc) DeleteDC(entry->memory_dc);
    if (valid_key(entry->asset_key)) stats[entry->asset_key].releases++;
    memset(entry, 0, sizeof(*entry));
}

static WorldgenUiSurfaceEntry *find_entry(int asset_key, int variant_key,
                                          int width, int height) {
    int i;
    for (i = 0; i < WORLDGEN_UI_SURFACE_CAPACITY; i++) {
        WorldgenUiSurfaceEntry *entry = &entries[i];
        if (entry->occupied && entry->asset_key == asset_key &&
            entry->variant_key == variant_key && entry->width == width &&
            entry->height == height) return entry;
    }
    return NULL;
}

static WorldgenUiSurfaceEntry *replacement_entry(void) {
    WorldgenUiSurfaceEntry *oldest = &entries[0];
    int i;
    for (i = 0; i < WORLDGEN_UI_SURFACE_CAPACITY; i++) {
        if (!entries[i].occupied) return &entries[i];
        if (entries[i].last_used < oldest->last_used) oldest = &entries[i];
    }
    release_entry(oldest);
    return oldest;
}

static int build_entry(WorldgenUiSurfaceEntry *entry, HDC target,
                       int asset_key, int variant_key, int width, int height,
                       WorldgenUiSurfaceRender render, void *context) {
    HDC memory_dc;
    HBITMAP bitmap;
    HGDIOBJ previous;
    if (!entry || !target || !render || width <= 0 || height <= 0) return 0;
    memory_dc = CreateCompatibleDC(target);
    if (!memory_dc) return 0;
    bitmap = CreateCompatibleBitmap(target, width, height);
    if (!bitmap) {
        DeleteDC(memory_dc);
        return 0;
    }
    previous = SelectObject(memory_dc, bitmap);
    if (!previous || previous == HGDI_ERROR) {
        DeleteObject(bitmap);
        DeleteDC(memory_dc);
        return 0;
    }
    PatBlt(memory_dc, 0, 0, width, height, BLACKNESS);
    if (!render(memory_dc, width, height, context)) {
        SelectObject(memory_dc, previous);
        DeleteObject(bitmap);
        DeleteDC(memory_dc);
        return 0;
    }
    entry->memory_dc = memory_dc;
    entry->bitmap = bitmap;
    entry->previous_bitmap = previous;
    entry->asset_key = asset_key;
    entry->variant_key = variant_key;
    entry->width = width;
    entry->height = height;
    entry->occupied = 1;
    if (valid_key(asset_key)) {
        stats[asset_key].builds++;
        stats[asset_key].allocations++;
    }
    return 1;
}

int worldgen_ui_surface_cache_draw(HDC target, int asset_key, int variant_key,
                                   RECT destination,
                                   WorldgenUiSurfaceRender render,
                                   void *context) {
    int width = destination.right - destination.left;
    int height = destination.bottom - destination.top;
    WorldgenUiSurfaceEntry *entry;
    if (!target || width <= 0 || height <= 0 || !valid_key(asset_key)) return 0;
    entry = find_entry(asset_key, variant_key, width, height);
    if (entry) {
        stats[asset_key].hits++;
    } else {
        entry = replacement_entry();
        if (!build_entry(entry, target, asset_key, variant_key, width, height,
                         render, context)) return 0;
    }
    use_clock++;
    if (!use_clock) use_clock = 1;
    entry->last_used = use_clock;
    return BitBlt(target, destination.left, destination.top, width, height,
                  entry->memory_dc, 0, 0, SRCCOPY) != 0;
}

void worldgen_ui_surface_cache_get_stats(int asset_key,
                                         WorldgenUiSurfaceStats *out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (valid_key(asset_key)) *out = stats[asset_key];
}

void worldgen_ui_surface_cache_release(void) {
    int i;
    for (i = 0; i < WORLDGEN_UI_SURFACE_CAPACITY; i++) release_entry(&entries[i]);
    use_clock = 0;
}

void worldgen_ui_surface_cache_reset_for_tests(void) {
    worldgen_ui_surface_cache_release();
    memset(stats, 0, sizeof(stats));
}
