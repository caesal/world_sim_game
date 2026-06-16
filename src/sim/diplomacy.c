#include "diplomacy.h"
#include "sim/expansion.h"
#include "core/dirty_flags.h"
#include "sim/diplomacy_borders.h"
#include "sim/diplomacy_policy.h"
#include "sim/maritime.h"
#include "sim/regions.h"
#include "war.h"
#include "sim/diplomacy_relation_score.h"
#include "sim/diplomacy_stability.h"
#include "sim/war_front.h"
#include "sim/war_desire.h"
#include "sim/simulation.h"
#include "sim/vassal.h"
#include <stdio.h>
#include <string.h>
#ifndef DIPLOMACY_ENABLE_ADVANCED_STATES
#define DIPLOMACY_ENABLE_ADVANCED_STATES 1
#endif
static DiplomacyRelation diplomacy_matrix[MAX_CIVS][MAX_CIVS];
static int diplomacy_contacts_dirty = 1;
static void set_relation_pair_directional(int civ_a, int civ_b,
                                          DiplomacyRelation ab, DiplomacyRelation ba) {
    if (civ_a < 0 || civ_a >= MAX_CIVS || civ_b < 0 || civ_b >= MAX_CIVS || civ_a == civ_b) return;
    diplomacy_matrix[civ_a][civ_b] = ab;
    diplomacy_matrix[civ_b][civ_a] = ba;
    dirty_mark_diplomacy();
}
static void set_relation_pair_shared(int civ_a, int civ_b, DiplomacyRelation relation) {
    DiplomacyRelation ab = relation, ba = relation;
    if (civ_a < 0 || civ_a >= MAX_CIVS || civ_b < 0 || civ_b >= MAX_CIVS || civ_a == civ_b) return;
    if (diplomacy_matrix[civ_a][civ_b].state != DIPLOMACY_NONE) {
        ab.relation_score = diplomacy_matrix[civ_a][civ_b].relation_score;
    }
    if (diplomacy_matrix[civ_b][civ_a].state != DIPLOMACY_NONE) {
        ba.relation_score = diplomacy_matrix[civ_b][civ_a].relation_score;
    }
    set_relation_pair_directional(civ_a, civ_b, ab, ba);
}
static DiplomacyRelation default_relation(DiplomacyStatus state, int score) {
    DiplomacyRelation relation;
    memset(&relation, 0, sizeof(relation));
    relation.state = state;
    relation.relation_score = score;
    relation.years_known = state == DIPLOMACY_NONE ? 0 : 1;
    relation.overlord = -1;
    relation.vassal = -1;
    relation.last_war_winner = -1;
    relation.last_war_loser = -1;
    relation.last_war_result = DIP_LAST_WAR_NONE;
    return relation;
}
void diplomacy_reset(void) {
    int a;
    int b;
    diplomacy_borders_reset();
    war_desire_reset_all();
    diplomacy_relation_score_reset();
    diplomacy_stability_reset();
    diplomacy_contacts_dirty = 1;
    for (a = 0; a < MAX_CIVS; a++) {
        for (b = 0; b < MAX_CIVS; b++) {
            diplomacy_matrix[a][b] = default_relation(a == b ? DIPLOMACY_PEACE : DIPLOMACY_NONE,
                                                       a == b ? 100 : 0);
        }
    }
}
void diplomacy_mark_contacts_dirty(void) {
    diplomacy_borders_mark_dirty();
    diplomacy_contacts_dirty = 1;
    dirty_mark_diplomacy();
}
static int is_valid_civ(int civ_id) { return civ_id >= 0 && civ_id < civ_count && civs[civ_id].alive; }
static int is_sovereign_actor(int civ_id) { return is_valid_civ(civ_id) && vassal_overlord(civ_id) < 0; }
static int same_heritage(int civ_a, int civ_b) { return civs[civ_a].heritage == civs[civ_b].heritage; }
int diplomacy_land_contact_stats(int civ_a, int civ_b, int *border_length, int *natural_barrier) {
    return (civ_a < 0 || civ_a >= MAX_CIVS || civ_b < 0 || civ_b >= MAX_CIVS) ? 0 :
           diplomacy_pair_contact_stats(civ_a, civ_b, border_length, natural_barrier);
}
static int resource_deficit_value(int value, int target) { return clamp(target - value, 0, target); }
static int resource_surplus_value(int value, int target) { return clamp(value - target, 0, 10); }
static int trade_fit_one_way(CountrySummary needer, CountrySummary supplier) {
    int score = 0;
    if (needer.food < 5 && supplier.food > 6) score += 20;
    if (needer.water < 5 && supplier.water > 6) score += 20;
    if (needer.minerals < 5 && supplier.minerals > 6) score += 15;
    if (needer.wood < 5 && supplier.wood > 6) score += 12;
    if (needer.stone < 5 && supplier.stone > 6) score += 12;
    if (needer.money < 5 && supplier.money > 6) score += 10;
    if (needer.livestock < 5 && supplier.livestock > 6) score += 10;
    if (needer.pop_capacity < 5 && supplier.pop_capacity > 6) score += 8;
    return clamp(score, 0, 100);
}
static int abundant_resource_count(CountrySummary summary) {
    int count = 0;
    if (summary.food >= 6) count++;
    if (summary.livestock >= 6) count++;
    if (summary.wood >= 6) count++;
    if (summary.stone >= 6) count++;
    if (summary.minerals >= 6) count++;
    if (summary.water >= 6) count++;
    if (summary.pop_capacity >= 6) count++;
    if (summary.money >= 6) count++;
    return count;
}
static int compute_prosperity_trade(CountrySummary a, CountrySummary b) {
    int score = 0;
    if (a.food >= 6 && b.food >= 6) score += 5;
    if (a.water >= 6 && b.water >= 6) score += 5;
    if (a.pop_capacity >= 6 && b.pop_capacity >= 6) score += 5;
    if (a.money >= 6 && b.money >= 6) score += 6;
    if (a.ports > 0 && b.ports > 0) score += 8;
    return clamp(score, 0, 28);
}
static int compute_diversity_exchange(CountrySummary a, CountrySummary b) {
    int a_diversity = abundant_resource_count(a);
    int b_diversity = abundant_resource_count(b);
    int shared_diversity = a_diversity < b_diversity ? a_diversity : b_diversity;
    return clamp((shared_diversity - 2) * 5, 0, 20);
}
static int compute_trade_fit(int civ_a, int civ_b) {
    CountrySummary a = summarize_country(civ_a);
    CountrySummary b = summarize_country(civ_b);
    int fit = (trade_fit_one_way(a, b) + trade_fit_one_way(b, a)) / 2;
    int militarism = ((civs[civ_a].aggression + civs[civ_b].aggression) * 3) / 2;
    /* Rich neighbors can still have trade stability through prosperity and diversity. */
    fit += compute_prosperity_trade(a, b);
    fit += compute_diversity_exchange(a, b);
    fit += maritime_trade_bonus(civ_a, civ_b);
    if (same_heritage(civ_a, civ_b)) fit += 10;
    return clamp(fit - militarism, 0, 100);
}
static int compute_resource_conflict(int civ_a, int civ_b) {
    CountrySummary a = summarize_country(civ_a);
    CountrySummary b = summarize_country(civ_b);
    int conflict = 0;
    conflict += resource_deficit_value(a.food, 5) && resource_deficit_value(b.food, 5) ? 14 : 0;
    conflict += resource_deficit_value(a.water, 5) && resource_deficit_value(b.water, 5) ? 16 : 0;
    conflict += resource_deficit_value(a.minerals, 5) && resource_deficit_value(b.minerals, 5) ? 14 : 0;
    conflict += resource_deficit_value(a.wood, 5) && resource_deficit_value(b.wood, 5) ? 10 : 0;
    conflict += resource_deficit_value(a.stone, 5) && resource_deficit_value(b.stone, 5) ? 10 : 0;
    conflict += resource_deficit_value(a.money, 5) && resource_deficit_value(b.money, 5) ? 8 : 0;
    conflict += resource_surplus_value(a.minerals, 6) && resource_surplus_value(b.minerals, 6) ? 6 : 0;
    conflict += resource_surplus_value(a.money, 6) && resource_surplus_value(b.money, 6) ? 5 : 0;
    return clamp(conflict, 0, 100);
}
static int compute_border_tension(int civ_a, int civ_b, DiplomacyRelation relation) {
    int militarism = (civs[civ_a].aggression + civs[civ_b].aggression) * 3;
    int border_pressure = clamp(relation.border_length / 20, 0, 20);
    int barrier_relief = clamp(relation.natural_barrier / 10, 0, 28);
    int capital_id = civs[civ_a].capital_city;
    int blocked = 0;
    int tension;
    if (capital_id >= 0 && capital_id < city_count && cities[capital_id].alive) {
        blocked = world_nearby_enemy_border(civ_a, cities[capital_id].x, cities[capital_id].y, 8) ? 8 : 0;
    }
    tension = relation.resource_conflict + border_pressure + militarism + blocked -
              (relation.trade_fit * 3) / 5 - barrier_relief;
    if (same_heritage(civ_a, civ_b)) tension -= 8;
    if (relation.border_length <= 0) tension = tension * 2 / 5;
    return clamp(tension, 0, 100);
}
static int war_desire_for_pair(int civ_a, int civ_b, DiplomacyRelation relation) {
    return war_desire_calculate(civ_a, civ_b, relation).final_desire;
}

static void log_relation_transition(int civ_a, int civ_b, DiplomacyStatus old_state, DiplomacyStatus new_state) {
    if (old_state == new_state) return;
    if (new_state == DIPLOMACY_ALLIANCE) {
        event_log_push_structured(EVENT_TYPE_DIPLOMACY_ALLIANCE, EVENT_SEVERITY_INFO,
                                  civ_a, civ_b, -1, -1, 0, 0, "");
    } else if (old_state == DIPLOMACY_ALLIANCE) {
        event_log_push_structured(EVENT_TYPE_DIPLOMACY_ALLIANCE_ENDED, EVENT_SEVERITY_WARNING,
                                  civ_a, civ_b, -1, -1, 0, 0, "");
    } else if (new_state == DIPLOMACY_PEACE) {
        event_log_push_structured(EVENT_TYPE_DIPLOMACY_PEACE, EVENT_SEVERITY_INFO,
                                  civ_a, civ_b, -1, -1, 0, 0, "");
    } else if (new_state == DIPLOMACY_TENSE) {
        event_log_push_structured(EVENT_TYPE_DIPLOMACY_TENSE, EVENT_SEVERITY_WARNING,
                                  civ_a, civ_b, -1, -1, 0, 0, "");
    }
}

static int soft_transition_allowed(int civ_a, int civ_b, DiplomacyStatus current, DiplomacyStatus desired, const DiplomacyRelation *relation) {
    if (current == desired || desired == DIPLOMACY_ALLIANCE) return 1;
    if (current == DIPLOMACY_PEACE && desired == DIPLOMACY_TENSE && diplomacy_policy_severe_crisis(relation)) return 1;
    return diplomacy_stability_state_years(civ_a, civ_b) >= DIPLOMACY_SOFT_GRACE_YEARS;
}

static void prepare_soft_state(int civ_a, int civ_b, DiplomacyRelation *relation, DiplomacyStatus desired) {
    int allow;
    if (!relation) return;
    allow = soft_transition_allowed(civ_a, civ_b, relation->state, desired, relation);
    relation->state = diplomacy_stability_step_pair(civ_a, civ_b, relation->state, desired, allow);
    if (relation->state == desired && desired != DIPLOMACY_TENSE) relation->easing_years = 0;
}

static void refresh_known_relation(int civ_a, int civ_b) {
    DiplomacyRelation ab = diplomacy_matrix[civ_a][civ_b];
    DiplomacyRelation ba = diplomacy_matrix[civ_b][civ_a];
    DiplomacyRelation relation = ab;
    DiplomacyStatus old_state = ab.state;
    DiplomacyContactKind contact_kind;
    int score_ab, score_ba;
    if (relation.state == DIPLOMACY_NONE) return;
    if (relation.state != DIPLOMACY_VASSAL && (!is_sovereign_actor(civ_a) || !is_sovereign_actor(civ_b))) {
        if (relation.state == DIPLOMACY_ALLIANCE) log_relation_transition(civ_a, civ_b, relation.state, DIPLOMACY_NONE);
        set_relation_pair_shared(civ_a, civ_b, default_relation(DIPLOMACY_NONE, 0));
        diplomacy_relation_score_reset_pair(civ_a, civ_b);
        diplomacy_stability_reset_pair(civ_a, civ_b);
        return;
    }
    contact_kind = diplomacy_direct_contact_kind(civ_a, civ_b);
    relation.contact_kind = contact_kind;
    if (contact_kind == DIP_CONTACT_LAND_BORDER) {
        diplomacy_pair_contact_stats(civ_a, civ_b, &relation.border_length, &relation.natural_barrier);
    }
    else { relation.border_length = 0; relation.natural_barrier = 0; }
    if (contact_kind == DIP_CONTACT_NONE && relation.state != DIPLOMACY_WAR &&
        relation.state != DIPLOMACY_TRUCE &&
        relation.state != DIPLOMACY_VASSAL) {
        if (relation.state == DIPLOMACY_ALLIANCE) log_relation_transition(civ_a, civ_b, relation.state, DIPLOMACY_NONE);
        relation = default_relation(DIPLOMACY_NONE, 0);
        set_relation_pair_shared(civ_a, civ_b, relation);
        diplomacy_relation_score_reset_pair(civ_a, civ_b);
        diplomacy_stability_reset_pair(civ_a, civ_b);
        return;
    }
    relation.years_distant_known = 0;
    relation.trade_fit = compute_trade_fit(civ_a, civ_b);
    relation.resource_conflict = compute_resource_conflict(civ_a, civ_b);
    relation.border_tension = compute_border_tension(civ_a, civ_b, relation);
    relation.years_known++;
    ab = relation; ba = relation;
    ab.relation_score = diplomacy_matrix[civ_a][civ_b].relation_score;
    ba.relation_score = diplomacy_matrix[civ_b][civ_a].relation_score;
    score_ab = diplomacy_relation_score_apply_year(civ_a, civ_b, ab);
    score_ba = diplomacy_relation_score_apply_year(civ_b, civ_a, ba);
    relation.relation_score = score_ab;
    if (relation.state == DIPLOMACY_TRUCE) {
        relation.truce_years_left = clamp(relation.truce_years_left - 1, 0, 100);
        if (relation.truce_years_left == 0) {
            if (contact_kind == DIP_CONTACT_NONE) relation = default_relation(DIPLOMACY_NONE, 0);
            else {
                relation.state = DIPLOMACY_TENSE;
                relation.truce_initial_years = 0;
            }
            diplomacy_stability_force_pair(civ_a, civ_b, relation.state);
        }
    }
#if DIPLOMACY_ENABLE_ADVANCED_STATES
    else if (relation.state == DIPLOMACY_VASSAL) {
        relation.vassal_years++;
        vassal_try_auto_annex(&relation);
        relation.border_tension = clamp(relation.border_tension - 4, 0, 100);
        diplomacy_stability_force_pair(civ_a, civ_b, DIPLOMACY_VASSAL);
    } else if (relation.state == DIPLOMACY_ALLIANCE) {
        prepare_soft_state(civ_a, civ_b, &relation,
                           diplomacy_policy_alliance_exit_requested(&relation, score_ab, score_ba) ?
                           DIPLOMACY_PEACE : DIPLOMACY_ALLIANCE);
        diplomacy_policy_update_post_war_memory(&relation, diplomacy_policy_calm_post_war_relation(&relation));
    }
#endif
    else if (relation.state == DIPLOMACY_PEACE) {
        if (diplomacy_policy_peace_tense_requested(&relation, score_ab, score_ba)) {
            prepare_soft_state(civ_a, civ_b, &relation, DIPLOMACY_TENSE);
        }
#if DIPLOMACY_ENABLE_ADVANCED_STATES
        else if (diplomacy_policy_mutual_score(score_ab, score_ba) >= 80 &&
                 !diplomacy_policy_alliance_hard_blocked(&relation, contact_kind)) {
            prepare_soft_state(civ_a, civ_b, &relation, DIPLOMACY_ALLIANCE);
        }
#endif
        else {
            prepare_soft_state(civ_a, civ_b, &relation, DIPLOMACY_PEACE);
            diplomacy_policy_update_post_war_memory(&relation, diplomacy_policy_calm_post_war_relation(&relation));
        }
    } else if (relation.state == DIPLOMACY_TENSE) {
        int desire_a, desire_b;
        ab = relation; ba = relation; ab.relation_score = score_ab; ba.relation_score = score_ba;
        desire_a = war_desire_for_pair(civ_a, civ_b, ab);
        desire_b = war_desire_for_pair(civ_b, civ_a, ba);
        if ((score_ab <= -75 && desire_a >= 70) || (score_ba <= -75 && desire_b >= 70)) {
            int started = desire_a >= desire_b ? war_start(civ_a, civ_b) : war_start(civ_b, civ_a);
            if (started) relation = diplomacy_matrix[civ_a][civ_b];
        } else if (diplomacy_policy_tense_recovery_requested(&relation, score_ab, score_ba)) {
            prepare_soft_state(civ_a, civ_b, &relation, DIPLOMACY_PEACE);
            diplomacy_policy_update_post_war_memory(&relation, diplomacy_policy_calm_post_war_relation(&relation));
        } else {
            prepare_soft_state(civ_a, civ_b, &relation, DIPLOMACY_TENSE);
            diplomacy_policy_apply_tension_easing(&relation, desire_a, desire_b);
        }
    }
    log_relation_transition(civ_a, civ_b, old_state, relation.state);
    ab = relation; ba = relation;
    ab.relation_score = score_ab; ba.relation_score = score_ba;
    set_relation_pair_directional(civ_a, civ_b, ab, ba);
}
void diplomacy_update_contacts(void) {
    int a;
    int b;
    if (!diplomacy_contacts_dirty && !diplomacy_borders_dirty()) return;
    diplomacy_borders_ensure();
    for (a = 0; a < civ_count; a++) {
        if (!is_valid_civ(a)) continue;
        for (b = a + 1; b < civ_count; b++) {
            DiplomacyRelation relation;
            int border = 0;
            int barrier = 0;
            DiplomacyContactKind contact_kind;
            if (!is_sovereign_actor(a) || !is_sovereign_actor(b)) continue;
            contact_kind = diplomacy_direct_contact_kind(a, b);
            if (contact_kind == DIP_CONTACT_NONE) continue;
            relation = diplomacy_matrix[a][b];
            if (relation.state == DIPLOMACY_NONE) {
                relation = default_relation(DIPLOMACY_PEACE, same_heritage(a, b) ? 5 : 0);
                diplomacy_stability_force_pair(a, b, DIPLOMACY_PEACE);
                event_log_push_structured(EVENT_TYPE_DIPLOMACY_PEACE, EVENT_SEVERITY_INFO,
                                          a, b, -1, -1, 0, 0, "");
            }
            relation.contact_kind = contact_kind;
            relation.years_distant_known = 0;
            if (contact_kind == DIP_CONTACT_LAND_BORDER) {
                diplomacy_pair_contact_stats(a, b, &border, &barrier);
                relation.border_length = border;
                relation.natural_barrier = barrier;
            } else { relation.border_length = 0; relation.natural_barrier = 0; }
            set_relation_pair_shared(a, b, relation);
        }
    }
    diplomacy_contacts_dirty = 0;
}
void diplomacy_update_year(void) {
    int a;
    int b;
    diplomacy_update_contacts();
    war_desire_reset_all();
    diplomacy_relation_score_begin_year();
    for (a = 0; a < civ_count; a++) {
        if (!is_valid_civ(a)) continue;
        for (b = a + 1; b < civ_count; b++) {
            if (!is_valid_civ(b)) continue;
            refresh_known_relation(a, b);
        }
    }
    diplomacy_relation_score_end_year();
}
DiplomacyStatus diplomacy_status(int civ_a, int civ_b) {
    if (civ_a < 0 || civ_a >= MAX_CIVS || civ_b < 0 || civ_b >= MAX_CIVS) return DIPLOMACY_NONE;
    if (civ_a >= civ_count || civ_b >= civ_count || !civs[civ_a].alive || !civs[civ_b].alive) return DIPLOMACY_NONE;
    return diplomacy_matrix[civ_a][civ_b].state;
}
DiplomacyRelation diplomacy_relation(int civ_a, int civ_b) {
    if (civ_a < 0 || civ_a >= MAX_CIVS || civ_b < 0 || civ_b >= MAX_CIVS)
        return default_relation(DIPLOMACY_NONE, 0);
    if (civ_a >= civ_count || civ_b >= civ_count || !civs[civ_a].alive || !civs[civ_b].alive)
        return default_relation(DIPLOMACY_NONE, 0);
    return diplomacy_matrix[civ_a][civ_b];
}
void diplomacy_record_war_result_kind(int winner, int loser, DiplomacyLastWarResult result) {
    DiplomacyRelation win_rel, lose_rel;
    int winner_score = -25, loser_score = -45;
    if (winner < 0 || winner >= MAX_CIVS || loser < 0 || loser >= MAX_CIVS || winner == loser) return;
    if (!diplomacy_policy_last_war_result_has_winner(result)) result = DIP_LAST_WAR_MILITARY;
    if (result == DIP_LAST_WAR_SURRENDER || result == DIP_LAST_WAR_DECISIVE) { winner_score = -30; loser_score = -60; }
    win_rel = diplomacy_matrix[winner][loser];
    lose_rel = diplomacy_matrix[loser][winner];
    if (win_rel.state == DIPLOMACY_NONE) win_rel = default_relation(DIPLOMACY_PEACE, winner_score);
    if (lose_rel.state == DIPLOMACY_NONE) lose_rel = default_relation(DIPLOMACY_PEACE, loser_score);
    win_rel.last_war_winner = lose_rel.last_war_winner = winner;
    win_rel.last_war_loser = lose_rel.last_war_loser = loser;
    win_rel.last_war_result = lose_rel.last_war_result = result;
    win_rel.relation_score = winner_score; lose_rel.relation_score = loser_score;
    set_relation_pair_directional(winner, loser, win_rel, lose_rel);
    diplomacy_relation_score_reset_pair(winner, loser);
}
void diplomacy_record_war_result(int winner, int loser) {
    diplomacy_record_war_result_kind(winner, loser, DIP_LAST_WAR_MILITARY);
}
void diplomacy_record_war_no_winner(int civ_a, int civ_b, DiplomacyLastWarResult result) {
    DiplomacyRelation ab, ba;
    if (civ_a < 0 || civ_a >= MAX_CIVS || civ_b < 0 || civ_b >= MAX_CIVS || civ_a == civ_b) return;
    if (diplomacy_policy_last_war_result_has_winner(result) || result == DIP_LAST_WAR_NONE) result = DIP_LAST_WAR_NEGOTIATED_TRUCE;
    ab = diplomacy_matrix[civ_a][civ_b];
    ba = diplomacy_matrix[civ_b][civ_a];
    if (ab.state == DIPLOMACY_NONE) ab = default_relation(DIPLOMACY_PEACE, -20);
    if (ba.state == DIPLOMACY_NONE) ba = default_relation(DIPLOMACY_PEACE, -30);
    ab.last_war_winner = ba.last_war_winner = result == DIP_LAST_WAR_OFFENSIVE_HALTED ? civ_a : -1;
    ab.last_war_loser = ba.last_war_loser = result == DIP_LAST_WAR_OFFENSIVE_HALTED ? civ_b : -1;
    ab.last_war_result = ba.last_war_result = result;
    ab.relation_score = -20; ba.relation_score = -30;
    set_relation_pair_directional(civ_a, civ_b, ab, ba);
    diplomacy_relation_score_reset_pair(civ_a, civ_b);
}
void diplomacy_record_war_interrupted(int civ_a, int civ_b) {
    diplomacy_record_war_no_winner(civ_a, civ_b, DIP_LAST_WAR_FRONT_SEVERED);
}
int diplomacy_last_war_desire(int civ_id) { return war_desire_last_final(civ_id); }
const char *diplomacy_last_war_reason(int civ_id) {
    return war_desire_last_reason(civ_id);
}
void diplomacy_clear_civ(int civ_id) {
    int i;
    if (civ_id < 0 || civ_id >= MAX_CIVS) return;
    for (i = 0; i < MAX_CIVS; i++) {
        DiplomacyRelation r = default_relation(civ_id == i ? DIPLOMACY_PEACE : DIPLOMACY_NONE, civ_id == i ? 100 : 0);
        diplomacy_matrix[civ_id][i] = r; diplomacy_matrix[i][civ_id] = r;
    }
    diplomacy_relation_score_clear_civ(civ_id);
    diplomacy_stability_clear_civ(civ_id);
    diplomacy_borders_clear_civ(civ_id);
    war_desire_clear_civ(civ_id);
    diplomacy_mark_contacts_dirty();
}
void diplomacy_force_war(int civ_a, int civ_b) {
    DiplomacyRelation ab, ba;
    if (civ_a < 0 || civ_a >= MAX_CIVS || civ_b < 0 || civ_b >= MAX_CIVS || civ_a == civ_b) return;
    if (!is_sovereign_actor(civ_a) || !is_sovereign_actor(civ_b)) return;
    if (!war_has_active_front(civ_a, civ_b)) return;
    ab = diplomacy_matrix[civ_a][civ_b];
    ba = diplomacy_matrix[civ_b][civ_a];
    if (ab.state == DIPLOMACY_NONE) ab = default_relation(DIPLOMACY_PEACE, -75);
    if (ba.state == DIPLOMACY_NONE) ba = default_relation(DIPLOMACY_PEACE, -75);
    ab.state = ba.state = DIPLOMACY_WAR;
    ab.truce_years_left = ba.truce_years_left = 0; ab.truce_initial_years = ba.truce_initial_years = 0;
    ab.border_tension = ba.border_tension = clamp(ab.border_tension + 25, 0, 100);
    ab.easing_years = ba.easing_years = 0; ab.vassal_years = ba.vassal_years = 0;
    ab.overlord = ba.overlord = -1; ab.vassal = ba.vassal = -1;
    set_relation_pair_directional(civ_a, civ_b, ab, ba);
    diplomacy_stability_force_pair(civ_a, civ_b, DIPLOMACY_WAR);
}
void diplomacy_start_truce(int civ_a, int civ_b, int years, int relation_score) {
    DiplomacyRelation ab, ba;
    int fallback = diplomacy_relation_score_from_legacy(relation_score);
    if (civ_a < 0 || civ_a >= MAX_CIVS || civ_b < 0 || civ_b >= MAX_CIVS || civ_a == civ_b) return;
    ab = diplomacy_matrix[civ_a][civ_b];
    ba = diplomacy_matrix[civ_b][civ_a];
    if (ab.state == DIPLOMACY_NONE) ab = default_relation(DIPLOMACY_TRUCE, fallback);
    if (ba.state == DIPLOMACY_NONE) ba = default_relation(DIPLOMACY_TRUCE, fallback);
    ab.state = ba.state = DIPLOMACY_TRUCE;
    ab.contact_kind = ba.contact_kind = diplomacy_direct_contact_kind(civ_a, civ_b);
    ab.truce_years_left = ba.truce_years_left = clamp(years, 0, 100);
    ab.truce_initial_years = ba.truce_initial_years = clamp(years, 0, 100);
    if (ab.last_war_result == DIP_LAST_WAR_NONE) ab.relation_score = fallback;
    if (ba.last_war_result == DIP_LAST_WAR_NONE) ba.relation_score = fallback;
    ab.border_tension = ba.border_tension = clamp(ab.border_tension / 2, 0, 100);
    ab.easing_years = ba.easing_years = 0; ab.vassal_years = ba.vassal_years = 0;
    ab.overlord = ba.overlord = -1; ab.vassal = ba.vassal = -1;
    set_relation_pair_directional(civ_a, civ_b, ab, ba);
    diplomacy_stability_force_pair(civ_a, civ_b, DIPLOMACY_TRUCE);
    diplomacy_relation_score_reset_pair(civ_a, civ_b);
    if (years > 0) {
        event_log_push_structured(EVENT_TYPE_TRUCE_SIGNED, EVENT_SEVERITY_INFO,
                                  civ_a, civ_b, -1, -1, years, 0, "");
    }
}
void diplomacy_start_vassal(int overlord, int vassal, int relation_score) {
#if DIPLOMACY_ENABLE_ADVANCED_STATES
    DiplomacyRelation relation;
    int score = diplomacy_relation_score_from_legacy(relation_score);
    if (overlord < 0 || overlord >= MAX_CIVS || vassal < 0 || vassal >= MAX_CIVS || overlord == vassal) return;
    relation = diplomacy_matrix[overlord][vassal];
    if (relation.state == DIPLOMACY_NONE) relation = default_relation(DIPLOMACY_VASSAL, score);
    relation.state = DIPLOMACY_VASSAL; relation.truce_years_left = 0; relation.truce_initial_years = 0;
    relation.relation_score = score;
    relation.border_tension = clamp(relation.border_tension / 3, 0, 100);
    relation.easing_years = 0; relation.vassal_years = 0; relation.overlord = overlord; relation.vassal = vassal;
    set_relation_pair_shared(overlord, vassal, relation);
    diplomacy_stability_force_pair(overlord, vassal, DIPLOMACY_VASSAL);
    diplomacy_relation_score_reset_pair(overlord, vassal);
#else
    diplomacy_start_truce(overlord, vassal, 10, relation_score);
#endif
}
void diplomacy_restore_relation(int civ_a, int civ_b, DiplomacyRelation relation) {
    if (civ_a < 0 || civ_b < 0 || civ_a >= MAX_CIVS || civ_b >= MAX_CIVS) return;
    diplomacy_matrix[civ_a][civ_b] = relation;
    if (civ_a != civ_b) diplomacy_stability_force_pair(civ_a, civ_b, relation.state);
}
void diplomacy_sanitize_loaded(void) {
    int a, b;
    for (a = 0; a < MAX_CIVS; a++) for (b = 0; b < MAX_CIVS; b++) {
        DiplomacyRelation *r = &diplomacy_matrix[a][b];
        if (a == b || a >= civ_count || b >= civ_count || !civs[a].alive || !civs[b].alive)
            *r = default_relation(a == b ? DIPLOMACY_PEACE : DIPLOMACY_NONE, a == b ? 100 : 0);
        else if (!diplomacy_policy_last_war_result_valid(r->last_war_result)) {
            r->last_war_result = DIP_LAST_WAR_NONE; r->last_war_winner = -1; r->last_war_loser = -1;
        } else if (!diplomacy_policy_last_war_result_has_winner(r->last_war_result) &&
                   !(r->last_war_result == DIP_LAST_WAR_OFFENSIVE_HALTED &&
                     r->last_war_winner >= 0 && r->last_war_winner < civ_count &&
                     r->last_war_loser >= 0 && r->last_war_loser < civ_count &&
                     r->last_war_winner != r->last_war_loser)) {
            r->last_war_winner = -1; r->last_war_loser = -1;
        }
        r->relation_score = clamp(r->relation_score, -100, 100);
    }
    diplomacy_relation_score_reset();
    diplomacy_stability_reset();
    diplomacy_mark_contacts_dirty();
}
