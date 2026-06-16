#ifndef WORLD_SIM_DECISION_SNAPSHOT_H
#define WORLD_SIM_DECISION_SNAPSHOT_H

#include "sim/expansion.h"
#include "sim/stability_decision.h"
#include "sim/war_desire.h"

typedef struct {
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

void decision_snapshot_for_civ(int civ_id, DecisionSnapshot *out);
void decision_snapshot_cache_reset(void);
void decision_snapshot_cache_mark_dirty(int civ_id);
void decision_snapshot_cache_mark_all_dirty(void);
void decision_snapshot_cache_update_budgeted(int max_civs);
int decision_snapshot_cached(int civ_id, DecisionSnapshot *out);
int decision_snapshot_cache_valid_count(void);
int decision_snapshot_cache_dirty_count(void);
int decision_snapshot_cache_last_update_ms(void);
int decision_snapshot_cache_last_update_count(void);

#endif
