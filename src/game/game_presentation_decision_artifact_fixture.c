#include "game/game_presentation_decision_artifact_fixture.h"

#include "sim/stability_decision.h"
#include "sim/war_desire.h"

#include <stdio.h>
#include <string.h>

static void fill_decision(SnapshotCiv *civ, int late) {
    DecisionSnapshot *decision = &civ->decision;
    DecisionStabilityBreakdown *stability = &decision->stability_breakdown;
    ExpansionAIDiagnostics *expansion = &decision->expansion;

    civ->alive = 1;
    civ->id = late ? 199 : 0;
    civ->uid = late ? 9199 : 9000;
    civ->color = late ? RGB(72, 139, 191) : RGB(183, 103, 76);
    civ->current_soldiers = late ? 12800 : 7400;
    civ->treasury = late ? 8600 : 5100;
    civ->summary.population = late ? 640000 : 280000;
    snprintf(civ->name_en, sizeof(civ->name_en), "%s",
             late ? "Late Meridian" : "Early Meridian");
    snprintf(civ->name_zh, sizeof(civ->name_zh), "%s",
             late ? "后期经线国" : "前期经线国");
    snprintf(civ->main_intent, sizeof(civ->main_intent), "%s",
             late ? "War" : "Expansion");
    snprintf(civ->decision_expansion_reason,
             sizeof(civ->decision_expansion_reason), "%s",
             late ? "Claimed nearby region" : "Claimed adjacent region");
    snprintf(civ->decision_war_reason, sizeof(civ->decision_war_reason),
             "%s", late ? "Ready at frontier" : "Below threshold");

    decision->published_revision = late ? UINT64_C(7000000199) :
                                          UINT64_C(7000000001);
    decision->city_slots_remaining = late ? 3 : 5;
    decision->city_capacity_ready = 1;
    decision->main_intent = civ->main_intent;
    decision->expansion_reason = civ->decision_expansion_reason;
    decision->war_reason = civ->decision_war_reason;
    decision->expansion_weight = late ? 58 : 76;
    decision->war_weight = late ? 81 : 49;
    decision->stability_weight = late ? 67 : 73;
    decision->next_expansion_months = late ? 9 : 4;
    decision->next_diplomacy_months = late ? 5 : 8;
    decision->next_battle_months = late ? 2 : 7;
    decision->next_collapse_years = late ? 3 : 11;
    decision->capital_region = late ? 802 : 14;
    decision->capital_connected_regions = late ? 17 : 11;
    decision->owned_regions = late ? 21 : 13;
    decision->disconnected_components = late ? 2 : 1;
    decision->longest_disconnected_months = late ? 74 : 19;
    decision->capital_connected_percent = late ? 81 : 85;
    decision->capital_core_port_count = late ? 4 : 2;
    decision->capital_core_network_count = late ? 3 : 2;
    decision->disconnected_has_port = 1;
    decision->disconnected_has_network = late ? 0 : 1;
    decision->disconnected_network_matches_capital = late ? 0 : 1;

    expansion->land_adjacent_unowned_regions = late ? 3 : 6;
    expansion->land_nearby_unowned_regions = late ? 8 : 4;
    expansion->shallow_sea_reachable_regions = late ? 5 : 2;
    expansion->maritime_reachable_regions = late ? 7 : 3;
    expansion->deep_sea_reachable_regions = late ? 4 : 1;
    expansion->port_candidate_regions = late ? 9 : 5;
    expansion->global_unowned_regions = late ? 97 : 143;
    expansion->global_unowned_percent = late ? 24 : 38;
    expansion->population_pressure = late ? 63 : 47;
    expansion->resource_pressure = late ? 56 : 34;
    expansion->expansion_need = late ? 78 : 66;
    expansion->expansion_threshold = 70;
    expansion->raw_expansion_desire = late ? 88 : 79;
    expansion->expansion_desire = late ? 69 : 74;
    expansion->stability_expansion_penalty = late ? 19 : 5;
    expansion->stability_gate_mode = late ? STABILITY_MODE_CRISIS :
                                            STABILITY_MODE_CAUTIOUS;
    expansion->stability_blocked = late;
    expansion->tech_expansion_percent = late ? 82 : 58;
    expansion->claim_cooldown_months = 18;
    expansion->months_until_next_claim = late ? 9 : 4;
    expansion->claim_budget = late ? 2 : 3;
    expansion->maritime.own_port_count = late ? 5 : 2;
    expansion->maritime.port_candidate_regions = late ? 9 : 5;
    expansion->maritime.shallow_reachable_regions = late ? 5 : 2;
    expansion->maritime.maritime_reachable_regions = late ? 7 : 3;
    expansion->maritime.deep_reachable_regions = late ? 4 : 1;
    expansion->maritime.blocked_no_shallow_path = late ? 2 : 1;

    decision->war_desire = late ? 81 : 57;
    decision->war_pre_stability_desire = late ? 94 : 62;
    decision->war_raw_desire = late ? 75 : 57;
    decision->war_threshold = 70;
    decision->war_readiness_percent = late ? 86 : 64;
    decision->war_readiness_cap = late ? 90 : 75;
    decision->war_readiness_cap_applied = 1;
    decision->war_aggression_score = late ? 24 : 15;
    decision->war_border_score = late ? 18 : 12;
    decision->war_resource_score = late ? 11 : 8;
    decision->war_crisis_score = late ? 16 : 5;
    decision->war_open_target_count = late ? 4 : 2;
    decision->war_strength_score = late ? 21 : 9;
    decision->war_trade_penalty = late ? 7 : 4;
    decision->war_truce_penalty = late ? 0 : 6;
    decision->war_post_war_cooldown_penalty = late ? 3 : 0;
    decision->war_disorder_penalty = late ? 12 : 5;
    decision->war_frontier_penalty = late ? 0 : 8;
    decision->war_heritage_affinity_penalty = late ? 4 : 2;
    decision->war_stability_penalty = late ? 19 : 5;
    decision->war_own_soldiers = civ->current_soldiers;
    decision->war_enemy_soldiers = late ? 10200 : 8300;
    decision->war_result = late ? WAR_DESIRE_RESULT_READY :
                                  WAR_DESIRE_RESULT_BELOW_THRESHOLD;

    stability->effective_disorder = late ? 7 : 41;
    stability->resource_pressure = late ? 6 : 34;
    stability->base_contribution = late ? 7 : 41;
    stability->war_status_contribution = late ? 12 : 0;
    stability->territory_fragmentation_contribution = late ? 48 : 24;
    stability->capital_connectivity_contribution = 0;
    stability->vassal_governance_contribution = late ? 0 : 8;
    stability->high_disorder_contribution = 0;
    stability->raw_total = late ? 67 : 73;
    stability->final_intent = late ? 67 : 73;
    decision->stability_pressure = stability->base_contribution;
    decision->stability_mode = late ? STABILITY_MODE_CRISIS :
                                      STABILITY_MODE_CAUTIOUS;
    decision->stability_mode_months = late ? 27 : 8;
    decision->stability_recovery_months = late ? 14 : 5;
    decision->stability_expansion_penalty = late ? 19 : 5;
    decision->stability_expansion_blocked = late;
    decision->stability_peace_bonus = late ? 23 : 7;
    decision->stability_allows_war = !late;
    decision->stability_allows_expansion = 1;
    snprintf(decision->stability_reason, sizeof(decision->stability_reason),
             "%s", late ? "Crisis recovery" : "Cautious recovery");
}

void game_presentation_decision_artifact_fill_fixture(
    RenderSnapshot *snapshot) {
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->world_generated = 1;
    snapshot->map_w = 96;
    snapshot->map_h = 64;
    snapshot->year = 42;
    snapshot->month = 7;
    snapshot->civ_count = MAX_CIVS;
    snapshot->civ_alive_count = 2;
    snapshot->civs_revision = 1201;
    snapshot->revision = 1202;
    fill_decision(&snapshot->civs[0], 0);
    fill_decision(&snapshot->civs[199], 1);
}
