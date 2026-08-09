#ifndef WORLD_SIM_DECISION_SNAPSHOT_H
#define WORLD_SIM_DECISION_SNAPSHOT_H

#include <stdint.h>

#include "sim/expansion.h"
#include "sim/stability_decision.h"
#include "sim/war_desire.h"

typedef struct {
    int effective_disorder;
    int resource_pressure;
    int base_contribution;
    int war_status_contribution;
    int territory_fragmentation_contribution;
    int capital_connectivity_contribution;
    int vassal_governance_contribution;
    int high_disorder_contribution;
    int raw_total;
    int final_intent;
} DecisionStabilityBreakdown;

typedef struct {
    uint64_t published_revision;
    int city_slots_remaining;
    int city_capacity_ready;
    ExpansionAIDiagnostics expansion;
    int war_desire;
    int war_pre_stability_desire;
    int war_raw_desire;
    int war_threshold;
    int war_readiness_percent;
    int war_readiness_cap;
    int war_readiness_cap_applied;
    int war_aggression_score;
    int war_border_score;
    int war_resource_score;
    int war_population_pressure;
    int war_resource_pressure;
    int war_crisis_score;
    int war_open_target_count;
    int war_global_unowned_percent;
    int war_strength_score;
    int war_trade_penalty;
    int war_truce_penalty;
    int war_post_war_cooldown_penalty;
    int war_disorder_penalty;
    int war_frontier_penalty;
    int war_heritage_affinity_penalty;
    int war_stability_penalty;
    int war_stability_blocked;
    int war_own_soldiers;
    int war_enemy_soldiers;
    int war_result;
    int stability_pressure;
    int stability_mode;
    int stability_mode_months;
    int stability_recovery_months;
    int stability_expansion_penalty;
    int stability_expansion_blocked;
    int stability_peace_bonus;
    int stability_allows_war;
    int stability_allows_expansion;
    DecisionStabilityBreakdown stability_breakdown;
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
    int collapse_single_result;
    int collapse_single_candidate;
    const char *main_intent;
    const char *expansion_reason;
    const char *war_reason;
    char stability_reason[128];
} DecisionSnapshot;

void decision_stability_breakdown_calculate(
    int effective_disorder, int resource_pressure,
    int disconnected_components, int owned_regions,
    int capital_connected_percent, int vassal_governance_disorder,
    int war_active, DecisionStabilityBreakdown *out);
void decision_snapshot_for_civ(int civ_id, DecisionSnapshot *out);
void decision_snapshot_refresh_countdowns(int civ_id, DecisionSnapshot *out);

#endif
