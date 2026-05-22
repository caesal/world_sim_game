#include "sim/stability_decision.h"

#include "core/game_state.h"
#include "sim/regions.h"
#include "sim/territory_integrity.h"
#include "sim/vassal.h"

#include <string.h>

#define STABILITY_RECOVER_MONTHS 12

static StabilityMode stability_modes[MAX_CIVS];
static int stability_mode_months[MAX_CIVS];
static int stability_recover_months[MAX_CIVS];

static int valid_civ(int civ_id) {
    return civ_id >= 0 && civ_id < civ_count && civs[civ_id].alive;
}

static StabilityMode target_mode_for_disorder(int disorder) {
    if (disorder >= 100) return STABILITY_MODE_COLLAPSE;
    if (disorder >= 90) return STABILITY_MODE_EMERGENCY;
    if (disorder >= 75) return STABILITY_MODE_CRISIS;
    if (disorder >= 60) return STABILITY_MODE_REORGANIZING;
    if (disorder >= 45) return STABILITY_MODE_CAUTIOUS;
    return STABILITY_MODE_NORMAL;
}

static int threshold_for_mode(StabilityMode mode) {
    switch (mode) {
        case STABILITY_MODE_COLLAPSE: return 100;
        case STABILITY_MODE_EMERGENCY: return 90;
        case STABILITY_MODE_CRISIS: return 75;
        case STABILITY_MODE_REORGANIZING: return 60;
        case STABILITY_MODE_CAUTIOUS: return 45;
        default: return 0;
    }
}

void stability_decision_reset(void) {
    memset(stability_modes, 0, sizeof(stability_modes));
    memset(stability_mode_months, 0, sizeof(stability_mode_months));
    memset(stability_recover_months, 0, sizeof(stability_recover_months));
}

void stability_decision_update_month(int civ_id) {
    StabilityMode current;
    StabilityMode target;
    if (!valid_civ(civ_id)) {
        if (civ_id >= 0 && civ_id < MAX_CIVS) {
            stability_modes[civ_id] = STABILITY_MODE_NORMAL;
            stability_mode_months[civ_id] = 0;
            stability_recover_months[civ_id] = 0;
        }
        return;
    }
    current = stability_modes[civ_id];
    target = target_mode_for_disorder(civs[civ_id].disorder);
    if (target > current) {
        stability_modes[civ_id] = target;
        stability_mode_months[civ_id] = 1;
        stability_recover_months[civ_id] = 0;
        return;
    }
    if (target < current && civs[civ_id].disorder < threshold_for_mode(current)) {
        stability_recover_months[civ_id]++;
        if (stability_recover_months[civ_id] >= STABILITY_RECOVER_MONTHS) {
            stability_modes[civ_id] = target;
            stability_mode_months[civ_id] = 1;
            stability_recover_months[civ_id] = 0;
            return;
        }
    } else {
        stability_recover_months[civ_id] = 0;
    }
    stability_mode_months[civ_id]++;
}

void stability_decision_update_all(void) {
    int i;
    for (i = 0; i < civ_count; i++) stability_decision_update_month(i);
}

StabilityMode stability_mode_for_civ(int civ_id) {
    StabilityMode target;
    if (!valid_civ(civ_id)) return STABILITY_MODE_NORMAL;
    target = target_mode_for_disorder(civs[civ_id].disorder);
    return target > stability_modes[civ_id] ? target : stability_modes[civ_id];
}

int stability_mode_months_for_civ(int civ_id) {
    return civ_id >= 0 && civ_id < MAX_CIVS ? stability_mode_months[civ_id] : 0;
}

int stability_recovery_months_remaining(int civ_id) {
    if (civ_id < 0 || civ_id >= MAX_CIVS) return 0;
    if (stability_recover_months[civ_id] <= 0) return STABILITY_RECOVER_MONTHS;
    return max(0, STABILITY_RECOVER_MONTHS - stability_recover_months[civ_id]);
}

static int has_disconnected_land(int civ_id) {
    TerritoryIntegrityStats stats;
    territory_integrity_get_stats(civ_id, &stats);
    return stats.disconnected_components > 0;
}

static void owned_neighbor_counts(int civ_id, int region_id, int *connected, int *disconnected) {
    const NaturalRegion *region = regions_get(region_id);
    int i;
    *connected = 0;
    *disconnected = 0;
    if (!region) return;
    for (i = 0; i < region->neighbor_count; i++) {
        int n = region->neighbors[i];
        const NaturalRegion *near_region = regions_get(n);
        if (!near_region || near_region->owner_civ != civ_id) continue;
        if (territory_integrity_region_is_capital_connected(civ_id, n)) (*connected)++;
        else (*disconnected)++;
    }
}

static int war_is_core_connection(int civ_id, int target_id) {
    int i, j;
    if (!has_disconnected_land(civ_id)) return 0;
    if (target_id < 0) return 1;
    for (i = 0; i < region_count; i++) {
        const NaturalRegion *region = regions_get(i);
        if (!region || region->owner_civ != civ_id) continue;
        if (territory_integrity_region_is_capital_connected(civ_id, i)) continue;
        for (j = 0; j < region->neighbor_count; j++) {
            const NaturalRegion *near_region = regions_get(region->neighbors[j]);
            int owner = near_region ? near_region->owner_civ : -1;
            if (owner == target_id || vassal_root_overlord(owner) == target_id) return 1;
        }
    }
    return 0;
}

int stability_apply_war_desire_gate(int civ_id, int target_id, int desire,
                                    int *out_penalty, int *out_blocked) {
    StabilityMode mode = stability_mode_for_civ(civ_id);
    int gated = desire;
    if (out_blocked) *out_blocked = 0;
    if (mode == STABILITY_MODE_CAUTIOUS) {
        gated = desire * 35 / 100;
        if (!war_is_core_connection(civ_id, target_id)) gated = min(gated, 55);
    } else if (mode >= STABILITY_MODE_REORGANIZING) {
        gated = 0;
        if (out_blocked) *out_blocked = 1;
    }
    gated = clamp(gated, 0, 100);
    if (out_penalty) *out_penalty = max(0, desire - gated);
    return gated;
}

int stability_apply_expansion_desire_gate(int civ_id, int desire,
                                          int *out_penalty, int *out_blocked) {
    StabilityMode mode = stability_mode_for_civ(civ_id);
    int gated = desire;
    if (out_blocked) *out_blocked = 0;
    if (mode == STABILITY_MODE_CAUTIOUS) gated = desire / 2;
    else if (mode >= STABILITY_MODE_REORGANIZING) {
        gated = has_disconnected_land(civ_id) ? min(desire, 45) : 0;
        if (out_blocked && gated == 0) *out_blocked = 1;
    }
    gated = clamp(gated, 0, 160);
    if (out_penalty) *out_penalty = max(0, desire - gated);
    return gated;
}

int stability_allows_new_war(int civ_id, int target_id) {
    StabilityMode mode = stability_mode_for_civ(civ_id);
    if (mode >= STABILITY_MODE_REORGANIZING) return 0;
    if (mode == STABILITY_MODE_CAUTIOUS && !war_is_core_connection(civ_id, target_id)) return 0;
    return 1;
}

int stability_allows_expansion_attempt(int civ_id) {
    StabilityMode mode = stability_mode_for_civ(civ_id);
    if (mode <= STABILITY_MODE_CAUTIOUS) return 1;
    return mode < STABILITY_MODE_COLLAPSE && has_disconnected_land(civ_id);
}

int stability_allows_expansion_region(int civ_id, int region_id) {
    StabilityMode mode = stability_mode_for_civ(civ_id);
    int connected = 0;
    int disconnected = 0;
    if (mode == STABILITY_MODE_NORMAL) return 1;
    owned_neighbor_counts(civ_id, region_id, &connected, &disconnected);
    if (mode == STABILITY_MODE_CAUTIOUS) return connected > 0;
    return connected > 0 && disconnected > 0;
}

int stability_allows_overseas_expansion(int civ_id) {
    return stability_mode_for_civ(civ_id) == STABILITY_MODE_NORMAL;
}

int stability_peace_pressure_bonus(int civ_id) {
    switch (stability_mode_for_civ(civ_id)) {
        case STABILITY_MODE_REORGANIZING: return 18;
        case STABILITY_MODE_CRISIS: return 45;
        case STABILITY_MODE_EMERGENCY:
        case STABILITY_MODE_COLLAPSE: return 70;
        default: return 0;
    }
}

int stability_should_tail_cut_war(int civ_id) {
    return stability_mode_for_civ(civ_id) >= STABILITY_MODE_CRISIS;
}
