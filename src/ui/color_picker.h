#ifndef WORLD_SIM_COLOR_PICKER_H
#define WORLD_SIM_COLOR_PICKER_H

#include <windows.h>

#include "core/value_types.h"

void color_picker_open_setup(Color32 color);
void color_picker_open_civ(int civ_id, Color32 color);
void color_picker_close(void);
int color_picker_active(void);
void color_picker_draw(HDC hdc, RECT client);
int color_picker_mouse_down(HWND hwnd, RECT client, int x, int y);
int color_picker_mouse_move(HWND hwnd, RECT client, int x, int y);
int color_picker_mouse_up(HWND hwnd);

#endif
