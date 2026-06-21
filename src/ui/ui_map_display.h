#ifndef WORLD_SIM_UI_MAP_DISPLAY_H
#define WORLD_SIM_UI_MAP_DISPLAY_H

#include <windows.h>

int ui_set_map_display_mode(HWND hwnd, int mode_index);
int ui_handle_top_map_display_click(HWND hwnd, RECT client, int mouse_x, int mouse_y);
const char *ui_map_display_label(int mode_index, int language);
const char *ui_primary_panel_tab_label(int language);

#endif
