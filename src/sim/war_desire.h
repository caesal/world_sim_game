#ifndef WORLD_SIM_WAR_DESIRE_H
#define WORLD_SIM_WAR_DESIRE_H

#include "sim/diplomacy.h"

typedef enum {
    WAR_DESIRE_RESULT_NONE,
    WAR_DESIRE_RESULT_NO_FRONT,
    WAR_DESIRE_RESULT_FRONTIER,
    WAR_DESIRE_RESULT_TRUCE,
    WAR_DESIRE_RESULT_LOW_READINESS,
    WAR_DESIRE_RESULT_STABILITY,
    WAR_DESIRE_RESULT_READY,
    WAR_DESIRE_RESULT_BELOW_THRESHOLD
} WarDesireResult;

typedef struct {
    int raw_desire;
    int pre_stability_desire;
    int final_desire;
    int threshold;
    int readiness_percent;
    int readiness_cap;
    int readiness_cap_applied;
    int aggression_score;
    int border_score;
    int resource_score;
    int population_pressure;
    int resource_pressure;
    int crisis_score;
    int open_target_count;
    int global_unowned_percent;
    int strength_score;
    int trade_penalty;
    int truce_penalty;
    int disorder_penalty;
    int frontier_penalty;
    int heritage_affinity_penalty;
    int stability_penalty;
    int stability_mode;
    int stability_blocked;
    int own_soldiers;
    int enemy_soldiers;
    WarDesireResult result;
    char reason[128];
} WarDesireBreakdown;

WarDesireBreakdown war_desire_calculate(int civ_a, int civ_b, DiplomacyRelation relation);
const WarDesireBreakdown *war_desire_last_breakdown(int civ_id);
int war_desire_last_final(int civ_id);
const char *war_desire_last_reason(int civ_id);
void war_desire_reset_all(void);
void war_desire_clear_civ(int civ_id);

#endif
