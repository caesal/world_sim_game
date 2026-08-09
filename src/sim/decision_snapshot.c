#include "decision_snapshot.h"

#include "core/game_state.h"
#include "sim/collapse.h"
#include "sim/diplomacy.h"
#include "sim/disorder.h"
#include "sim/economy.h"
#include "sim/population.h"
#include "sim/stability_decision.h"
#include "sim/territory_integrity.h"
#include "sim/vassal.h"
#include "sim/war.h"

#include <stdio.h>
#include <string.h>

static int years_to_decade_check(void) {
    int years_left = 25 - (year % 25);
    return years_left <= 0 ? 25 : years_left;
}

void decision_stability_breakdown_calculate(
    int effective_disorder, int resource_pressure,
    int disconnected_components, int owned_regions,
    int capital_connected_percent, int vassal_governance_disorder,
    int war_active, DecisionStabilityBreakdown *out) {
    int raw_total;
    if (!out) return;
    memset(out, 0, sizeof(*out));
    out->effective_disorder = effective_disorder;
    out->resource_pressure = resource_pressure;
    out->base_contribution = clamp(max(effective_disorder, resource_pressure), 0, 160);
    out->war_status_contribution = war_active ? 12 : 0;
    out->territory_fragmentation_contribution = disconnected_components * 24;
    if (owned_regions > 0 && capital_connected_percent < 80) {
        out->capital_connectivity_contribution = 20 + (80 - capital_connected_percent);
    }
    out->vassal_governance_contribution = vassal_governance_disorder / 2;
    out->high_disorder_contribution = effective_disorder >= 70 ? 30 : 0;
    raw_total = out->base_contribution;
    raw_total += out->territory_fragmentation_contribution;
    raw_total += out->capital_connectivity_contribution;
    raw_total += out->vassal_governance_contribution;
    raw_total += out->war_status_contribution;
    raw_total += out->high_disorder_contribution;
    out->raw_total = raw_total;
    out->final_intent = clamp(raw_total, 0, 100);
}

void decision_snapshot_refresh_countdowns(int civ_id, DecisionSnapshot *out) {
    int effective_disorder;
    if (!out) return;
    if (civ_id < 0 || civ_id >= civ_count || civ_id >= MAX_CIVS || !civs[civ_id].alive) {
        out->expansion.months_until_next_claim = 0;
        out->next_expansion_months = 0;
        out->next_diplomacy_months = 0;
        out->next_battle_months = 0;
        out->next_collapse_years = 0;
        return;
    }
    effective_disorder = economy_effective_disorder_for_civ(civ_id);
    out->expansion.months_until_next_claim = expansion_civ_months_until_claim(civ_id);
    out->next_expansion_months = out->expansion.months_until_next_claim;
    out->next_diplomacy_months = 12 - ((month - 1) % 12);
    out->next_battle_months = WAR_BATTLE_INTERVAL_MONTHS -
                              (((year * 12 + month) - 1) % WAR_BATTLE_INTERVAL_MONTHS);
    out->next_collapse_years = effective_disorder >= 100 ? 0 : years_to_decade_check();
}

void decision_snapshot_for_civ(int civ_id, DecisionSnapshot *out) {
    int resource_score;
    int expansion;
    int war;
    int effective_disorder;
    int stability_vassal_governance;
    int stability_war_active;
    TerritoryIntegrityStats integrity;

    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (civ_id < 0 || civ_id >= civ_count) return;

    out->city_slots_remaining = max(0, MAX_CITIES - city_count);
    out->city_capacity_ready = out->city_slots_remaining > 0;
    resource_score = expansion_resource_score_for_civ(civ_id);
    out->expansion = expansion_ai_diagnostics(civ_id, resource_score);
    out->war_desire = diplomacy_last_war_desire(civ_id);
    {
        const WarDesireBreakdown *war_breakdown = war_desire_last_breakdown(civ_id);
        out->war_pre_stability_desire = war_breakdown->pre_stability_desire;
        out->war_raw_desire = war_breakdown->raw_desire;
        out->war_threshold = war_breakdown->threshold;
        out->war_readiness_percent = war_breakdown->readiness_percent;
        out->war_readiness_cap = war_breakdown->readiness_cap;
        out->war_readiness_cap_applied = war_breakdown->readiness_cap_applied;
        out->war_aggression_score = war_breakdown->aggression_score;
        out->war_border_score = war_breakdown->border_score;
        out->war_resource_score = war_breakdown->resource_score;
        out->war_population_pressure = war_breakdown->population_pressure;
        out->war_resource_pressure = war_breakdown->resource_pressure;
        out->war_crisis_score = war_breakdown->crisis_score;
        out->war_open_target_count = war_breakdown->open_target_count;
        out->war_global_unowned_percent = war_breakdown->global_unowned_percent;
        out->war_strength_score = war_breakdown->strength_score;
        out->war_trade_penalty = war_breakdown->trade_penalty;
        out->war_truce_penalty = war_breakdown->truce_penalty;
        out->war_post_war_cooldown_penalty = war_breakdown->post_war_cooldown_penalty;
        out->war_disorder_penalty = war_breakdown->disorder_penalty;
        out->war_frontier_penalty = war_breakdown->frontier_penalty;
        out->war_heritage_affinity_penalty = war_breakdown->heritage_affinity_penalty;
        out->war_stability_penalty = war_breakdown->stability_penalty;
        out->war_stability_blocked = war_breakdown->stability_blocked;
        out->war_own_soldiers = war_breakdown->own_soldiers;
        out->war_enemy_soldiers = war_breakdown->enemy_soldiers;
        out->war_result = war_breakdown->result;
    }
    territory_integrity_get_stats(civ_id, &integrity);
    effective_disorder = economy_effective_disorder_for_civ(civ_id);
    out->stability_pressure = max(effective_disorder, civs[civ_id].resource_pressure);
    out->stability_mode = stability_mode_for_civ(civ_id);
    out->stability_mode_months = stability_mode_months_for_civ(civ_id);
    out->stability_recovery_months = stability_recovery_months_remaining(civ_id);
    out->stability_expansion_penalty = out->expansion.stability_expansion_penalty;
    out->stability_expansion_blocked = out->expansion.stability_blocked;
    out->stability_peace_bonus = stability_peace_pressure_bonus(civ_id);
    out->stability_allows_war = stability_allows_new_war(civ_id, -1);
    out->stability_allows_expansion = stability_allows_expansion_attempt(civ_id);
    out->capital_region = integrity.capital_region;
    out->capital_connected_regions = integrity.capital_connected_regions;
    out->owned_regions = integrity.owned_regions;
    out->disconnected_components = integrity.disconnected_components;
    out->longest_disconnected_months = integrity.longest_disconnected_months;
    out->capital_connected_percent = integrity.capital_connected_percent;
    out->capital_core_port_count = integrity.capital_core_port_count;
    out->capital_core_network_count = integrity.capital_core_network_count;
    out->disconnected_has_port = integrity.disconnected_has_port;
    out->disconnected_has_network = integrity.disconnected_has_network;
    out->disconnected_network_matches_capital = integrity.disconnected_network_matches_capital;
    out->collapse_single_result = collapse_single_province_preview(civ_id, &out->collapse_single_candidate);
    decision_snapshot_refresh_countdowns(civ_id, out);
    out->expansion_reason = expansion_last_reason(civ_id);
    out->war_reason = diplomacy_last_war_reason(civ_id);

    expansion = clamp(out->expansion.expansion_desire, 0, 160);
    war = clamp(out->war_desire, 0, 160);
    stability_vassal_governance = vassal_governance_disorder(civ_id);
    stability_war_active = war_active_for_civ(civ_id);
    decision_stability_breakdown_calculate(
        effective_disorder, civs[civ_id].resource_pressure,
        integrity.disconnected_components, integrity.owned_regions,
        integrity.capital_connected_percent, stability_vassal_governance,
        stability_war_active, &out->stability_breakdown);
    if (out->expansion.land_adjacent_unowned_regions > 0 ||
        out->expansion.shallow_sea_reachable_regions > 0) {
        expansion += 20;
    }
    if (out->expansion.global_unowned_percent > 20 &&
        (out->expansion.nearby_unowned_regions +
         out->expansion.shallow_sea_reachable_regions +
         out->expansion.maritime_reachable_regions +
         out->expansion.deep_sea_reachable_regions) > 0) {
        war = war * 65 / 100;
    }

    out->expansion_weight = clamp(expansion, 0, 100);
    out->war_weight = clamp(war, 0, 100);
    out->stability_weight = out->stability_breakdown.final_intent;

    if (out->stability_weight >= out->expansion_weight &&
        out->stability_weight >= out->war_weight) {
        out->main_intent = "Stability";
    } else if (out->war_weight > out->expansion_weight) {
        out->main_intent = "War";
    } else {
        out->main_intent = "Expansion";
    }
    if (integrity.disconnected_components > 0) {
        snprintf(out->stability_reason, sizeof(out->stability_reason),
                 "Enclave disconnected for %d years %d months; capital core covers %d%%.",
                 integrity.longest_disconnected_months / 12,
                 integrity.longest_disconnected_months % 12,
                 integrity.capital_connected_percent);
    } else if (effective_disorder >= 70) {
        snprintf(out->stability_reason, sizeof(out->stability_reason),
                 "Disorder is high; avoid shocks while recovery continues.");
    } else if (war_active_for_civ(civ_id)) {
        snprintf(out->stability_reason, sizeof(out->stability_reason),
                 "War pressure is active; stability favors consolidation.");
    } else {
        snprintf(out->stability_reason, sizeof(out->stability_reason),
                 "Capital core connected; stability pressure is routine.");
    }
}
