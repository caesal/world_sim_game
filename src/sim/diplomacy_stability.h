#ifndef WORLD_SIM_DIPLOMACY_STABILITY_H
#define WORLD_SIM_DIPLOMACY_STABILITY_H

#include "sim/diplomacy.h"

#define DIPLOMACY_SOFT_TRANSITION_YEARS 5
#define DIPLOMACY_SOFT_GRACE_YEARS 10

void diplomacy_stability_reset(void);
void diplomacy_stability_clear_civ(int civ_id);
void diplomacy_stability_reset_pair(int civ_a, int civ_b);
void diplomacy_stability_force_pair(int civ_a, int civ_b, DiplomacyStatus state);
DiplomacyStatus diplomacy_stability_step_pair(int civ_a, int civ_b,
                                              DiplomacyStatus current,
                                              DiplomacyStatus desired,
                                              int allow_soft_change);
int diplomacy_stability_state_years(int civ_a, int civ_b);
int diplomacy_stability_candidate_state(int civ_a, int civ_b);
int diplomacy_stability_candidate_years(int civ_a, int civ_b);
int diplomacy_stability_grace_years_left(int civ_a, int civ_b);

#endif
