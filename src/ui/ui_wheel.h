#ifndef WORLD_SIM_UI_WHEEL_H
#define WORLD_SIM_UI_WHEEL_H

#include <windows.h>

void ui_wheel_accumulate(HWND hwnd, int screen_x, int screen_y, int delta);
void ui_wheel_process_pending(HWND hwnd);
void ui_wheel_invalidate_map_viewport(HWND hwnd);

#endif
