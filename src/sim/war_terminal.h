#ifndef WORLD_SIM_WAR_TERMINAL_H
#define WORLD_SIM_WAR_TERMINAL_H

#include "sim/war.h"

void war_terminal_finish(ActiveWar *war, WarOutcome outcome, int margin,
                         int last_war_result);
void war_terminal_front_severed(ActiveWar *war);
void war_terminal_offensive_halted(ActiveWar *war, int emit_announcement,
                                   int truce_years, int relation_score);
void war_terminal_interrupt(ActiveWar *war);
void war_terminal_end_direct_for_civ(int civ_id);
int war_terminal_end_direct_for_civ_no_winner(int civ_id, int last_war_result,
                                              int truce_years, int relation_score);

#endif
