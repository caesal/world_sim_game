#ifndef WORLD_SIM_TERRAIN_PRESENT_H
#define WORLD_SIM_TERRAIN_PRESENT_H

#include "render/render_common.h"

void draw_crisp_map_surface(HDC hdc, MapLayout layout);
void draw_zoom_aware_map_surface(HDC hdc, RECT client, MapLayout layout);

#endif
