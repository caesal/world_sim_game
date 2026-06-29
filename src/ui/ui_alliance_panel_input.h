#ifndef WORLD_SIM_UI_ALLIANCE_PANEL_INPUT_H
#define WORLD_SIM_UI_ALLIANCE_PANEL_INPUT_H

#include <windows.h>

int ui_handle_alliance_panel_click(HWND hwnd, RECT client, int mouse_x, int mouse_y);
int ui_alliance_panel_hover_hit(RECT client, int mouse_x, int mouse_y);
int ui_alliance_panel_passive_tooltip_hit(int mouse_x, int mouse_y);
int ui_alliance_panel_owns_input(void);

#endif
