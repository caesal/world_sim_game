#ifndef WORLD_SIM_UI_WORLD_ANNOUNCEMENT_H
#define WORLD_SIM_UI_WORLD_ANNOUNCEMENT_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int ui_world_announcement_handle_click(HWND hwnd, RECT client, int x, int y);
void ui_world_announcement_update_hover(HWND hwnd, RECT client, int x, int y);
void ui_world_announcement_mouse_leave(HWND hwnd);
void ui_world_announcement_tick(HWND hwnd, DWORD now);
void ui_world_announcement_invalidate(HWND hwnd);
int ui_world_announcement_hover_control(RECT client, int x, int y);
RECT ui_world_announcement_dirty_rect(RECT client);

#endif
