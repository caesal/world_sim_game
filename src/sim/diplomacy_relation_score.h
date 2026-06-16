#ifndef WORLD_SIM_DIPLOMACY_RELATION_SCORE_H
#define WORLD_SIM_DIPLOMACY_RELATION_SCORE_H

#include "sim/diplomacy.h"

#define DIP_REL_TOP_FACTORS 3
#define DIP_REL_FACTOR_SLOTS 12

typedef enum {
    DIP_REL_FACTOR_NONE,
    DIP_REL_FACTOR_CONTACT,
    DIP_REL_FACTOR_HERITAGE,
    DIP_REL_FACTOR_TRADE,
    DIP_REL_FACTOR_LONG_PEACE,
    DIP_REL_FACTOR_ALLIANCE,
    DIP_REL_FACTOR_SHARED_WAR,
    DIP_REL_FACTOR_SHARED_THREAT,
    DIP_REL_FACTOR_POWER,
    DIP_REL_FACTOR_RESOURCE,
    DIP_REL_FACTOR_BORDER,
    DIP_REL_FACTOR_TRUCE_RECOVERY,
    DIP_REL_FACTOR_QUIET_DRIFT,
    DIP_REL_FACTOR_WAR_MEMORY
} DiplomacyRelationFactor;

typedef struct {
    int yearly_delta_x10;
    int positive_x10;
    int negative_x10;
    int factor_ids[DIP_REL_FACTOR_SLOTS];
    int factor_delta_x10[DIP_REL_FACTOR_SLOTS];
    int factor_values[DIP_REL_FACTOR_SLOTS];
} DiplomacyRelationBreakdown;

void diplomacy_relation_score_reset(void);
void diplomacy_relation_score_clear_civ(int civ_id);
void diplomacy_relation_score_begin_year(void);
void diplomacy_relation_score_end_year(void);
void diplomacy_relation_score_reset_pair(int civ_a, int civ_b);
int diplomacy_relation_score_apply_year(int civ_a, int civ_b, DiplomacyRelation relation);
DiplomacyRelationBreakdown diplomacy_relation_breakdown(int civ_a, int civ_b);
int diplomacy_relation_score_from_legacy(int score);
int diplomacy_relation_score_last_pairs(void);
int diplomacy_relation_score_last_update_ms(void);

#endif
