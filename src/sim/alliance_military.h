#ifndef WORLD_SIM_ALLIANCE_MILITARY_H
#define WORLD_SIM_ALLIANCE_MILITARY_H

#include "sim/war.h"

int alliance_military_support_for_war(const ActiveWar *war, int attacker_side);
int alliance_military_apply_support_casualties(const ActiveWar *war, int attacker_side);

#endif
