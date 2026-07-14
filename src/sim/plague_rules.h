#ifndef WORLD_SIM_PLAGUE_RULES_H
#define WORLD_SIM_PLAGUE_RULES_H

#include "sim/plague_types.h"

int plague_rules_absolute_month(int year, int month);
int plague_rules_scheduled_check_due(int absolute_month);
int plague_rules_next_scheduled_check(int absolute_month);
PlagueSize plague_rules_size_from_roll(int roll_0_to_99);
PlagueSize plague_rules_size_from_distribution(
    const PlagueProbabilityDistribution *probabilities, int roll_0_to_99);
int plague_rules_size_values(PlagueSize size, PlagueSizeRules *out);
int plague_rules_spore_budget(PlagueSize size, int frozen_occupied_cities);
int plague_rules_severity_from_roll(PlagueSize size, int roll);
int plague_rules_infection_duration_from_roll(int roll);
int plague_rules_initial_pulse_count(int duration_months);
int plague_rules_pulse_is_before_recovery(int pulse_month, int recovery_month);
int plague_rules_persistence_legal(int recovery_month, int cap_month);
void plague_rules_action_weights(PlagueSize size, int remaining_months,
                                 int last_active_city, int eligible_target_count,
                                 PlagueActionWeights *out);
PlagueAction plague_rules_choose_action(PlagueSize size, int remaining_months,
                                        int last_active_city, int eligible_target_count,
                                        int spread_legal, int persist_legal, int roll);
void plague_rules_route_weights(int deep_unlocked, int land_available,
                                int shallow_available, int deep_available,
                                PlagueRouteWeights *out);
int plague_rules_route_legal(PlagueRouteType route, int deep_unlocked);
PlagueRouteType plague_rules_choose_route(const PlagueRouteWeights *weights, int roll);
int plague_rules_immunity_percent_for_duration(int episode_duration_months);
int plague_rules_next_immunity_tier(int duration_months, int *out_percent,
                                    int *out_months_remaining);
int plague_rules_immunity_weight_percent(int immunity_percent);
int plague_rules_annual_mortality_percent(int severity);
uint64_t plague_rules_monthly_mortality_q32(int severity);
int plague_rules_monthly_deaths(int surviving_population, int severity,
                                uint64_t *carry_q32);

#endif
