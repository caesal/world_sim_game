#ifndef WORLD_SIM_WAR_H
#define WORLD_SIM_WAR_H

#include "../core/game_state.h"

typedef enum {
    WAR_OUTCOME_NONE,
    WAR_OUTCOME_ATTACKER_WIN,
    WAR_OUTCOME_DEFENDER_WIN,
    WAR_OUTCOME_STALEMATE
} WarOutcome;

typedef struct {
    int active;
    int attacker;
    int defender;
    int initial_soldiers_a;
    int initial_soldiers_b;
    int soldiers_a;
    int soldiers_b;
    int casualties_a;
    int casualties_b;
    int wins_a;
    int wins_b;
    int years;
    int supply_fail_a;
    int supply_fail_b;
} ActiveWar;

void war_reset(void);
int war_start(int attacker, int defender);
void war_update_year(void);
int war_active_between(int civ_a, int civ_b);
ActiveWar war_state_between(int civ_a, int civ_b);
const char *war_outcome_name(WarOutcome outcome);

#endif
