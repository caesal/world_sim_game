#ifndef WORLD_SIM_PANEL_COUNTRY_TECH_H
#define WORLD_SIM_PANEL_COUNTRY_TECH_H

#include <windows.h>

#include "ui/ui_widgets.h"

int country_tech_tab_height(int civ_id);
void draw_country_tech_tab(HDC hdc, UiCursor *cursor, int civ_id);

#endif
