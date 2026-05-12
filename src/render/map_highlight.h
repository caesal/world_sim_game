#ifndef WORLD_SIM_MAP_HIGHLIGHT_H
#define WORLD_SIM_MAP_HIGHLIGHT_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "ui/ui_types.h"

void draw_country_highlight(HDC hdc, RECT client, MapLayout layout);

#endif
