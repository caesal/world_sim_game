#ifndef WORLD_SIM_WAR_RESOLUTION_H
#define WORLD_SIM_WAR_RESOLUTION_H

#include "sim/war.h"

int war_owned_province_count(int civ_id);
void war_apply_outcome(int attacker, int defender, WarOutcome outcome, int margin,
                       int loser_casualties, int loser_initial_soldiers);

#endif
