#ifndef WORLD_SIM_PANEL_COUNTRY_DIPLOMACY_HITS_H
#define WORLD_SIM_PANEL_COUNTRY_DIPLOMACY_HITS_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void country_diplomacy_hit_reset(void);
void country_diplomacy_hit_add(RECT rect, int civ_id);
int country_diplomacy_civ_hit_test(int mouse_x, int mouse_y);

#endif
