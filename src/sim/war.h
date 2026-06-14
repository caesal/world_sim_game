#ifndef WORLD_SIM_WAR_H
#define WORLD_SIM_WAR_H

#include "core/game_state.h"

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
    int initial_national_a;
    int initial_national_b;
    int initial_soldiers_a;
    int initial_soldiers_b;
    int soldiers_a;
    int soldiers_b;
    int casualties_a;
    int casualties_b;
    int support_casualties_a;
    int support_casualties_b;
    int wins_a;
    int wins_b;
    int years;
    int supply_fail_a;
    int supply_fail_b;
    int temporary_soldiers_a;
    int temporary_soldiers_b;
    int mercenary_hired_a;
    int mercenary_hired_b;
} ActiveWar;

#define WAR_SAVE_SLOT_COUNT (MAX_CIVS * MAX_CIVS / 2)
#define WAR_BATTLE_INTERVAL_YEARS 2
#define WAR_BATTLE_INTERVAL_MONTHS (WAR_BATTLE_INTERVAL_YEARS * 12)

void war_reset(void);
int war_start(int attacker, int defender);
void war_update_year(void);
int war_update_year_step(int *cursor, int max_wars, int *tail_cut_done, int *changed);
int war_active_between(int civ_a, int civ_b);
ActiveWar war_state_between(int civ_a, int civ_b);
int war_estimated_soldiers(int civ_id);
int war_current_soldiers_for_civ(int civ_id);
int war_deployed_soldiers_for_civ(int civ_id);
int war_available_reserve_for_civ(int civ_id);
int war_front_count_for_civ(int civ_id);
int war_active_for_civ(int civ_id);
int war_has_empty_slot(void);
int war_vassal_support_used_for_overlord(int overlord, int vassal);
int war_vassal_support_casualties(int vassal);
int war_peace_pressure_between(int civ_id, int other_id);
int war_total_started_count(void);
void war_end_direct_for_civ(int civ_id);
int war_end_direct_for_civ_no_winner(int civ_id, int last_war_result,
                                     int truce_years, int relation_score);
int war_start_independence(int attacker, int defender);
void war_copy_save_state(ActiveWar *wars, int war_count, int *support, int support_count, int *total_started);
void war_restore_save_state(const ActiveWar *wars, int war_count, const int *support, int support_count, int total_started);
const char *war_outcome_name(WarOutcome outcome);

#endif
