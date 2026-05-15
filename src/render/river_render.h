#ifndef WORLD_SIM_RIVER_RENDER_H
#define WORLD_SIM_RIVER_RENDER_H

#include <windows.h>

#include "render/river_geometry.h"
#include "ui/ui_layout.h"

void river_render_draw_layer(HDC hdc, RECT client, MapLayout layout);
const HydrologyRenderStats *river_render_stats(void);

#endif
