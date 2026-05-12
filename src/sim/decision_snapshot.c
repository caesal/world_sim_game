#include "decision_snapshot.h"

#include "core/game_state.h"
#include "sim/collapse.h"
#include "sim/diplomacy.h"
#include "sim/disorder.h"
#include "sim/population.h"
#include "sim/territory_integrity.h"
#include "sim/vassal.h"
#include "sim/war.h"

#include <stdio.h>
#include <string.h>

static int years_to_decade_check(void) {
    int years_left = 25 - (year % 25);
    return years_left <= 0 ? 25 : years_left;
}

void decision_snapshot_for_civ(int civ_id, DecisionSnapshot *out) {
    int resource_score;
    int expansion;
    int war;
    int stability;
    TerritoryIntegrityStats integrity;

    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (civ_id < 0 || civ_id >= civ_count) return;

    resource_score = expansion_resource_score_for_civ(civ_id);
    out->expansion = expansion_ai_diagnostics(civ_id, resource_score);
    out->war_desire = diplomacy_last_war_desire(civ_id);
    territory_integrity_get_stats(civ_id, &integrity);
    out->stability_pressure = max(civs[civ_id].disorder, population_pressure_for_civ(civ_id));
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
    out->next_expansion_months = out->expansion.months_until_next_claim;
    out->next_diplomacy_months = 12 - ((month - 1) % 12);
    out->next_battle_months = 36 - (((year * 12 + month) - 1) % 36);
    out->next_collapse_years = civs[civ_id].disorder >= 100 ? 0 : years_to_decade_check();
    out->expansion_reason = expansion_last_reason(civ_id);
    out->war_reason = diplomacy_last_war_reason(civ_id);

    expansion = clamp(out->expansion.expansion_desire, 0, 160);
    war = clamp(out->war_desire, 0, 160);
    stability = clamp(out->stability_pressure, 0, 160);
    stability += integrity.disconnected_components * 24;
    if (integrity.owned_regions > 0 && integrity.capital_connected_percent < 80) {
        stability += 20 + (80 - integrity.capital_connected_percent);
    }
    stability += vassal_governance_disorder(civ_id) / 2;
    if (war_active_for_civ(civ_id)) stability += 12;
    if (out->expansion.land_adjacent_unowned_regions > 0 ||
        out->expansion.shallow_sea_reachable_regions > 0) {
        expansion += 20;
    }
    if (civs[civ_id].disorder >= 70) stability += 30;
    if (out->expansion.global_unowned_percent > 20) war = war * 65 / 100;

    out->expansion_weight = clamp(expansion, 0, 100);
    out->war_weight = clamp(war, 0, 100);
    out->stability_weight = clamp(stability, 0, 100);

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
    } else if (civs[civ_id].disorder >= 70) {
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
