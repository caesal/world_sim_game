#ifndef WORLD_SIM_DECISION_SNAPSHOT_H
#define WORLD_SIM_DECISION_SNAPSHOT_H

#include "sim/expansion.h"

typedef struct {
    ExpansionAIDiagnostics expansion;
    int war_desire;
    int stability_pressure;
    int expansion_weight;
    int war_weight;
    int stability_weight;
    int next_expansion_months;
    int next_diplomacy_months;
    int next_battle_months;
    int next_collapse_years;
    const char *main_intent;
    const char *expansion_reason;
    const char *war_reason;
} DecisionSnapshot;

void decision_snapshot_for_civ(int civ_id, DecisionSnapshot *out);

#endif
