#include "render/render_layer_cache.h"
#include "core/profiler.h"
#include "render/render_allocation_diagnostics.h"
#include "ui/ui_layout.h"
#include <string.h>

static void release_layer_cache(LayerCache *cache) {
    if (cache->dc && cache->old_bitmap &&
        (HGDIOBJ)cache->old_bitmap != HGDI_ERROR)
        SelectObject(cache->dc, cache->old_bitmap);
    if (cache->bitmap) DeleteObject(cache->bitmap);
    if (cache->dc) DeleteDC(cache->dc);
    memset(cache, 0, sizeof(*cache));
}

unsigned int render_layer_mix_key(unsigned int key, int value) {
    return key * 1000003u ^ (unsigned int)value;
}

unsigned int render_layer_layout_key(RECT client, MapLayout layout, int side_w, int display) {
    unsigned int key = 2166136261u;
    key = render_layer_mix_key(key, client.right - client.left);
    key = render_layer_mix_key(key, client.bottom - client.top);
    key = render_layer_mix_key(key, layout.map_x);
    key = render_layer_mix_key(key, layout.map_y);
    key = render_layer_mix_key(key, layout.draw_w);
    key = render_layer_mix_key(key, layout.draw_h);
    key = render_layer_mix_key(key, side_w);
    return render_layer_mix_key(key, display);
}

int render_layer_cache_ensure(HDC hdc, LayerCache *cache, RECT client, MapLayout layout,
                              int side_w, int display) {
    int width = client.right - client.left;
    int height = client.bottom - client.top;
    if (width <= 0 || height <= 0) return 0;
    if (!cache->dc || cache->width != width || cache->height != height) {
        release_layer_cache(cache);
        render_allocation_note_attempt(RENDER_ALLOCATION_LAYER_CACHE, width, height);
        if (render_allocation_inject_failure(RENDER_ALLOCATION_LAYER_CACHE,
                                             width, height)) return 0;
        cache->dc = CreateCompatibleDC(hdc);
        cache->bitmap = CreateCompatibleBitmap(hdc, width, height);
        if (!cache->dc || !cache->bitmap) {
            render_allocation_note_failure(RENDER_ALLOCATION_LAYER_CACHE, width, height);
            release_layer_cache(cache);
            return 0;
        }
        profiler_add_gdi_recreate();
        cache->old_bitmap = SelectObject(cache->dc, cache->bitmap);
        if (!cache->old_bitmap ||
            (HGDIOBJ)cache->old_bitmap == HGDI_ERROR) {
            cache->old_bitmap = NULL;
            render_allocation_note_failure(RENDER_ALLOCATION_LAYER_CACHE,
                                           width, height);
            release_layer_cache(cache);
            return 0;
        }
        cache->width = width;
        cache->height = height;
    }
    cache->map_x = layout.map_x;
    cache->map_y = layout.map_y;
    cache->draw_w = layout.draw_w;
    cache->draw_h = layout.draw_h;
    cache->side_w = side_w;
    cache->display = display;
    cache->valid = 1;
    return 1;
}

int render_layer_cache_matches(const LayerCache *cache, RECT client, MapLayout layout,
                               unsigned int key, int display) {
    int width = client.right - client.left;
    int height = client.bottom - client.top;
    return cache->valid && cache->key == key && cache->width == width &&
           cache->height == height && cache->map_x == layout.map_x &&
           cache->map_y == layout.map_y && cache->draw_w == layout.draw_w &&
           cache->draw_h == layout.draw_h && cache->display == display;
}

int render_layer_cache_content_matches(const LayerCache *cache, RECT client,
                                       unsigned int key, int display) {
    int width = client.right - client.left;
    int height = client.bottom - client.top;
    return cache && cache->valid && cache->key == key &&
           cache->width == width && cache->height == height &&
           cache->display == display;
}

int render_layer_cache_preview_presentable(const LayerCache *cache, RECT client, int display) {
    int width = client.right - client.left, height = client.bottom - client.top;
    return cache->valid && cache->width == width && cache->height == height &&
           cache->display == display;
}

void render_layer_cache_blit_viewport(HDC hdc, RECT client, const LayerCache *cache) {
    RECT viewport = get_map_viewport_rect(client);
    BitBlt(hdc, viewport.left, viewport.top, viewport.right - viewport.left,
           viewport.bottom - viewport.top, cache->dc, viewport.left, viewport.top, SRCCOPY);
}

void render_layer_cache_clear_transparent(LayerCache *cache) {
    RECT rect = {0, 0, cache->width, cache->height};
    HBRUSH brush = CreateSolidBrush(RGB(255, 0, 255));
    FillRect(cache->dc, &rect, brush);
    DeleteObject(brush);
}

void render_layer_cache_transparent_viewport(HDC hdc, RECT client, const LayerCache *cache) {
    RECT viewport = get_map_viewport_rect(client);
    TransparentBlt(hdc, viewport.left, viewport.top, viewport.right - viewport.left,
                   viewport.bottom - viewport.top, cache->dc, viewport.left, viewport.top,
                   viewport.right - viewport.left, viewport.bottom - viewport.top,
                   RGB(255, 0, 255));
}

RenderLayerCacheMemory render_layer_cache_memory(const LayerCache *cache) {
    RenderLayerCacheMemory memory = {0};
    if (!cache) return memory;
    if (cache->bitmap) {
        memory.bitmaps = 1;
        memory.bitmap_bytes =
            (uint64_t)max(0, cache->width) * (uint64_t)max(0, cache->height) * 4u;
    }
    if (cache->dc) memory.dcs = 1;
    return memory;
}

void render_layer_cache_memory_add(RenderLayerCacheMemory *total,
                                   RenderLayerCacheMemory value) {
    if (!total) return;
    total->bitmaps += value.bitmaps;
    total->dcs += value.dcs;
    total->bitmap_bytes += value.bitmap_bytes;
}
