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

static MapLayerCache map_cache;

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
    draw_map_labels(hdc, client, layout);
    draw_map_grid_overlay(hdc, client, layout);
}

static void draw_cached_map_layers(HDC hdc, RECT client, MapLayout layout) {
    if (!map_cache_matches(client, layout)) {
        if (ensure_map_cache(hdc, client, layout)) render_map_layers(map_cache.dc, client, layout);
        else render_map_layers(hdc, client, layout);
    }
    if (map_cache.valid) BitBlt(hdc, 0, 0, map_cache.width, map_cache.height, map_cache.dc, 0, 0, SRCCOPY);
}

static void render_world(HDC hdc, RECT client) {
    MapLayout layout = get_map_layout(client);

    draw_cached_map_layers(hdc, client, layout);
    draw_selected_tile(hdc, layout);
    draw_top_bar(hdc, client);
    draw_bottom_bar(hdc, client);
    draw_map_legend(hdc, client);
    draw_side_panel(hdc, client);
}

void paint_window(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT client;
    HDC mem_dc;
    HBITMAP bitmap;
    HBITMAP old_bitmap;

    GetClientRect(hwnd, &client);
    mem_dc = CreateCompatibleDC(hdc);
    bitmap = CreateCompatibleBitmap(hdc, client.right - client.left, client.bottom - client.top);
    old_bitmap = SelectObject(mem_dc, bitmap);
    render_world(mem_dc, client);
    BitBlt(hdc, 0, 0, client.right - client.left, client.bottom - client.top, mem_dc, 0, 0, SRCCOPY);
    SelectObject(mem_dc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(mem_dc);
    EndPaint(hwnd, &ps);
}
