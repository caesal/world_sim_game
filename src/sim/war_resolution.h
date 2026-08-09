#ifndef WORLD_SIM_WAR_RESOLUTION_H
#define WORLD_SIM_WAR_RESOLUTION_H

#include "sim/war.h"
#include "sim/war_history_types.h"

int war_owned_province_count(int civ_id);
void war_apply_outcome(int attacker, int defender, WarOutcome outcome, int margin,
                       int loser_casualties, int loser_initial_soldiers);
void war_apply_outcome_with_result(int attacker, int defender, WarOutcome outcome, int margin,
                                   int loser_casualties, int loser_initial_soldiers, int last_war_result);
WarSettlementResult war_apply_outcome_with_result_capture(
    int attacker, int defender, WarOutcome outcome, int margin,
    int loser_casualties, int loser_initial_soldiers, int last_war_result);

#endif
