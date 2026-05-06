#include "render_internal.h"

#include "core/dirty_flags.h"
#include "render/cartography_layers.h"

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

static LayerCache terrain_cache;
static LayerCache political_cache;
static LayerCache coast_cache;
static LayerCache border_cache;
static LayerCache window_backbuffer;

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
           cache->display == display_mode &&
           cache->revision == world_visual_revision;
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
    cache->revision = world_visual_revision;
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
    draw_crisp_map_surface(hdc, layout);
    if (layout.tile_size >= 12 && visible_tile_bounds(client, layout, &min_x, &max_x, &min_y, &max_y)) {
        for (y = min_y; y <= max_y; y++) {
            for (x = min_x; x <= max_x; x++) draw_land_texture(hdc, layout, x, y);
        }
    }
}

static int rebuild_cached_layer(HDC hdc, LayerCache *cache, RECT client, MapLayout layout,
                                HDC source_dc, void (*draw_extra)(HDC, RECT, MapLayout)) {
    if (!ensure_layer_cache(hdc, cache, client, layout)) return 0;
    if (source_dc) BitBlt(cache->dc, 0, 0, cache->width, cache->height, source_dc, 0, 0, SRCCOPY);
    else fill_rect(cache->dc, client, RGB(79, 160, 215));
    if (draw_extra) draw_extra(cache->dc, client, layout);
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
        draw_rivers(hdc, client, layout);
        draw_cartography_region_borders(hdc, client, layout);
        draw_cartography_province_borders(hdc, client, layout);
        draw_cartography_country_borders(hdc, client, layout);
        draw_cartography_coastline(hdc, client, layout);
    }
    draw_map_grid_overlay(hdc, client, layout);
}

static int can_preview_map_cache(RECT client) {
    return map_interaction_preview && border_cache.valid &&
           border_cache.width == client.right - client.left &&
           border_cache.height == client.bottom - client.top &&
           border_cache.side_w == side_panel_w &&
           border_cache.display == display_mode &&
           border_cache.revision == world_visual_revision;
}

static void draw_map_cache_preview(HDC hdc, RECT client, MapLayout layout) {
    fill_rect(hdc, client, RGB(79, 160, 215));
    SetStretchBltMode(hdc, HALFTONE);
    SetBrushOrgEx(hdc, 0, 0, NULL);
    StretchBlt(hdc, layout.map_x, layout.map_y, layout.draw_w, layout.draw_h,
               border_cache.dc, border_cache.map_x, border_cache.map_y,
               border_cache.draw_w, border_cache.draw_h, SRCCOPY);
}

static void draw_cached_static_map(HDC hdc, RECT client, MapLayout layout) {
    int terrain_dirty;
    int political_dirty;
    int coast_dirty;
    int border_dirty;

    if (can_preview_map_cache(client)) {
        draw_map_cache_preview(hdc, client, layout);
        return;
    }
    terrain_dirty = dirty_render_terrain() || !layer_cache_matches(&terrain_cache, client, layout);
    if (terrain_dirty) {
        rebuild_cached_layer(hdc, &terrain_cache, client, layout, NULL, draw_terrain_layer);
        dirty_clear_render_terrain();
    }
    political_dirty = terrain_dirty || dirty_render_political() ||
                      !layer_cache_matches(&political_cache, client, layout);
    if (political_dirty) {
        rebuild_cached_layer(hdc, &political_cache, client, layout, terrain_cache.dc, draw_political_extra);
        dirty_clear_render_political();
    }
    coast_dirty = political_dirty || dirty_render_coast() || !layer_cache_matches(&coast_cache, client, layout);
    if (coast_dirty) {
        rebuild_cached_layer(hdc, &coast_cache, client, layout, political_cache.dc, draw_coast_extra);
        dirty_clear_render_coast();
    }
    border_dirty = coast_dirty || dirty_render_borders() || !layer_cache_matches(&border_cache, client, layout);
    if (border_dirty) {
        rebuild_cached_layer(hdc, &border_cache, client, layout, coast_cache.dc, draw_border_extra);
        dirty_clear_render_borders();
    }
    if (border_cache.valid) BitBlt(hdc, 0, 0, border_cache.width, border_cache.height, border_cache.dc, 0, 0, SRCCOPY);
}

static void render_world(HDC hdc, RECT client) {
    MapLayout layout = get_map_layout(client);

    draw_cached_static_map(hdc, client, layout);
    if (world_generated) {
        draw_maritime_routes(hdc, client, layout);
        dirty_clear_render_maritime();
        draw_plague_region_overlay(hdc, client, layout);
        dirty_clear_render_plague();
        draw_cities(hdc, layout);
        draw_map_labels(hdc, client, layout);
        dirty_clear_render_labels();
        draw_selected_tile(hdc, layout);
    } else {
        dirty_clear_render_maritime();
        dirty_clear_render_plague();
        dirty_clear_render_labels();
    }
    draw_top_bar(hdc, client);
    draw_bottom_bar(hdc, client);
    draw_map_legend(hdc, client);
    draw_side_panel(hdc, client);
}

void paint_window(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT client;
    int width;
    int height;

    GetClientRect(hwnd, &client);
    width = client.right - client.left;
    height = client.bottom - client.top;
    if (ensure_layer_cache(hdc, &window_backbuffer, client, get_map_layout(client))) {
        render_world(window_backbuffer.dc, client);
        BitBlt(hdc, 0, 0, width, height, window_backbuffer.dc, 0, 0, SRCCOPY);
    } else {
        render_world(hdc, client);
    }
    EndPaint(hwnd, &ps);
}
