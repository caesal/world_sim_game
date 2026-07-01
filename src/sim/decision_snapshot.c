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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    int valid;
    int dirty;
    int uid;
    DecisionSnapshot snapshot;
    char main_intent[32];
    char expansion_reason[128];
    char war_reason[128];
} DecisionSnapshotCacheEntry;

static DecisionSnapshotCacheEntry decision_cache[MAX_CIVS];
static int decision_cache_cursor;
static int decision_cache_last_update_ms;
static int decision_cache_last_update_count;

static int years_to_decade_check(void) {
    int years_left = 25 - (year % 25);
    return years_left <= 0 ? 25 : years_left;
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

static void bind_cache_strings(DecisionSnapshotCacheEntry *entry) {
    entry->snapshot.main_intent = entry->main_intent;
    entry->snapshot.expansion_reason = entry->expansion_reason;
    entry->snapshot.war_reason = entry->war_reason;
}

static void store_cached_decision(int civ_id, const DecisionSnapshot *snapshot) {
    DecisionSnapshotCacheEntry *entry;

    if (civ_id < 0 || civ_id >= MAX_CIVS || !snapshot) return;
    entry = &decision_cache[civ_id];
    entry->snapshot = *snapshot;
    snprintf(entry->main_intent, sizeof(entry->main_intent), "%s",
             snapshot->main_intent ? snapshot->main_intent : "");
    snprintf(entry->expansion_reason, sizeof(entry->expansion_reason), "%s",
             snapshot->expansion_reason ? snapshot->expansion_reason : "");
    snprintf(entry->war_reason, sizeof(entry->war_reason), "%s",
             snapshot->war_reason ? snapshot->war_reason : "");
    bind_cache_strings(entry);
    entry->uid = civs[civ_id].uid;
    entry->valid = 1;
    entry->dirty = 0;
}

static int cache_entry_needs_update(int civ_id) {
    DecisionSnapshotCacheEntry *entry;

    if (civ_id < 0 || civ_id >= civ_count || civ_id >= MAX_CIVS || !civs[civ_id].alive) return 0;
    entry = &decision_cache[civ_id];
    return !entry->valid || entry->dirty || entry->uid != civs[civ_id].uid;
}

void decision_snapshot_for_civ(int civ_id, DecisionSnapshot *out) {
    int resource_score;
    int expansion;
    int war;
    int stability;
    int effective_disorder;
    TerritoryIntegrityStats integrity;

    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (civ_id < 0 || civ_id >= civ_count) return;

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
    if (effective_disorder >= 70) stability += 30;
    if (out->expansion.global_unowned_percent > 20 &&
        (out->expansion.nearby_unowned_regions +
         out->expansion.shallow_sea_reachable_regions +
         out->expansion.maritime_reachable_regions +
         out->expansion.deep_sea_reachable_regions) > 0) {
        war = war * 65 / 100;
    }

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

void decision_snapshot_cache_reset(void) {
    memset(decision_cache, 0, sizeof(decision_cache));
    decision_cache_cursor = 0;
    decision_cache_last_update_ms = 0;
    decision_cache_last_update_count = 0;
}

void decision_snapshot_cache_mark_dirty(int civ_id) {
    if (civ_id < 0 || civ_id >= MAX_CIVS) return;
    decision_cache[civ_id].dirty = 1;
}

void decision_snapshot_cache_mark_all_dirty(void) {
    int i;

    for (i = 0; i < MAX_CIVS; i++) decision_cache[i].dirty = 1;
    decision_cache_cursor = 0;
}

void decision_snapshot_cache_update_budgeted(int max_civs) {
    DWORD start = GetTickCount();
    int scanned = 0;
    int updated = 0;

    if (max_civs <= 0) max_civs = 1;
    while (scanned < MAX_CIVS && updated < max_civs) {
        int civ_id = (decision_cache_cursor + scanned) % MAX_CIVS;
        scanned++;
        if (civ_id >= civ_count || !civs[civ_id].alive) {
            decision_cache[civ_id].valid = 0;
            decision_cache[civ_id].dirty = 0;
            continue;
        }
        if (!cache_entry_needs_update(civ_id)) continue;
        {
            DecisionSnapshot snapshot;
            decision_snapshot_for_civ(civ_id, &snapshot);
            store_cached_decision(civ_id, &snapshot);
            updated++;
        }
    }
    decision_cache_cursor = (decision_cache_cursor + scanned) % MAX_CIVS;
    decision_cache_last_update_ms = (int)(GetTickCount() - start);
    decision_cache_last_update_count = updated;
}

int decision_snapshot_cached(int civ_id, DecisionSnapshot *out) {
    DecisionSnapshotCacheEntry *entry;

    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    if (civ_id < 0 || civ_id >= civ_count || civ_id >= MAX_CIVS || !civs[civ_id].alive) return 0;
    entry = &decision_cache[civ_id];
    if (!entry->valid || entry->dirty || entry->uid != civs[civ_id].uid) return 0;
    bind_cache_strings(entry);
    *out = entry->snapshot;
    decision_snapshot_refresh_countdowns(civ_id, out);
    return 1;
}

int decision_snapshot_cache_valid_count(void) {
    int i;
    int count = 0;

    for (i = 0; i < civ_count && i < MAX_CIVS; i++) {
        if (civs[i].alive && decision_cache[i].valid &&
            !decision_cache[i].dirty && decision_cache[i].uid == civs[i].uid) count++;
    }
    return count;
}

int decision_snapshot_cache_dirty_count(void) {
    int i;
    int count = 0;

    for (i = 0; i < civ_count && i < MAX_CIVS; i++) {
        if (cache_entry_needs_update(i)) count++;
    }
    return count;
}

int decision_snapshot_cache_last_update_ms(void) { return decision_cache_last_update_ms; }
int decision_snapshot_cache_last_update_count(void) { return decision_cache_last_update_count; }
