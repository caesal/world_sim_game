#ifndef WORLD_SIM_UI_PLAGUE_INPUT_H
#define WORLD_SIM_UI_PLAGUE_INPUT_H

#include <windows.h>

int ui_plague_input_mouse_down(HWND hwnd, RECT client, int panel_width,
                               int x, int y);
int ui_plague_input_mouse_move(HWND hwnd, RECT client, int panel_width,
                               int x, int y);
int ui_plague_input_mouse_up(HWND hwnd, RECT client, int panel_width,
                             int x, int y);
void ui_plague_input_panel_closed(void);

#endif
