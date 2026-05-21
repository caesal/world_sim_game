#ifndef WORLD_SIM_DIPLOMACY_MAP_ANIM_H
#define WORLD_SIM_DIPLOMACY_MAP_ANIM_H

void diplomacy_map_anim_consume_events(void);
int diplomacy_map_anim_active(void);

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include "platform/platform_types.h"
#include "ui/ui_types.h"

void draw_diplomacy_map_animations(HDC hdc, RECT client, MapLayout layout);
#endif

#endif
