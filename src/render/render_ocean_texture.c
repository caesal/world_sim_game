#include "render/render_ocean_texture.h"

#include "render/render_layer_cache.h"
#include "render/render_ocean_assets.h"
#include <string.h>

#define OCEAN_TEXTURE_SOURCE_FALLBACK 0x46414c4cu
#define OCEAN_TEXTURE_SOURCE_PNG 0x504e4701u

static LayerCache ocean_texture_cache;
OceanTextureDebugStats ocean_texture_debug_stats;
static uint64_t ocean_texture_generation_value;
static unsigned int ocean_texture_source_identity;
static int ocean_texture_score_value;
static int raster_failed_width;
static int raster_failed_height;

static COLORREF ocean_texture_base_color(void) {
    return RGB(55, 135, 199);
}

static int fill_rect_color(HDC hdc, RECT rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    int ok;
    if (!brush) return 0;
    ok = FillRect(hdc, &rect, brush);
    DeleteObject(brush);
    return ok;
}

static unsigned int texture_key(int width, int height,
                                unsigned int source_identity) {
    unsigned int key = 2166136261u;
    key = render_layer_mix_key(key, width);
    key = render_layer_mix_key(key, height);
    return render_layer_mix_key(key, (int)source_identity);
}

static int texture_cache_matches(int width, int height, unsigned int key) {
    return ocean_texture_cache.valid && ocean_texture_cache.key == key &&
           ocean_texture_cache.width == width &&
           ocean_texture_cache.height == height;
}

static void refresh_debug_state(void) {
    RenderLayerCacheMemory memory =
        render_layer_cache_memory(&ocean_texture_cache);
    ocean_texture_debug_stats.generation =
        ocean_texture_generation_value;
    ocean_texture_debug_stats.identity =
        ocean_texture_cache.valid ? ocean_texture_cache.key : 0u;
    ocean_texture_debug_stats.source_identity =
        ocean_texture_source_identity;
    ocean_texture_debug_stats.valid = ocean_texture_cache.valid;
    ocean_texture_debug_stats.width = ocean_texture_cache.width;
    ocean_texture_debug_stats.height = ocean_texture_cache.height;
    ocean_texture_debug_stats.tile_px = ocean_assets_texture_tile_px();
    ocean_texture_debug_stats.phase_x = 0;
    ocean_texture_debug_stats.phase_y = 0;
    ocean_texture_debug_stats.texture_loaded =
        ocean_texture_source_identity == OCEAN_TEXTURE_SOURCE_PNG;
    ocean_texture_debug_stats.persistent_bitmaps = memory.bitmaps;
    ocean_texture_debug_stats.persistent_dcs = memory.dcs;
    ocean_texture_debug_stats.persistent_bytes = memory.bitmap_bytes;
    ocean_texture_debug_stats.bitmap_identity =
        (uintptr_t)ocean_texture_cache.bitmap;
    ocean_texture_debug_stats.dc_identity =
        (uintptr_t)ocean_texture_cache.dc;
}

int render_ocean_texture_ensure(HDC hdc, RECT client) {
    MapLayout empty_layout = {0};
    RECT surface;
    unsigned int source_identity;
    unsigned int key;
    int width = client.right - client.left;
    int height = client.bottom - client.top;
    int needs_allocation;
    int texture_loaded;

    ocean_texture_debug_stats.ensure_calls++;
    if (!hdc || width <= 0 || height <= 0) return 0;
    texture_loaded = ocean_assets_texture_ready();
    source_identity = texture_loaded &&
                      (raster_failed_width != width ||
                       raster_failed_height != height)
                          ? OCEAN_TEXTURE_SOURCE_PNG
                          : OCEAN_TEXTURE_SOURCE_FALLBACK;
    key = texture_key(width, height, source_identity);
    if (texture_cache_matches(width, height, key)) {
        ocean_texture_debug_stats.cache_hits++;
        refresh_debug_state();
        return 1;
    }

    needs_allocation = !ocean_texture_cache.dc ||
                       ocean_texture_cache.width != width ||
                       ocean_texture_cache.height != height;
    if (!render_layer_cache_ensure(hdc, &ocean_texture_cache, client,
                                   empty_layout, 0, 0)) {
        refresh_debug_state();
        return 0;
    }
    if (needs_allocation) ocean_texture_debug_stats.allocation_rebuilds++;
    ocean_texture_cache.valid = 0;

    surface.left = 0;
    surface.top = 0;
    surface.right = width;
    surface.bottom = height;
    if (!fill_rect_color(ocean_texture_cache.dc, surface,
                         ocean_texture_base_color())) {
        refresh_debug_state();
        return 0;
    }
    if (source_identity == OCEAN_TEXTURE_SOURCE_PNG &&
        ocean_assets_draw_texture_tiled_loaded(
            ocean_texture_cache.dc, surface,
            ocean_assets_texture_tile_px())) {
        raster_failed_width = raster_failed_height = 0;
        ocean_texture_score_value = 900;
    } else {
        if (source_identity == OCEAN_TEXTURE_SOURCE_PNG) {
            raster_failed_width = width;
            raster_failed_height = height;
        }
        source_identity = OCEAN_TEXTURE_SOURCE_FALLBACK;
        key = texture_key(width, height, source_identity);
        if (!fill_rect_color(ocean_texture_cache.dc, surface,
                             ocean_texture_base_color())) {
            refresh_debug_state();
            return 0;
        }
        ocean_texture_score_value = 330;
    }
    ocean_texture_cache.key = key;
    ocean_texture_cache.valid = 1;
    ocean_texture_source_identity = source_identity;
    ocean_texture_generation_value++;
    ocean_texture_debug_stats.rebuilds++;
    refresh_debug_state();
    return 1;
}

int render_ocean_texture_copy(HDC hdc, RECT rect) {
    RECT bounds;
    RECT clipped;
    int width;
    int height;
    BOOL copied;

    ocean_texture_debug_stats.copy_calls++;
    if (!hdc || !ocean_texture_cache.valid || !ocean_texture_cache.dc) {
        ocean_texture_debug_stats.copy_failures++;
        return 0;
    }
    bounds.left = 0;
    bounds.top = 0;
    bounds.right = ocean_texture_cache.width;
    bounds.bottom = ocean_texture_cache.height;
    if (!IntersectRect(&clipped, &rect, &bounds)) {
        ocean_texture_debug_stats.copy_failures++;
        return 0;
    }
    width = clipped.right - clipped.left;
    height = clipped.bottom - clipped.top;
    copied = BitBlt(hdc, clipped.left, clipped.top, width, height,
                    ocean_texture_cache.dc, clipped.left, clipped.top,
                    SRCCOPY);
    if (!copied) {
        ocean_texture_debug_stats.copy_failures++;
        return 0;
    }
    ocean_texture_debug_stats.copied_pixels +=
        (uint64_t)width * (uint64_t)height;
    return 1;
}

int render_ocean_texture_score(void) {
    return ocean_texture_score_value;
}

RenderLayerCacheMemory render_ocean_texture_memory(void) {
    return render_layer_cache_memory(&ocean_texture_cache);
}

OceanTextureDebugStats render_ocean_texture_debug_stats(void) {
    refresh_debug_state();
    return ocean_texture_debug_stats;
}

uint64_t render_ocean_texture_generation(void) {
    return ocean_texture_generation_value;
}

unsigned int render_ocean_texture_identity(void) {
    return ocean_texture_cache.valid ? ocean_texture_cache.key : 0u;
}

void render_ocean_texture_reset_debug(void) {
    memset(&ocean_texture_debug_stats, 0,
           sizeof(ocean_texture_debug_stats));
    refresh_debug_state();
}

void render_ocean_texture_invalidate(void) {
    ocean_texture_cache.valid = 0;
    ocean_texture_cache.key = 0;
    ocean_texture_source_identity = 0;
    ocean_texture_score_value = 0;
    raster_failed_width = raster_failed_height = 0;
    refresh_debug_state();
}
