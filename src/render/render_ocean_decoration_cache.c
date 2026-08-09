#include "render/render_ocean_decoration_cache.h"

#include "core/profiler.h"
#include "render/render_allocation_diagnostics.h"

#include <string.h>

static OceanDecorationLayer exterior_layer;
static OceanDecorationLayer interior_layer;
static OceanDecorationCacheStats stats;
static unsigned int surface_identity_sequence;

static OceanDecorationLayer *layer_for(OceanDecorationLayerKind kind) {
    return kind == OCEAN_DECORATION_LAYER_EXTERIOR
               ? &exterior_layer : &interior_layer;
}

static void release_layer(OceanDecorationLayer *layer) {
    if (layer->dc && layer->old_bitmap &&
        (HGDIOBJ)layer->old_bitmap != HGDI_ERROR)
        SelectObject(layer->dc, layer->old_bitmap);
    if (layer->bitmap) DeleteObject(layer->bitmap);
    if (layer->dc) DeleteDC(layer->dc);
    memset(layer, 0, sizeof(*layer));
}

OceanDecorationLayer *ocean_decoration_cache_find(
    OceanDecorationLayerKind kind, int width, int height, unsigned int key) {
    OceanDecorationLayer *layer = layer_for(kind);
    if (layer->valid && layer->width == width && layer->height == height &&
        layer->key == key) {
        stats.hits++;
        return layer;
    }
    stats.misses++;
    return NULL;
}

OceanDecorationLayer *ocean_decoration_cache_prepare(
    HDC hdc, OceanDecorationLayerKind kind, int width, int height) {
    BITMAPINFO info;
    OceanDecorationLayer *layer = layer_for(kind);
    if (!hdc || width <= 0 || height <= 0) return NULL;
    if (layer->dc && layer->bitmap && layer->pixels &&
        layer->width == width && layer->height == height) {
        layer->valid = 0;
        return layer;
    }
    release_layer(layer);
    render_allocation_note_attempt(RENDER_ALLOCATION_LAYER_CACHE, width, height);
    if (render_allocation_inject_failure(RENDER_ALLOCATION_LAYER_CACHE,
                                         width, height)) return NULL;
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    layer->dc = CreateCompatibleDC(hdc);
    layer->bitmap = CreateDIBSection(hdc, &info, DIB_RGB_COLORS,
                                     (void **)&layer->pixels, NULL, 0);
    if (!layer->dc || !layer->bitmap || !layer->pixels) {
        render_allocation_note_failure(RENDER_ALLOCATION_LAYER_CACHE,
                                       width, height);
        release_layer(layer);
        return NULL;
    }
    profiler_add_gdi_recreate();
    layer->old_bitmap = SelectObject(layer->dc, layer->bitmap);
    if (!layer->old_bitmap || (HGDIOBJ)layer->old_bitmap == HGDI_ERROR) {
        layer->old_bitmap = NULL;
        render_allocation_note_failure(RENDER_ALLOCATION_LAYER_CACHE,
                                       width, height);
        release_layer(layer);
        return NULL;
    }
    layer->width = width;
    layer->height = height;
    surface_identity_sequence++;
    if (!surface_identity_sequence) surface_identity_sequence++;
    layer->surface_identity = surface_identity_sequence;
    if (kind == OCEAN_DECORATION_LAYER_EXTERIOR)
        stats.exterior_allocations++;
    else
        stats.interior_allocations++;
    return layer;
}

int ocean_decoration_cache_clear(OceanDecorationLayerKind kind,
                                 OceanDecorationLayer *layer) {
    uint64_t pixels;
    if (!layer || !layer->pixels || layer->width <= 0 || layer->height <= 0)
        return 0;
    pixels = (uint64_t)layer->width * (uint64_t)layer->height;
    memset(layer->pixels, 0, (size_t)pixels * sizeof(*layer->pixels));
    layer->valid = 0;
    if (kind == OCEAN_DECORATION_LAYER_EXTERIOR) {
        stats.exterior_clears++;
        stats.exterior_cleared_pixels += pixels;
    } else {
        stats.interior_clears++;
        stats.interior_cleared_pixels += pixels;
    }
    return 1;
}

void ocean_decoration_cache_commit(OceanDecorationLayerKind kind,
                                   OceanDecorationLayer *layer,
                                   unsigned int key) {
    if (!layer) return;
    layer->key = key;
    layer->valid = 1;
    if (kind == OCEAN_DECORATION_LAYER_EXTERIOR)
        stats.exterior_rebuilds++;
    else
        stats.interior_rebuilds++;
}

void ocean_decoration_cache_note_present(OceanDecorationLayerKind kind) {
    if (kind == OCEAN_DECORATION_LAYER_EXTERIOR)
        stats.exterior_presents++;
    else
        stats.interior_presents++;
}

void ocean_decoration_cache_invalidate(void) {
    exterior_layer.valid = 0;
    interior_layer.valid = 0;
}

void ocean_decoration_cache_reset_debug(void) {
    memset(&stats, 0, sizeof(stats));
}

const OceanDecorationCacheStats *ocean_decoration_cache_stats(void) {
    stats.persistent_bitmaps = (exterior_layer.bitmap ? 1 : 0) +
                               (interior_layer.bitmap ? 1 : 0);
    stats.persistent_dcs = (exterior_layer.dc ? 1 : 0) +
                          (interior_layer.dc ? 1 : 0);
    stats.exterior_retained_bytes = exterior_layer.bitmap
        ? (uint64_t)exterior_layer.width * exterior_layer.height * 4u : 0;
    stats.interior_retained_bytes = interior_layer.bitmap
        ? (uint64_t)interior_layer.width * interior_layer.height * 4u : 0;
    stats.persistent_bitmap_bytes = stats.exterior_retained_bytes +
                                    stats.interior_retained_bytes;
    stats.exterior_key = exterior_layer.valid ? exterior_layer.key : 0;
    stats.interior_key = interior_layer.valid ? interior_layer.key : 0;
    stats.exterior_surface_identity = exterior_layer.surface_identity;
    stats.interior_surface_identity = interior_layer.surface_identity;
    return &stats;
}
