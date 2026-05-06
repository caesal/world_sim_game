#ifndef WORLD_SIM_PLAGUE_VISUAL_H
#define WORLD_SIM_PLAGUE_VISUAL_H

#include "render.h"
#include "ui/ui_layout.h"

int plague_visual_tick(int elapsed_ms);
int plague_visual_active(void);
int plague_visual_route_intensity(int route_id);
void draw_plague_visual_regions(HDC hdc, RECT client, MapLayout layout);

#endif
