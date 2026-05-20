#ifndef WORLD_SIM_UI_DEBUG_INPUT_H
#define WORLD_SIM_UI_DEBUG_INPUT_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int ui_handle_debug_panel_click(HWND hwnd, RECT client, int mouse_x, int mouse_y);
int ui_handle_debug_panel_wheel(HWND hwnd, RECT client, POINT point, int steps);

#endif
