#ifndef WORLD_SIM_PLAGUE_VISUAL_H
#define WORLD_SIM_PLAGUE_VISUAL_H

int plague_visual_tick(int elapsed_ms);
int plague_visual_active(void);
int plague_visual_route_intensity(int route_id);
int plague_visual_fog_rebuild_interval_ms(void);
int plague_visual_fog_rebuild_count(void);
int plague_visual_last_fog_rebuild_ms(void);

#ifdef _WIN32
#include "render.h"
#include "ui/ui_layout.h"

void draw_plague_visual_regions(HDC hdc, RECT client, MapLayout layout);
#endif

#endif
