#ifndef WORLD_SIM_UI_MAP_INPUT_H
#define WORLD_SIM_UI_MAP_INPUT_H

#include <windows.h>

int ui_map_screen_to_tile(HWND hwnd, int mouse_x, int mouse_y, int *out_x, int *out_y);
int ui_map_point_in_viewport_blank(RECT client, int mouse_x, int mouse_y);
void ui_select_tile_from_mouse(HWND hwnd, int mouse_x, int mouse_y);

#endif
