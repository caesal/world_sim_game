#ifndef WORLD_SIM_DIPLOMACY_H
#define WORLD_SIM_DIPLOMACY_H

#include "core/game_state.h"

typedef enum {
    DIPLOMACY_NONE,
    DIPLOMACY_PEACE,
    DIPLOMACY_ALLIANCE,
    DIPLOMACY_TENSE,
    DIPLOMACY_TRUCE,
    DIPLOMACY_WAR,
    DIPLOMACY_VASSAL
} DiplomacyStatus;

typedef struct {
    DiplomacyStatus state;
    int relation_score;
    int border_tension;
    int trade_fit;
    int resource_conflict;
    int truce_years_left;
    int border_length;
    int natural_barrier;
    int years_known;
    int vassal_years;
    int easing_years;
    int overlord;
    int vassal;
} DiplomacyRelation;

typedef struct {
    int border_length[MAX_CIVS][MAX_CIVS];
    int natural_barrier[MAX_CIVS][MAX_CIVS];
    int dirty;
} BorderContactCache;

void diplomacy_reset(void);
void diplomacy_clear_civ(int civ_id);
void diplomacy_mark_contacts_dirty(void);
void diplomacy_update_contacts(void);
void diplomacy_update_year(void);
DiplomacyStatus diplomacy_status(int civ_a, int civ_b);
DiplomacyRelation diplomacy_relation(int civ_a, int civ_b);
void diplomacy_force_war(int civ_a, int civ_b);
void diplomacy_start_truce(int civ_a, int civ_b, int years, int relation_score);
void diplomacy_start_vassal(int overlord, int vassal, int relation_score);
const char *diplomacy_status_name(DiplomacyStatus status);
int diplomacy_last_war_desire(int civ_id);
const char *diplomacy_last_war_reason(int civ_id);

#endif
