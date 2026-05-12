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
    int capital_region;
    int capital_connected_regions;
    int owned_regions;
    int disconnected_components;
    int longest_disconnected_months;
    int capital_connected_percent;
    int capital_core_port_count;
    int capital_core_network_count;
    int disconnected_has_port;
    int disconnected_has_network;
    int disconnected_network_matches_capital;
    const char *main_intent;
    const char *expansion_reason;
    const char *war_reason;
    char stability_reason[128];
} DecisionSnapshot;

void decision_snapshot_for_civ(int civ_id, DecisionSnapshot *out);

#endif
