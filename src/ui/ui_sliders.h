#ifndef WORLD_SIM_UI_SLIDERS_H
#define WORLD_SIM_UI_SLIDERS_H

#include "core/game_types.h"

int divider_hit_test(HWND hwnd, int mouse_x, int mouse_y);
RECT setup_slider_rect(HWND hwnd, int index);
int setup_slider_hit_test(HWND hwnd, int mouse_x, int mouse_y);
void update_setup_slider(HWND hwnd, int index, int mouse_x);

#endif
