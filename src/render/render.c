#include "render_internal.h"

static void render_world(HDC hdc, RECT client) {
    int min_x;
    int max_x;
    int min_y;
    int max_y;
    int y;
    int x;
    MapLayout layout = get_map_layout(client);

    fill_rect(hdc, client, RGB(79, 160, 215));
    draw_crisp_map_surface(hdc, layout);
    if (layout.tile_size >= 12 && visible_tile_bounds(client, layout, &min_x, &max_x, &min_y, &max_y)) {
        for (y = min_y; y <= max_y; y++) {
            for (x = min_x; x <= max_x; x++) draw_land_texture(hdc, layout, x, y);
        }
    }
    if (layout.tile_size >= 1) {
        draw_rivers(hdc, client, layout);
        draw_borders(hdc, client, layout);
    }
    draw_cities(hdc, layout);
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
