#include "render_internal.h"

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
} MapLayerCache;

typedef struct {
    HDC dc;
    HBITMAP bitmap;
    HBITMAP old_bitmap;
    int width;
    int height;
} WindowBackbuffer;

static MapLayerCache map_cache;
static WindowBackbuffer window_backbuffer;

static void release_map_cache(void) {
    if (map_cache.dc && map_cache.old_bitmap) SelectObject(map_cache.dc, map_cache.old_bitmap);
    if (map_cache.bitmap) DeleteObject(map_cache.bitmap);
    if (map_cache.dc) DeleteDC(map_cache.dc);
    memset(&map_cache, 0, sizeof(map_cache));
}

static int map_cache_matches(RECT client, MapLayout layout) {
    return map_cache.valid &&
           map_cache.width == client.right - client.left &&
           map_cache.height == client.bottom - client.top &&
           map_cache.map_x == layout.map_x &&
           map_cache.map_y == layout.map_y &&
           map_cache.draw_w == layout.draw_w &&
           map_cache.draw_h == layout.draw_h &&
           map_cache.side_w == side_panel_w &&
           map_cache.display == display_mode &&
           map_cache.revision == world_visual_revision;
}

static int ensure_map_cache(HDC hdc, RECT client, MapLayout layout) {
    int width = client.right - client.left;
    int height = client.bottom - client.top;

    if (width <= 0 || height <= 0) return 0;
    if (!map_cache.dc || map_cache.width != width || map_cache.height != height) {
        release_map_cache();
        map_cache.dc = CreateCompatibleDC(hdc);
        map_cache.bitmap = CreateCompatibleBitmap(hdc, width, height);
        if (!map_cache.dc || !map_cache.bitmap) {
            release_map_cache();
            return 0;
        }
        map_cache.old_bitmap = SelectObject(map_cache.dc, map_cache.bitmap);
        map_cache.width = width;
        map_cache.height = height;
    }
    map_cache.map_x = layout.map_x;
    map_cache.map_y = layout.map_y;
    map_cache.draw_w = layout.draw_w;
    map_cache.draw_h = layout.draw_h;
    map_cache.side_w = side_panel_w;
    map_cache.display = display_mode;
    map_cache.revision = world_visual_revision;
    map_cache.valid = 1;
    return 1;
}

static int can_preview_map_cache(RECT client) {
    return map_interaction_preview && map_cache.valid &&
           map_cache.width == client.right - client.left &&
           map_cache.height == client.bottom - client.top &&
           map_cache.side_w == side_panel_w &&
           map_cache.display == display_mode &&
           map_cache.revision == world_visual_revision;
}

static void draw_map_cache_preview(HDC hdc, RECT client, MapLayout layout) {
    fill_rect(hdc, client, RGB(79, 160, 215));
    SetStretchBltMode(hdc, COLORONCOLOR);
    StretchBlt(hdc, layout.map_x, layout.map_y, layout.draw_w, layout.draw_h,
               map_cache.dc, map_cache.map_x, map_cache.map_y,
               map_cache.draw_w, map_cache.draw_h, SRCCOPY);
}

static void render_map_layers(HDC hdc, RECT client, MapLayout layout) {
    int min_x;
    int max_x;
    int min_y;
    int max_y;
    int y;
    int x;

    fill_rect(hdc, client, RGB(79, 160, 215));
    draw_crisp_map_surface(hdc, layout);
    draw_political_region_fills(hdc, client, layout);
    draw_coast_halo(hdc, client, layout);
    draw_plague_region_overlay(hdc, client, layout);
    if (layout.tile_size >= 12 && visible_tile_bounds(client, layout, &min_x, &max_x, &min_y, &max_y)) {
        for (y = min_y; y <= max_y; y++) {
            for (x = min_x; x <= max_x; x++) draw_land_texture(hdc, layout, x, y);
        }
    }
    if (layout.tile_size >= 1) {
        draw_rivers(hdc, client, layout);
        draw_maritime_routes(hdc, client, layout);
        draw_province_border_paths(hdc, client, layout);
        draw_country_border_paths(hdc, client, layout);
        draw_coastline_paths(hdc, client, layout);
    }
    draw_cities(hdc, layout);
    draw_plague_city_overlay(hdc, client, layout);
    draw_map_grid_overlay(hdc, client, layout);
}

static void draw_cached_map_layers(HDC hdc, RECT client, MapLayout layout) {
    if (!map_cache_matches(client, layout)) {
        if (can_preview_map_cache(client)) {
            draw_map_cache_preview(hdc, client, layout);
            return;
        }
        if (ensure_map_cache(hdc, client, layout)) render_map_layers(map_cache.dc, client, layout);
        else render_map_layers(hdc, client, layout);
    }
    if (map_cache.valid) BitBlt(hdc, 0, 0, map_cache.width, map_cache.height, map_cache.dc, 0, 0, SRCCOPY);
}

static void render_world(HDC hdc, RECT client) {
    MapLayout layout = get_map_layout(client);

    draw_cached_map_layers(hdc, client, layout);
    draw_map_labels(hdc, client, layout);
    draw_selected_tile(hdc, layout);
    draw_top_bar(hdc, client);
    draw_bottom_bar(hdc, client);
    draw_map_legend(hdc, client);
    draw_side_panel(hdc, client);
}

static void release_window_backbuffer(void) {
    if (window_backbuffer.dc && window_backbuffer.old_bitmap) {
        SelectObject(window_backbuffer.dc, window_backbuffer.old_bitmap);
    }
    if (window_backbuffer.bitmap) DeleteObject(window_backbuffer.bitmap);
    if (window_backbuffer.dc) DeleteDC(window_backbuffer.dc);
    memset(&window_backbuffer, 0, sizeof(window_backbuffer));
}

static int ensure_window_backbuffer(HDC hdc, int width, int height) {
    if (width <= 0 || height <= 0) return 0;
    if (window_backbuffer.dc && window_backbuffer.width == width && window_backbuffer.height == height) return 1;
    release_window_backbuffer();
    window_backbuffer.dc = CreateCompatibleDC(hdc);
    window_backbuffer.bitmap = CreateCompatibleBitmap(hdc, width, height);
    if (!window_backbuffer.dc || !window_backbuffer.bitmap) {
        release_window_backbuffer();
        return 0;
    }
    window_backbuffer.old_bitmap = SelectObject(window_backbuffer.dc, window_backbuffer.bitmap);
    window_backbuffer.width = width;
    window_backbuffer.height = height;
    return 1;
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
    if (ensure_window_backbuffer(hdc, width, height)) {
        render_world(window_backbuffer.dc, client);
        BitBlt(hdc, 0, 0, width, height, window_backbuffer.dc, 0, 0, SRCCOPY);
    } else {
        render_world(hdc, client);
    }
    EndPaint(hwnd, &ps);
}
