#include "sim/diplomacy_policy.h"

#include "core/constants.h"

#define POST_WAR_MEMORY_CLEAR_YEARS 5
#define TRUCE_END_TENSE_THRESHOLD 75

int diplomacy_policy_last_war_result_valid(int result) {
    return result >= DIP_LAST_WAR_NONE && result <= DIP_LAST_WAR_FRONT_SEVERED;
}

int diplomacy_policy_last_war_result_has_winner(int result) {
    return result == DIP_LAST_WAR_DECISIVE || result == DIP_LAST_WAR_MILITARY ||
           result == DIP_LAST_WAR_SURRENDER;
}

int diplomacy_policy_has_post_war_memory(const DiplomacyRelation *relation) {
    return relation && relation->last_war_result != DIP_LAST_WAR_NONE;
}

void diplomacy_policy_clear_post_war_memory(DiplomacyRelation *relation) {
    if (!relation) return;
    relation->last_war_result = DIP_LAST_WAR_NONE;
    relation->last_war_winner = -1;
    relation->last_war_loser = -1;
}

int diplomacy_policy_calm_post_war_relation(const DiplomacyRelation *relation) {
    return relation && relation->relation_score >= -10 && relation->border_tension < 55 &&
           relation->resource_conflict < 80;
}

int diplomacy_policy_severe_pressure(const DiplomacyRelation *relation) {
    return relation && (relation->border_tension >= TRUCE_END_TENSE_THRESHOLD ||
                        relation->resource_conflict >= 80);
}

int diplomacy_policy_severe_crisis(const DiplomacyRelation *relation) {
    return relation && (relation->border_tension >= 90 || relation->resource_conflict >= 90);
}

int diplomacy_policy_mutual_score(int score_ab, int score_ba) {
    return score_ab < score_ba ? score_ab : score_ba;
}

int diplomacy_policy_alliance_exit_requested(const DiplomacyRelation *relation,
                                             int score_ab, int score_ba) {
    int mutual = diplomacy_policy_mutual_score(score_ab, score_ba);
    return relation && (mutual <= 55 || diplomacy_policy_severe_pressure(relation));
}

int diplomacy_policy_peace_tense_requested(const DiplomacyRelation *relation,
                                           int score_ab, int score_ba) {
    int mutual = diplomacy_policy_mutual_score(score_ab, score_ba);
    return relation && (mutual <= -45 || diplomacy_policy_severe_pressure(relation));
}

int diplomacy_policy_tense_recovery_requested(const DiplomacyRelation *relation,
                                              int score_ab, int score_ba) {
    int mutual = diplomacy_policy_mutual_score(score_ab, score_ba);
    if (!relation || relation->truce_years_left > 0) return 0;
    if (mutual >= 70) return 1;
    return mutual >= -20 && relation->border_tension < 60 &&
           relation->resource_conflict < 70;
}

void diplomacy_policy_update_post_war_memory(DiplomacyRelation *relation, int calm) {
    if (!diplomacy_policy_has_post_war_memory(relation)) return;
    if (!calm) {
        relation->easing_years = 0;
        return;
    }
    relation->easing_years = clamp(relation->easing_years + 1, 0, 100);
    if (relation->easing_years >= POST_WAR_MEMORY_CLEAR_YEARS) {
        diplomacy_policy_clear_post_war_memory(relation);
    }
}

void diplomacy_policy_apply_tension_easing(DiplomacyRelation *relation, int desire_a, int desire_b) {
    int calm;
    if (!relation) return;
    calm = relation->trade_fit >= 40 && relation->border_tension < 45 &&
           relation->resource_conflict < 55 && desire_a < 45 && desire_b < 45;
    if (!calm) {
        relation->easing_years = 0;
        return;
    }
    relation->easing_years = clamp(relation->easing_years + 1, 0, 100);
    if (relation->easing_years >= 3) {
        relation->border_tension = clamp(relation->border_tension - 2, 0, 100);
    }
    if (diplomacy_policy_has_post_war_memory(relation) &&
        relation->easing_years >= POST_WAR_MEMORY_CLEAR_YEARS) {
        diplomacy_policy_clear_post_war_memory(relation);
    }
}

int diplomacy_policy_alliance_hard_blocked(const DiplomacyRelation *relation, int contact_kind) {
    if (!relation) return 1;
    if (contact_kind == DIP_CONTACT_NONE) return 1;
    return relation->state == DIPLOMACY_WAR || relation->state == DIPLOMACY_TRUCE ||
           relation->state == DIPLOMACY_VASSAL;
}

const char *diplomacy_policy_alliance_block_reason(const DiplomacyRelation *relation, int contact_kind) {
    if (!relation) return "invalid";
    if (contact_kind == DIP_CONTACT_NONE) return "no-contact";
    if (relation->state == DIPLOMACY_WAR) return "war";
    if (relation->state == DIPLOMACY_TRUCE) return "truce";
    if (relation->state == DIPLOMACY_VASSAL) return "vassal";
    return "";
}
