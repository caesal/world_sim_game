#include "sim/war_desire.h"

#include "core/game_state.h"
#include "sim/economy.h"
#include "sim/expansion.h"
#include "sim/simulation.h"
#include "sim/stability_decision.h"
#include "sim/war.h"
#include "sim/war_front.h"

#include <stdio.h>
#include <string.h>

static WarDesireBreakdown last_breakdowns[MAX_CIVS];

static int resource_deficit_value(int value, int target) {
    return clamp(target - value, 0, target);
}

static int resource_need_score(int civ_id) {
    CountrySummary summary = summarize_country(civ_id);
    int score = 0;
    score += resource_deficit_value(summary.food, 5) * 3;
    score += resource_deficit_value(summary.water, 5) * 3;
    score += resource_deficit_value(summary.minerals, 5) * 2;
    score += resource_deficit_value(summary.wood, 5) * 2;
    score += resource_deficit_value(summary.money, 5) * 2;
    score += civs[civ_id].resource_pressure / 5;
    return clamp(score, 0, 30);
}

static int crisis_score_for_civ(int civ_id, int population_pressure) {
    Civilization *civ = &civs[civ_id];
    int population_over = max(0, population_pressure - 100);
    int score = clamp(civ->resource_pressure, 0, 100) * 35 / 100;
    score += clamp(population_over * 2, 0, 30);
    score += clamp(civ->treasury_deficit_years * 4, 0, 20);
    if (civ->treasury_last_deficit > 0) score += 8;
    if (civ->treasury <= 0 && civ->treasury_last_deficit > 0) score += 10;
    return clamp(score, 0, 45);
}

static int country_strength_score(int civ_id) {
    CountrySummary summary = summarize_country(civ_id);
    Civilization *civ = &civs[civ_id];
    return summary.population / 800 + summary.food * 2 + summary.water * 2 + summary.money * 2 +
           summary.minerals * 2 + civ->military * 7 + civ->production * 4 +
           civ->logistics * 4 + civ->cohesion * 3 - economy_effective_disorder_for_civ(civ_id) / 2;
}

static int readiness_cap_for_ratio(int ratio_percent, int extreme_pressure) {
    if (ratio_percent < 35) return 55;
    if (ratio_percent < 60) return extreme_pressure ? 75 : 65;
    if (ratio_percent < 85) return extreme_pressure ? 85 : 75;
    return 100;
}

static void set_reason(WarDesireBreakdown *out, const char *reason) {
    snprintf(out->reason, sizeof(out->reason), "%s", reason ? reason : "");
}

static void store_last(int civ_id, WarDesireBreakdown out) {
    if (civ_id < 0 || civ_id >= MAX_CIVS) return;
    if (out.final_desire >= last_breakdowns[civ_id].final_desire ||
        out.raw_desire > last_breakdowns[civ_id].raw_desire) {
        last_breakdowns[civ_id] = out;
    }
}

WarDesireBreakdown war_desire_calculate(int civ_a, int civ_b, DiplomacyRelation relation) {
    WarDesireBreakdown out;
    ExpansionAIDiagnostics expansion_ai;
    int sea_targets;
    int open_targets;
    int strength_delta;
    int extreme_pressure;
    int desire;

    memset(&out, 0, sizeof(out));
    out.threshold = 70;
    out.readiness_cap = 100;
    if (civ_a < 0 || civ_a >= civ_count || civ_b < 0 || civ_b >= civ_count ||
        !civs[civ_a].alive || !civs[civ_b].alive) {
        out.result = WAR_DESIRE_RESULT_NONE;
        set_reason(&out, "No war decision yet.");
        store_last(civ_a, out);
        return out;
    }

    out.resource_score = resource_need_score(civ_a);
    expansion_ai = expansion_ai_diagnostics(civ_a, expansion_resource_score_for_civ(civ_a));
    out.population_pressure = expansion_ai.population_pressure;
    out.resource_pressure = expansion_ai.resource_pressure;
    out.crisis_score = crisis_score_for_civ(civ_a, expansion_ai.population_pressure);
    out.global_unowned_percent = expansion_ai.global_unowned_percent;
    out.aggression_score = civs[civ_a].aggression * 4;
    out.border_score = relation.border_tension / 2;
    desire = out.aggression_score + out.border_score + out.resource_score + out.crisis_score;
    strength_delta = country_strength_score(civ_a) - country_strength_score(civ_b);
    if (strength_delta > 0) out.strength_score = clamp(strength_delta / 8, 0, 25);
    desire += out.strength_score;

    out.trade_penalty = (relation.trade_fit * 3) / 5;
    out.truce_penalty = relation.truce_years_left > 0 ? 50 : 0;
    out.disorder_penalty = economy_effective_disorder_for_civ(civ_a) / 2;
    out.heritage_affinity_penalty = civs[civ_a].heritage == civs[civ_b].heritage ? 8 : 0;
    desire -= out.trade_penalty + out.truce_penalty + out.disorder_penalty + out.heritage_affinity_penalty;
    out.own_soldiers = war_current_soldiers_for_civ(civ_a);
    out.enemy_soldiers = war_current_soldiers_for_civ(civ_b);
    out.readiness_percent = clamp(out.own_soldiers * 100 / max(1, out.enemy_soldiers), 0, 999);

    sea_targets = expansion_ai.shallow_sea_reachable_regions + expansion_ai.maritime_reachable_regions +
                  expansion_ai.deep_sea_reachable_regions;
    open_targets = expansion_ai.nearby_unowned_regions + sea_targets;
    out.open_target_count = open_targets;
    out.pre_stability_desire = clamp(desire, 0, 100);
    if (!war_has_active_front(civ_a, civ_b)) {
        out.result = WAR_DESIRE_RESULT_NO_FRONT;
        out.raw_desire = 0;
        out.final_desire = 0;
        set_reason(&out, "No active front.");
        store_last(civ_a, out);
        return out;
    }
    if (expansion_ai.global_unowned_percent >= 35 && open_targets > 0) {
        out.frontier_penalty = 100;
        desire = 0;
    } else if (open_targets > 0 &&
               relation.border_tension < 95 && out.resource_score < 28) {
        out.frontier_penalty = clamp(expansion_ai.land_adjacent_unowned_regions * 18 +
                                     expansion_ai.land_nearby_unowned_regions * 7 +
                                     expansion_ai.shallow_sea_reachable_regions * 10 +
                                     expansion_ai.maritime_reachable_regions * 8 +
                                     expansion_ai.deep_sea_reachable_regions * 4 +
                                     expansion_ai.global_unowned_percent * 2, 0, 100);
        desire -= out.frontier_penalty;
    }
    out.pre_stability_desire = clamp(desire, 0, 100);
    out.raw_desire = stability_apply_war_desire_gate(civ_a, civ_b, out.pre_stability_desire,
                                                     &out.stability_penalty,
                                                     &out.stability_blocked);
    out.stability_mode = stability_mode_for_civ(civ_a);
    extreme_pressure = relation.border_tension >= 95 || out.resource_score >= 28 || out.crisis_score >= 32;
    out.readiness_cap = readiness_cap_for_ratio(out.readiness_percent, extreme_pressure);
    out.readiness_cap_applied = out.readiness_cap < 100 && out.raw_desire > out.readiness_cap;
    out.final_desire = clamp(min(out.raw_desire, out.readiness_cap), 0, 100);

    if (out.stability_blocked) out.result = WAR_DESIRE_RESULT_STABILITY;
    else if (out.truce_penalty > 0) out.result = WAR_DESIRE_RESULT_TRUCE;
    else if (out.frontier_penalty > 0) out.result = WAR_DESIRE_RESULT_FRONTIER;
    else if (out.readiness_cap_applied) out.result = WAR_DESIRE_RESULT_LOW_READINESS;
    else if (out.final_desire >= out.threshold) out.result = WAR_DESIRE_RESULT_READY;
    else out.result = WAR_DESIRE_RESULT_BELOW_THRESHOLD;

    if (out.stability_blocked) set_reason(&out, "Stability gate blocks proactive war.");
    else if (out.result == WAR_DESIRE_RESULT_LOW_READINESS) set_reason(&out, "Military readiness caps war desire.");
    else if (out.result == WAR_DESIRE_RESULT_FRONTIER) set_reason(&out, "Reachable expansion targets suppress war.");
    else if (out.result == WAR_DESIRE_RESULT_READY) set_reason(&out, "War desire reaches the declaration threshold.");
    else if (out.result == WAR_DESIRE_RESULT_TRUCE) set_reason(&out, "Truce blocks a new war.");
    else if (out.crisis_score > 0 && out.open_target_count <= 0) {
        set_reason(&out, "Crisis pressure raises war desire; final score remains below threshold.");
    }
    else set_reason(&out, "War desire is below threshold.");
    store_last(civ_a, out);
    return out;
}

const WarDesireBreakdown *war_desire_last_breakdown(int civ_id) {
    static const WarDesireBreakdown empty;
    return (civ_id >= 0 && civ_id < MAX_CIVS) ? &last_breakdowns[civ_id] : &empty;
}

int war_desire_last_final(int civ_id) {
    return (civ_id >= 0 && civ_id < MAX_CIVS) ? last_breakdowns[civ_id].final_desire : 0;
}

const char *war_desire_last_reason(int civ_id) {
    const WarDesireBreakdown *out = war_desire_last_breakdown(civ_id);
    return out->reason[0] ? out->reason : "No war decision yet.";
}

void war_desire_reset_all(void) {
    memset(last_breakdowns, 0, sizeof(last_breakdowns));
}

void war_desire_clear_civ(int civ_id) {
    if (civ_id >= 0 && civ_id < MAX_CIVS) memset(&last_breakdowns[civ_id], 0, sizeof(last_breakdowns[civ_id]));
}
