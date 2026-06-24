#include "sim/diplomacy_relation_score.h"

#include "core/game_state.h"
#include "sim/alliance.h"
#include "sim/alliance_contact.h"
#include "sim/simulation.h"
#include "sim/war.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>

static DiplomacyRelationBreakdown breakdowns[MAX_CIVS][MAX_CIVS];
static int score_remainders_x100[MAX_CIVS][MAX_CIVS];
static int strength_cache[MAX_CIVS];
static int threat_cache[MAX_CIVS];
static DWORD update_start_tick;
static int last_pairs;
static int last_update_ms;

static int valid_alive(int civ_id) {
    return civ_id >= 0 && civ_id < civ_count && civ_id < MAX_CIVS && civs[civ_id].alive;
}

static int strength_for_civ(int civ_id) {
    CountrySummary s;
    Civilization *civ;
    if (!valid_alive(civ_id)) return 0;
    s = summarize_country(civ_id);
    civ = &civs[civ_id];
    return max(1, s.population / 800 + s.food * 2 + s.water * 2 + s.money * 2 +
               s.minerals * 2 + civ->military * 7 + civ->production * 4 +
               civ->logistics * 4 + civ->cohesion * 3);
}

static void compute_threats(void) {
    int a, b;
    for (a = 0; a < MAX_CIVS; a++) threat_cache[a] = -1;
    for (a = 0; a < civ_count && a < MAX_CIVS; a++) {
        int best = -1, best_strength = 0, own = max(1, alliance_defensive_bloc_power(a));
        if (!valid_alive(a)) continue;
        for (b = 0; b < civ_count && b < MAX_CIVS; b++) {
            int strength;
            if (a == b || !valid_alive(b)) continue;
            strength = strength_cache[b];
            if (strength * 100 < own * 150) continue;
            if (strength > best_strength) { best = b; best_strength = strength; }
        }
        threat_cache[a] = best;
    }
}

void diplomacy_relation_score_reset(void) {
    memset(breakdowns, 0, sizeof(breakdowns));
    memset(score_remainders_x100, 0, sizeof(score_remainders_x100));
    memset(strength_cache, 0, sizeof(strength_cache));
    memset(threat_cache, -1, sizeof(threat_cache));
    last_pairs = 0;
    last_update_ms = 0;
}

void diplomacy_relation_score_clear_civ(int civ_id) {
    int i;
    if (civ_id < 0 || civ_id >= MAX_CIVS) return;
    for (i = 0; i < MAX_CIVS; i++) {
        memset(&breakdowns[civ_id][i], 0, sizeof(breakdowns[civ_id][i]));
        memset(&breakdowns[i][civ_id], 0, sizeof(breakdowns[i][civ_id]));
        score_remainders_x100[civ_id][i] = 0;
        score_remainders_x100[i][civ_id] = 0;
    }
}

void diplomacy_relation_score_reset_pair(int civ_a, int civ_b) {
    if (civ_a < 0 || civ_b < 0 || civ_a >= MAX_CIVS || civ_b >= MAX_CIVS) return;
    score_remainders_x100[civ_a][civ_b] = 0;
    score_remainders_x100[civ_b][civ_a] = 0;
    memset(&breakdowns[civ_a][civ_b], 0, sizeof(breakdowns[civ_a][civ_b]));
    memset(&breakdowns[civ_b][civ_a], 0, sizeof(breakdowns[civ_b][civ_a]));
}

void diplomacy_relation_score_begin_year(void) {
    int i;
    update_start_tick = GetTickCount();
    last_pairs = 0;
    memset(breakdowns, 0, sizeof(breakdowns));
    for (i = 0; i < civ_count && i < MAX_CIVS; i++) strength_cache[i] = strength_for_civ(i);
    compute_threats();
}

void diplomacy_relation_score_end_year(void) {
    last_update_ms = (int)(GetTickCount() - update_start_tick);
}

static void add_factor(DiplomacyRelationBreakdown *b, int id, int delta_x100, int value) {
    int i, slot = -1, weakest = -1, weakest_magnitude = 0, magnitude = abs(delta_x100);
    if (!b || id == DIP_REL_FACTOR_NONE || delta_x100 == 0) return;
    b->yearly_delta_x100 += delta_x100;
    if (delta_x100 > 0) b->positive_x100 += delta_x100;
    else b->negative_x100 += -delta_x100;
    for (i = 0; i < DIP_REL_FACTOR_SLOTS; i++) {
        if (b->factor_ids[i] == DIP_REL_FACTOR_NONE) {
            if (slot < 0) slot = i;
            continue;
        }
        if (weakest < 0 || abs(b->factor_delta_x100[i]) < weakest_magnitude) {
            weakest = i;
            weakest_magnitude = abs(b->factor_delta_x100[i]);
        }
    }
    if (slot < 0 && weakest >= 0 && magnitude > weakest_magnitude) slot = weakest;
    if (slot >= 0) {
        b->factor_ids[slot] = id;
        b->factor_delta_x100[slot] = delta_x100;
        b->factor_values[slot] = value;
    }
}

static int shared_active_enemy(int civ_a, int civ_b) {
    int i;
    if (!war_active_for_civ(civ_a) || !war_active_for_civ(civ_b)) return 0;
    for (i = 0; i < civ_count && i < MAX_CIVS; i++) {
        if (i != civ_a && i != civ_b && war_active_between(civ_a, i) &&
            war_active_between(civ_b, i)) return 1;
    }
    return 0;
}

static void add_power_factor(DiplomacyRelationBreakdown *b, int civ_a, int civ_b) {
    int own = max(1, alliance_defensive_bloc_power(civ_a));
    int ratio = strength_cache[civ_b] * 100 / own;
    if (ratio >= 400) add_factor(b, DIP_REL_FACTOR_POWER, -400, ratio);
    else if (ratio >= 250) add_factor(b, DIP_REL_FACTOR_POWER, -250, ratio);
    else if (ratio >= 150) add_factor(b, DIP_REL_FACTOR_POWER, -150, ratio);
}

static void add_contempt_factor(DiplomacyRelationBreakdown *b, int civ_a, int civ_b) {
    int own = strength_cache[civ_a];
    int target = strength_cache[civ_b];
    int ratio;
    if (own <= 0 || target <= 0) return;
    ratio = target * 100 / own;
    if (ratio <= 45) add_factor(b, DIP_REL_FACTOR_CONTEMPT, -200, ratio);
}

int diplomacy_relation_score_from_legacy(int score) {
    return clamp(score - 50, -100, 100);
}

int diplomacy_relation_score_apply_year(int civ_a, int civ_b, DiplomacyRelation relation) {
    DiplomacyRelationBreakdown b;
    int total_x100, new_score, active_delta, alliance_contact;
    memset(&b, 0, sizeof(b));
    if (!valid_alive(civ_a) || !valid_alive(civ_b) || relation.state == DIPLOMACY_NONE) {
        breakdowns[civ_a][civ_b] = b;
        return 0;
    }
    alliance_contact = alliance_diplomatic_contact_between(civ_a, civ_b);
    if (relation.state == DIPLOMACY_ALLIANCE) add_factor(&b, DIP_REL_FACTOR_ALLIANCE, 100, 1);
    else if (relation.contact_kind != DIP_CONTACT_NONE || alliance_contact)
        add_factor(&b, DIP_REL_FACTOR_CONTACT, 50, alliance_contact ? 4 : relation.contact_kind);
    if ((relation.contact_kind != DIP_CONTACT_NONE || alliance_contact) &&
        civs[civ_a].heritage == civs[civ_b].heritage)
        add_factor(&b, DIP_REL_FACTOR_HERITAGE, 10, 15);
    if (relation.trade_fit >= 70) add_factor(&b, DIP_REL_FACTOR_TRADE, 200, relation.trade_fit);
    else if (relation.trade_fit >= 40) add_factor(&b, DIP_REL_FACTOR_TRADE, 100, relation.trade_fit);
    if (relation.state != DIPLOMACY_TRUCE && relation.state != DIPLOMACY_WAR) {
        if (relation.years_known >= 200) add_factor(&b, DIP_REL_FACTOR_LONG_PEACE, 150, relation.years_known);
        else if (relation.years_known >= 75) add_factor(&b, DIP_REL_FACTOR_LONG_PEACE, 75, relation.years_known);
    }
    if (shared_active_enemy(civ_a, civ_b)) add_factor(&b, DIP_REL_FACTOR_SHARED_WAR, 300, 1);
    if (threat_cache[civ_a] >= 0 && threat_cache[civ_a] == threat_cache[civ_b]) {
        if (threat_cache[civ_a] != civ_a && threat_cache[civ_a] != civ_b)
            add_factor(&b, DIP_REL_FACTOR_SHARED_THREAT, 100, threat_cache[civ_a]);
    }
    add_power_factor(&b, civ_a, civ_b);
    add_contempt_factor(&b, civ_a, civ_b);
    if (relation.resource_conflict >= 75) add_factor(&b, DIP_REL_FACTOR_RESOURCE, -200, relation.resource_conflict);
    else if (relation.resource_conflict >= 45) add_factor(&b, DIP_REL_FACTOR_RESOURCE, -100, relation.resource_conflict);
    if (relation.border_tension >= 75) add_factor(&b, DIP_REL_FACTOR_BORDER, -150, relation.border_tension);
    else if (relation.border_tension >= 45) add_factor(&b, DIP_REL_FACTOR_BORDER, -75, relation.border_tension);
    if (relation.state == DIPLOMACY_TRUCE && relation.relation_score < 0) {
        add_factor(&b, DIP_REL_FACTOR_TRUCE_RECOVERY, 25, relation.truce_years_left);
    }
    if (relation.state != DIPLOMACY_TRUCE && relation.state != DIPLOMACY_WAR &&
        relation.last_war_result != DIP_LAST_WAR_NONE) {
        if (relation.easing_years < 50) add_factor(&b, DIP_REL_FACTOR_WAR_MEMORY, -75, relation.easing_years);
        else if (relation.easing_years < 100) add_factor(&b, DIP_REL_FACTOR_WAR_MEMORY, -25, relation.easing_years);
    }
    active_delta = b.positive_x100 - b.negative_x100;
    if (relation.state == DIPLOMACY_PEACE && relation.truce_years_left <= 0 &&
        relation.last_war_result == DIP_LAST_WAR_NONE && relation.trade_fit < 35 &&
        relation.border_tension < 35 && relation.resource_conflict < 35 &&
        active_delta >= -25 && active_delta <= 25 && relation.relation_score != 0) {
        add_factor(&b, DIP_REL_FACTOR_QUIET_DRIFT, relation.relation_score > 0 ? -25 : 25, 0);
    }
    total_x100 = relation.relation_score * 100 + score_remainders_x100[civ_a][civ_b] + b.yearly_delta_x100;
    total_x100 = clamp(total_x100, -10000, 10000);
    new_score = total_x100 / 100;
    if (relation.state == DIPLOMACY_TRUCE && relation.relation_score < 0 && new_score > 0) new_score = 0;
    score_remainders_x100[civ_a][civ_b] = total_x100 - new_score * 100;
    breakdowns[civ_a][civ_b] = b;
    last_pairs++;
    return clamp(new_score, -100, 100);
}

DiplomacyRelationBreakdown diplomacy_relation_breakdown(int civ_a, int civ_b) {
    DiplomacyRelationBreakdown empty;
    memset(&empty, 0, sizeof(empty));
    if (civ_a < 0 || civ_b < 0 || civ_a >= MAX_CIVS || civ_b >= MAX_CIVS) return empty;
    return breakdowns[civ_a][civ_b];
}

int diplomacy_relation_score_last_pairs(void) { return last_pairs; }
int diplomacy_relation_score_last_update_ms(void) { return last_update_ms; }
