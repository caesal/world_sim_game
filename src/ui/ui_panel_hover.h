#ifndef WORLD_SIM_UI_PANEL_HOVER_H
#define WORLD_SIM_UI_PANEL_HOVER_H

#include <windows.h>

int ui_panel_hover_target_key(RECT client, int x, int y);
void ui_panel_hover_reset(void);
void ui_panel_hover_update(HWND hwnd, RECT client, int old_x, int old_y,
                           int new_x, int new_y);
void ui_panel_hover_leave(HWND hwnd, RECT client);

#endif
