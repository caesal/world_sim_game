#ifndef WORLD_SIM_WAR_ECONOMY_H
#define WORLD_SIM_WAR_ECONOMY_H

#include "sim/war.h"

void war_economy_try_hire_mercenaries(ActiveWar *war);
int war_economy_absorb_casualties(ActiveWar *war, int attacker_side, int casualties);
int war_economy_temporary_soldiers_for_civ(const ActiveWar *war, int civ_id);

#endif
