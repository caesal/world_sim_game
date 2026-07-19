#include "render/render_static_map_surface.h"

#include "core/profiler.h"
#include "render/render_common.h"
#include "render/render_allocation_diagnostics.h"

#include <string.h>

void render_static_map_surface_release(MapLayerCache *cache) {
    if (!cache) return;
    if (cache->dc && cache->old_bitmap &&
        (HGDIOBJ)cache->old_bitmap != HGDI_ERROR)
        SelectObject(cache->dc, cache->old_bitmap);
    if (cache->bitmap) DeleteObject(cache->bitmap);
    if (cache->dc) DeleteDC(cache->dc);
    memset(cache, 0, sizeof(*cache));
}

int render_static_map_surface_ensure(HDC hdc, MapLayerCache *cache,
                                     int width, int height) {
    BITMAPINFO info;
    if (!hdc || !cache || width <= 0 || height <= 0) return 0;
    if (cache->dc && cache->bitmap && cache->pixels &&
        cache->width == width && cache->height == height) return 1;
    render_static_map_surface_release(cache);
    render_allocation_note_attempt(RENDER_ALLOCATION_STATIC_MAP, width, height);
    if (render_allocation_inject_failure(RENDER_ALLOCATION_STATIC_MAP,
                                         width, height)) return 0;
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    cache->dc = CreateCompatibleDC(hdc);
    cache->bitmap = CreateDIBSection(hdc, &info, DIB_RGB_COLORS,
                                     (void **)&cache->pixels, NULL, 0);
    if (!cache->dc || !cache->bitmap || !cache->pixels) {
        render_allocation_note_failure(RENDER_ALLOCATION_STATIC_MAP, width, height);
        render_static_map_surface_release(cache);
        return 0;
    }
    profiler_add_gdi_recreate();
    cache->old_bitmap = SelectObject(cache->dc, cache->bitmap);
    if (!cache->old_bitmap || (HGDIOBJ)cache->old_bitmap == HGDI_ERROR) {
        cache->old_bitmap = NULL;
        render_allocation_note_failure(RENDER_ALLOCATION_STATIC_MAP,
                                       width, height);
        render_static_map_surface_release(cache);
        return 0;
    }
    cache->width = width;
    cache->height = height;
    return 1;
}

int render_static_map_surface_matches(const MapLayerCache *cache, int width, int height,
                                      int revision, int display_key) {
    return cache && cache->valid && cache->width == width && cache->height == height &&
           cache->display == display_key && cache->revision == revision;
}

int render_static_map_surface_presentable(const MapLayerCache *cache, int width, int height,
                                          int display_key) {
    return cache && cache->valid && cache->width == width && cache->height == height &&
           cache->display == display_key;
}

void render_static_map_surface_mark_valid(MapLayerCache *cache, int revision,
                                          int display_key, int complete) {
    if (!cache) return;
    cache->revision = revision;
    cache->display = display_key;
    cache->valid = 1;
    cache->complete = complete;
}

void render_static_map_surface_alpha(HDC destination, const MapLayerCache *source) {
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    if (!destination || !source || !source->valid) return;
    AlphaBlend(destination, 0, 0, source->width, source->height,
               source->dc, 0, 0, source->width, source->height, blend);
}

int render_static_map_surface_categorical_stretch_mode(void) {
    return COLORONCOLOR;
}

static int begin_present(HDC destination, RECT client, int stretch_mode) {
    RECT content = get_map_content_rect(client);
    int saved = SaveDC(destination);
    IntersectClipRect(destination, content.left, content.top, content.right, content.bottom);
    SetStretchBltMode(destination, stretch_mode);
    return saved;
}

void render_static_map_surface_present(HDC destination, RECT client, MapLayout layout,
                                       const MapLayerCache *cache, int stretch_mode) {
    int saved;
    if (!destination || !cache || !cache->valid) return;
    saved = begin_present(destination, client, stretch_mode);
    StretchBlt(destination, layout.map_x, layout.map_y, layout.draw_w, layout.draw_h,
               cache->dc, 0, 0, cache->width, cache->height, SRCCOPY);
    RestoreDC(destination, saved);
}

void render_static_map_surface_present_alpha(HDC destination, RECT client,
                                             MapLayout layout,
                                             const MapLayerCache *cache,
                                             int stretch_mode) {
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    int saved;
    if (!destination || !cache || !cache->valid) return;
    saved = begin_present(destination, client, stretch_mode);
    AlphaBlend(destination, layout.map_x, layout.map_y,
               layout.draw_w, layout.draw_h,
               cache->dc, 0, 0, cache->width, cache->height, blend);
    RestoreDC(destination, saved);
}

void render_static_map_surface_present_transparent(HDC destination, RECT client,
                                                   MapLayout layout,
                                                   const MapLayerCache *cache,
                                                   int stretch_mode, COLORREF key) {
    int saved;
    if (!destination || !cache || !cache->valid) return;
    saved = begin_present(destination, client, stretch_mode);
    TransparentBlt(destination, layout.map_x, layout.map_y, layout.draw_w, layout.draw_h,
                   cache->dc, 0, 0, cache->width, cache->height, key);
    RestoreDC(destination, saved);
}
