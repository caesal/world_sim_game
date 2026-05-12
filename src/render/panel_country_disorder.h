#ifndef WORLD_SIM_PANEL_COUNTRY_DISORDER_H
#define WORLD_SIM_PANEL_COUNTRY_DISORDER_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "ui/ui_widgets.h"

int country_disorder_tab_height(int civ_id);
void draw_country_disorder_tab(HDC hdc, UiCursor *cursor, int civ_id);

#endif
