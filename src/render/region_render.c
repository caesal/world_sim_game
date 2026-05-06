#include "region_render.h"

#include "core/game_types.h"
#include "render/render_map_internal.h"
#include "ui/ui_types.h"

void draw_natural_region_boundaries(HDC hdc, RECT client, MapLayout layout) {
    HPEN pen;
    HPEN old_pen;
    int min_x;
    int max_x;
    int min_y;
    int max_y;
    int x;
    int y;
    int width;

    if (!world_generated) return;
    if (display_mode != DISPLAY_REGIONS && !(display_mode == DISPLAY_ALL && civ_count == 0)) return;
    if (!visible_tile_bounds(client, layout, &min_x, &max_x, &min_y, &max_y)) return;
    width = display_mode == DISPLAY_REGIONS ? max(1, layout.tile_size / 2) : 1;
    pen = CreatePen(PS_SOLID, width, display_mode == DISPLAY_REGIONS ? RGB(38, 44, 38) : RGB(65, 76, 68));
    old_pen = SelectObject(hdc, pen);
    for (y = min_y; y <= max_y; y++) {
        for (x = min_x; x <= max_x; x++) {
            int id = world[y][x].region_id;
            if (id < 0) continue;
            if (x + 1 < MAP_W && world[y][x + 1].region_id != id) {
                int sx = tile_right(layout, x);
                MoveToEx(hdc, sx, tile_top(layout, y), NULL);
                LineTo(hdc, sx, tile_bottom(layout, y));
            }
            if (y + 1 < MAP_H && world[y + 1][x].region_id != id) {
                int sy = tile_bottom(layout, y);
                MoveToEx(hdc, tile_left(layout, x), sy, NULL);
                LineTo(hdc, tile_right(layout, x), sy);
            }
        }
    }
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
}
