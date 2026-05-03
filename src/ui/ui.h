#ifndef WORLD_SIM_UI_H
#define WORLD_SIM_UI_H

#include "core/game_types.h"

LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
int handle_shortcut(HWND hwnd, WPARAM key);
int is_game_shortcut(WPARAM key);
int is_game_char_shortcut(WPARAM key);
int handle_char_shortcut(HWND hwnd, WPARAM key);

#endif
