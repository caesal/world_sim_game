#ifndef WORLD_SIM_CIV_COLORS_H
#define WORLD_SIM_CIV_COLORS_H

#include "core/value_types.h"

Color32 civilization_pick_auto_color(int civ_id, int seed_region);
Color32 civilization_pick_distinct_color(int civ_id, Color32 preferred_color,
                                         int parent_civ_id, int seed_region);
int civilization_colors_debug_check(void);
int civilization_repair_alive_colors(void);

#endif
