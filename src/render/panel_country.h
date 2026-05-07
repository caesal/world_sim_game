#ifndef WORLD_SIM_PANEL_COUNTRY_H
#define WORLD_SIM_PANEL_COUNTRY_H

#include <windows.h>

#define COUNTRY_PANEL_HIT_NONE -1
#define COUNTRY_PANEL_HIT_TOGGLE_FALLEN -2
#define COUNTRY_PANEL_HIT_BACK_TO_LIST -3

int country_panel_hit_test(RECT client, int mouse_x, int mouse_y);
int country_panel_scroll(RECT client, int delta);

#endif
