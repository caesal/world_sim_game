#ifndef WORLD_SIM_DIPLOMACY_MAP_ANIM_H
#define WORLD_SIM_DIPLOMACY_MAP_ANIM_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "ui/ui_types.h"

void diplomacy_map_anim_consume_events(void);
int diplomacy_map_anim_active(void);
void draw_diplomacy_map_animations(HDC hdc, RECT client, MapLayout layout);

#endif
