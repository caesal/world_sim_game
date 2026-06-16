#ifndef WORLD_SIM_DIPLOMACY_POLICY_H
#define WORLD_SIM_DIPLOMACY_POLICY_H

#include "sim/diplomacy.h"

int diplomacy_policy_last_war_result_valid(int result);
int diplomacy_policy_last_war_result_has_winner(int result);
int diplomacy_policy_has_post_war_memory(const DiplomacyRelation *relation);
void diplomacy_policy_clear_post_war_memory(DiplomacyRelation *relation);
int diplomacy_policy_calm_post_war_relation(const DiplomacyRelation *relation);
int diplomacy_policy_severe_pressure(const DiplomacyRelation *relation);
int diplomacy_policy_severe_crisis(const DiplomacyRelation *relation);
int diplomacy_policy_mutual_score(int score_ab, int score_ba);
int diplomacy_policy_alliance_exit_requested(const DiplomacyRelation *relation,
                                             int score_ab, int score_ba);
int diplomacy_policy_peace_tense_requested(const DiplomacyRelation *relation,
                                           int score_ab, int score_ba);
int diplomacy_policy_tense_recovery_requested(const DiplomacyRelation *relation,
                                              int score_ab, int score_ba);
void diplomacy_policy_update_post_war_memory(DiplomacyRelation *relation, int calm);
void diplomacy_policy_apply_tension_easing(DiplomacyRelation *relation, int desire_a, int desire_b);
int diplomacy_policy_alliance_hard_blocked(const DiplomacyRelation *relation, int contact_kind);
const char *diplomacy_policy_alliance_block_reason(const DiplomacyRelation *relation, int contact_kind);

#endif
