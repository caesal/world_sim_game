#ifndef WORLD_SIM_DIPLOMACY_MAP_MARKER_CACHE_H
#define WORLD_SIM_DIPLOMACY_MAP_MARKER_CACHE_H

#include "render/icons.h"
#include <windows.h>

void diplomacy_map_marker_cache_draw(HDC hdc, POINT center, COLORREF color, IconId icon);
void diplomacy_map_marker_cache_reset(void);

#endif
