#ifndef WORLD_SIM_STABILITY_DECISION_H
#define WORLD_SIM_STABILITY_DECISION_H

typedef enum {
    STABILITY_MODE_NORMAL,
    STABILITY_MODE_CAUTIOUS,
    STABILITY_MODE_REORGANIZING,
    STABILITY_MODE_CRISIS,
    STABILITY_MODE_EMERGENCY,
    STABILITY_MODE_COLLAPSE
} StabilityMode;

void stability_decision_reset(void);
void stability_decision_update_month(int civ_id);
void stability_decision_update_all(void);
StabilityMode stability_mode_for_civ(int civ_id);
int stability_mode_months_for_civ(int civ_id);
int stability_recovery_months_remaining(int civ_id);
int stability_apply_war_desire_gate(int civ_id, int target_id, int desire,
                                    int *out_penalty, int *out_blocked);
int stability_apply_expansion_desire_gate(int civ_id, int desire,
                                          int *out_penalty, int *out_blocked);
int stability_allows_new_war(int civ_id, int target_id);
int stability_allows_expansion_attempt(int civ_id);
int stability_allows_expansion_region(int civ_id, int region_id);
int stability_allows_overseas_expansion(int civ_id);
int stability_peace_pressure_bonus(int civ_id);
int stability_should_tail_cut_war(int civ_id);

#endif
