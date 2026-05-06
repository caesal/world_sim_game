#ifndef WORLD_SIM_RENDER_MAP_INTERNAL_H
#define WORLD_SIM_RENDER_MAP_INTERNAL_H

#include "render_common.h"
#include "border_paths.h"
#include "map_labels.h"
#include "plague_render.h"
#include "plague_visual.h"
#include "ui/ui_layout.h"

int visible_tile_bounds(RECT client, MapLayout layout, int *min_x, int *max_x, int *min_y, int *max_y);
COLORREF tile_display_color(int x, int y);
int tile_left(MapLayout layout, int x);
int tile_right(MapLayout layout, int x);
int tile_top(MapLayout layout, int y);
int tile_bottom(MapLayout layout, int y);

void draw_crisp_map_surface(HDC hdc, MapLayout layout);
void draw_land_texture(HDC hdc, MapLayout layout, int x, int y);
void draw_rivers(HDC hdc, RECT client, MapLayout layout);
void draw_maritime_routes(HDC hdc, RECT client, MapLayout layout);
void draw_cities(HDC hdc, MapLayout layout);
void draw_selected_tile(HDC hdc, MapLayout layout);

#endif
