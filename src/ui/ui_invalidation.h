#ifndef WORLD_SIM_UI_INVALIDATION_H
#define WORLD_SIM_UI_INVALIDATION_H

#include "platform/platform_types.h"

void ui_invalidate_side_panel(HWND hwnd);
void ui_invalidate_map_viewport(HWND hwnd);
void ui_invalidate_top_bar(HWND hwnd);
void ui_invalidate_bottom_bar(HWND hwnd);
void ui_invalidate_full(HWND hwnd);
void ui_invalidate_game_redraw(HWND hwnd, int redraw_flags);

#endif
