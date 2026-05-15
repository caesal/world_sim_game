#include "render_internal.h"

#include "core/dirty_flags.h"
#include "core/profiler.h"
#include "core/render_snapshot.h"
#include "core/state_lock.h"
#include "render/cartography_layers.h"
#include "render/diplomacy_map_anim.h"
#include "render/map_highlight.h"
#include "render/pause_menu_render.h"
#include "render/render_context.h"
#include "render/river_render.h"
#include "render/worldgen_progress_overlay.h"
#include "core/worldgen_progress.h"

typedef struct {
    HDC dc;
    HBITMAP bitmap;
    HBITMAP old_bitmap;
    int width;
    int height;
    int map_x;
    int map_y;
    int draw_w;
    int draw_h;
    int side_w;
    int display;
    int revision;
    int valid;
} LayerCache;

#define MAP_LAYER_CACHE_SCALE 2

static LayerCache terrain_cache;
static LayerCache political_cache;
static LayerCache coast_cache;
static LayerCache hydrology_cache;
static LayerCache border_cache;
static LayerCache ui_cache;
static LayerCache window_backbuffer;

static int combined_revision(int a, int b) {
    return (a * 1000003) ^ b;
}

static void release_layer_cache(LayerCache *cache) {
    if (cache->dc && cache->old_bitmap) SelectObject(cache->dc, cache->old_bitmap);
    if (cache->bitmap) DeleteObject(cache->bitmap);
    if (cache->dc) DeleteDC(cache->dc);
    memset(cache, 0, sizeof(*cache));
}

static int layer_cache_matches(const LayerCache *cache, RECT client, MapLayout layout) {
    return cache->valid &&
           cache->width == client.right - client.left &&
           cache->height == client.bottom - client.top &&
           cache->map_x == layout.map_x &&
           cache->map_y == layout.map_y &&
           cache->draw_w == layout.draw_w &&
           cache->draw_h == layout.draw_h &&
           cache->side_w == side_panel_w &&
           cache->display == display_mode;
}

static int map_cache_width(void) {
    return max(1, MAP_W * MAP_LAYER_CACHE_SCALE);
}

static int map_cache_height(void) {
    return max(1, MAP_H * MAP_LAYER_CACHE_SCALE);
}

static MapLayout map_cache_layout(const LayerCache *cache) {
    MapLayout layout;
    layout.map_x = 0;
    layout.map_y = 0;
    layout.draw_w = cache->width;
    layout.draw_h = cache->height;
    layout.tile_size = max(1, cache->width / max(1, MAP_W));
    return layout;
}

static int map_layer_cache_matches(const LayerCache *cache, int revision) {
    return cache->valid &&
           cache->width == map_cache_width() &&
           cache->height == map_cache_height() &&
           cache->display == display_mode &&
           cache->revision == revision;
}

static int ensure_layer_cache(HDC hdc, LayerCache *cache, RECT client, MapLayout layout) {
    int width = client.right - client.left;
    int height = client.bottom - client.top;

    if (width <= 0 || height <= 0) return 0;
    if (!cache->dc || cache->width != width || cache->height != height) {
        release_layer_cache(cache);
        cache->dc = CreateCompatibleDC(hdc);
        cache->bitmap = CreateCompatibleBitmap(hdc, width, height);
        if (!cache->dc || !cache->bitmap) {
            release_layer_cache(cache);
            return 0;
        }
        profiler_add_gdi_recreate();
        cache->old_bitmap = SelectObject(cache->dc, cache->bitmap);
        cache->width = width;
        cache->height = height;
    }
    cache->map_x = layout.map_x;
    cache->map_y = layout.map_y;
    cache->draw_w = layout.draw_w;
    cache->draw_h = layout.draw_h;
    cache->side_w = side_panel_w;
    cache->display = display_mode;
    cache->valid = 1;
    return 1;
}

static int ensure_map_layer_cache(HDC hdc, LayerCache *cache) {
    int width = map_cache_width();
    int height = map_cache_height();

    if (width <= 0 || height <= 0) return 0;
    if (!cache->dc || cache->width != width || cache->height != height) {
        release_layer_cache(cache);
        cache->dc = CreateCompatibleDC(hdc);
        cache->bitmap = CreateCompatibleBitmap(hdc, width, height);
        if (!cache->dc || !cache->bitmap) {
            release_layer_cache(cache);
            return 0;
        }
        profiler_add_gdi_recreate();
        cache->old_bitmap = SelectObject(cache->dc, cache->bitmap);
        cache->width = width;
        cache->height = height;
    }
    cache->map_x = 0;
    cache->map_y = 0;
    cache->draw_w = width;
    cache->draw_h = height;
    cache->side_w = 0;
    cache->display = display_mode;
    cache->valid = 1;
    return 1;
}

static void draw_blank_map(HDC hdc, RECT client, MapLayout layout) {
    RECT map_rect = {layout.map_x, layout.map_y, layout.map_x + layout.draw_w, layout.map_y + layout.draw_h};

    fill_rect(hdc, client, RGB(79, 160, 215));
    fill_rect(hdc, map_rect, RGB(64, 133, 178));
    draw_map_grid_overlay(hdc, client, layout);
}

static void draw_terrain_layer(HDC hdc, RECT client, MapLayout layout) {
    int min_x;
    int max_x;
    int min_y;
    int max_y;
    int y;
    int x;

    fill_rect(hdc, client, RGB(79, 160, 215));
    if (!world_generated) {
        draw_blank_map(hdc, client, layout);
        return;
    }
    draw_zoom_aware_map_surface(hdc, client, layout);
    if (layout.tile_size >= 12 && visible_tile_bounds(client, layout, &min_x, &max_x, &min_y, &max_y)) {
        for (y = min_y; y <= max_y; y++) {
            for (x = min_x; x <= max_x; x++) draw_land_texture(hdc, layout, x, y);
        }
    }
}

static int rebuild_cached_map_layer(HDC hdc, LayerCache *cache, HDC source_dc,
                                    void (*draw_extra)(HDC, RECT, MapLayout)) {
    RECT cache_rect;
    MapLayout cache_layout;
    if (!ensure_map_layer_cache(hdc, cache)) return 0;
    cache_rect = (RECT){0, 0, cache->width, cache->height};
    cache_layout = map_cache_layout(cache);
    if (source_dc) BitBlt(cache->dc, 0, 0, cache->width, cache->height, source_dc, 0, 0, SRCCOPY);
    else fill_rect(cache->dc, cache_rect, RGB(79, 160, 215));
    if (draw_extra) draw_extra(cache->dc, cache_rect, cache_layout);
    return 1;
}

static void draw_political_extra(HDC hdc, RECT client, MapLayout layout) {
    if (world_generated) draw_cartography_political_fills(hdc, client, layout);
}

static void draw_coast_extra(HDC hdc, RECT client, MapLayout layout) {
    if (world_generated) draw_cartography_coast_halo(hdc, client, layout);
}

static void draw_border_extra(HDC hdc, RECT client, MapLayout layout) {
    if (world_generated && layout.tile_size >= 1) {
        draw_cartography_region_borders(hdc, client, layout);
        draw_cartography_province_borders(hdc, client, layout);
        draw_cartography_country_borders(hdc, client, layout);
        draw_cartography_coastline(hdc, client, layout);
    }
    draw_map_grid_overlay(hdc, client, layout);
}

static void draw_hydrology_extra(HDC hdc, RECT client, MapLayout layout) {
    if (world_generated && layout.tile_size >= 1) river_render_draw_layer(hdc, client, layout);
}

static int can_preview_map_cache(void) {
    return map_interaction_preview && border_cache.valid &&
           border_cache.display == display_mode &&
           !dirty_render_terrain() && !dirty_render_political() &&
           !dirty_render_coast() && !dirty_render_hydrology() &&
           !dirty_render_borders();
}

static void present_map_cache(HDC hdc, RECT client, MapLayout layout, const LayerCache *cache) {
    int saved_dc;
    fill_rect(hdc, client, RGB(79, 160, 215));
    if (!cache->valid) return;
    saved_dc = SaveDC(hdc);
    IntersectClipRect(hdc, client.left, TOP_BAR_H, client.right - side_panel_w,
                      client.bottom - BOTTOM_BAR_H);
    SetStretchBltMode(hdc, layout.tile_size <= 2 ? HALFTONE : COLORONCOLOR);
    SetBrushOrgEx(hdc, 0, 0, NULL);
    StretchBlt(hdc, layout.map_x, layout.map_y, layout.draw_w, layout.draw_h,
               cache->dc, 0, 0, cache->width, cache->height, SRCCOPY);
    RestoreDC(hdc, saved_dc);
}

static void draw_map_cache_preview(HDC hdc, RECT client, MapLayout layout) {
    present_map_cache(hdc, client, layout, &border_cache);
}

static void draw_cached_static_map(HDC hdc, RECT client, MapLayout layout) {
    int terrain_dirty;
    int political_dirty;
    int coast_dirty;
    int hydrology_dirty;
    int border_dirty;
    int hydrology_revision;
    int border_revision;

    if (can_preview_map_cache()) {
        draw_map_cache_preview(hdc, client, layout);
        return;
    }
    terrain_dirty = dirty_render_terrain() ||
                    !map_layer_cache_matches(&terrain_cache, dirty_revision_terrain());
    if (terrain_dirty) {
        ProfilerCallTrace trace = profiler_call_begin();
        if (rebuild_cached_map_layer(hdc, &terrain_cache, NULL, draw_terrain_layer)) {
            terrain_cache.revision = dirty_revision_terrain();
            profiler_add_render_rebuild(PROFILER_RENDER_TERRAIN);
        }
        profiler_call_end_quiet("render_rebuild_terrain", -1, -1, trace);
        dirty_clear_render_terrain();
    }
    coast_dirty = terrain_dirty || dirty_render_coast() ||
                  !map_layer_cache_matches(&coast_cache, dirty_revision_coast());
    if (coast_dirty) {
        ProfilerCallTrace trace = profiler_call_begin();
        if (rebuild_cached_map_layer(hdc, &coast_cache, terrain_cache.dc, draw_coast_extra)) {
            coast_cache.revision = dirty_revision_coast();
            profiler_add_render_rebuild(PROFILER_RENDER_COAST);
        }
        profiler_call_end_quiet("render_rebuild_coast", -1, -1, trace);
        dirty_clear_render_coast();
    }
    political_dirty = coast_dirty || dirty_render_political() ||
                      !map_layer_cache_matches(&political_cache, dirty_revision_ownership());
    if (political_dirty) {
        ProfilerCallTrace trace = profiler_call_begin();
        if (rebuild_cached_map_layer(hdc, &political_cache, coast_cache.dc, draw_political_extra)) {
            political_cache.revision = dirty_revision_ownership();
            profiler_add_render_rebuild(PROFILER_RENDER_POLITICAL);
        }
        profiler_call_end_quiet("render_rebuild_political", -1, -1, trace);
        dirty_clear_render_political();
    }
    hydrology_revision = dirty_revision_hydrology();
    hydrology_dirty = political_dirty || dirty_render_hydrology() ||
                      !map_layer_cache_matches(&hydrology_cache, hydrology_revision);
    if (hydrology_dirty) {
        ProfilerCallTrace trace = profiler_call_begin();
        if (rebuild_cached_map_layer(hdc, &hydrology_cache, political_cache.dc, draw_hydrology_extra)) {
            hydrology_cache.revision = hydrology_revision;
        }
        profiler_call_end_quiet("render_rebuild_hydrology", -1, -1, trace);
        dirty_clear_render_hydrology();
    }
    border_revision = combined_revision(combined_revision(dirty_revision_ownership(),
                                                          dirty_revision_province()),
                                        dirty_revision_coast());
    border_revision = combined_revision(border_revision, dirty_revision_hydrology());
    border_dirty = political_dirty || dirty_render_borders() ||
                   hydrology_dirty ||
                   !map_layer_cache_matches(&border_cache, border_revision);
    if (border_dirty) {
        ProfilerCallTrace trace = profiler_call_begin();
        HDC border_source = hydrology_cache.valid ? hydrology_cache.dc : political_cache.dc;
        if (rebuild_cached_map_layer(hdc, &border_cache, border_source, draw_border_extra)) {
            border_cache.revision = border_revision;
            profiler_add_render_rebuild(PROFILER_RENDER_BORDER);
        }
        profiler_call_end_quiet("render_rebuild_border", -1, -1, trace);
        dirty_clear_render_borders();
    }
    if (border_cache.valid) present_map_cache(hdc, client, layout, &border_cache);
}

static void draw_cached_static_map_nonblocking(HDC hdc, RECT client, MapLayout layout) {
    if (state_try_read_lock()) {
        draw_cached_static_map(hdc, client, layout);
        state_read_unlock();
    } else if (border_cache.valid) {
        present_map_cache(hdc, client, layout, &border_cache);
    } else {
        draw_blank_map(hdc, client, layout);
    }
}

static void draw_legacy_overlay_nonblocking(HDC hdc, RECT client, MapLayout layout) {
    if (!state_try_read_lock()) return;
    draw_country_highlight(hdc, client, layout);
    state_read_unlock();
}

static void blit_cache_rect(HDC hdc, const LayerCache *cache, RECT rect) {
    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;
    if (!cache->valid || w <= 0 || h <= 0) return;
    BitBlt(hdc, rect.left, rect.top, w, h, cache->dc, rect.left, rect.top, SRCCOPY);
}

static void blit_cached_ui_regions(HDC hdc, RECT client) {
    RECT top = {client.left, client.top, client.right, TOP_BAR_H};
    RECT bottom = {client.left, client.bottom - BOTTOM_BAR_H, client.right, client.bottom};
    RECT panel = {client.right - side_panel_w, TOP_BAR_H, client.right, client.bottom};
    RECT legend = get_map_legend_box_rect(client);
    blit_cache_rect(hdc, &ui_cache, top);
    blit_cache_rect(hdc, &ui_cache, bottom);
    blit_cache_rect(hdc, &ui_cache, panel);
    if (!IsRectEmpty(&legend)) blit_cache_rect(hdc, &ui_cache, legend);
}

static void draw_stale_ui_indicator(HDC hdc, RECT client) {
    RECT panel = {client.right - side_panel_w + 10, TOP_BAR_H + 8,
                  client.right - 10, TOP_BAR_H + 30};
    RECT badge = {panel.right - 118, panel.top, panel.right, panel.bottom};
    if (badge.left < panel.left) badge.left = panel.left;
    fill_rect_alpha(hdc, badge, RGB(42, 48, 54), 210);
    draw_center_text(hdc, badge, tr("Updating data", "数据更新中"), RGB(218, 226, 232));
}

static void draw_legacy_ui_nonblocking(HDC hdc, RECT client) {
    MapLayout layout = get_map_layout(client);
    if (state_try_read_lock()) {
        if (ensure_layer_cache(hdc, &ui_cache, client, layout)) {
            BitBlt(ui_cache.dc, 0, 0, ui_cache.width, ui_cache.height, hdc, 0, 0, SRCCOPY);
            draw_top_bar(ui_cache.dc, client);
            draw_bottom_bar(ui_cache.dc, client);
            draw_map_legend(ui_cache.dc, client);
            draw_side_panel(ui_cache.dc, client);
            if (pause_menu_open) draw_pause_menu_overlay(ui_cache.dc, client);
            BitBlt(hdc, 0, 0, ui_cache.width, ui_cache.height, ui_cache.dc, 0, 0, SRCCOPY);
        } else {
            draw_top_bar(hdc, client);
            draw_bottom_bar(hdc, client);
            draw_map_legend(hdc, client);
            draw_side_panel(hdc, client);
            if (pause_menu_open) draw_pause_menu_overlay(hdc, client);
        }
        state_read_unlock();
    } else if (layer_cache_matches(&ui_cache, client, layout)) {
        blit_cached_ui_regions(hdc, client);
        draw_stale_ui_indicator(hdc, client);
    } else {
        RECT top = {client.left, client.top, client.right, TOP_BAR_H};
        RECT bottom = {client.left, client.bottom - BOTTOM_BAR_H, client.right, client.bottom};
        RECT panel = {client.right - side_panel_w, TOP_BAR_H, client.right, client.bottom};
        fill_rect(hdc, top, RGB(28, 34, 42));
        fill_rect(hdc, bottom, RGB(28, 34, 42));
        fill_rect(hdc, panel, RGB(34, 42, 50));
        draw_text_line(hdc, panel.left + 18, panel.top + 24, tr("Preparing UI cache...", "正在准备界面缓存..."),
                       RGB(218, 226, 232));
    }
}

static void render_world(HDC hdc, RECT client) {
    MapLayout layout = get_map_layout(client);
    const RenderSnapshot *snapshot = render_context_snapshot();
    int snapshot_world_ready = snapshot && snapshot->world_generated;
    int labels_dirty = dirty_render_labels();
    WorldGenProgress progress;

    worldgen_progress_get(&progress);
    if (progress.active) {
        RECT viewport = get_map_viewport_rect(client);
        fill_rect(hdc, client, RGB(13, 17, 21));
        draw_legacy_ui_nonblocking(hdc, client);
        fill_rect(hdc, viewport, RGB(13, 17, 21));
        draw_worldgen_progress_overlay(hdc, client);
        return;
    }
    draw_cached_static_map_nonblocking(hdc, client, layout);
    if (snapshot_world_ready) {
        if (!map_interaction_preview) {
            draw_maritime_routes(hdc, client, layout);
            dirty_clear_render_maritime();
            draw_plague_region_overlay(hdc, client, layout);
            dirty_clear_render_plague();
        }
        draw_legacy_overlay_nonblocking(hdc, client, layout);
        diplomacy_map_anim_consume_events();
        draw_diplomacy_map_animations(hdc, client, layout);
        if (!map_interaction_preview) {
            draw_cities(hdc, layout);
            draw_map_labels(hdc, client, layout);
            if (labels_dirty) profiler_add_render_rebuild(PROFILER_RENDER_LABEL);
            dirty_clear_render_labels();
        }
        draw_selected_tile(hdc, layout);
    } else {
        dirty_clear_render_maritime();
        dirty_clear_render_plague();
        dirty_clear_render_labels();
    }
    draw_legacy_ui_nonblocking(hdc, client);
    draw_worldgen_progress_overlay(hdc, client);
}

void paint_window(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT client;
    const RenderSnapshot *snapshot;
    DWORD render_start = GetTickCount();
    int width;
    int height;

    GetClientRect(hwnd, &client);
    width = client.right - client.left;
    height = client.bottom - client.top;
    snapshot = render_snapshot_acquire();
    render_context_begin(snapshot);
    if (ensure_layer_cache(hdc, &window_backbuffer, client, get_map_layout(client))) {
        render_world(window_backbuffer.dc, client);
        BitBlt(hdc, 0, 0, width, height, window_backbuffer.dc, 0, 0, SRCCOPY);
    } else {
        render_world(hdc, client);
    }
    render_context_end();
    render_snapshot_release(snapshot);
    profiler_record_render_ms((int)(GetTickCount() - render_start));
    EndPaint(hwnd, &ps);
}
