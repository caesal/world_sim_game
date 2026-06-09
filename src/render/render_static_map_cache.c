#include "render/render_static_map_cache.h"

#include "core/dirty_flags.h"
#include "core/profiler.h"
#include "core/render_snapshot.h"
#include "core/render_snapshot_keys.h"
#include "render/render_context.h"
#include "render/river_render.h"
#include "render/render_static_map_cache_internal.h"
#include "render/snapshot_map_layers.h"
#include "render/render_static_map_cache_status.h"
#include "world/terrain_query.h"
#include <stdio.h>

static MapLayerCache physical_cache;
static MapLayerCache fill_cache;
static MapLayerCache coast_cache;
static MapLayerCache hydrology_cache;
static MapLayerCache border_cache;
static MapLayerCache static_map_cache;
static int cache_needs_work;

static int combined_revision(int a, int b) { return (a * 1000003) ^ b; }

static int cache_w(void) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    return max(1, (snapshot ? snapshot->map_w : DEFAULT_MAP_W) * MAP_LAYER_CACHE_SCALE);
}

static int cache_h(void) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    return max(1, (snapshot ? snapshot->map_h : DEFAULT_MAP_H) * MAP_LAYER_CACHE_SCALE);
}

static MapLayout cache_layout(const MapLayerCache *cache) {
    MapLayout layout = {0};
    layout.draw_w = cache->width; layout.draw_h = cache->height;
    layout.tile_size = MAP_LAYER_CACHE_SCALE; return layout;
}

static void release_cache(MapLayerCache *cache) {
    if (cache->dc && cache->old_bitmap) SelectObject(cache->dc, cache->old_bitmap);
    if (cache->bitmap) DeleteObject(cache->bitmap);
    if (cache->dc) DeleteDC(cache->dc);
    memset(cache, 0, sizeof(*cache));
}

static int ensure_cache(HDC hdc, MapLayerCache *cache) {
    BITMAPINFO info;
    int width = cache_w();
    int height = cache_h();
    if (cache->dc && cache->bitmap && cache->width == width && cache->height == height) return 1;
    release_cache(cache);
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
        release_cache(cache);
        return 0;
    }
    profiler_add_gdi_recreate();
    cache->old_bitmap = SelectObject(cache->dc, cache->bitmap);
    cache->width = width;
    cache->height = height;
    return 1;
}

static int cache_matches(const MapLayerCache *cache, int revision) {
    return cache->valid && cache->width == cache_w() && cache->height == cache_h() &&
           cache->display == display_mode && cache->revision == revision;
}

static int cache_presentable(const MapLayerCache *cache) {
    return cache && cache->valid && cache->width == cache_w() && cache->height == cache_h() &&
           cache->display == display_mode;
}

static void mark_cache_valid(MapLayerCache *cache, int revision, int complete) {
    cache->revision = revision;
    cache->display = display_mode;
    cache->valid = 1;
    cache->complete = complete;
}

static int display_requires_fill_layer(void) {
    return display_mode == DISPLAY_POLITICAL || display_mode == DISPLAY_ALL ||
           display_mode == DISPLAY_REGIONS;
}

static int cache_stretch_mode(MapLayout layout) {
    if (auto_run && world_generated && speed_index >= SPEED_COUNT - 1) return COLORONCOLOR;
    return layout.tile_size <= 2 ? HALFTONE : COLORONCOLOR;
}

static unsigned int pixel_from_color(COLORREF color, int alpha) {
    int r = GetRValue(color);
    int g = GetGValue(color);
    int b = GetBValue(color);
    alpha = clamp(alpha, 0, 255);
    if (alpha < 255) {
        r = r * alpha / 255;
        g = g * alpha / 255;
        b = b * alpha / 255;
    }
    return (unsigned int)b | ((unsigned int)g << 8) |
           ((unsigned int)r << 16) | ((unsigned int)alpha << 24);
}

static int snap_land(const SnapshotTile *tile) { return tile && is_land((Geography)tile->geography); }

static COLORREF water_color(const SnapshotTile *tile) {
    if (!tile || tile->water_depth == WATER_DEPTH_NONE) return RGB(38, 92, 154);
    return blend_color(RGB(92, 177, 214), RGB(38, 92, 154),
                       clamp(tile->water_deep_percent, 0, 100));
}

static COLORREF overview_color_snapshot(const SnapshotTile *tile) {
    COLORREF base;
    COLORREF climate;
    int blend;
    if (!tile) return RGB(38, 92, 154);
    base = snap_land(tile) ? geography_color((Geography)tile->geography) : water_color(tile);
    climate = climate_color((Climate)tile->climate);
    blend = snap_land(tile) ? 48 : 18;
    base = blend_color(base, climate, blend);
    if (!snap_land(tile)) return base;
    if (tile->elevation > 55) {
        return blend_color(base, RGB(38, 35, 32), clamp((tile->elevation - 55) / 3, 0, 18));
    }
    return blend_color(base, RGB(236, 230, 198), clamp((55 - tile->elevation) / 4, 0, 12));
}

static COLORREF physical_color(const SnapshotTile *tile) {
    if (!tile) return RGB(38, 92, 154);
    if (display_mode == DISPLAY_CLIMATE) return climate_color((Climate)tile->climate);
    if (display_mode == DISPLAY_GEOGRAPHY) {
        return snap_land(tile) ? geography_color((Geography)tile->geography) : water_color(tile);
    }
    return overview_color_snapshot(tile);
}

static void clear_pixels(MapLayerCache *cache, unsigned int value) {
    int total = cache->width * cache->height;
    int i;
    for (i = 0; i < total; i++) cache->pixels[i] = value;
}

static void fill_scaled_tile_pixels(MapLayerCache *cache, int tile_x, int tile_y,
                                    unsigned int value) {
    int x0 = tile_x * MAP_LAYER_CACHE_SCALE;
    int y0 = tile_y * MAP_LAYER_CACHE_SCALE;
    int y;
    int x;
    for (y = 0; y < MAP_LAYER_CACHE_SCALE && y0 + y < cache->height; y++) {
        unsigned int *row = &cache->pixels[(y0 + y) * cache->width + x0];
        for (x = 0; x < MAP_LAYER_CACHE_SCALE && x0 + x < cache->width; x++) {
            row[x] = value;
        }
    }
}

static void build_physical_pixels(MapLayerCache *cache, const RenderSnapshot *snapshot) {
    int x, y;
    for (y = 0; y < snapshot->map_h; y++) {
        for (x = 0; x < snapshot->map_w; x++) {
            const SnapshotTile *tile = &snapshot->tiles[y * snapshot->map_w + x];
            fill_scaled_tile_pixels(cache, x, y, pixel_from_color(physical_color(tile), 255));
        }
    }
}

static void build_fill_pixels(MapLayerCache *cache, const RenderSnapshot *snapshot) {
    int x, y;
    clear_pixels(cache, 0);
    if (display_mode != DISPLAY_POLITICAL && display_mode != DISPLAY_ALL &&
        display_mode != DISPLAY_REGIONS) return;
    for (y = 0; y < snapshot->map_h; y++) {
        for (x = 0; x < snapshot->map_w; x++) {
            const SnapshotTile *tile = &snapshot->tiles[y * snapshot->map_w + x];
            COLORREF color;
            int alpha;
            if (!snap_land(tile)) continue;
            if (display_mode == DISPLAY_REGIONS && tile->region_id >= 0) {
                int id = tile->region_id;
                color = RGB(92 + (id * 37) % 112, 105 + (id * 53) % 96, 86 + (id * 29) % 104);
                alpha = 96;
            } else if (tile->owner >= 0 && tile->owner < snapshot->civ_count &&
                       snapshot->civs[tile->owner].alive) {
                if (display_mode == DISPLAY_POLITICAL) {
                    color = soften_political_color((COLORREF)snapshot->civs[tile->owner].color);
                    alpha = POLITICAL_FILL_ALPHA;
                } else {
                    color = (COLORREF)snapshot->civs[tile->owner].color;
                    alpha = 112;
                }
            } else continue;
            fill_scaled_tile_pixels(cache, x, y, pixel_from_color(color, alpha));
        }
    }
}

static int rebuild_overlay_layer(HDC hdc, MapLayerCache *cache,
                                 void (*draw_extra)(HDC, RECT, MapLayout)) {
    RECT rect;
    if (!ensure_cache(hdc, cache)) return 0;
    rect = (RECT){0, 0, cache->width, cache->height};
    clear_pixels(cache, pixel_from_color(MAP_TRANSPARENT_KEY, 255));
    if (draw_extra) draw_extra(cache->dc, rect, cache_layout(cache));
    cache->display = display_mode;
    cache->valid = 1;
    return 1;
}

static void alpha_cache(HDC dst, const MapLayerCache *src) {
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    AlphaBlend(dst, 0, 0, src->width, src->height,
               src->dc, 0, 0, src->width, src->height, blend);
}

static int compose_static_map(HDC hdc, int revision) {
    RECT rect;
    if (!physical_cache.valid || !ensure_cache(hdc, &static_map_cache)) return 0;
    rect = (RECT){0, 0, static_map_cache.width, static_map_cache.height};
    FillRect(static_map_cache.dc, &rect, GetStockObject(BLACK_BRUSH));
    BitBlt(static_map_cache.dc, 0, 0, static_map_cache.width, static_map_cache.height,
           physical_cache.dc, 0, 0, SRCCOPY);
    if (fill_cache.valid) alpha_cache(static_map_cache.dc, &fill_cache);
    if (coast_cache.valid) TransparentBlt(static_map_cache.dc, 0, 0, static_map_cache.width, static_map_cache.height,
                                          coast_cache.dc, 0, 0, coast_cache.width, coast_cache.height,
                                          MAP_TRANSPARENT_KEY);
    if (hydrology_cache.valid) TransparentBlt(static_map_cache.dc, 0, 0, static_map_cache.width, static_map_cache.height,
                                              hydrology_cache.dc, 0, 0, hydrology_cache.width, hydrology_cache.height,
                                              MAP_TRANSPARENT_KEY);
    if (border_cache.valid) TransparentBlt(static_map_cache.dc, 0, 0, static_map_cache.width, static_map_cache.height,
                                           border_cache.dc, 0, 0, border_cache.width, border_cache.height,
                                           MAP_TRANSPARENT_KEY);
    mark_cache_valid(&static_map_cache, revision, 1);
    return 1;
}

static void present_cache(HDC hdc, RECT client, MapLayout layout, const MapLayerCache *cache) {
    RECT content = get_map_content_rect(client);
    int saved_dc;
    fill_rect(hdc, get_map_viewport_rect(client), RGB(79, 160, 215));
    if (!cache->valid) return;
    saved_dc = SaveDC(hdc);
    IntersectClipRect(hdc, content.left, content.top, content.right, content.bottom);
    SetStretchBltMode(hdc, cache_stretch_mode(layout));
    StretchBlt(hdc, layout.map_x, layout.map_y, layout.draw_w, layout.draw_h,
               cache->dc, 0, 0, cache->width, cache->height, SRCCOPY);
    RestoreDC(hdc, saved_dc);
}

static void present_partial_static_cache(HDC hdc, RECT client, MapLayout layout,
                                         int fill_key, int coast_key, int hydro_key, int border_key) {
    RECT content = get_map_content_rect(client);
    int saved_dc;
    fill_rect(hdc, get_map_viewport_rect(client), RGB(79, 160, 215));
    saved_dc = SaveDC(hdc);
    IntersectClipRect(hdc, content.left, content.top, content.right, content.bottom);
    SetStretchBltMode(hdc, cache_stretch_mode(layout));
    StretchBlt(hdc, layout.map_x, layout.map_y, layout.draw_w, layout.draw_h,
               physical_cache.dc, 0, 0, physical_cache.width, physical_cache.height, SRCCOPY);
    if (cache_matches(&fill_cache, fill_key)) {
        BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        AlphaBlend(hdc, layout.map_x, layout.map_y, layout.draw_w, layout.draw_h,
                   fill_cache.dc, 0, 0, fill_cache.width, fill_cache.height, blend);
    }
    if (cache_matches(&coast_cache, coast_key)) {
        TransparentBlt(hdc, layout.map_x, layout.map_y, layout.draw_w, layout.draw_h,
                       coast_cache.dc, 0, 0, coast_cache.width, coast_cache.height,
                       MAP_TRANSPARENT_KEY);
    }
    if (cache_matches(&hydrology_cache, hydro_key)) {
        TransparentBlt(hdc, layout.map_x, layout.map_y, layout.draw_w, layout.draw_h,
                       hydrology_cache.dc, 0, 0, hydrology_cache.width, hydrology_cache.height,
                       MAP_TRANSPARENT_KEY);
    }
    if (cache_matches(&border_cache, border_key)) {
        TransparentBlt(hdc, layout.map_x, layout.map_y, layout.draw_w, layout.draw_h,
                       border_cache.dc, 0, 0, border_cache.width, border_cache.height,
                       MAP_TRANSPARENT_KEY);
    }
    RestoreDC(hdc, saved_dc);
}

static int physical_revision(int tile_key) { return combined_revision(tile_key, display_mode); }

static int fill_revision(int tile_key, int region_key, int civ_key) {
    int key = combined_revision(tile_key, region_key);
    key = combined_revision(key, civ_key);
    return combined_revision(key, display_mode);
}

static int border_revision(int tile_key, int region_key) { return combined_revision(tile_key, region_key); }

static int static_revision(int physical_key, int fill_key, int coast_key,
                           int hydro_key, int border_key) {
    return combined_revision(combined_revision(physical_key, fill_key),
                             combined_revision(coast_key, combined_revision(hydro_key, border_key)));
}

static void draw_blank(HDC hdc, RECT client, MapLayout layout) {
    RECT map_rect = {layout.map_x, layout.map_y, layout.map_x + layout.draw_w, layout.map_y + layout.draw_h};
    fill_rect(hdc, get_map_viewport_rect(client), RGB(79, 160, 215));
    fill_rect(hdc, map_rect, RGB(64, 133, 178));
}

static int static_layers_ready(void) {
    return physical_cache.valid && fill_cache.valid && coast_cache.valid && hydrology_cache.valid && border_cache.valid;
}

static int static_cache_work_pending(int physical_key, int fill_key, int coast_key,
                                     int hydro_key, int border_key, int static_key) {
    return dirty_render_terrain() || dirty_render_political() ||
           dirty_render_coast() || dirty_render_hydrology() ||
           dirty_render_borders() ||
           !cache_matches(&physical_cache, physical_key) ||
           !cache_matches(&fill_cache, fill_key) ||
           !cache_matches(&coast_cache, coast_key) ||
           !cache_matches(&hydrology_cache, hydro_key) ||
           !cache_matches(&border_cache, border_key) ||
           !cache_matches(&static_map_cache, static_key);
}

static const MapLayerCache *best_present_cache(int static_key) {
    if (static_map_cache.complete && cache_matches(&static_map_cache, static_key)) return &static_map_cache;
    if (display_requires_fill_layer()) return NULL;
    if (static_map_cache.complete && cache_presentable(&static_map_cache)) return &static_map_cache;
    if (cache_presentable(&physical_cache)) return &physical_cache;
    return NULL;
}

static int rebuild_fill_layer(HDC hdc, int fill_key, int live_fill_key) {
    render_static_map_cache_status_note_reason(RENDER_STATIC_MAP_REASON_FILL);
    if (!ensure_cache(hdc, &fill_cache)) return 0;
    build_fill_pixels(&fill_cache, render_context_snapshot());
    mark_cache_valid(&fill_cache, fill_key, 1);
    profiler_add_render_rebuild(PROFILER_RENDER_POLITICAL);
    if (fill_key == live_fill_key) dirty_clear_render_political();
    return 1;
}

void draw_cached_static_map_nonblocking(HDC hdc, RECT client, MapLayout layout) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    int physical_key, fill_key, coast_key, hydro_key, border_key, static_key;
    int live_tile_key, live_region_key, live_civ_key;
    int live_physical_key, live_fill_key, live_coast_key, live_hydro_key, live_border_key, live_static_key;
    int physical_needs, fill_needs, coast_needs, hydro_needs, border_needs;
    int built_primary_step = 0;
    const MapLayerCache *present;
    if (!snapshot || !snapshot->world_generated) {
        cache_needs_work = 0;
        render_static_map_cache_status_reset_keys();
        render_static_map_cache_status_set_presented(0, 0, 0, 0);
        if (static_map_cache.complete && cache_presentable(&static_map_cache)) {
            render_static_map_cache_status_set_presented(0, 1, 1, 0);
            present_cache(hdc, client, layout, &static_map_cache);
        } else draw_blank(hdc, client, layout);
        return;
    }
    physical_key = physical_revision(snapshot->terrain_revision);
    fill_key = fill_revision(snapshot->terrain_revision, snapshot->regions_revision, snapshot->civ_visual_revision);
    coast_key = snapshot->coast_revision;
    hydro_key = snapshot->hydrology_revision;
    border_key = border_revision(snapshot->terrain_revision, snapshot->regions_revision);
    static_key = static_revision(physical_key, fill_key, coast_key, hydro_key, border_key);
    live_tile_key = dirty_revision_terrain();
    live_region_key = render_snapshot_regions_revision_key();
    live_civ_key = render_snapshot_civ_visual_revision_key();
    live_physical_key = physical_revision(live_tile_key);
    live_fill_key = fill_revision(live_tile_key, live_region_key, live_civ_key);
    live_coast_key = dirty_revision_coast();
    live_hydro_key = dirty_revision_hydrology();
    live_border_key = border_revision(live_tile_key, live_region_key);
    live_static_key = static_revision(live_physical_key, live_fill_key, live_coast_key,
                                      live_hydro_key, live_border_key);
    render_static_map_cache_status_set_keys(static_key, live_static_key, fill_key, border_key,
                                            live_fill_key, live_border_key);
    if (map_interaction_preview && static_map_cache.complete && cache_presentable(&static_map_cache) &&
        (!display_requires_fill_layer() || cache_matches(&static_map_cache, static_key))) {
        int current = cache_matches(&static_map_cache, static_key);
        int ownership_current = !display_requires_fill_layer() || current;
        cache_needs_work = 0;
        render_static_map_cache_status_set_presented(current, static_map_cache.complete,
                                                     current, ownership_current);
        render_static_map_cache_status_note_published(fill_key, border_key, ownership_current);
        present_cache(hdc, client, layout, &static_map_cache);
        return;
    }
    physical_needs = !cache_matches(&physical_cache, physical_key) ||
                     (dirty_render_terrain() && physical_key == live_physical_key);
    fill_needs = !cache_matches(&fill_cache, fill_key) ||
                 (dirty_render_political() && fill_key == live_fill_key);
    coast_needs = !cache_matches(&coast_cache, coast_key) ||
                  (dirty_render_coast() && coast_key == live_coast_key);
    hydro_needs = !cache_matches(&hydrology_cache, hydro_key) ||
                  (dirty_render_hydrology() && hydro_key == live_hydro_key);
    border_needs = !cache_matches(&border_cache, border_key) ||
                   (dirty_render_borders() && border_key == live_border_key);

    if (physical_needs) {
        ProfilerCallTrace trace = profiler_call_begin();
        render_static_map_cache_status_note_reason(RENDER_STATIC_MAP_REASON_PHYSICAL);
        if (ensure_cache(hdc, &physical_cache)) {
            build_physical_pixels(&physical_cache, snapshot);
            mark_cache_valid(&physical_cache, physical_key, !display_requires_fill_layer());
            profiler_add_render_rebuild(PROFILER_RENDER_TERRAIN);
            if (physical_key == live_physical_key) dirty_clear_render_terrain();
        }
        built_primary_step = 1;
        profiler_call_end_quiet("render_rebuild_physical_base", -1, -1, trace);
    }
    if (display_requires_fill_layer() && fill_needs &&
        cache_matches(&physical_cache, physical_key) && rebuild_fill_layer(hdc, fill_key, live_fill_key)) {
        fill_needs = 0;
        built_primary_step = 1;
    }
    if (!built_primary_step && fill_needs) {
        rebuild_fill_layer(hdc, fill_key, live_fill_key);
    } else if (!built_primary_step && display_requires_fill_layer() && border_needs &&
               cache_matches(&fill_cache, fill_key)) {
        render_static_map_cache_status_note_reason(RENDER_STATIC_MAP_REASON_BORDER);
        if (rebuild_overlay_layer(hdc, &border_cache, NULL)) {
            render_static_map_cache_build_border_pixels(&border_cache, render_context_snapshot());
            mark_cache_valid(&border_cache, border_key, 1); profiler_add_render_rebuild(PROFILER_RENDER_BORDER);
            if (border_key == live_border_key) dirty_clear_render_borders();
        }
    } else if (!built_primary_step && coast_needs) {
        render_static_map_cache_status_note_reason(RENDER_STATIC_MAP_REASON_COAST);
        if (rebuild_overlay_layer(hdc, &coast_cache, draw_snapshot_coast_layer)) {
            mark_cache_valid(&coast_cache, coast_key, 1); profiler_add_render_rebuild(PROFILER_RENDER_COAST);
            if (coast_key == live_coast_key) dirty_clear_render_coast();
        }
    } else if (!built_primary_step && hydro_needs) {
        render_static_map_cache_status_note_reason(RENDER_STATIC_MAP_REASON_HYDRO);
        river_render_set_lod_tile_size(16);
        if (rebuild_overlay_layer(hdc, &hydrology_cache, draw_snapshot_hydrology_layer)) {
            mark_cache_valid(&hydrology_cache, hydro_key, 1);
            if (hydro_key == live_hydro_key) dirty_clear_render_hydrology();
        }
    } else if (!built_primary_step && border_needs) {
        render_static_map_cache_status_note_reason(RENDER_STATIC_MAP_REASON_BORDER);
        if (rebuild_overlay_layer(hdc, &border_cache, NULL)) {
            render_static_map_cache_build_border_pixels(&border_cache, render_context_snapshot());
            mark_cache_valid(&border_cache, border_key, 1); profiler_add_render_rebuild(PROFILER_RENDER_BORDER);
            if (border_key == live_border_key) dirty_clear_render_borders();
        }
    } else if (!built_primary_step && static_layers_ready() && !cache_matches(&static_map_cache, static_key)) {
        compose_static_map(hdc, static_key);
    }

    present = best_present_cache(static_key);
    if (present) {
        int current = present == &static_map_cache && cache_matches(&static_map_cache, static_key);
        int safe = present == &static_map_cache ? present->complete : !display_requires_fill_layer();
        int full = present == &static_map_cache && present->complete && current;
        int ownership_current = safe && (!display_requires_fill_layer() || current);
        render_static_map_cache_status_set_presented(current, safe, full, ownership_current);
        render_static_map_cache_status_note_published(fill_key, border_key, ownership_current);
        present_cache(hdc, client, layout, present);
    }
    else if (display_requires_fill_layer() && cache_matches(&physical_cache, physical_key) &&
             cache_matches(&fill_cache, fill_key) && cache_matches(&border_cache, border_key)) {
        render_static_map_cache_status_set_presented(0, 1, 0, 1);
        render_static_map_cache_status_note_published(fill_key, border_key, 1);
        present_partial_static_cache(hdc, client, layout, fill_key, coast_key, hydro_key, border_key);
    } else if (display_requires_fill_layer()) {
        render_static_map_cache_status_set_presented(0, 0, 0, 0);
        draw_blank(hdc, client, layout);
    }
    else {
        render_static_map_cache_status_set_presented(0, 0, 0, 0);
        draw_blank(hdc, client, layout);
    }
    cache_needs_work = static_cache_work_pending(physical_key, fill_key, coast_key,
                                                 hydro_key, border_key, static_key);
}

int render_static_map_cache_needs_work(void) { return cache_needs_work; }
