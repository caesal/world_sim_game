#ifndef WORLD_SIM_PANEL_DEBUG_CONTROLS_H
#define WORLD_SIM_PANEL_DEBUG_CONTROLS_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "ui/ui_widgets.h"

enum {
    DEBUG_PLAGUE_SWITCH_SYSTEM,
    DEBUG_PLAGUE_SWITCH_VISUALS,
    DEBUG_PLAGUE_SWITCH_COUNT
};

void draw_debug_plague_perf_controls(HDC hdc, UiCursor *cursor);
int debug_panel_plague_perf_switch_hit_test(RECT client, int mouse_x, int mouse_y);

#endif
